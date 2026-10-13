// F2a: Flush가 완료되면 그때까지 쓴 데이터가 NAND에 있다 (버퍼에만 있으면 안 된다). 정상 종료도 같다
#include "hcheck.h"

static unsigned char buf[64 * 4096];

static int on_nand(unsigned long long lba, unsigned gen)
{
	unsigned char want[4096];
	unsigned d, b, r;
	hfill(want, lba, 1, gen);
	return sim_find_row(want, 4096, &d, &b, &r);
}

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	hfill(buf, 500, 40, 3);
	MUST(SIM_SUCCESS(sim_write(500, 40, buf)), "쓰기");
	int before = 0;
	for (unsigned i = 0; i < 40; i += 4)
		before += on_nand(500 + i, 3);
	sim_status s = sim_flush();
	EXPECT(SIM_SUCCESS(s), "Flush %s", hst(s));
	int missing = 0;
	for (unsigned i = 0; i < 40; i += 4)
		if (!on_nand(500 + i, 3))
			missing++;
	EXPECT(missing == 0, "Flush 뒤에도 NAND에 없는 슬라이스 %d개 (Flush 전에 이미 있던 것 %d개)", missing, before);
	/* 정상 종료 */
	hfill(buf, 900, 16, 4);
	MUST(SIM_SUCCESS(sim_write(900, 16, buf)), "쓰기");
	EXPECT(sim_shutdown(), "정상 종료가 끝나지 않음");
	missing = 0;
	for (unsigned i = 0; i < 16; i += 4)
		if (!on_nand(900 + i, 4))
			missing++;
	EXPECT(missing == 0, "정상 종료 뒤에도 NAND에 없는 슬라이스 %d개", missing);
	return DONE();
}

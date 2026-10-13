// F2b: spare 메타가 깨진 페이지는 믿지 않는다. 같은 LBA의 최신 판의 메타가 깨지면 그 앞의 온전한 판이 읽힌다
#include "hcheck.h"

static unsigned char buf[16 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	hfill(buf, 4000, 4, 1);
	MUST(SIM_SUCCESS(sim_write(4000, 4, buf)), "1판 쓰기");
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	hfill(buf, 4000, 4, 2);
	MUST(SIM_SUCCESS(sim_write(4000, 4, buf)), "2판 쓰기");
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	sim_power_cut();
	unsigned d, b, r;
	hfill(buf, 4000, 1, 2);
	MUST(sim_find_row(buf, 4096, &d, &b, &r), "2판을 NAND에서 못 찾음");
	/* 2판 페이지의 spare(ECC가 쓰는 끝 8바이트 앞까지)의 비트를 뒤집는다 */
	unsigned char *sp = (unsigned char *)sim_row_spare(d, b, r);
	for (unsigned i = 1; i < sim_geo().spareBytes - 8; i++)
		sp[i] ^= 0x5A;
	MUST(sim_power_on(), "부팅");
	MUST(SIM_SUCCESS(sim_read(4000, 4, buf)), "읽기");
	EXPECT(hsame(buf, 4000, 4, 1), "메타가 깨진 2판 대신 1판이 읽혀야 함 (앞 8바이트 %llu, 세대 %u)", *(unsigned long long *)buf,
		*(unsigned *)(buf + 8));
	return DONE();
}

// F1a: 아직 NAND에 내리지 않은(버퍼에만 있는) 슬라이스를 deallocate하면 나중에 버퍼를 비울 때 되살아나면 안 된다
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	/* NAND에 옛 판을 둔다 */
	hfill(buf, 4000, 32, 1);
	MUST(SIM_SUCCESS(sim_write(4000, 32, buf)), "쓰기");
	for (unsigned i = 0; i < 2; i++) {
		hfill(buf, 8000 + i * 256, 256, 1);
		MUST(SIM_SUCCESS(sim_write(8000 + i * 256, 256, buf)), "내보내기");
	}
	/* 새 판을 써서 버퍼에만 둔다 */
	hfill(buf, 4000, 32, 2);
	MUST(SIM_SUCCESS(sim_write(4000, 32, buf)), "새 판 쓰기");
	unsigned long long r[][2] = { { 4000, 32 } };
	sim_status s = hdsm(1, 1, r);
	EXPECT(SIM_SUCCESS(s), "deallocate %s", hst(s));
	EXPECT(SIM_SUCCESS(sim_read(4000, 32, buf)) && hzero(buf, 32 * 4096), "deallocate 바로 뒤 0이 아님");
	/* 버퍼를 돌려 더러운 항목이 있었다면 내려가게 한다 */
	for (unsigned i = 0; i < 2; i++) {
		hfill(buf, 12000 + i * 256, 256, 1);
		MUST(SIM_SUCCESS(sim_write(12000 + i * 256, 256, buf)), "버퍼 돌리기");
	}
	EXPECT(SIM_SUCCESS(sim_read(4000, 32, buf)) && hzero(buf, 32 * 4096), "버퍼를 비운 뒤 deallocate한 데이터가 되살아남 (앞 8바이트 %llu)",
		*(unsigned long long *)buf);
	return DONE();
}

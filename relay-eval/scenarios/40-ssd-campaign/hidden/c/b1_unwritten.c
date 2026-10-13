// B1: 쓰지 않은 LBA를 읽으면 0이어야 한다 (버퍼에 남은 다른 LBA의 데이터가 나오면 안 된다)
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	for (unsigned i = 0; i < 4; i++) {
		hfill(buf, i * 256, 256, 1);
		MUST(SIM_SUCCESS(sim_write(i * 256, 256, buf)), "쓰기");
	}
	/* 버퍼 항목을 쓴 데이터로 채우고 돌린다 */
	for (unsigned i = 0; i < 4; i++)
		MUST(SIM_SUCCESS(sim_read(i * 256, 256, buf)), "읽기");
	unsigned long long lbas[] = { 20000, 20005, 21003, 9000 };
	for (unsigned k = 0; k < 4; k++) {
		memset(buf, 0xEE, 4 * 4096);
		sim_status s = sim_read(lbas[k], 4, buf);
		EXPECT(SIM_SUCCESS(s), "LBA %llu 읽기 %s", lbas[k], hst(s));
		EXPECT(hzero(buf, 4 * 4096), "쓰지 않은 LBA %llu~%llu가 0이 아님 (앞 8바이트 %llu)", lbas[k], lbas[k] + 3,
			*(unsigned long long *)buf);
	}
	return DONE();
}

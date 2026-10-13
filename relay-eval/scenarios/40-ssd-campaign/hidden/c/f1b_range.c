// F1b: 범위 하나라도 이름공간 밖이면 LBA Out of Range(SCT 0, SC 80h)이고 다른 범위도 처리하지 않는다
#include "hcheck.h"

static unsigned char buf[64 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	hfill(buf, 0, 16, 1);
	MUST(SIM_SUCCESS(sim_write(0, 16, buf)), "쓰기");
	hfill(buf, cap - 8, 8, 1);
	MUST(SIM_SUCCESS(sim_write(cap - 8, 8, buf)), "끝 쓰기");
	unsigned long long r[][2] = { { 0, 8 }, { cap - 4, 8 } };
	sim_status s = hdsm(1, 2, r);
	EXPECT(s.ok && s.sct == 0 && s.sc == 0x80, "끝을 넘는 범위: %s", hst(s));
	MUST(SIM_SUCCESS(sim_read(0, 16, buf)), "읽기");
	EXPECT(hsame(buf, 0, 16, 1), "거절된 명령의 다른 범위(0~7)가 deallocate됨");
	MUST(SIM_SUCCESS(sim_read(cap - 8, 8, buf)), "읽기");
	EXPECT(hsame(buf, cap - 8, 8, 1), "거절된 명령의 범위 안쪽이 deallocate됨");
	unsigned long long r2[][2] = { { cap + 100, 4 } };
	s = hdsm(1, 1, r2);
	EXPECT(s.ok && s.sct == 0 && s.sc == 0x80, "밖에서 시작하는 범위: %s", hst(s));
	/* 끝까지는 된다 */
	unsigned long long r3[][2] = { { cap - 8, 8 } };
	s = hdsm(1, 1, r3);
	EXPECT(SIM_SUCCESS(s), "끝까지의 범위: %s", hst(s));
	MUST(SIM_SUCCESS(sim_read(cap - 8, 8, buf)), "읽기");
	EXPECT(hzero(buf, 8 * 4096), "끝까지의 범위가 0이 아님");
	return DONE();
}

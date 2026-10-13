// F1b: 슬라이스 일부만 걸친 deallocate는 그 LBA만 0이 되고 나머지는 그대로 (NAND에 있을 때와 버퍼에 있을 때, 쓰지 않은 슬라이스)
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	hfill(buf, 100, 8, 1);
	MUST(SIM_SUCCESS(sim_write(100, 8, buf)), "쓰기");
	for (unsigned i = 0; i < 2; i++) {
		hfill(buf, 8000 + i * 256, 256, 1);
		MUST(SIM_SUCCESS(sim_write(8000 + i * 256, 256, buf)), "내보내기");
	}
	/* NAND에 있는 슬라이스 100~103의 101~102, 슬라이스에 걸친 103~104 */
	unsigned long long r[][2] = { { 101, 2 }, { 105, 2 } };
	sim_status s = hdsm(1, 2, r);
	EXPECT(SIM_SUCCESS(s), "deallocate %s", hst(s));
	MUST(SIM_SUCCESS(sim_read(100, 8, buf)), "읽기");
	EXPECT(hsame(buf, 100, 1, 1), "100이 바뀜");
	EXPECT(hzero(buf + 1 * 4096, 2 * 4096), "101~102가 0이 아님");
	EXPECT(hsame(buf + 3 * 4096, 103, 2, 1), "103~104가 바뀜");
	EXPECT(hzero(buf + 5 * 4096, 2 * 4096), "105~106이 0이 아님");
	EXPECT(hsame(buf + 7 * 4096, 107, 1, 1), "107이 바뀜");
	/* 버퍼에서 내보낸 뒤에도 같다 */
	for (unsigned i = 0; i < 2; i++)
		MUST(SIM_SUCCESS(sim_read(8000 + i * 256, 256, buf)), "버퍼 돌리기");
	MUST(SIM_SUCCESS(sim_read(100, 8, buf)), "읽기");
	EXPECT(hsame(buf, 100, 1, 1) && hzero(buf + 4096, 2 * 4096) && hsame(buf + 3 * 4096, 103, 2, 1) &&
			hzero(buf + 5 * 4096, 2 * 4096) && hsame(buf + 7 * 4096, 107, 1, 1),
		"버퍼를 돌린 뒤 부분 deallocate 결과가 다름");
	/* 버퍼에만 있는(더러운) 슬라이스의 일부 */
	hfill(buf, 200, 4, 2);
	MUST(SIM_SUCCESS(sim_write(200, 4, buf)), "쓰기");
	unsigned long long r2[][2] = { { 202, 1 } };
	EXPECT(SIM_SUCCESS(hdsm(1, 1, r2)), "deallocate");
	for (unsigned i = 0; i < 2; i++)
		MUST(SIM_SUCCESS(sim_read(8000 + i * 256, 256, buf)), "버퍼 돌리기");
	MUST(SIM_SUCCESS(sim_read(200, 4, buf)), "읽기");
	EXPECT(hsame(buf, 200, 2, 2) && hzero(buf + 2 * 4096, 4096) && hsame(buf + 3 * 4096, 203, 1, 2), "버퍼에 있던 슬라이스의 부분 deallocate");
	/* 쓰지 않은 슬라이스의 일부: 그대로 0 */
	unsigned long long r3[][2] = { { 20001, 2 } };
	EXPECT(SIM_SUCCESS(hdsm(1, 1, r3)), "deallocate");
	MUST(SIM_SUCCESS(sim_read(20000, 4, buf)), "읽기");
	EXPECT(hzero(buf, 4 * 4096), "쓰지 않은 슬라이스");
	EXPECT(sim_violations() == 0, "NAND 위반 %s", sim_violation_msg());
	return DONE();
}

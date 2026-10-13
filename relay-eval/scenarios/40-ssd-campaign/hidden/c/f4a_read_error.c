// F4a: 끝내 읽지 못한 읽기는 Unrecovered Read Error(SCT 2, SC 81h)이고 깨진 데이터를 성공으로 돌려주지 않는다(다시 읽어도).
// 몇 번 다시 읽어 읽히면 성공이다. 다른 LBA는 영향이 없다
#include "hcheck.h"

static unsigned char buf[64 * 4096];

static void evict(void)
{
	for (unsigned i = 0; i < 80; i++)
		MUST(SIM_SUCCESS(sim_read(16000 + i * 4, 4, buf)), "버퍼 돌리기");
}

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	hfill(buf, 1000, 64, 1);
	MUST(SIM_SUCCESS(sim_write(1000, 64, buf)), "쓰기");
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	evict();
	unsigned d, b, r;
	hfill(buf, 1008, 1, 1);
	MUST(sim_find_row(buf, 4096, &d, &b, &r), "1008의 페이지를 못 찾음");
	sim_fail_read(d, b, r, 1, -1);
	sim_status s = sim_read(1008, 4, buf);
	EXPECT(s.ok && s.sct == 2 && s.sc == 0x81, "고칠 수 없는 읽기: %s", hst(s));
	s = sim_read(1008, 4, buf);
	EXPECT(s.ok && s.sct == 2 && s.sc == 0x81, "다시 읽어도 Unrecovered Read Error여야 함(버퍼의 깨진 데이터를 성공으로): %s", hst(s));
	s = sim_read(1004, 8, buf);
	EXPECT(s.ok && s.sct == 2 && s.sc == 0x81, "실패한 슬라이스를 포함한 읽기: %s", hst(s));
	s = sim_read(1012, 4, buf);
	EXPECT(SIM_SUCCESS(s) && hsame(buf, 1012, 4, 1), "다른 슬라이스 읽기: %s", hst(s));
	s = sim_read(1000, 8, buf);
	EXPECT(SIM_SUCCESS(s) && hsame(buf, 1000, 8, 1), "앞 슬라이스 읽기: %s", hst(s));
	/* 몇 번 다시 읽으면 읽히는 경우 */
	hfill(buf, 1040, 1, 1);
	MUST(sim_find_row(buf, 4096, &d, &b, &r), "1040의 페이지를 못 찾음");
	sim_fail_read(d, b, r, 1, 3);
	s = sim_read(1040, 4, buf);
	EXPECT(SIM_SUCCESS(s) && hsame(buf, 1040, 4, 1), "다시 읽어 읽히면 성공: %s", hst(s));
	EXPECT(sim_running(), "펌웨어가 멈춤");
	return DONE();
}

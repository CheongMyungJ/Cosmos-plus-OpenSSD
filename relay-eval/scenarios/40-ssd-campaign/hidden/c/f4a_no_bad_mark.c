// F4a: 읽기 실패와 비트 오류가 많은 읽기는 블록을 배드로 만들지 않는다. 정상 종료 뒤 다시 켜도 그 블록의 다른 데이터가 그대로다
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
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	for (unsigned i = 0; i < 80; i++)
		MUST(SIM_SUCCESS(sim_read(16000 + i * 4, 4, buf)), "버퍼 돌리기");
	unsigned d, b, r;
	hfill(buf, 300, 1, 1);
	MUST(sim_find_row(buf, 4096, &d, &b, &r), "300의 페이지를 못 찾음");
	sim_fail_read(d, b, r, 1, -1);
	hfill(buf, 520, 1, 1);
	MUST(sim_find_row(buf, 4096, &d, &b, &r), "520의 페이지를 못 찾음");
	sim_fail_read(d, b, r, 2, -1);
	sim_status s = sim_read(300, 4, buf);
	EXPECT(s.ok && s.sct == 2 && s.sc == 0x81, "고칠 수 없는 읽기: %s", hst(s));
	s = sim_read(520, 4, buf);
	EXPECT(SIM_SUCCESS(s) && hsame(buf, 520, 4, 1), "비트 오류가 많아도 고쳐지면 성공: %s", hst(s));
	MUST(sim_shutdown(), "정상 종료");
	MUST(sim_power_on(), "부팅");
	int bad = 0;
	for (unsigned l = 0; l < 1024; l += 4) {
		if (l == 300)
			continue;
		s = sim_read(l, 4, buf);
		if (!SIM_SUCCESS(s) || !hsame(buf, l, 4, 1))
			bad++;
	}
	EXPECT(bad == 0, "다시 켠 뒤 읽기 실패나 경고가 난 블록의 데이터가 사라짐 (%d개 슬라이스)", bad);
	return DONE();
}

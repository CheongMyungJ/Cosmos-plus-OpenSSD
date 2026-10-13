// B2: 이름공간 끝을 넘는 읽기·쓰기는 LBA Out of Range(SCT 0, SC 80h)로 거절하고 한 블록도 쓰지 않는다. 펌웨어는 멈추지 않는다
#include "hcheck.h"

static unsigned char buf[64 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	hfill(buf, cap - 8, 8, 1);
	MUST(SIM_SUCCESS(sim_write(cap - 8, 8, buf)), "끝 8블록 쓰기");
	/* 끝에 걸친 쓰기 */
	hfill(buf, cap - 4, 8, 2);
	sim_status s = sim_write(cap - 4, 8, buf);
	EXPECT(s.ok && s.sct == 0 && s.sc == 0x80, "끝에 걸친 쓰기: %s", hst(s));
	MUST(sim_running(), "끝에 걸친 쓰기 뒤 펌웨어가 멈춤");
	MUST(SIM_SUCCESS(sim_read(cap - 8, 8, buf)), "읽기");
	EXPECT(hsame(buf, cap - 8, 8, 1), "거절된 쓰기가 범위 안의 블록을 바꿈");
	/* 밖에서 시작하는 쓰기·읽기 */
	s = sim_write(cap, 1, buf);
	EXPECT(s.ok && s.sct == 0 && s.sc == 0x80, "끝 바로 뒤 쓰기: %s", hst(s));
	MUST(sim_running(), "밖 쓰기 뒤 펌웨어가 멈춤");
	s = sim_write(cap + 1000, 4, buf);
	EXPECT(s.ok && s.sct == 0 && s.sc == 0x80, "밖 쓰기: %s", hst(s));
	MUST(sim_running(), "밖 쓰기 뒤 펌웨어가 멈춤");
	s = sim_read(cap + 5, 2, buf);
	EXPECT(s.ok && s.sct == 0 && s.sc == 0x80, "밖 읽기: %s", hst(s));
	MUST(sim_running(), "밖 읽기 뒤 펌웨어가 멈춤");
	s = sim_read(cap - 2, 4, buf);
	EXPECT(s.ok && s.sct == 0 && s.sc == 0x80, "끝에 걸친 읽기: %s", hst(s));
	/* 64비트 시작 LBA의 위쪽 */
	uint32_t dw[16] = { 0 };
	dw[0] = 0x01;
	dw[1] = 1;
	dw[10] = 0;
	dw[11] = 1;
	dw[12] = 0;
	s = sim_io(dw, buf, 4096, 1);
	EXPECT(s.ok && s.sct == 0 && s.sc == 0x80, "위쪽 32비트가 있는 시작 LBA: %s", hst(s));
	MUST(sim_running(), "펌웨어가 멈춤");
	/* 그 뒤에도 정상 */
	hfill(buf, 100, 4, 3);
	EXPECT(SIM_SUCCESS(sim_write(100, 4, buf)), "정상 쓰기");
	EXPECT(SIM_SUCCESS(sim_read(100, 4, buf)) && hsame(buf, 100, 4, 3), "정상 읽기");
	EXPECT(SIM_SUCCESS(sim_read(cap - 8, 8, buf)) && hsame(buf, cap - 8, 8, 1), "끝 블록 그대로");
	return DONE();
}

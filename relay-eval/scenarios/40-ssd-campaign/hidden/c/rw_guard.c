// 기존 읽기·쓰기가 그대로인지: 정렬된 쓰기와 읽기, 덮어쓰기, 쓴 슬라이스 안의 부분 쓰기, 큰 전송, 마지막 LBA
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	MUST(cap > 1024, "용량 %llu", (unsigned long long)cap);
	hfill(buf, 100, 8, 1);
	EXPECT(SIM_SUCCESS(sim_write(100, 8, buf)), "쓰기");
	memset(buf, 0, 8 * 4096);
	EXPECT(SIM_SUCCESS(sim_read(100, 8, buf)), "읽기");
	EXPECT(hsame(buf, 100, 8, 1), "읽은 데이터");
	for (unsigned g = 2; g <= 4; g++) {
		hfill(buf, 40, 16, g);
		EXPECT(SIM_SUCCESS(sim_write(40, 16, buf)), "덮어쓰기");
	}
	EXPECT(SIM_SUCCESS(sim_read(40, 16, buf)), "읽기");
	EXPECT(hsame(buf, 40, 16, 4), "덮어쓴 뒤 최신");
	hfill(buf, 0, 8, 1);
	EXPECT(SIM_SUCCESS(sim_write(0, 8, buf)), "쓰기");
	hfill(buf, 5, 2, 2);
	EXPECT(SIM_SUCCESS(sim_write(5, 2, buf)), "부분 쓰기");
	/* 버퍼에서 내보내고 NAND에서 다시 읽는다 */
	for (unsigned i = 0; i < 80; i++) {
		hfill(buf, 2000 + i * 4, 4, 9);
		sim_write(2000 + i * 4, 4, buf);
	}
	EXPECT(SIM_SUCCESS(sim_read(0, 8, buf)), "읽기");
	EXPECT(hsame(buf, 0, 5, 1) && hsame(buf + 5 * 4096, 5, 2, 2) && hsame(buf + 7 * 4096, 7, 1, 1), "부분 쓰기 뒤 슬라이스");
	hfill(buf, 4096, 256, 3);
	EXPECT(SIM_SUCCESS(sim_write(4096, 256, buf)), "큰 쓰기");
	memset(buf, 0, sizeof buf);
	EXPECT(SIM_SUCCESS(sim_read(4096, 256, buf)), "큰 읽기");
	EXPECT(hsame(buf, 4096, 256, 3), "큰 전송 데이터");
	hfill(buf, cap - 4, 4, 5);
	EXPECT(SIM_SUCCESS(sim_write(cap - 4, 4, buf)), "마지막 LBA 쓰기");
	EXPECT(SIM_SUCCESS(sim_read(cap - 4, 4, buf)) && hsame(buf, cap - 4, 4, 5), "마지막 LBA 읽기");
	EXPECT(sim_violations() == 0, "NAND 위반 %s", sim_violation_msg());
	return DONE();
}

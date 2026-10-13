// spare 첫 바이트(0번 행의 공장 배드 블록 표시 자리)에 0xFF가 아닌 값을 쓰지 않는다. GC가 도는 부하에서 본다
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	for (unsigned g = 1; g <= 2; g++)
		for (uint64_t l = 0; l + 256 <= cap; l += 256) {
			hfill(buf, l, 256, g);
			MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
		}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	EXPECT(sim_bad_mark_writes() == 0, "공장 배드 마크 자리에 0xFF가 아닌 값을 쓴 프로그램 %u번", sim_bad_mark_writes());
	return DONE();
}

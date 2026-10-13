// F2b: 부팅이 사용자 블록을 모두 지우지 않는다 (데이터가 남아야 하고, 부팅마다 블록을 지우면 수명도 준다)
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	hfill(buf, 0, 256, 1);
	MUST(SIM_SUCCESS(sim_write(0, 256, buf)), "쓰기");
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	sim_power_cut();
	uint64_t before = sim_op_count(SIM_OP_ERASE);
	MUST(sim_power_on(), "부팅");
	uint64_t erases = sim_op_count(SIM_OP_ERASE) - before;
	EXPECT(erases <= 8, "부팅 한 번에 지우기 %llu번", (unsigned long long)erases);
	EXPECT(SIM_SUCCESS(sim_read(0, 256, buf)) && hsame(buf, 0, 256, 1), "부팅 뒤 데이터");
	return DONE();
}

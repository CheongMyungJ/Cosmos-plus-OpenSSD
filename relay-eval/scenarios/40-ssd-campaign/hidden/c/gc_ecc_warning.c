// GC가 옮기려고 읽는 페이지에서 비트 오류가 문턱을 넘어도(고칠 수 있음) GC 읽기는 경고로 다루지 않는다(일부러 끔).
// die 0의 모든 페이지에 "비트 오류 많음"을 넣고 덮어써 GC를 돌린 뒤, 펌웨어 로그의 경고 수와 NAND 상태를 찍는다
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	for (uint64_t l = 0; l + 256 <= cap; l += 256) {
		hfill(buf, l, 256, 1);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
	}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	sim_geometry g = sim_geo();
	unsigned faults = 0;
	for (unsigned b = 1; b < g.blocksPerDie && faults < 60; b++)
		for (unsigned r = 0; r < g.rowsPerBlock && faults < 60; r++)
			if (sim_row_programmed(0, b, r)) {
				sim_fail_read(0, b, r, 2, -1);
				faults++;
			}
	uint64_t logBefore = strlen(sim_log_tail(SIM_LOG_BYTES));
	(void)logBefore;
	unsigned seed = 99;
	for (int i = 0; i < 6000; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = (seed >> 8) % (cap / 4);
		hfill(buf, s * 4, 4, 2);
		MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "덮어쓰기");
	}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	const char *log = sim_log_tail(SIM_LOG_BYTES);
	unsigned warnings = 0;
	for (const char *p = log; (p = strstr(p, "Uncorrectable Soon")) != NULL; p++)
		warnings++;
	printf("faults=%u gc-erase=%llu warnings=%u\n", faults, (unsigned long long)sim_op_count(SIM_OP_ERASE), warnings);
	return 0;
}

// GC가 돌아도 데이터가 그대로인지: 가득 채워 두 번 덮어쓰기, 무작위 덮어쓰기
#include "hcheck.h"

static unsigned char buf[256 * 4096];
static unsigned gens[8192];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	uint64_t slices = cap / 4;
	MUST(slices <= 8192, "용량");
	for (unsigned g = 1; g <= 2; g++)
		for (uint64_t l = 0; l + 256 <= cap; l += 256) {
			hfill(buf, l, 256, g);
			MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
		}
	for (uint64_t s = 0; s < slices; s++)
		gens[s] = 2;
	unsigned seed = 777;
	for (int i = 0; i < 8000; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = (seed >> 8) % slices;
		gens[s]++;
		hfill(buf, s * 4, 4, gens[s]);
		MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "덮어쓰기 %d", i);
	}
	int bad = 0;
	for (uint64_t s = 0; s < slices; s++) {
		MUST(SIM_SUCCESS(sim_read(s * 4, 4, buf)), "읽기");
		if (!hsame(buf, s * 4, 4, gens[s]))
			bad++;
	}
	EXPECT(bad == 0, "데이터가 다른 슬라이스 %d개", bad);
	EXPECT(sim_violations() == 0, "NAND 위반 %s", sim_violation_msg());
	return DONE();
}

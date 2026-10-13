// GC: 이름공간을 가득 채우고 다시 덮어써 GC가 돌아도 데이터가 그대로인지
#include "check.h"

static unsigned char buf[256 * 4096];
static unsigned gens[1 << 16];

static void full_overwrite_keeps_data(void)
{
	REQUIRE(sim_power_on());
	uint64_t cap = sim_capacity();
	REQUIRE(cap > 0 && cap % 256 == 0);
	uint64_t erasesAtBoot = sim_op_count(SIM_OP_ERASE);
	for (unsigned gen = 1; gen <= 2; gen++)
		for (uint64_t l = 0; l < cap; l += 256) {
			fill_pattern(buf, l, 256, gen);
			REQUIRE(SIM_SUCCESS(sim_write(l, 256, buf)));
		}
	CHECK(sim_op_count(SIM_OP_ERASE) > erasesAtBoot);
	int bad = 0;
	for (uint64_t l = 0; l < cap; l += 256) {
		REQUIRE(SIM_SUCCESS(sim_read(l, 256, buf)));
		if (!same_pattern(buf, l, 256, 2))
			bad++;
	}
	CHECK(bad == 0);
	CHECK(sim_violations() == 0);
}

static void random_overwrite_keeps_data(void)
{
	REQUIRE(sim_power_on());
	uint64_t cap = sim_capacity();
	uint64_t slices = cap / 4;
	REQUIRE(slices <= sizeof gens / sizeof gens[0]);
	for (uint64_t l = 0; l < cap; l += 256) {
		fill_pattern(buf, l, 256, 1);
		REQUIRE(SIM_SUCCESS(sim_write(l, 256, buf)));
	}
	for (uint64_t s = 0; s < slices; s++)
		gens[s] = 1;
	unsigned seed = 12345;
	for (int i = 0; i < 6000; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = (seed >> 8) % slices;
		gens[s]++;
		fill_pattern(buf, s * 4, 4, gens[s]);
		REQUIRE(SIM_SUCCESS(sim_write(s * 4, 4, buf)));
	}
	int bad = 0;
	for (uint64_t s = 0; s < slices; s++) {
		REQUIRE(SIM_SUCCESS(sim_read(s * 4, 4, buf)));
		if (!same_pattern(buf, s * 4, 4, gens[s]))
			bad++;
	}
	CHECK(bad == 0);
	CHECK(sim_violations() == 0);
}

int main(void)
{
	RUN(full_overwrite_keeps_data);
	RUN(random_overwrite_keeps_data);
	return CHECK_DONE();
}

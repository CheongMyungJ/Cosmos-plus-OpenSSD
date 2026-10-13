// 기본 읽기·쓰기: 부팅, 이름공간 크기, 정렬된 쓰기와 읽기, 덮어쓰기, 슬라이스(4블록)에 걸친 쓰기
#include "check.h"

static unsigned char buf[256 * 4096];

static void boots_and_reports_capacity(void)
{
	REQUIRE(sim_power_on());
	uint64_t cap = sim_capacity();
	CHECK(cap > 0);
	CHECK(cap % 4 == 0);
}

static void write_then_read(void)
{
	REQUIRE(sim_power_on());
	fill_pattern(buf, 100, 8, 1);
	CHECK(SIM_SUCCESS(sim_write(100, 8, buf)));
	memset(buf, 0, 8 * 4096);
	CHECK(SIM_SUCCESS(sim_read(100, 8, buf)));
	CHECK(same_pattern(buf, 100, 8, 1));
}

static void overwrite_keeps_latest(void)
{
	REQUIRE(sim_power_on());
	for (unsigned gen = 1; gen <= 5; gen++) {
		fill_pattern(buf, 40, 16, gen);
		CHECK(SIM_SUCCESS(sim_write(40, 16, buf)));
	}
	CHECK(SIM_SUCCESS(sim_read(40, 16, buf)));
	CHECK(same_pattern(buf, 40, 16, 5));
}

static void unaligned_write_inside_written_slice(void)
{
	REQUIRE(sim_power_on());
	fill_pattern(buf, 0, 8, 1);
	CHECK(SIM_SUCCESS(sim_write(0, 8, buf)));
	fill_pattern(buf, 5, 2, 2);
	CHECK(SIM_SUCCESS(sim_write(5, 2, buf)));
	CHECK(SIM_SUCCESS(sim_read(0, 8, buf)));
	CHECK(same_pattern(buf, 0, 5, 1));
	CHECK(same_pattern(buf + 5 * 4096, 5, 2, 2));
	CHECK(same_pattern(buf + 7 * 4096, 7, 1, 1));
}

static void large_transfer(void)
{
	REQUIRE(sim_power_on());
	fill_pattern(buf, 1000, 256, 3);
	CHECK(SIM_SUCCESS(sim_write(1000, 256, buf)));
	memset(buf, 0, sizeof buf);
	CHECK(SIM_SUCCESS(sim_read(1000, 256, buf)));
	CHECK(same_pattern(buf, 1000, 256, 3));
	CHECK(sim_violations() == 0);
}

int main(void)
{
	RUN(boots_and_reports_capacity);
	RUN(write_then_read);
	RUN(overwrite_keeps_latest);
	RUN(unaligned_write_inside_written_slice);
	RUN(large_transfer);
	return CHECK_DONE();
}

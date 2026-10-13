// F1a: deallocate한 데이터는 GC가 옮기지 않는다. 가득 채운 뒤 대부분을 deallocate하고 다시 쓰면 GC 복사가 거의 없어야 한다
#include "hcheck.h"

static unsigned char buf[256 * 4096];
static unsigned long long ranges[256][2];
static unsigned n;

static void add(unsigned long long lba, unsigned long long len)
{
	if (!len)
		return;
	ranges[n][0] = lba;
	ranges[n][1] = len;
	if (++n == 256) {
		MUST(SIM_SUCCESS(hdsm(1, n, (const unsigned long long(*)[2])ranges)), "deallocate");
		n = 0;
	}
}

/* 슬라이스 4개 묶음 g마다 g % 4번째 슬라이스만 남긴다 (남는 슬라이스가 다이마다 고르게) */
static unsigned keepOf(uint64_t g)
{
	return (unsigned)((g + g / 4) % 4);
}

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	for (uint64_t l = 0; l + 256 <= cap; l += 256) {
		hfill(buf, l, 256, 1);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
	}
	/* deallocate부터 센다: 0을 써서 흉내 내면 그만큼 프로그램이 는다 */
	uint64_t before = sim_op_count(SIM_OP_PROGRAM);
	for (uint64_t g = 0; (g + 1) * 16 <= cap; g++) {
		unsigned k = keepOf(g);
		add(g * 16, k * 4);
		add(g * 16 + (k + 1) * 4, (3 - k) * 4);
	}
	if (n)
		MUST(SIM_SUCCESS(hdsm(1, n, (const unsigned long long(*)[2])ranges)), "deallocate");
	/* deallocate한 자리에 다시 써서 GC를 돌린다 */
	uint64_t written = 0;
	for (uint64_t g = 0; (g + 1) * 16 <= cap; g++) {
		unsigned k = keepOf(g);
		for (unsigned s = 0; s < 4; s++)
			if (s != k) {
				hfill(buf, g * 16 + s * 4, 4, 2);
				MUST(SIM_SUCCESS(sim_write(g * 16 + s * 4, 4, buf)), "다시 쓰기");
				written++;
			}
	}
	uint64_t programs = sim_op_count(SIM_OP_PROGRAM) - before;
	/* 남긴 슬라이스(1/4)만 옮기면 GC 복사는 쓴 양의 절반 아래다. deallocate한 것까지 옮기면 쓴 양보다 많다 */
	EXPECT(programs <= written + written / 2, "프로그램 %llu번 (호스트가 쓴 슬라이스 %llu개): deallocate한 데이터를 GC가 옮김",
		(unsigned long long)programs, (unsigned long long)written);
	int bad = 0;
	for (uint64_t g = 0; (g + 1) * 16 <= cap; g++) {
		MUST(SIM_SUCCESS(sim_read(g * 16, 16, buf)), "읽기");
		unsigned k = keepOf(g);
		for (unsigned s = 0; s < 4; s++)
			if (!hsame(buf + s * 4 * 4096, g * 16 + s * 4, 4, s == k ? 1 : 2))
				bad++;
	}
	EXPECT(bad == 0, "데이터가 다른 슬라이스 %d개", bad);
	EXPECT(sim_violations() == 0, "NAND 위반 %s", sim_violation_msg());
	fprintf(stderr, "프로그램 %llu, 쓴 슬라이스 %llu\n", (unsigned long long)programs, (unsigned long long)written);
	return DONE();
}

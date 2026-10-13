// 리팩터링 확인용: 정해진 부하를 돌린 뒤 NAND의 최종 상태(프로그램된 행의 데이터·spare, 블록별 지우기 횟수)와 NAND 명령 수를 찍는다.
// 기준 레포와 결과 레포에서 같은 값이 나와야 한다 (FTL의 결정은 NAND 명령이 언제 실행되는지와 무관하다)
#include "hcheck.h"

static unsigned char buf[256 * 4096];

static unsigned long long fnv(unsigned long long h, const void *p, size_t n)
{
	const unsigned char *b = p;
	for (size_t i = 0; i < n; i++) {
		h ^= b[i];
		h *= 1099511628211ull;
	}
	return h;
}

static void dump(const char *tag)
{
	sim_geometry g = sim_geo();
	unsigned long long h = 1469598103934665603ull;
	unsigned long long programmed = 0;
	for (unsigned d = 0; d < g.dies; d++)
		for (unsigned b = 0; b < g.blocksPerDie; b++) {
			unsigned e = sim_erase_count(d, b);
			h = fnv(h, &e, 4);
			for (unsigned r = 0; r < g.rowsPerBlock; r++)
				if (sim_row_programmed(d, b, r)) {
					unsigned key[3] = { d, b, r };
					h = fnv(h, key, sizeof key);
					h = fnv(h, sim_row_data(d, b, r), g.bytesPerPage);
					h = fnv(h, sim_row_spare(d, b, r), g.spareBytes - 8);
					programmed++;
				}
		}
	printf("%s nand=%016llx rows=%llu program=%llu erase=%llu read=%llu violations=%u\n", tag, h, programmed,
		(unsigned long long)sim_op_count(SIM_OP_PROGRAM), (unsigned long long)sim_op_count(SIM_OP_ERASE),
		(unsigned long long)sim_op_count(SIM_OP_READ_TRANSFER), sim_violations());
}

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	printf("capacity=%llu\n", (unsigned long long)cap);
	for (uint64_t l = 0; l + 256 <= cap; l += 256) {
		hfill(buf, l, 256, 1);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
	}
	unsigned seed = 4242;
	for (int i = 0; i < 3000; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = (seed >> 8) % (cap / 4);
		unsigned off = (seed >> 3) % 4, n = 1 + (seed >> 5) % (4 - off);
		hfill(buf, s * 4 + off, n, 2 + i);
		MUST(SIM_SUCCESS(sim_write(s * 4 + off, n, buf)), "덮어쓰기");
		if (i % 7 == 0)
			MUST(SIM_SUCCESS(sim_read(((seed >> 11) % (cap / 4)) * 4, 4, buf)), "읽기");
	}
	dump("after-writes");
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	dump("after-flush");
	unsigned long long r[][2] = { { 1000, 600 }, { 9001, 2 }, { 20000, 4096 } };
	sim_status s = hdsm(1, 3, r);
	printf("dsm sct=%u sc=%u\n", s.sct, s.sc);
	for (int i = 0; i < 800; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t sl = (seed >> 8) % (cap / 4);
		hfill(buf, sl * 4, 4, 9000 + i);
		MUST(SIM_SUCCESS(sim_write(sl * 4, 4, buf)), "덮어쓰기");
	}
	dump("after-dsm");
	MUST(sim_shutdown(), "정상 종료");
	MUST(sim_power_on(), "부팅");
	for (int i = 0; i < 400; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t sl = (seed >> 8) % (cap / 4);
		hfill(buf, sl * 4, 4, 20000 + i);
		MUST(SIM_SUCCESS(sim_write(sl * 4, 4, buf)), "덮어쓰기");
	}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	sim_power_cut();
	MUST(sim_power_on(), "부팅");
	unsigned long long sum = 1469598103934665603ull;
	for (uint64_t l = 0; l + 256 <= cap; l += 256) {
		MUST(SIM_SUCCESS(sim_read(l, 256, buf)), "읽기");
		sum = fnv(sum, buf, sizeof buf);
	}
	printf("read-back=%016llx\n", sum);
	dump("end");
	return 0;
}

// F2b: Flush 뒤 전원이 갑자기 나가도 데이터가 남고, 덮어쓴 LBA는 최신 판이 읽힌다 (GC가 돈 뒤에도). 여러 번 껐다 켜도 된다
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
	for (uint64_t l = 0; l + 256 <= cap; l += 256) {
		hfill(buf, l, 256, 1);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
	}
	for (uint64_t s = 0; s < slices; s++)
		gens[s] = 1;
	unsigned seed = 31;
	for (int i = 0; i < 4000; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = (seed >> 8) % slices;
		hfill(buf, s * 4, 4, ++gens[s]);
		MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "덮어쓰기");
	}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	for (int cycle = 0; cycle < 3; cycle++) {
		sim_power_cut();
		MUST(sim_power_on(), "%d번째 전원 차단 뒤 부팅", cycle + 1);
		EXPECT(sim_capacity() == cap, "전원 차단 뒤 용량이 바뀜");
		int bad = 0;
		for (uint64_t s = 0; s < slices; s++) {
			MUST(SIM_SUCCESS(sim_read(s * 4, 4, buf)), "읽기");
			if (!hsame(buf, s * 4, 4, gens[s]))
				bad++;
		}
		EXPECT(bad == 0, "%d번째 전원 차단 뒤 최신 판이 아닌 슬라이스 %d개", cycle + 1, bad);
		/* 다음 차례 사이에 조금 더 쓴다 */
		for (int i = 0; i < 500; i++) {
			seed = seed * 1103515245u + 12345u;
			uint64_t s = (seed >> 8) % slices;
			hfill(buf, s * 4, 4, ++gens[s]);
			MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "덮어쓰기");
		}
		MUST(SIM_SUCCESS(sim_flush()), "Flush");
	}
	EXPECT(sim_violations() == 0, "NAND 위반 %s", sim_violation_msg());
	return DONE();
}

// F3b: 차가운 데이터가 많은 디스크에서 작은 영역만 오래 덮어써 지우기 횟수 차이가 문턱(100)을 넘으면, 차가운 데이터가 든 블록도 옮겨져 지워진다
// (평준화가 없으면 차가운 블록은 한 번도 지워지지 않는다).
// 옮긴 데이터는 그대로 읽히고, 옮기는 쓰기도 공장 배드 마크 자리를 0xFF로 둔다. 전원을 껐다 켜도 그대로다
#include "wl_common.h"

static unsigned char buf[256 * 4096];

static unsigned spread(unsigned die, unsigned *minOut, unsigned *maxOut)
{
	unsigned mn = ~0u, mx = 0;
	for (unsigned b = 0; b < WL_BLOCKS; b++) {
		if (wl_virtual(b) < 0)
			continue;
		unsigned e = sim_erase_count(die, b);
		if (e < mn)
			mn = e;
		if (e > mx)
			mx = e;
	}
	*minOut = mn;
	*maxOut = mx;
	return mx - mn;
}

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	for (uint64_t l = 0; l + 256 <= cap * 3 / 4; l += 256) {
		hfill(buf, l, 256, 1);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "차가운 데이터");
	}
	unsigned seed = 77;
	uint64_t hotLo = cap * 3 / 4 / 4, hotN = 1200;
	for (int i = 0; i < 300000; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = hotLo + (seed >> 8) % hotN;
		hfill(buf, s * 4, 4, 2);
		MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "뜨거운 쓰기 %d", i);
	}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	for (unsigned d = 1; d < WL_DIES; d++) {
		unsigned mn, mx, sp = spread(d, &mn, &mx);
		fprintf(stderr, "die %u: 지우기 횟수 %u~%u (차이 %u)\n", d, mn, mx, sp);
		EXPECT(mx > 110, "die %u: 가장 많이 지운 블록이 %u번뿐 (부하가 모자람)", d, mx);
		EXPECT(mn >= 5, "die %u: 가장 적게 지운 블록이 %u번 (가장 많이 %u번): 차가운 블록을 옮기지 않음", d, mn, mx);
	}
	int bad = 0;
	for (uint64_t l = 0; l + 256 <= cap * 3 / 4; l += 256) {
		MUST(SIM_SUCCESS(sim_read(l, 256, buf)), "읽기");
		for (unsigned b = 0; b < 256; b += 4)
			if (l + b < hotLo * 4 && !hsame(buf + (size_t)b * 4096, l + b, 4, 1))
				bad++;
	}
	EXPECT(bad == 0, "옮긴 차가운 데이터가 다른 슬라이스 %d개", bad);
	EXPECT(sim_bad_mark_writes() == 0, "공장 배드 마크 자리에 0xFF가 아닌 값 %u번", sim_bad_mark_writes());
	sim_power_cut();
	MUST(sim_power_on(), "부팅");
	bad = 0;
	for (uint64_t l = 0; l + 256 <= hotLo * 4; l += 256) {
		MUST(SIM_SUCCESS(sim_read(l, 256, buf)), "읽기");
		if (!hsame(buf, l, 256, 1))
			bad++;
	}
	EXPECT(bad == 0, "전원 차단 뒤 옮긴 데이터가 다른 묶음 %d개", bad);
	EXPECT(sim_violations() == 0, "NAND 위반 %s", sim_violation_msg());
	return DONE();
}

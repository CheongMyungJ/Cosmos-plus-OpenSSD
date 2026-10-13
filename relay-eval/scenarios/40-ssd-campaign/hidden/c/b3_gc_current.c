// B3: 가득 찬 디스크에서 작은 영역을 계속 덮어쓰다(쓰기 사이에 읽기로 버퍼를 돌림) 다른 데이터도 쓰면 펌웨어가 멈추면 안 된다.
// 쓰는 중인 현재 블록이 모두 무효가 된 순간 빈 블록이 모자라 GC가 돌면 그 블록이 희생으로 골릴 수 있다
#include "hcheck.h"

static unsigned char buf[256 * 4096];
static unsigned char one[4 * 4096];
static unsigned gens[8192];

static uint64_t cursor, cursorLo, cursorHi;

/* 슬라이스 하나를 쓰고 64번 읽어 그것만 NAND로 내보낸다 */
static int flush_slice(uint64_t lba, unsigned gen)
{
	hfill(one, lba, 4, gen);
	sim_status s = sim_write(lba, 4, one);
	if (!SIM_SUCCESS(s))
		return 0;
	gens[lba / 4] = gen;
	for (int i = 0; i < 64; i++) {
		s = sim_read(cursor, 4, buf);
		if (!SIM_SUCCESS(s))
			return 0;
		cursor += 4;
		if (cursor >= cursorHi)
			cursor = cursorLo;
	}
	return 1;
}

static int scenario(unsigned setup, unsigned rounds, unsigned warm, unsigned period)
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
	for (uint64_t s = 0; s < setup; s++) {
		uint64_t l = (s * 28 * 4 + (s / 256) * 8) % cap;
		hfill(buf, l, 4, 2);
		MUST(SIM_SUCCESS(sim_write(l, 4, buf)), "덮어쓰기");
		gens[l / 4] = 2;
	}
	cursorLo = cap * 6 / 7 / 4 * 4;
	cursorHi = cap / 4 * 4;
	cursor = cursorLo;
	unsigned ny = 0;
	for (unsigned r = 0; r < rounds; r++) {
		for (unsigned d = 0; d < 4; d++)
			if (!flush_slice((uint64_t)(4000 + d) * 4, 10 + r)) {
				fprintf(stderr, "setup %u: %u번째 회차에서 명령 실패 (펌웨어가 멈춤)\n%s\n", setup, r, sim_log_tail(400));
				return 0;
			}
		if (r >= warm && r % period == 0)
			for (unsigned d = 0; d < 4; d++)
				if (!flush_slice((uint64_t)(5000 + ny++) * 4, 3)) {
					fprintf(stderr, "setup %u: %u번째 회차에서 명령 실패 (펌웨어가 멈춤)\n%s\n", setup, r, sim_log_tail(400));
					return 0;
				}
	}
	int bad = 0;
	for (uint64_t l = 0; l + 256 <= cap; l += 256) {
		if (!SIM_SUCCESS(sim_read(l, 256, buf)))
			return 0;
		for (unsigned b = 0; b < 256; b += 4)
			if (!hsame(buf + (size_t)b * 4096, l + b, 4, gens[(l + b) / 4]))
				bad++;
	}
	if (bad)
		fprintf(stderr, "setup %u: 데이터가 다른 슬라이스 %d개\n", setup, bad);
	return bad == 0 && sim_violations() == 0;
}

int main(void)
{
	unsigned cfg[][3] = { { 300, 120, 1 }, { 320, 120, 1 }, { 280, 120, 2 }, { 300, 140, 1 } };
	for (unsigned i = 0; i < 4; i++)
		EXPECT(scenario(cfg[i][0], 360, cfg[i][1], cfg[i][2]), "재현 부하 %u/%u/%u", cfg[i][0], cfg[i][1], cfg[i][2]);
	return DONE();
}

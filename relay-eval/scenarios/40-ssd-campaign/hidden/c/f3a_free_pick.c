// F3a: 새로 쓰기 시작하는 블록은 그 다이의 빈 블록 가운데 지우기 횟수가 가장 적은 것이고, 같으면 번호가 작은 것이다.
// 정상 종료 뒤 다시 켜도 지우기 횟수가 이어진다
#include "wl_common.h"

static unsigned char buf[256 * 4096];
static wl_state st;
static int violations, ties, opens;

/* 기록의 [from, to)를 따라가며, 블록을 새로 열 때(지운 블록의 첫 프로그램) 고른 블록이 맞는지 본다 */
static void follow(uint64_t from, uint64_t to, int check)
{
	for (uint64_t i = from; i < to; i++) {
		const sim_op *o = sim_oplog(i);
		unsigned d, b, r;
		if (o->op != SIM_OP_PROGRAM && o->op != SIM_OP_ERASE)
			continue;
		wl_decode(o, &d, &b, &r);
		if (d >= WL_DIES || b >= WL_BLOCKS)
			continue;
		if (o->op == SIM_OP_ERASE) {
			if (!o->result) {
				st.erases[d][b]++;
				st.programmed[d][b] = 0;
			}
			continue;
		}
		if (o->result)
			continue;
		if (st.programmed[d][b] == 0 && check && d > 0 && wl_virtual(b) >= 0) {
			/* 이 다이에서 지금 지워진(빈) 사용자 블록 가운데 쓴 적 있는 것과 견준다 */
			opens++;
			for (unsigned x = 0; x < WL_BLOCKS; x++) {
				if (x == b || wl_virtual(x) < 0 || !st.used[d][x] || st.programmed[d][x] != 0)
					continue;
				if (st.erases[d][x] < st.erases[d][b]) {
					if (violations++ < 5)
						fprintf(stderr, "die %u: 지우기 %u번인 블록 %d를 열었는데 %u번인 빈 블록 %d가 있음\n", d, st.erases[d][b],
							wl_virtual(b), st.erases[d][x], wl_virtual(x));
				} else if (st.erases[d][x] == st.erases[d][b] && wl_virtual(x) < wl_virtual(b)) {
					if (ties++ < 5)
						fprintf(stderr, "die %u: 지우기 %u번으로 같은데 번호가 큰 블록 %d를 열었음 (작은 %d가 비어 있음)\n", d,
							st.erases[d][b], wl_virtual(b), wl_virtual(x));
				}
			}
		}
		st.programmed[d][b]++;
		st.used[d][b] = 1;
	}
}

static void hot(unsigned seedInit, int n, uint64_t range, unsigned gen)
{
	unsigned seed = seedInit;
	for (int i = 0; i < n; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = (seed >> 8) % range;
		hfill(buf, s * 4, 4, gen);
		MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "덮어쓰기");
	}
}

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	for (unsigned d = 0; d < WL_DIES; d++)
		for (unsigned b = 0; b < WL_BLOCKS; b++)
			st.erases[d][b] = sim_erase_count(d, b);
	uint64_t mark = sim_oplog_count();
	/* 1) 가득 채우고 작은 영역만 오래 덮어써, 몇 블록만 많이 지운다 (나머지는 거의 지우지 않음) */
	for (uint64_t l = 0; l + 256 <= cap; l += 256) {
		hfill(buf, l, 256, 1);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
	}
	hot(5, 30000, cap / 64, 2);
	/* 2) 모두 deallocate하고 정상 종료한 뒤 다시 켠다: 빈 블록 목록에 많이 지운 블록과 거의 안 지운 블록이 섞인다 */
	unsigned long long r[][2] = { { 0, cap } };
	MUST(SIM_SUCCESS(hdsm(1, 1, r)), "deallocate");
	MUST(sim_shutdown(), "정상 종료");
	MUST(sim_power_on(), "부팅");
	follow(mark, sim_oplog_count(), 0);
	/* 3) 새로 쓰며 여는 블록을 본다 */
	mark = sim_oplog_count();
	for (uint64_t l = 0; l + 256 <= cap * 3 / 4; l += 256) {
		hfill(buf, l, 256, 3);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "다시 채우기");
	}
	hot(9, 6000, cap / 8, 4);
	follow(mark, sim_oplog_count(), 1);
	fprintf(stderr, "연 블록 %d개, 더 적게 지운 빈 블록이 있었던 경우 %d, 같은데 번호가 큰 경우 %d\n", opens, violations, ties);
	/* NAND 명령은 고른 뒤에 실행되므로(버퍼를 기다리는 쓰기가 뒤에 실행됨) 기록으로 본 판정에는 드물게 어긋남이 생긴다 */
	EXPECT(opens > 40, "연 블록이 너무 적음 %d", opens);
	EXPECT(violations <= 2, "지우기 횟수가 가장 적은 빈 블록을 고르지 않음 %d번 / %d", violations, opens);
	EXPECT(ties <= 2, "지우기 횟수가 같을 때 번호가 작은 블록을 고르지 않음 %d번 / %d", ties, opens);
	return DONE();
}

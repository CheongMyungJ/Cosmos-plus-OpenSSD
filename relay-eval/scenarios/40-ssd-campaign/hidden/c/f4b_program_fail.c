// F4b: 프로그램이 실패해도 데이터를 잃지 않고, 실패한 블록은 더 쓰지 않는다(은퇴). 정상 종료 뒤 다시 켜도 데이터가 그대로이고
// 그 블록은 다시 쓰이지 않는다
#include "wl_common.h"

static unsigned char buf[256 * 4096];
static unsigned gens[8192];

struct failed {
	unsigned die, blk;
	uint64_t firstFail;	/* 첫 실패의 기록 번호 */
	unsigned attemptsAfter;
};

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	uint64_t slices = cap / 4;
	/* die 1과 2에서 사용자 블록 두 개씩(LUN마다 하나, 예비 블록이 LUN마다 모자라지 않게)을 영구히 프로그램 실패로 만든다 */
	struct failed f[4];
	unsigned nf = 0;
	unsigned picks[2] = { 5, 26 };
	for (unsigned d = 1; d <= 2; d++) {
		for (unsigned p = 0; p < 2; p++) {
			unsigned b = picks[p];
			{
				sim_fail_program(d, b, -1);
				f[nf].die = d;
				f[nf].blk = b;
				f[nf].firstFail = 0;
				f[nf].attemptsAfter = 0;
				nf++;
			}
		}
	}
	uint64_t mark = sim_oplog_count();
	uint64_t filled = 0;
	for (uint64_t l = 0; l + 256 <= cap * 7 / 10; l += 256) {
		hfill(buf, l, 256, 1);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
		filled = l + 256;
	}
	for (uint64_t s = 0; s < slices; s++)
		gens[s] = (s * 4 < filled) ? 1 : 0;
	unsigned seed = 3;
	for (int i = 0; i < 8000; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = (seed >> 8) % (filled / 4);
		hfill(buf, s * 4, 4, ++gens[s]);
		MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "덮어쓰기 %d", i);
	}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	/* 기록에서 실패와 그 뒤의 시도를 센다 */
	uint64_t n = sim_oplog_count();
	unsigned failures = 0;
	for (uint64_t i = mark; i < n; i++) {
		const sim_op *o = sim_oplog(i);
		if (o->op != SIM_OP_PROGRAM)
			continue;
		unsigned d, b, r;
		wl_decode(o, &d, &b, &r);
		for (unsigned k = 0; k < nf; k++)
			if (f[k].die == d && f[k].blk == b) {
				if (!f[k].firstFail) {
					f[k].firstFail = i + 1;
					failures++;
				} else
					f[k].attemptsAfter++;
			}
	}
	EXPECT(failures >= 2, "실패한 블록에 쓰기를 거의 시도하지 않음 (%u개): 부하가 모자람", failures);
	for (unsigned k = 0; k < nf; k++)
		EXPECT(f[k].attemptsAfter <= 64, "die %u 블록 %u: 실패 뒤에도 프로그램을 %u번 더 시도함 (은퇴하지 않음)", f[k].die, f[k].blk,
			f[k].attemptsAfter);
	int bad = 0;
	for (uint64_t s = 0; s < slices; s++)
		if (gens[s]) {
			MUST(SIM_SUCCESS(sim_read(s * 4, 4, buf)), "읽기");
			if (!hsame(buf, s * 4, 4, gens[s]))
				bad++;
		}
	EXPECT(bad == 0, "프로그램 실패 뒤 잃은 슬라이스 %d개", bad);
	/* 정상 종료 뒤 다시 켜도 데이터가 그대로이고 실패한 블록에 다시 쓰지 않는다 */
	MUST(sim_shutdown(), "정상 종료");
	MUST(sim_power_on(), "부팅");
	mark = sim_oplog_count();
	bad = 0;
	for (uint64_t s = 0; s < slices; s++)
		if (gens[s]) {
			MUST(SIM_SUCCESS(sim_read(s * 4, 4, buf)), "읽기");
			if (!hsame(buf, s * 4, 4, gens[s]))
				bad++;
		}
	EXPECT(bad == 0, "다시 켠 뒤 잃은 슬라이스 %d개", bad);
	for (int i = 0; i < 6000; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = (seed >> 8) % (filled / 4);
		hfill(buf, s * 4, 4, ++gens[s]);
		MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "덮어쓰기");
	}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	n = sim_oplog_count();
	unsigned again = 0;
	for (uint64_t i = mark; i < n; i++) {
		const sim_op *o = sim_oplog(i);
		if (o->op != SIM_OP_PROGRAM)
			continue;
		unsigned d, b, r;
		wl_decode(o, &d, &b, &r);
		for (unsigned k = 0; k < nf; k++)
			if (f[k].firstFail && f[k].die == d && f[k].blk == b)
				again++;
	}
	EXPECT(again == 0, "다시 켠 뒤 은퇴한 블록에 프로그램을 %u번 시도함", again);
	return DONE();
}

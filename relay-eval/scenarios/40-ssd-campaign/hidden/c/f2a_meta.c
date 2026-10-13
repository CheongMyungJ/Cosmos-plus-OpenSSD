// F2a: 사용자 데이터 페이지마다 spare에 메타가 있고(GC 복사 포함), 같은 LBA를 다시 쓰면 메타가 달라진다(순번)
#include "hcheck.h"

static unsigned char buf[256 * 4096];

static int blank(const unsigned char *p, unsigned n, unsigned char v)
{
	for (unsigned i = 0; i < n; i++)
		if (p[i] != v)
			return 0;
	return 1;
}

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	uint64_t first = sim_oplog_count();
	/* 가득 채우고 일부를 덮어써 GC 복사가 일어나게 한다 */
	for (uint64_t l = 0; l + 256 <= cap; l += 256) {
		hfill(buf, l, 256, 1);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
	}
	unsigned seed = 9;
	for (int i = 0; i < 3000; i++) {
		seed = seed * 1103515245u + 12345u;
		uint64_t s = (seed >> 8) % (cap / 4);
		hfill(buf, s * 4, 4, 2);
		MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "덮어쓰기");
	}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	sim_geometry g = sim_geo();
	/* 이번에 프로그램한 사용자 페이지(배드 블록 표 블록 0 제외)의 spare */
	uint64_t n = sim_oplog_count(), checked = 0, empty = 0;
	for (uint64_t i = first; i < n; i++) {
		const sim_op *o = sim_oplog(i);
		if (o->op != SIM_OP_PROGRAM || o->result)
			continue;
		unsigned lun = o->row >= 0x00200000, within = o->row - (lun ? 0x00200000 : 0);
		unsigned blk = lun * (g.blocksPerDie / 2) + within / g.rowsPerBlock, row = within % g.rowsPerBlock;
		unsigned die = o->ch + o->way * g.channels;
		if (blk == 0 || !sim_row_programmed(die, blk, row))
			continue;
		const unsigned char *sp = sim_row_spare(die, blk, row);
		checked++;
		if (blank(sp + 1, 64, 0xFF) || blank(sp + 1, 64, 0x00))
			empty++;
	}
	EXPECT(checked > 1000, "확인한 페이지 %llu개", (unsigned long long)checked);
	EXPECT(empty == 0, "spare에 메타가 없는 사용자 페이지 %llu개 / %llu개", (unsigned long long)empty, (unsigned long long)checked);
	/* 같은 LBA의 두 판은 메타가 다르다 */
	hfill(buf, 4000, 4, 7);
	MUST(SIM_SUCCESS(sim_write(4000, 4, buf)), "쓰기");
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	hfill(buf, 4000, 4, 8);
	MUST(SIM_SUCCESS(sim_write(4000, 4, buf)), "다시 쓰기");
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	unsigned d1, b1, r1, d2, b2, r2;
	hfill(buf, 4000, 1, 7);
	int f1 = sim_find_row(buf, 4096, &d1, &b1, &r1);
	hfill(buf, 4000, 1, 8);
	int f2 = sim_find_row(buf, 4096, &d2, &b2, &r2);
	EXPECT(f1 && f2, "두 판이 NAND에 없음");
	if (f1 && f2)
		EXPECT(memcmp(sim_row_spare(d1, b1, r1), sim_row_spare(d2, b2, r2), 64) != 0, "같은 LBA의 두 판의 spare 메타가 같음");
	return DONE();
}

// 마모 평준화 시험의 공용: NAND 명령 기록을 따라가며 블록마다 지움/프로그램 상태와 지우기 횟수를 센다
#ifndef WL_COMMON_H_
#define WL_COMMON_H_

#include "hcheck.h"

#define WL_DIES 4
#define WL_BLOCKS 36

typedef struct {
	unsigned erases[WL_DIES][WL_BLOCKS];	/* 시험 동안 본 지우기 수 + 시작 때 지우기 횟수 */
	int programmed[WL_DIES][WL_BLOCKS];		/* 지운 뒤 프로그램한 행 수 */
	int used[WL_DIES][WL_BLOCKS];			/* 시험 동안 사용자 데이터를 쓴 적 있음 */
} wl_state;

static inline void wl_decode(const sim_op *o, unsigned *die, unsigned *blk, unsigned *row)
{
	sim_geometry g = sim_geo();
	unsigned lun = o->row >= 0x00200000, within = o->row - (lun ? 0x00200000 : 0);
	*blk = lun * (g.blocksPerDie / 2) + within / g.rowsPerBlock;
	*row = within % g.rowsPerBlock;
	*die = o->ch + o->way * g.channels;
}

/* 물리 블록 → 가상 블록 번호 (가상 0번은 배드 블록 표가 든 물리 0번 대신 예비 블록 16번으로 옮겨져 있다). 사용자 블록이 아니면 -1 */
static inline int wl_virtual(unsigned blk)
{
	if (blk == 16)
		return 0;
	if (blk >= 1 && blk < 16)
		return (int)blk;
	if (blk >= 18 && blk < 34)
		return (int)(blk - 2);
	return -1;
}

#endif

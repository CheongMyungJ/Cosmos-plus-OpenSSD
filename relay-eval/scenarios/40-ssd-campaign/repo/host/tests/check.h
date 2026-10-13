// 아주 작은 시험 틀. 시험 하나마다 시뮬레이터를 새 NAND로 준비한다
#ifndef CHECK_H_
#define CHECK_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim.h"

static int checkFailures = 0;

#define CHECK(cond)                                                              \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: 실패: %s\n", __FILE__, __LINE__, #cond);     \
			checkFailures++;                                                     \
		}                                                                        \
	} while (0)

#define REQUIRE(cond)                                                            \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: 실패(중단): %s\n", __FILE__, __LINE__, #cond); \
			checkFailures++;                                                     \
			return;                                                              \
		}                                                                        \
	} while (0)

#define RUN(fn)                                                                  \
	do {                                                                         \
		int before = checkFailures;                                              \
		sim_init();                                                              \
		fn();                                                                    \
		sim_power_cut();                                                         \
		fprintf(stderr, "%s %s\n", checkFailures == before ? "ok  " : "FAIL", #fn); \
	} while (0)

#define CHECK_DONE() (checkFailures ? (fprintf(stderr, "실패 %d개\n", checkFailures), 1) : 0)

/** LBA마다 다른 4KiB 무늬. 앞 8바이트는 LBA, 다음 4바이트는 세대(같은 LBA를 다시 쓸 때 바꿈) */
static inline void fill_pattern(void *buf, unsigned long long lba, unsigned blocks, unsigned gen)
{
	unsigned char *p = buf;
	for (unsigned b = 0; b < blocks; b++) {
		unsigned long long l = lba + b;
		unsigned char *q = p + (size_t)b * 4096;
		memcpy(q, &l, 8);
		memcpy(q + 8, &gen, 4);
		for (unsigned i = 12; i < 4096; i++)
			q[i] = (unsigned char)((l * 131u + gen * 31u + i) & 0xFF);
	}
}

static inline int same_pattern(const void *buf, unsigned long long lba, unsigned blocks, unsigned gen)
{
	unsigned char *want = malloc((size_t)blocks * 4096);
	fill_pattern(want, lba, blocks, gen);
	int ok = memcmp(buf, want, (size_t)blocks * 4096) == 0;
	free(want);
	return ok;
}

#endif

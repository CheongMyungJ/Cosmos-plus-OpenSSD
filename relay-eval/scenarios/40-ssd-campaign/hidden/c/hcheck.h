// 숨긴 시험의 C 쪽 도구. 레포의 host/tests/check.h에 기대지 않는다
#ifndef HCHECK_H_
#define HCHECK_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim.h"

static int hFailures = 0;

#define EXPECT(cond, ...)                                                        \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: 실패: %s — ", __FILE__, __LINE__, #cond);    \
			fprintf(stderr, __VA_ARGS__);                                        \
			fprintf(stderr, "\n");                                               \
			hFailures++;                                                         \
		}                                                                        \
	} while (0)

#define MUST(cond, ...)                                                          \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: 실패(중단): %s — ", __FILE__, __LINE__, #cond); \
			fprintf(stderr, __VA_ARGS__);                                        \
			fprintf(stderr, "\n");                                               \
			exit(1);                                                             \
		}                                                                        \
	} while (0)

#define DONE() (hFailures ? (fprintf(stderr, "실패 %d개\n", hFailures), 1) : 0)

static inline void hfill(void *buf, unsigned long long lba, unsigned blocks, unsigned gen)
{
	unsigned char *p = buf;
	for (unsigned b = 0; b < blocks; b++) {
		unsigned long long l = lba + b;
		unsigned char *q = p + (size_t)b * 4096;
		memcpy(q, &l, 8);
		memcpy(q + 8, &gen, 4);
		for (unsigned i = 12; i < 4096; i++)
			q[i] = (unsigned char)((l * 157u + gen * 37u + i * 7u) & 0xFF);
	}
}

static inline int hsame(const void *buf, unsigned long long lba, unsigned blocks, unsigned gen)
{
	unsigned char want[4096];
	for (unsigned b = 0; b < blocks; b++) {
		hfill(want, lba + b, 1, gen);
		if (memcmp((const unsigned char *)buf + (size_t)b * 4096, want, 4096) != 0)
			return 0;
	}
	return 1;
}

static inline int hzero(const void *buf, size_t n)
{
	const unsigned char *p = buf;
	for (size_t i = 0; i < n; i++)
		if (p[i])
			return 0;
	return 1;
}

static inline const char *hst(sim_status s)
{
	static char b[96];
	snprintf(b, sizeof b, "ok=%d hung=%d crashed=%d sct=%u sc=0x%02x", s.ok, s.hung, s.crashed, s.sct, s.sc);
	return b;
}

/* Dataset Management (09h). ad: Deallocate 비트. ranges: {slba, nlb} 쌍 n개 */
static inline sim_status hdsm(int ad, unsigned n, const unsigned long long (*ranges)[2])
{
	static unsigned char r[4096];
	memset(r, 0, sizeof r);
	for (unsigned i = 0; i < n; i++) {
		unsigned int len = (unsigned int)ranges[i][1];
		unsigned long long slba = ranges[i][0];
		memcpy(r + i * 16 + 4, &len, 4);
		memcpy(r + i * 16 + 8, &slba, 8);
	}
	uint32_t dw[16] = { 0 };
	dw[0] = 0x09;
	dw[1] = 1;
	dw[10] = (n - 1) & 0xFF;
	dw[11] = ad ? 4 : 0;
	return sim_io(dw, r, n * 16, 1);
}

/* Get Log Page (02h). lid, 바이트 수 */
static inline sim_status hlog(unsigned lid, void *buf, unsigned bytes)
{
	uint32_t dw[16] = { 0 };
	dw[0] = 0x02;
	dw[1] = 0xFFFFFFFF;
	dw[10] = (lid & 0xFF) | (((bytes / 4 - 1) & 0xFFF) << 16);
	return sim_admin(dw, buf, bytes, 0);
}

static inline unsigned long long hle64(const unsigned char *p)
{
	unsigned long long v;
	memcpy(&v, p, 8);
	return v;
}

#endif

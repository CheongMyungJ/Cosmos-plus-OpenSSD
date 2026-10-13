// F5a: Percentage Used는 평균 지우기 횟수 ÷ 3000을 백분율로 올림, Media and Data Integrity Errors는 Unrecovered Read Error 수
#include "wl_common.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	uint64_t cap = sim_capacity();
	/* 평균 지우기 횟수가 30~60 사이가 되도록 덮어쓴다 */
	unsigned seed = 21;
	for (uint64_t l = 0; l + 256 <= cap * 2 / 3; l += 256) {
		hfill(buf, l, 256, 1);
		MUST(SIM_SUCCESS(sim_write(l, 256, buf)), "채우기");
	}
	double avg = 0;
	for (int round = 0; round < 40 && avg < 34; round++) {
		for (int i = 0; i < 20000; i++) {
			seed = seed * 1103515245u + 12345u;
			uint64_t s = (seed >> 8) % (cap * 2 / 3 / 4);
			hfill(buf, s * 4, 4, 2);
			MUST(SIM_SUCCESS(sim_write(s * 4, 4, buf)), "덮어쓰기");
		}
		unsigned long long sum = 0, n = 0;
		for (unsigned d = 0; d < WL_DIES; d++)
			for (unsigned b = 0; b < WL_BLOCKS; b++)
				if (wl_virtual(b) >= 0) {
					sum += sim_erase_count(d, b);
					n++;
				}
		avg = (double)sum / n;
	}
	unsigned char log[512];
	MUST(SIM_SUCCESS(hlog(2, log, 512)), "SMART 로그");
	fprintf(stderr, "평균 지우기 %.1f, Percentage Used %u\n", avg, log[5]);
	EXPECT(avg > 31 && avg < 59, "부하가 맞지 않음: 평균 %.1f", avg);
	EXPECT(log[5] == 2, "평균 지우기 %.1f회면 Percentage Used는 2 (3000회 기준 올림), 받은 값 %u", avg, log[5]);
	/* 고칠 수 없는 읽기 두 번 */
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	for (unsigned i = 0; i < 80; i++)
		MUST(SIM_SUCCESS(sim_read(cap - 400 + (i % 64) * 4, 4, buf)), "버퍼 돌리기");
	unsigned long long me0 = hle64(log + 160);
	for (unsigned k = 0; k < 2; k++) {
		unsigned d, b, r;
		uint64_t lba = 100 + k * 40;
		MUST(SIM_SUCCESS(sim_read(lba, 1, buf)), "읽기");
		MUST(sim_find_row(buf, 4096, &d, &b, &r), "페이지를 못 찾음");
		sim_fail_read(d, b, r, 1, -1);
		for (unsigned i = 0; i < 80; i++)
			MUST(SIM_SUCCESS(sim_read(cap - 800 + i * 4, 4, buf)), "버퍼 돌리기");
		sim_status s = sim_read(lba, 4, buf);
		EXPECT(s.ok && s.sct == 2 && s.sc == 0x81, "고칠 수 없는 읽기 %s", hst(s));
	}
	MUST(SIM_SUCCESS(hlog(2, log, 512)), "SMART 로그");
	EXPECT(hle64(log + 160) - me0 == 2, "Media and Data Integrity Errors가 %llu 늘어남 (2여야 함)", hle64(log + 160) - me0);
	return DONE();
}

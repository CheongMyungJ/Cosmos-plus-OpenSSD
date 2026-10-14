// F5a: SMART 로그(02h)의 명령 수와 Data Units(512바이트 1000개 단위, 올림), 처음 값들, 지원하지 않는 로그는 Invalid Log Page
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	unsigned char log[512];
	sim_status s = hlog(2, log, 512);
	MUST(SIM_SUCCESS(s), "SMART 로그: %s", hst(s));
	EXPECT(log[3] == 100, "처음 Available Spare %u (100이어야 함)", log[3]);
	EXPECT(log[4] == 10, "Available Spare Threshold %u (10이어야 함)", log[4]);
	EXPECT(log[0] == 0, "Critical Warning 0x%x", log[0]);
	EXPECT(log[5] == 0, "새 장치의 Percentage Used %u", log[5]);
	unsigned long long r0 = hle64(log + 64), w0 = hle64(log + 80);
	unsigned long long units[2] = { 0, 0 };
	unsigned cmds[2] = { 0, 0 };
	/* 크기가 다른 쓰기 37번, 읽기 23번 */
	for (unsigned i = 0; i < 37; i++) {
		unsigned n = 1 + (i * 7) % 19;
		hfill(buf, i * 32, n, 1);
		MUST(SIM_SUCCESS(sim_write(i * 32, n, buf)), "쓰기");
		units[1] += n * 8;
		cmds[1]++;
	}
	for (unsigned i = 0; i < 23; i++) {
		unsigned n = 1 + (i * 5) % 13;
		MUST(SIM_SUCCESS(sim_read(i * 32, n, buf)), "읽기");
		units[0] += n * 8;
		cmds[0]++;
	}
	/* 큰 쓰기 (1MiB) 한 번 */
	hfill(buf, 8192, 256, 1);
	MUST(SIM_SUCCESS(sim_write(8192, 256, buf)), "큰 쓰기");
	units[1] += 256 * 8;
	cmds[1]++;
	/* Flush, DSM, 범위 밖 쓰기는 세지 않는다 */
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	unsigned long long r[][2] = { { 0, 8 } };
	MUST(SIM_SUCCESS(hdsm(1, 1, r)), "DSM");
	s = hlog(2, log, 512);
	MUST(SIM_SUCCESS(s), "SMART 로그: %s", hst(s));
	EXPECT(hle64(log + 64) - r0 == cmds[0], "Host Read Commands %llu (기대 %u)", hle64(log + 64) - r0, cmds[0]);
	EXPECT(hle64(log + 80) - w0 == cmds[1], "Host Write Commands %llu (기대 %u)", hle64(log + 80) - w0, cmds[1]);
	unsigned long long dur = hle64(log + 32), duw = hle64(log + 48);
	EXPECT(dur == (units[0] + 999) / 1000, "Data Units Read %llu (512바이트 %llu개의 1000 단위 올림: %llu)", dur, units[0], (units[0] + 999) / 1000);
	EXPECT(duw == (units[1] + 999) / 1000, "Data Units Written %llu (512바이트 %llu개의 1000 단위 올림: %llu)", duw, units[1], (units[1] + 999) / 1000);
	EXPECT(hle64(log + 40) == 0 && hle64(log + 56) == 0, "128비트 값의 위쪽");
	/* 지원하지 않는 로그 */
	s = hlog(0x55, log, 512);
	EXPECT(s.ok && s.sct == 1 && s.sc == 0x09, "지원하지 않는 로그 페이지: %s", hst(s));
	/* 짧게 달라고 하면 그만큼만 */
	memset(log, 0xAB, sizeof log);
	s = hlog(2, log, 64);
	EXPECT(SIM_SUCCESS(s), "64바이트 SMART 로그: %s", hst(s));
	EXPECT(sim_running(), "펌웨어가 멈춤");
	return DONE();
}

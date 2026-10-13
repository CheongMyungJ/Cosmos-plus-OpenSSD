// F5b: SMART 카운터는 정상 종료 뒤에도 이어지고, Power Cycles는 켤 때마다, Unsafe Shutdowns는 정상 종료 없이 꺼졌을 때만 는다
#include "hcheck.h"

static unsigned char buf[64 * 4096];
static unsigned char log[512];

static void smart(const char *when)
{
	sim_status s = hlog(2, log, 512);
	MUST(SIM_SUCCESS(s), "%s: SMART 로그 %s", when, hst(s));
}

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	smart("처음");
	EXPECT(hle64(log + 112) == 1, "처음 Power Cycles %llu (1이어야 함)", hle64(log + 112));
	EXPECT(hle64(log + 144) == 0, "처음 Unsafe Shutdowns %llu", hle64(log + 144));
	for (unsigned i = 0; i < 30; i++) {
		hfill(buf, i * 16, 16, 1);
		MUST(SIM_SUCCESS(sim_write(i * 16, 16, buf)), "쓰기");
	}
	for (unsigned i = 0; i < 12; i++)
		MUST(SIM_SUCCESS(sim_read(i * 16, 16, buf)), "읽기");
	smart("쓴 뒤");
	unsigned long long reads = hle64(log + 64), writes = hle64(log + 80), duw = hle64(log + 48), dur = hle64(log + 32);
	MUST(sim_shutdown(), "정상 종료");
	MUST(sim_power_on(), "부팅");
	smart("정상 종료 뒤");
	EXPECT(hle64(log + 112) == 2, "정상 종료 뒤 Power Cycles %llu (2)", hle64(log + 112));
	EXPECT(hle64(log + 144) == 0, "정상 종료 뒤 Unsafe Shutdowns %llu (0)", hle64(log + 144));
	EXPECT(hle64(log + 64) == reads && hle64(log + 80) == writes, "정상 종료 뒤 명령 수 읽기 %llu/%llu 쓰기 %llu/%llu",
		hle64(log + 64), reads, hle64(log + 80), writes);
	EXPECT(hle64(log + 48) == duw && hle64(log + 32) == dur, "정상 종료 뒤 Data Units 읽기 %llu/%llu 쓰기 %llu/%llu", hle64(log + 32), dur,
		hle64(log + 48), duw);
	sim_power_cut();
	MUST(sim_power_on(), "부팅");
	smart("전원 차단 뒤");
	EXPECT(hle64(log + 112) == 3, "전원 차단 뒤 Power Cycles %llu (3)", hle64(log + 112));
	EXPECT(hle64(log + 144) == 1, "전원 차단 뒤 Unsafe Shutdowns %llu (1)", hle64(log + 144));
	MUST(sim_shutdown(), "정상 종료");
	MUST(sim_power_on(), "부팅");
	smart("다시 정상 종료 뒤");
	EXPECT(hle64(log + 112) == 4, "Power Cycles %llu (4)", hle64(log + 112));
	EXPECT(hle64(log + 144) == 1, "Unsafe Shutdowns %llu (1 그대로)", hle64(log + 144));
	return DONE();
}

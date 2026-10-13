// F2b: deallocate한 LBA는 전원이 나간 뒤에도 0으로 읽힌다 (옛 데이터가 되살아나지 않는다). 그 뒤 다시 쓴 LBA는 새 데이터
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	for (unsigned i = 0; i < 8; i++) {
		hfill(buf, i * 256, 256, 1);
		MUST(SIM_SUCCESS(sim_write(i * 256, 256, buf)), "쓰기");
	}
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	unsigned long long r[][2] = { { 256, 512 }, { 1500, 4 } };
	MUST(SIM_SUCCESS(hdsm(1, 2, r)), "deallocate");
	hfill(buf, 300, 4, 2);
	MUST(SIM_SUCCESS(sim_write(300, 4, buf)), "다시 쓰기");
	MUST(SIM_SUCCESS(sim_flush()), "Flush");
	sim_power_cut();
	MUST(sim_power_on(), "부팅");
	MUST(SIM_SUCCESS(sim_read(256, 44, buf)), "읽기");
	EXPECT(hzero(buf, 44 * 4096), "deallocate한 256~299가 전원 차단 뒤 0이 아님");
	MUST(SIM_SUCCESS(sim_read(300, 4, buf)), "읽기");
	EXPECT(hsame(buf, 300, 4, 2), "deallocate 뒤 다시 쓴 300~303");
	MUST(SIM_SUCCESS(sim_read(304, 256, buf)), "읽기");
	EXPECT(hzero(buf, 256 * 4096), "deallocate한 304~559가 0이 아님");
	MUST(SIM_SUCCESS(sim_read(1500, 4, buf)), "읽기");
	EXPECT(hzero(buf, 4 * 4096), "deallocate한 1500~1503이 0이 아님");
	MUST(SIM_SUCCESS(sim_read(0, 256, buf)), "읽기");
	EXPECT(hsame(buf, 0, 256, 1), "이웃 0~255");
	MUST(SIM_SUCCESS(sim_read(768, 256, buf)), "읽기");
	EXPECT(hsame(buf, 768, 256, 1), "이웃 768~1023");
	/* 한 번 더 껐다 켜도 같다 */
	sim_power_cut();
	MUST(sim_power_on(), "부팅");
	MUST(SIM_SUCCESS(sim_read(256, 44, buf)), "읽기");
	EXPECT(hzero(buf, 44 * 4096), "두 번째 전원 차단 뒤 deallocate한 LBA가 0이 아님");
	return DONE();
}

// F1a: 슬라이스 전체 범위를 deallocate하면 0으로 읽히고 이웃은 그대로, AD 비트가 없으면 아무것도 하지 않는다
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	/* 0~1023을 쓰고 더 써서 NAND로 내보낸다 */
	for (unsigned i = 0; i < 8; i++) {
		hfill(buf, i * 256, 256, 1);
		MUST(SIM_SUCCESS(sim_write(i * 256, 256, buf)), "쓰기");
	}
	unsigned long long r1[][2] = { { 256, 256 }, { 1024, 8 } };
	sim_status s = hdsm(1, 2, r1);
	EXPECT(SIM_SUCCESS(s), "deallocate %s", hst(s));
	EXPECT(SIM_SUCCESS(sim_read(256, 256, buf)) && hzero(buf, 256 * 4096), "deallocate한 256~511이 0이 아님");
	EXPECT(SIM_SUCCESS(sim_read(1024, 8, buf)) && hzero(buf, 8 * 4096), "deallocate한 1024~1031이 0이 아님");
	EXPECT(SIM_SUCCESS(sim_read(0, 256, buf)) && hsame(buf, 0, 256, 1), "이웃 0~255가 바뀜");
	EXPECT(SIM_SUCCESS(sim_read(512, 256, buf)) && hsame(buf, 512, 256, 1), "이웃 512~767이 바뀜");
	EXPECT(SIM_SUCCESS(sim_read(1032, 8, buf)) && hsame(buf, 1032, 8, 1), "이웃 1032~1039가 바뀜");
	/* AD 없이(무결성 힌트만): 아무것도 하지 않는다 */
	unsigned long long r2[][2] = { { 1536, 64 } };
	s = hdsm(0, 1, r2);
	EXPECT(SIM_SUCCESS(s), "AD 없는 DSM %s", hst(s));
	EXPECT(SIM_SUCCESS(sim_read(1536, 64, buf)) && hsame(buf, 1536, 64, 1), "AD 없는 DSM이 데이터를 지움");
	/* 다시 쓰면 다시 읽힌다 */
	hfill(buf, 256, 4, 2);
	EXPECT(SIM_SUCCESS(sim_write(256, 4, buf)), "다시 쓰기");
	EXPECT(SIM_SUCCESS(sim_read(256, 4, buf)) && hsame(buf, 256, 4, 2), "deallocate 뒤 다시 쓴 데이터");
	return DONE();
}

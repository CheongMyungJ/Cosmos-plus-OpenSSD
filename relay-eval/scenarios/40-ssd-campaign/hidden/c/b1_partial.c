// B1: 쓰지 않은 슬라이스에 일부 LBA만 쓰면 나머지 LBA는 0이어야 한다 (읽기-수정-쓰기가 남은 버퍼를 쓰면 안 된다)
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	for (unsigned i = 0; i < 4; i++) {
		hfill(buf, i * 256, 256, 1);
		MUST(SIM_SUCCESS(sim_write(i * 256, 256, buf)), "쓰기");
	}
	for (unsigned i = 0; i < 4; i++)
		MUST(SIM_SUCCESS(sim_read(i * 256, 256, buf)), "읽기");
	/* 쓰지 않은 슬라이스 20100~20103의 20101 하나만 쓴다 */
	hfill(buf, 20101, 1, 7);
	MUST(SIM_SUCCESS(sim_write(20101, 1, buf)), "부분 쓰기");
	EXPECT(SIM_SUCCESS(sim_read(20100, 4, buf)), "읽기");
	EXPECT(hzero(buf, 4096) && hsame(buf + 4096, 20101, 1, 7) && hzero(buf + 2 * 4096, 2 * 4096),
		"버퍼에 있을 때: 쓴 LBA만 데이터, 나머지 0이어야 함 (20100의 앞 8바이트 %llu)", *(unsigned long long *)buf);
	/* 버퍼에서 내보낸 뒤 NAND에서 다시 읽는다 */
	for (unsigned i = 0; i < 4; i++)
		MUST(SIM_SUCCESS(sim_read(i * 256, 256, buf)), "읽기");
	EXPECT(SIM_SUCCESS(sim_read(20100, 4, buf)), "읽기");
	EXPECT(hzero(buf, 4096) && hsame(buf + 4096, 20101, 1, 7) && hzero(buf + 2 * 4096, 2 * 4096),
		"NAND에서: 쓴 LBA만 데이터, 나머지 0이어야 함 (20100의 앞 8바이트 %llu)", *(unsigned long long *)buf);
	/* 두 블록 걸친 부분 쓰기 */
	hfill(buf, 22002, 3, 8);
	MUST(SIM_SUCCESS(sim_write(22002, 3, buf)), "걸친 부분 쓰기");
	for (unsigned i = 0; i < 4; i++)
		MUST(SIM_SUCCESS(sim_read(i * 256, 256, buf)), "읽기");
	EXPECT(SIM_SUCCESS(sim_read(22000, 8, buf)), "읽기");
	EXPECT(hzero(buf, 2 * 4096) && hsame(buf + 2 * 4096, 22002, 3, 8) && hzero(buf + 5 * 4096, 3 * 4096), "걸친 부분 쓰기의 나머지 0");
	return DONE();
}

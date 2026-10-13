// B1: 빈 슬라이스를 0으로 채우는 것이 같은 버퍼 항목에서 아직 NAND로 내리지 않은 앞 데이터를 지우면 안 된다
#include "hcheck.h"

static unsigned char buf[256 * 4096];

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	/* 더러운 버퍼 항목을 가득 만든다 (쓰기만, 아직 NAND에 없음) */
	for (unsigned i = 0; i < 128; i++) {
		hfill(buf, i * 4, 4, 3);
		MUST(SIM_SUCCESS(sim_write(i * 4, 4, buf)), "쓰기");
	}
	/* 쓰지 않은 슬라이스를 읽고 부분 쓰기 해 더러운 항목을 내보내게 한다 */
	for (unsigned i = 0; i < 64; i++) {
		if (i % 2)
			MUST(SIM_SUCCESS(sim_read(20000 + i * 4, 4, buf)), "빈 읽기");
		else {
			hfill(buf, 20001 + i * 4, 1, 4);
			MUST(SIM_SUCCESS(sim_write(20001 + i * 4, 1, buf)), "빈 슬라이스 부분 쓰기");
		}
	}
	int bad = 0;
	for (unsigned i = 0; i < 128; i++) {
		MUST(SIM_SUCCESS(sim_read(i * 4, 4, buf)), "읽기");
		if (!hsame(buf, i * 4, 4, 3))
			bad++;
	}
	EXPECT(bad == 0, "내보낸 데이터 %d개 슬라이스가 망가짐", bad);
	return DONE();
}

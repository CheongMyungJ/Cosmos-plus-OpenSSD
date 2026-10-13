// F1b: Identify가 Dataset Management 지원(ONCS bit 2)과 deallocate 뒤 0 읽기(DLFEAT = 001b)를 광고한다
#include "hcheck.h"

int main(void)
{
	sim_init();
	MUST(sim_power_on(), "부팅");
	unsigned char id[4096];
	MUST(SIM_SUCCESS(sim_identify(1, id)), "Identify Controller");
	unsigned oncs = id[520] | (id[521] << 8);
	EXPECT(oncs & 0x4, "ONCS 0x%04x에 Dataset Management(bit 2)가 없음", oncs);
	MUST(SIM_SUCCESS(sim_identify(0, id)), "Identify Namespace");
	EXPECT((id[33] & 0x7) == 0x1, "DLFEAT 0x%02x: deallocate 뒤 0으로 읽힘(001b)이 아님", id[33]);
	return DONE();
}

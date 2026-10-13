// 호스트 시뮬레이터용 xparameters.h 대역: NSC 2채널과 NVMe 호스트 제어기의 레지스터 주소.
// NSC 주소는 채널을 가르는 데만 쓰고 읽거나 쓰지 않는다. NVMe 호스트 제어기 레지스터는 시뮬레이터가 메모리로 잡는다
#ifndef SIM_XPARAMETERS_H_
#define SIM_XPARAMETERS_H_

#define XPAR_TIGER4NSC_0_BASEADDR 0x43C00000
#define XPAR_TIGER4NSC_1_BASEADDR 0x43C10000

#define XPAR_NVMEHOSTCONTROLLER_0_BASEADDR 0x83C00000

#endif

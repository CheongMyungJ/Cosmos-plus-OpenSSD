// 호스트 시뮬레이터용 Xilinx BSP 대역. 출력은 시뮬레이터의 로그로 간다 (SIM_VERBOSE=1이면 stderr에도)
#ifndef SIM_XIL_PRINTF_H_
#define SIM_XIL_PRINTF_H_

void xil_printf(const char *fmt, ...);
char inbyte(void);

#endif

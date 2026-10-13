// 장치(펌웨어 프로세스) 쪽: NVMe 호스트 제어기 드라이버 대역(firmware/nvme/host_lld.c 대신), xil_printf, 부팅.
// 명령은 공유 메모리의 고리에서 꺼내고, DMA는 공유 메모리의 호스트 버퍼와 펌웨어 DRAM 사이의 복사다.
// 읽기·쓰기 명령은 실제 제어기처럼 자동 DMA가 NLB+1 블록을 다 옮기면 자동으로 성공 완료된다.
#define _GNU_SOURCE
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../../firmware/memory_map.h"
#include "../../firmware/nvme/host_lld.h"
#include "../../firmware/nvme/nvme.h"
#include "../../firmware/nvme/nvme_main.h"
#include "sim_shared.h"

extern volatile NVME_CONTEXT g_nvmeTask;
extern uint8_t *simHostMem;

HOST_DMA_STATUS g_hostDmaStatus;
HOST_DMA_ASSIST_STATUS g_hostDmaAssistStatus;

static uint32_t slotCmd[SIM_SLOTS][16];

static int verbose = -1;

void sim_log(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if (n >= (int)sizeof buf)
		n = sizeof buf - 1;
	if (verbose < 0)
		verbose = getenv("SIM_VERBOSE") && getenv("SIM_VERBOSE")[0] == '1';
	if (verbose)
		fputs(buf, stderr);
	for (int i = 0; i < n; i++)
		simShared->log[(simShared->logPos++) % SIM_LOG_BYTES] = buf[i];
}

void xil_printf(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	sim_log("%s", buf);
}

char inbyte(void)
{
	return simShared->inbyteChar;
}

static NVME_STATUS_REG *statusReg(void)
{
	return (NVME_STATUS_REG *)(uintptr_t)NVME_STATUS_REG_ADDR;
}

static void violation(const char *msg)
{
	simShared->violations++;
	if (simShared->violations == 1)
		snprintf(simShared->violationMsg, sizeof simShared->violationMsg, "%s", msg);
	sim_log("[sim] 위반: %s\n", msg);
}

/** PRP 주소를 호스트 버퍼 포인터로. 범위 밖이면 NULL */
static uint8_t *hostPtr(uint64_t pcie, size_t len)
{
	uint64_t base = (uint64_t)SIM_PCIE_H << 32;
	if (pcie < base || pcie - base + len > (uint64_t)SIM_SLOTS * SIM_SLOT_BYTES)
		return NULL;
	return simHostMem + (pcie - base);
}

static void complete(unsigned slot, unsigned specific, unsigned statusFieldWord)
{
	if (slot >= SIM_SLOTS) {
		violation("범위 밖 명령 슬롯의 완료");
		return;
	}
	sim_cpl_slot *c = &simShared->cpl[slot];
	c->completions++;
	if (c->state != SIM_CPL_PENDING)
		return;
	c->specific = specific;
	c->statusFieldWord = statusFieldWord;
	__atomic_store_n(&c->state, SIM_CPL_DONE, __ATOMIC_RELEASE);
}

void dev_irq_init()
{
}

void dev_irq_handler()
{
}

unsigned int check_nvme_cc_en()
{
	return statusReg()->ccEn;
}

void set_nvme_csts_rdy(unsigned int rdy)
{
	statusReg()->cstsRdy = rdy;
	simShared->ready = rdy;
}

void set_nvme_csts_shst(unsigned int shst)
{
	statusReg()->cstsShst = shst;
	simShared->shst = shst;
}

void set_nvme_admin_queue(unsigned int sqValid, unsigned int cqValid, unsigned int cqIrqEn)
{
	(void)sqValid;
	(void)cqValid;
	(void)cqIrqEn;
}

unsigned int get_nvme_cmd(unsigned short *qID, unsigned short *cmdSlotTag, unsigned int *cmdSeqNum, unsigned int *cmdDword)
{
	// 정상 종료 알림은 인터럽트로 온다. 시뮬레이터는 명령을 꺼낼 때 본다
	if (simShared->shutdownReq && g_nvmeTask.status == NVME_TASK_RUNNING) {
		statusReg()->ccShn = 1;
		g_nvmeTask.status = NVME_TASK_SHUTDOWN;
		return 0;
	}
	uint32_t head = simShared->cmdHead;
	if (head == __atomic_load_n(&simShared->cmdTail, __ATOMIC_ACQUIRE)) {
		sched_yield();
		return 0;
	}
	unsigned i = head % SIM_CMD_RING;
	unsigned slot = simShared->cmd[i].slot;
	*qID = simShared->cmd[i].qid;
	*cmdSlotTag = (unsigned short)slot;
	*cmdSeqNum = head;
	memcpy(cmdDword, simShared->cmd[i].dw, 64);
	memcpy(slotCmd[slot], simShared->cmd[i].dw, 64);
	__atomic_store_n(&simShared->cmdHead, head + 1, __ATOMIC_RELEASE);
	return 1;
}

void set_auto_nvme_cpl(unsigned int cmdSlotTag, unsigned int specific, unsigned int statusFieldWord)
{
	complete(cmdSlotTag, specific, statusFieldWord & 0xFFFF);
}

void set_nvme_slot_release(unsigned int cmdSlotTag)
{
	(void)cmdSlotTag;
}

void set_nvme_cpl(unsigned int sqId, unsigned int cid, unsigned int specific, unsigned int statusFieldWord)
{
	(void)sqId;
	complete(cid, specific, statusFieldWord & 0xFFFF);
}

void set_io_sq(unsigned int ioSqIdx, unsigned int valid, unsigned int cqVector, unsigned int qSzie, unsigned int pcieBaseAddrL,
	unsigned int pcieBaseAddrH)
{
	(void)ioSqIdx;
	(void)valid;
	(void)cqVector;
	(void)qSzie;
	(void)pcieBaseAddrL;
	(void)pcieBaseAddrH;
}

void set_io_cq(unsigned int ioCqIdx, unsigned int valid, unsigned int irqEn, unsigned int irqVector, unsigned int qSzie,
	unsigned int pcieBaseAddrL, unsigned int pcieBaseAddrH)
{
	(void)ioCqIdx;
	(void)valid;
	(void)irqEn;
	(void)irqVector;
	(void)qSzie;
	(void)pcieBaseAddrL;
	(void)pcieBaseAddrH;
}

void set_direct_tx_dma(unsigned int devAddr, unsigned int pcieAddrH, unsigned int pcieAddrL, unsigned int len)
{
	uint8_t *h = hostPtr(((uint64_t)pcieAddrH << 32) | pcieAddrL, len);
	if (!h) {
		violation("직접 DMA(장치→호스트)의 호스트 주소가 범위 밖");
		return;
	}
	memcpy(h, (void *)(uintptr_t)devAddr, len);
	g_hostDmaStatus.fifoTail.directDmaTx++;
	g_hostDmaStatus.directDmaTxCnt++;
	g_hostDmaStatus.fifoHead.directDmaTx = g_hostDmaStatus.fifoTail.directDmaTx;
}

void set_direct_rx_dma(unsigned int devAddr, unsigned int pcieAddrH, unsigned int pcieAddrL, unsigned int len)
{
	uint8_t *h = hostPtr(((uint64_t)pcieAddrH << 32) | pcieAddrL, len);
	if (!h) {
		violation("직접 DMA(호스트→장치)의 호스트 주소가 범위 밖");
		return;
	}
	memcpy((void *)(uintptr_t)devAddr, h, len);
	g_hostDmaStatus.fifoTail.directDmaRx++;
	g_hostDmaStatus.directDmaRxCnt++;
	g_hostDmaStatus.fifoHead.directDmaRx = g_hostDmaStatus.fifoTail.directDmaRx;
}

static void autoDma(unsigned slot, unsigned off4k, unsigned devAddr, unsigned autoCompletion, int tx)
{
	if (slot >= SIM_SLOTS) {
		violation("범위 밖 명령 슬롯의 DMA");
		return;
	}
	uint64_t prp1 = ((uint64_t)slotCmd[slot][7] << 32) | slotCmd[slot][6];
	uint8_t *h = hostPtr(prp1 + (uint64_t)off4k * 4096, 4096);
	if (!h) {
		violation("자동 DMA의 호스트 주소가 범위 밖");
		return;
	}
	if (tx)
		memcpy(h, (void *)(uintptr_t)devAddr, 4096);
	else
		memcpy((void *)(uintptr_t)devAddr, h, 4096);
	sim_cpl_slot *c = &simShared->cpl[slot];
	c->dmaBlocks++;
	if (autoCompletion && c->expectBlocks && c->dmaBlocks == c->expectBlocks)
		complete(slot, 0, 0);
}

void set_auto_tx_dma(unsigned int cmdSlotTag, unsigned int cmd4KBOffset, unsigned int devAddr, unsigned int autoCompletion)
{
	unsigned char tempTail;
	autoDma(cmdSlotTag, cmd4KBOffset, devAddr, autoCompletion, 1);
	tempTail = g_hostDmaStatus.fifoTail.autoDmaTx++;
	if (tempTail > g_hostDmaStatus.fifoTail.autoDmaTx)
		g_hostDmaAssistStatus.autoDmaTxOverFlowCnt++;
	g_hostDmaStatus.autoDmaTxCnt++;
	g_hostDmaStatus.fifoHead.autoDmaTx = g_hostDmaStatus.fifoTail.autoDmaTx;
}

void set_auto_rx_dma(unsigned int cmdSlotTag, unsigned int cmd4KBOffset, unsigned int devAddr, unsigned int autoCompletion)
{
	unsigned char tempTail;
	autoDma(cmdSlotTag, cmd4KBOffset, devAddr, autoCompletion, 0);
	tempTail = g_hostDmaStatus.fifoTail.autoDmaRx++;
	if (tempTail > g_hostDmaStatus.fifoTail.autoDmaRx)
		g_hostDmaAssistStatus.autoDmaRxOverFlowCnt++;
	g_hostDmaStatus.autoDmaRxCnt++;
	g_hostDmaStatus.fifoHead.autoDmaRx = g_hostDmaStatus.fifoTail.autoDmaRx;
}

void check_direct_tx_dma_done()
{
}

void check_direct_rx_dma_done()
{
}

void check_auto_tx_dma_done()
{
}

void check_auto_rx_dma_done()
{
}

unsigned int check_auto_tx_dma_partial_done(unsigned int tailIndex, unsigned int tailAssistIndex)
{
	(void)tailIndex;
	(void)tailAssistIndex;
	return 1;
}

unsigned int check_auto_rx_dma_partial_done(unsigned int tailIndex, unsigned int tailAssistIndex)
{
	(void)tailIndex;
	(void)tailAssistIndex;
	return 1;
}

/** 보드 주소 영역을 잡는다. 호스트 쪽이 sim_init에서 같은 자리를 PROT_NONE으로 비워 두었으므로 덮어 잡는다 */
static void mapFixed(uintptr_t addr, size_t len, const char *what)
{
	void *p = mmap((void *)addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE,
		-1, 0);
	if (p != (void *)addr) {
		fprintf(stderr, "[sim] %s 영역(0x%lx)을 잡지 못함\n", what, (unsigned long)addr);
		_exit(70);
	}
}

/** 펌웨어 프로세스의 시작. 돌아오지 않는다 */
void sim_device_main(void)
{
	// DRAM 영역의 주소와 크기는 sim_reserve_board_memory와 같다
	// 보드의 DRAM: 관리용 버퍼(0x0010_0000~0x002F_FFFF)와 FTL 영역(0x1000_0000~). 켤 때마다 0으로 시작한다
	mapFixed(DRAM_START_ADDR, NVME_MANAGEMENT_END_ADDR + 1 - DRAM_START_ADDR, "DRAM 관리");
	mapFixed(FTL_MANAGEMENT_START_ADDR, (size_t)(DRAM_END_ADDR + 1 - FTL_MANAGEMENT_START_ADDR), "DRAM FTL");
	mapFixed(HOST_IP_ADDR, 0x10000, "NVMe 호스트 제어기 레지스터");
	memset(&g_hostDmaStatus, 0, sizeof g_hostDmaStatus);
	memset(&g_hostDmaAssistStatus, 0, sizeof g_hostDmaAssistStatus);
	statusReg()->ccEn = 1;
	g_nvmeTask.status = NVME_TASK_WAIT_CC_EN;
	g_nvmeTask.cacheEn = 0;
	simShared->booted = 1;
	nvme_main();
	_exit(0);
}

/** 보드 주소 영역(DRAM, 레지스터)을 호스트 프로세스에서 비워 둔다. 공유 메모리가 그 자리에 잡히지 않게 한다 */
void sim_reserve_board_memory(void)
{
	static int done = 0;
	if (done)
		return;
	done = 1;
	struct { uintptr_t addr; size_t len; } r[] = {
		{ DRAM_START_ADDR, NVME_MANAGEMENT_END_ADDR + 1 - DRAM_START_ADDR },
		{ FTL_MANAGEMENT_START_ADDR, (size_t)(DRAM_END_ADDR + 1 - FTL_MANAGEMENT_START_ADDR) },
		{ HOST_IP_ADDR, 0x10000 },
	};
	for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++) {
		void *p = mmap((void *)r[i].addr, r[i].len, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE | MAP_NORESERVE, -1, 0);
		if (p != (void *)r[i].addr) {
			fprintf(stderr, "[sim] 보드 주소 0x%lx를 비워 두지 못함\n", (unsigned long)r[i].addr);
			exit(2);
		}
	}
}

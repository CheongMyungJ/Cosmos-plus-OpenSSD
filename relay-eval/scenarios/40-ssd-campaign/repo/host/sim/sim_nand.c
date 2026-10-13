// NSC(NAND 저장 제어기) 드라이버 대역. firmware/nsc_driver.c 대신 링크한다.
// 명령은 부르는 즉시 끝난다(준비/바쁨은 늘 준비). NAND 내용은 공유 메모리라 전원을 꺼도 남는다.
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "../../firmware/ftl_config.h"
#include "../../firmware/nsc_driver.h"
#include "../../firmware/request_schedule.h"
#include "sim_shared.h"

#define ROWS_PER_DIE (TOTAL_BLOCKS_PER_DIE * ROWS_PER_MLC_BLOCK)
#define USER_SPARE_BYTES BYTES_PER_SPARE_REGION_OF_PAGE
#define ECC_CRC_BYTES 8

// way마다 마지막 프로그램·지우기의 실패 여부 (상태 확인이 돌려준다)
static uint8_t lastFail[USER_CHANNELS][USER_WAYS];

static unsigned channelOf(V2FMCRegisters *dev)
{
	return (unsigned)(((uintptr_t)dev - XPAR_TIGER4NSC_0_BASEADDR) / 0x10000);
}

/** 행 주소를 다이 안의 (물리 블록, 행)으로 푼다. LUN 1은 LUN_1_BASE_ADDR부터다 */
static int decodeRow(unsigned rowAddress, unsigned *phyBlock, unsigned *rowInDie)
{
	unsigned lun = rowAddress >= LUN_1_BASE_ADDR ? 1 : 0;
	unsigned within = rowAddress - (lun ? LUN_1_BASE_ADDR : LUN_0_BASE_ADDR);
	unsigned block = within / ROWS_PER_MLC_BLOCK;
	unsigned page = within % ROWS_PER_MLC_BLOCK;
	if (block >= TOTAL_BLOCKS_PER_LUN)
		return -1;
	*phyBlock = lun * TOTAL_BLOCKS_PER_LUN + block;
	*rowInDie = *phyBlock * ROWS_PER_MLC_BLOCK + page;
	return 0;
}

static unsigned dieOf(unsigned ch, unsigned way)
{
	return ch + way * USER_CHANNELS;
}

static uint8_t *rowPtr(unsigned die, unsigned rowInDie)
{
	return simNandData + ((size_t)die * ROWS_PER_DIE + rowInDie) * BYTES_PER_NAND_ROW;
}

static void record(uint8_t op, unsigned ch, unsigned way, unsigned row, uint8_t result)
{
	uint64_t n = simShared->oplogCount;
	if (n < SIM_OPLOG_MAX) {
		simOplog[n].op = op;
		simOplog[n].ch = (uint8_t)ch;
		simOplog[n].way = (uint8_t)way;
		simOplog[n].result = result;
		simOplog[n].row = row;
	}
	simShared->oplogCount = n + 1;
	simShared->opCount[op]++;
}

static void violation(const char *msg, unsigned die, unsigned row)
{
	simShared->violations++;
	if (simShared->violations == 1) {
		char *p = simShared->violationMsg;
		int i = 0;
		while (*msg && i < 200)
			p[i++] = *msg++;
		p[i] = 0;
		sim_log("[sim] NAND 규칙 위반: %s (die %u row %u)\n", simShared->violationMsg, die, row);
	}
}

static void powerCut(void)
{
	sim_log("[sim] 전원 차단\n");
	_exit(77);
}

static uint32_t crc32(const uint8_t *p, size_t n)
{
	uint32_t c = 0xFFFFFFFFu;
	for (size_t i = 0; i < n; i++) {
		c ^= p[i];
		for (int k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
	}
	return ~c;
}

unsigned int V2FIsControllerBusy(V2FMCRegisters *dev)
{
	(void)dev;
	return 0;
}

void V2FResetSync(V2FMCRegisters *dev, int way)
{
	record(SIM_OP_RESET, channelOf(dev), (unsigned)way, 0, 0);
}

void V2FSetFeaturesSync(V2FMCRegisters *dev, int way, unsigned int feature0x02, unsigned int feature0x10,
	unsigned int feature0x01, unsigned int payLoadAddr)
{
	(void)feature0x02;
	(void)feature0x10;
	(void)feature0x01;
	(void)payLoadAddr;
	record(SIM_OP_SET_FEATURES, channelOf(dev), (unsigned)way, 0, 0);
}

void V2FGetFeaturesSync(V2FMCRegisters *dev, int way, unsigned int *feature0x01, unsigned int *feature0x02,
	unsigned int *feature0x10, unsigned int *feature0x30)
{
	(void)dev;
	(void)way;
	*feature0x01 = 0;
	*feature0x02 = 0;
	*feature0x10 = 0;
	*feature0x30 = 0;
}

void V2FReadPageTriggerAsync(V2FMCRegisters *dev, int way, unsigned int rowAddress)
{
	unsigned ch = channelOf(dev), blk, row;
	if (decodeRow(rowAddress, &blk, &row) != 0)
		violation("읽기 행 주소가 범위 밖", dieOf(ch, (unsigned)way), rowAddress);
	record(SIM_OP_READ_TRIGGER, ch, (unsigned)way, rowAddress, 0);
}

/** 읽기 고장 주입: 해당 행에 남은 고장이 있으면 그 종류(1 고칠 수 없음, 2 비트 오류 많음) */
static int readFault(unsigned die, unsigned row)
{
	for (int i = 0; i < SIM_READ_FAULTS; i++) {
		sim_read_fault *f = &simShared->readFaults[i];
		if (f->kind && f->die == (int32_t)die && f->row == (int32_t)row && f->remaining != 0) {
			if (f->remaining > 0)
				f->remaining--;
			return f->kind;
		}
	}
	return 0;
}

void V2FReadPageTransferAsync(V2FMCRegisters *dev, int way, void *pageDataBuffer, void *spareDataBuffer,
	unsigned int *errorInformation, unsigned int *completion, unsigned int rowAddress)
{
	unsigned ch = channelOf(dev), die = dieOf(ch, (unsigned)way), blk, row;
	uint8_t result = 0;
	*completion = 0;
	if (decodeRow(rowAddress, &blk, &row) != 0) {
		violation("읽기 행 주소가 범위 밖", die, rowAddress);
		errorInformation[0] = 0;
		errorInformation[1] = 0;
		*completion = 1;
		return;
	}
	if (simNandProgrammed[(size_t)die * ROWS_PER_DIE + row]) {
		uint8_t *r = rowPtr(die, row);
		memcpy(pageDataBuffer, r, BYTES_PER_DATA_REGION_OF_PAGE);
		memcpy(spareDataBuffer, r + BYTES_PER_DATA_REGION_OF_PAGE, USER_SPARE_BYTES);
	} else {
		// 지운 행은 0xFF로 읽히고 ECC도 통과한다
		memset(pageDataBuffer, 0xFF, BYTES_PER_DATA_REGION_OF_PAGE);
		memset(spareDataBuffer, 0xFF, USER_SPARE_BYTES);
	}
	// errorInformation[0]: CRC 유효(0x10000000), spare 조각 유효(0x01000000), 가장 나쁜 조각의 비트 오류 수(23:16)
	// errorInformation[1]: 데이터 조각이 모두 유효하면 0xffffffff
	unsigned worst = 0;
	errorInformation[0] = 0x11000000u;
	errorInformation[1] = 0xFFFFFFFFu;
	switch (readFault(die, row)) {
	case 1:
		errorInformation[1] = 0xFFFF0000u;
		memset(pageDataBuffer, 0xA5, BYTES_PER_DATA_REGION_OF_PAGE);
		result = 1;
		break;
	case 2:
		worst = BIT_ERROR_THRESHOLD_PER_CHUNK + 5;
		result = 2;
		break;
	}
	errorInformation[0] |= (worst & 0xFF) << 16;
	record(SIM_OP_READ_TRANSFER, ch, (unsigned)way, rowAddress, result);
	*completion = 1;
}

void V2FReadPageTransferRawAsync(V2FMCRegisters *dev, int way, void *pageDataBuffer, unsigned int *completion)
{
	// 실제 제어기는 앞의 읽기 트리거가 정한 행을 옮긴다. 시뮬레이터는 마지막 읽기 트리거의 행을 쓴다
	unsigned ch = channelOf(dev), die = dieOf(ch, (unsigned)way), blk, row;
	unsigned rowAddress = 0;
	*completion = 0;
	for (uint64_t i = simShared->oplogCount; i > 0; i--) {
		sim_op *o = &simOplog[(i - 1) % SIM_OPLOG_MAX];
		if (o->op == SIM_OP_READ_TRIGGER && o->ch == ch && o->way == way) {
			rowAddress = o->row;
			break;
		}
	}
	if (decodeRow(rowAddress, &blk, &row) == 0 && simNandProgrammed[(size_t)die * ROWS_PER_DIE + row]) {
		memcpy(pageDataBuffer, rowPtr(die, row), BYTES_PER_NAND_ROW);
	} else {
		memset(pageDataBuffer, 0xFF, BYTES_PER_NAND_ROW);
		// 공장 배드 블록은 0번 행과 마지막 행의 데이터 첫 바이트와 spare 첫 바이트가 0이다
		unsigned page = row % ROWS_PER_MLC_BLOCK;
		if (decodeRow(rowAddress, &blk, &row) == 0 && simFactoryBad[(size_t)die * TOTAL_BLOCKS_PER_DIE + blk] &&
			(page == BAD_BLOCK_MARK_PAGE0 || page == BAD_BLOCK_MARK_PAGE1)) {
			((uint8_t *)pageDataBuffer)[BAD_BLOCK_MARK_BYTE0] = 0;
			((uint8_t *)pageDataBuffer)[BAD_BLOCK_MARK_BYTE1] = 0;
		}
	}
	record(SIM_OP_READ_RAW, ch, (unsigned)way, rowAddress, 0);
	*completion = 1;
}

void V2FProgramPageAsync(V2FMCRegisters *dev, int way, unsigned int rowAddress, void *pageDataBuffer,
	void *spareDataBuffer)
{
	unsigned ch = channelOf(dev), die = dieOf(ch, (unsigned)way), blk, row;
	if (decodeRow(rowAddress, &blk, &row) != 0) {
		violation("프로그램 행 주소가 범위 밖", die, rowAddress);
		lastFail[ch][way] = 1;
		record(SIM_OP_PROGRAM, ch, (unsigned)way, rowAddress, 1);
		return;
	}
	if (simShared->cutAfterPrograms > 0 && --simShared->cutAfterPrograms == 0)
		powerCut();
	size_t idx = (size_t)die * TOTAL_BLOCKS_PER_DIE + blk;
	int32_t *pf = &simProgramFail[idx];
	if (*pf != 0) {
		if (*pf > 0)
			(*pf)--;
		lastFail[ch][way] = 1;
		record(SIM_OP_PROGRAM, ch, (unsigned)way, rowAddress, 1);
		return;
	}
	size_t r = (size_t)die * ROWS_PER_DIE + row;
	if (simNandProgrammed[r])
		violation("지우지 않은 행에 다시 프로그램", die, row);
	if (simLastRow[idx] >= (int32_t)row)
		violation("블록 안에서 앞 행으로 돌아가 프로그램", die, row);
	simLastRow[idx] = (int32_t)row;
	uint8_t *p = rowPtr(die, row);
	memcpy(p, pageDataBuffer, BYTES_PER_DATA_REGION_OF_PAGE);
	memcpy(p + BYTES_PER_DATA_REGION_OF_PAGE, spareDataBuffer, USER_SPARE_BYTES);
	// 사용자 spare의 마지막 8바이트는 ECC 엔진이 CRC로 덮어쓴다
	uint32_t c = crc32(p, BYTES_PER_DATA_REGION_OF_PAGE);
	uint8_t *tail = p + BYTES_PER_DATA_REGION_OF_PAGE + USER_SPARE_BYTES - ECC_CRC_BYTES;
	memcpy(tail, &c, 4);
	memcpy(tail + 4, &c, 4);
	memset(p + BYTES_PER_DATA_REGION_OF_PAGE + USER_SPARE_BYTES, 0xFF,
		BYTES_PER_NAND_ROW - BYTES_PER_DATA_REGION_OF_PAGE - USER_SPARE_BYTES);
	if ((row % ROWS_PER_MLC_BLOCK == BAD_BLOCK_MARK_PAGE0 || row % ROWS_PER_MLC_BLOCK == BAD_BLOCK_MARK_PAGE1) &&
		p[BAD_BLOCK_MARK_BYTE1] != 0xFF)
		simShared->badMarkWrites++;
	simNandProgrammed[r] = 1;
	lastFail[ch][way] = 0;
	record(SIM_OP_PROGRAM, ch, (unsigned)way, rowAddress, 0);
}

void V2FEraseBlockAsync(V2FMCRegisters *dev, int way, unsigned int rowAddress)
{
	unsigned ch = channelOf(dev), die = dieOf(ch, (unsigned)way), blk, row;
	if (decodeRow(rowAddress, &blk, &row) != 0) {
		violation("지우기 행 주소가 범위 밖", die, rowAddress);
		lastFail[ch][way] = 1;
		record(SIM_OP_ERASE, ch, (unsigned)way, rowAddress, 1);
		return;
	}
	if (simShared->cutAfterErases > 0 && --simShared->cutAfterErases == 0)
		powerCut();
	size_t idx = (size_t)die * TOTAL_BLOCKS_PER_DIE + blk;
	if (simEraseFail[idx] != 0) {
		if (simEraseFail[idx] > 0)
			simEraseFail[idx]--;
		lastFail[ch][way] = 1;
		record(SIM_OP_ERASE, ch, (unsigned)way, rowAddress, 1);
		return;
	}
	size_t first = (size_t)die * ROWS_PER_DIE + (size_t)blk * ROWS_PER_MLC_BLOCK;
	// 지운 행은 프로그램 표시만 지운다. 읽으면 0xFF다
	memset(simNandProgrammed + first, 0, ROWS_PER_MLC_BLOCK);
	simLastRow[idx] = -1;
	simEraseCount[idx]++;
	lastFail[ch][way] = 0;
	record(SIM_OP_ERASE, ch, (unsigned)way, rowAddress, 0);
}

void V2FStatusCheckAsync(V2FMCRegisters *dev, int way, unsigned int *statusReport)
{
	unsigned ch = channelOf(dev);
	unsigned status = 0x60u | (lastFail[ch][way] ? 1u : 0u);
	*statusReport = (status << 1) | 1u;
}

unsigned int V2FStatusCheckSync(V2FMCRegisters *dev, int way)
{
	unsigned int s;
	V2FStatusCheckAsync(dev, way, &s);
	return s >> 1;
}

unsigned int V2FReadyBusyAsync(V2FMCRegisters *dev)
{
	(void)dev;
	return 0xFFFFFFFFu;
}

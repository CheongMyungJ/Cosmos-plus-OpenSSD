// 시험 프로세스(호스트)와 펌웨어 프로세스(장치)가 함께 보는 공유 메모리.
// 펌웨어는 전원을 켤 때마다 새 자식 프로세스로 뜬다. DRAM(펌웨어의 전역 변수와 memory_map.h의 영역)은 프로세스와 함께
// 사라지고, 여기 있는 NAND 내용과 지우기 횟수만 전원 주기를 넘어 남는다.
#ifndef SIM_SHARED_H_
#define SIM_SHARED_H_

#include <stdint.h>

#define SIM_SLOTS 16
#define SIM_SLOT_BYTES (1u << 20)          // 명령 하나의 호스트 버퍼 (NVMe 블록 256개)
#define SIM_CMD_RING 64
#define SIM_LOG_BYTES (256u * 1024u)
#define SIM_OPLOG_MAX (1u << 21)
#define SIM_READ_FAULTS 64

// PRP 주소: 상위 32비트 SIM_PCIE_H, 하위 32비트 슬롯 * SIM_SLOT_BYTES
#define SIM_PCIE_H 1u

enum {
	SIM_OP_RESET = 1,
	SIM_OP_SET_FEATURES,
	SIM_OP_READ_TRIGGER,
	SIM_OP_READ_TRANSFER,
	SIM_OP_READ_RAW,
	SIM_OP_PROGRAM,
	SIM_OP_ERASE,
	SIM_OP_KINDS
};

typedef struct {
	uint8_t op;
	uint8_t ch;
	uint8_t way;
	uint8_t result;	// 0 성공, 1 실패, 2 경고(읽기의 비트 오류가 문턱을 넘음)
	uint32_t row;
} sim_op;

enum { SIM_CPL_NONE = 0, SIM_CPL_PENDING, SIM_CPL_DONE };

typedef struct {
	volatile uint32_t state;
	uint32_t statusFieldWord;
	uint32_t specific;
	uint32_t dmaBlocks;		// 자동 DMA로 옮긴 4KB 블록 수
	uint32_t expectBlocks;	// 자동 완료까지의 블록 수 (읽기·쓰기만)
	uint32_t completions;	// 완료를 몇 번 보냈나 (1이 정상)
} sim_cpl_slot;

typedef struct {
	int32_t die;
	int32_t row;		// 다이 안의 행 번호 (블록 * 행 수 + 페이지)
	int32_t kind;		// 1 고칠 수 없음, 2 고칠 수 있지만 비트 오류가 많음
	int32_t remaining;	// 남은 횟수 (-1 계속)
} sim_read_fault;

typedef struct {
	// 명령 고리: 호스트가 tail을 올리고 장치가 head를 올린다
	volatile uint32_t cmdHead;
	volatile uint32_t cmdTail;
	struct {
		uint16_t qid;
		uint16_t slot;
		uint32_t dw[16];
	} cmd[SIM_CMD_RING];
	sim_cpl_slot cpl[SIM_SLOTS];

	// 장치 상태
	volatile uint32_t booted;		// 펌웨어 프로세스가 떴음
	volatile uint32_t ready;		// CSTS.RDY
	volatile uint32_t shst;			// CSTS.SHST
	volatile uint32_t shutdownReq;	// 호스트가 CC.SHN을 씀
	char inbyteChar;				// 부팅 때 UART로 들어오는 글자 ('X'면 배드 블록 표를 새로 만든다)

	// 전원 차단 주입: 0보다 크면 그 수만큼의 프로그램(또는 지우기)을 한 뒤 다음 것 직전에 전원이 나간다
	volatile int64_t cutAfterPrograms;
	volatile int64_t cutAfterErases;

	// 고장 주입
	sim_read_fault readFaults[SIM_READ_FAULTS];

	// 관찰
	volatile uint64_t opCount[SIM_OP_KINDS];
	volatile uint64_t oplogCount;
	volatile uint32_t violations;	// NAND 규칙 위반 (지우지 않은 행에 다시 프로그램, 블록 안에서 거꾸로 프로그램)
	char violationMsg[256];
	volatile uint32_t badMarkWrites;	// 공장 배드 마크 자리(0번 행의 spare 첫 바이트)에 0xFF가 아닌 값을 쓴 횟수
	volatile uint32_t logPos;
	char log[SIM_LOG_BYTES];
} sim_shared;

extern sim_shared *simShared;

// NAND 배열: 다이마다 TOTAL_BLOCKS_PER_DIE 블록 × ROWS_PER_MLC_BLOCK 행, 행마다 BYTES_PER_NAND_ROW 바이트
// (앞 16KiB 데이터, 뒤 spare. spare 앞 256바이트가 ECC로 보호되는 사용자 spare이고 그 마지막 8바이트는 ECC 엔진이 쓴다)
extern uint8_t *simNandData;
extern uint8_t *simNandProgrammed;	// 행마다 1이면 프로그램됨
extern uint32_t *simEraseCount;		// [다이][물리 블록]
extern int32_t *simLastRow;			// [다이][물리 블록] 마지막으로 프로그램한 행 (-1 없음)
extern int32_t *simProgramFail;		// [다이][물리 블록] 남은 프로그램 실패 수 (-1 계속, 0 없음)
extern int32_t *simEraseFail;		// [다이][물리 블록]
extern uint8_t *simFactoryBad;		// [다이][물리 블록] 1이면 공장 배드 블록 (프로그램·지우기 실패, 배드 마크가 읽힘)
extern sim_op *simOplog;

void sim_log(const char *fmt, ...);

#endif

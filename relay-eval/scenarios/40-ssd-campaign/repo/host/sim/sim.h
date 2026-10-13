// 호스트 시뮬레이터의 시험용 API. 시험은 NVMe 명령을 넣고 완료와 데이터를 보며, NAND 모델을 들여다보고 고장을 넣는다.
// 펌웨어는 sim_power_on()마다 새 프로세스로 부팅한다. sim_power_cut()은 그 프로세스를 바로 죽인다(DRAM은 사라지고 NAND만 남음).
#ifndef SIM_H_
#define SIM_H_

#include <stddef.h>
#include <stdint.h>

#include "sim_shared.h"

// NVMe 상태 (완료의 Status Field에서 SCT와 SC)
typedef struct {
	int ok;				// 완료가 왔음
	int hung;			// 시간 안에 완료가 오지 않음 (펌웨어의 ASSERT는 무한 루프다)
	int crashed;		// 펌웨어 프로세스가 죽음 (assert 등)
	unsigned sct;
	unsigned sc;
	uint32_t dw0;		// 완료의 명령별 값 (specific)
} sim_status;

// 성공 완료인가. 명령을 바로 넣어도 된다(SIM_SUCCESS(sim_write(...))): 인자를 한 번만 평가한다
static inline int sim_success(sim_status s)
{
	return s.ok && s.sct == 0 && s.sc == 0;
}
#define SIM_SUCCESS(s) sim_success(s)

// 시뮬레이터를 새 NAND로 준비한다 (모든 블록 지운 상태). 펌웨어는 아직 꺼져 있다
void sim_init(void);
// 펌웨어를 켜고 CSTS.RDY까지 기다린다. 실패하면 0
int sim_power_on(void);
// 전원을 바로 끊는다
void sim_power_cut(void);
// 정상 종료 알림(CC.SHN)을 보내고 CSTS.SHST가 완료가 되면 전원을 끈다. 시간 안에 끝나지 않으면 0
int sim_shutdown(void);
// 펌웨어가 켜져 있나
int sim_running(void);

// 명령. dw[0]의 CID와 PRP(dw6~9)는 시뮬레이터가 채운다. buf는 PRP1이 가리키는 호스트 메모리로 복사되고(toDevice),
// 완료 뒤 호스트 메모리에서 buf로 복사된다(fromDevice). len은 SIM_SLOT_BYTES 이하
sim_status sim_admin(const uint32_t dw[16], void *buf, size_t len, int toDevice);
sim_status sim_io(const uint32_t dw[16], void *buf, size_t len, int toDevice);

// 자주 쓰는 명령. 블록은 NVMe 논리 블록(4KiB)
sim_status sim_write(uint64_t lba, unsigned blocks, const void *buf);
sim_status sim_read(uint64_t lba, unsigned blocks, void *buf);
sim_status sim_flush(void);
sim_status sim_identify(unsigned cns, void *buf4k);
// 이름공간 크기(Identify Namespace의 NSZE)
uint64_t sim_capacity(void);

// NAND 기하 (펌웨어 설정에서)
typedef struct {
	unsigned channels, ways, dies;
	unsigned blocksPerDie;		// 물리 블록 (예비 포함)
	unsigned rowsPerBlock;		// MLC 기준 행 수
	unsigned pagesPerBlock;		// 사용자 페이지 수 (SLC면 LSB 행만)
	unsigned bytesPerPage;		// 데이터 영역
	unsigned spareBytes;		// 사용자 spare
	unsigned rowBytes;			// 데이터 + 원시 spare
} sim_geometry;
sim_geometry sim_geo(void);

// NAND 들여다보기. die는 ch + way * channels, row는 블록 안의 행
int sim_row_programmed(unsigned die, unsigned phyBlock, unsigned row);
const uint8_t *sim_row_data(unsigned die, unsigned phyBlock, unsigned row);
const uint8_t *sim_row_spare(unsigned die, unsigned phyBlock, unsigned row);
uint32_t sim_erase_count(unsigned die, unsigned phyBlock);
// 데이터 영역이 prefix로 시작하는 프로그램된 행을 찾는다 (가장 최근에 프로그램한 것). 찾으면 1
int sim_find_row(const void *prefix, size_t n, unsigned *die, unsigned *phyBlock, unsigned *row);
// 지금까지의 NAND 명령 수 (SIM_OP_*)
uint64_t sim_op_count(int op);
// NAND 명령 기록 (최근 SIM_OPLOG_MAX개까지)
uint64_t sim_oplog_count(void);
const sim_op *sim_oplog(uint64_t i);
uint32_t sim_violations(void);
const char *sim_violation_msg(void);
uint32_t sim_bad_mark_writes(void);
// 펌웨어 로그(xil_printf)의 끝 부분
const char *sim_log_tail(size_t n);

// 고장 주입
void sim_factory_bad(unsigned die, unsigned phyBlock);
void sim_fail_program(unsigned die, unsigned phyBlock, int times);	// times -1이면 계속
void sim_fail_erase(unsigned die, unsigned phyBlock, int times);
// kind 1: 고칠 수 없는 읽기, 2: 고쳐지지만 비트 오류가 문턱을 넘음
void sim_fail_read(unsigned die, unsigned phyBlock, unsigned row, int kind, int times);
// n번째 프로그램(지우기) 직전에 전원이 나간다. 0이면 끈다
void sim_cut_after_programs(int64_t n);
void sim_cut_after_erases(int64_t n);
// 다음 부팅 때 UART로 들어갈 글자 ('X'면 배드 블록 표를 새로 만든다)
void sim_set_inbyte(char c);

#endif

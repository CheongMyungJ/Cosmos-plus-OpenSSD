// 호스트(시험 프로세스) 쪽: 공유 메모리 준비, 펌웨어 프로세스 켜고 끄기, 명령 넣고 완료 기다리기, NAND 들여다보기.
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../../firmware/ftl_config.h"
#include "sim.h"

#define ROWS_PER_DIE (TOTAL_BLOCKS_PER_DIE * ROWS_PER_MLC_BLOCK)
#define BLOCKS_ALL ((size_t)USER_DIES * TOTAL_BLOCKS_PER_DIE)
#define ROWS_ALL ((size_t)USER_DIES * ROWS_PER_DIE)

sim_shared *simShared;
uint8_t *simHostMem;
uint8_t *simNandData;
uint8_t *simNandProgrammed;
uint32_t *simEraseCount;
int32_t *simLastRow;
int32_t *simProgramFail;
int32_t *simEraseFail;
uint8_t *simFactoryBad;
sim_op *simOplog;

void sim_device_main(void);
void sim_reserve_board_memory(void);

static pid_t device = 0;
static unsigned nextSlot = 0;

static void *shared(size_t len)
{
	void *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (p == MAP_FAILED) {
		perror("[sim] mmap");
		exit(2);
	}
	return p;
}

static void unshared(void *p, size_t len)
{
	if (p)
		munmap(p, len);
}

static long timeoutMs(void)
{
	const char *t = getenv("SIM_TIMEOUT_MS");
	return t ? atol(t) : 5000;
}

static long nowMs(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void pause_us(long us)
{
	struct timespec ts = { 0, us * 1000L };
	nanosleep(&ts, NULL);
}

static void reap(void)
{
	if (device > 0) {
		kill(device, SIGKILL);
		waitpid(device, NULL, 0);
		device = 0;
	}
}

void sim_init(void)
{
	sim_reserve_board_memory();
	reap();
	unshared(simShared, sizeof(sim_shared));
	unshared(simHostMem, (size_t)SIM_SLOTS * SIM_SLOT_BYTES);
	unshared(simNandData, ROWS_ALL * BYTES_PER_NAND_ROW);
	unshared(simNandProgrammed, ROWS_ALL);
	unshared(simEraseCount, BLOCKS_ALL * 4);
	unshared(simLastRow, BLOCKS_ALL * 4);
	unshared(simProgramFail, BLOCKS_ALL * 4);
	unshared(simEraseFail, BLOCKS_ALL * 4);
	unshared(simFactoryBad, BLOCKS_ALL);
	unshared(simOplog, (size_t)SIM_OPLOG_MAX * sizeof(sim_op));
	simShared = shared(sizeof(sim_shared));
	simHostMem = shared((size_t)SIM_SLOTS * SIM_SLOT_BYTES);
	simNandData = shared(ROWS_ALL * BYTES_PER_NAND_ROW);
	simNandProgrammed = shared(ROWS_ALL);
	simEraseCount = shared(BLOCKS_ALL * 4);
	simLastRow = shared(BLOCKS_ALL * 4);
	simProgramFail = shared(BLOCKS_ALL * 4);
	simEraseFail = shared(BLOCKS_ALL * 4);
	simFactoryBad = shared(BLOCKS_ALL);
	simOplog = shared((size_t)SIM_OPLOG_MAX * sizeof(sim_op));
	for (size_t i = 0; i < BLOCKS_ALL; i++)
		simLastRow[i] = -1;
	signal(SIGPIPE, SIG_IGN);
}

int sim_running(void)
{
	if (device <= 0)
		return 0;
	int st;
	pid_t r = waitpid(device, &st, WNOHANG);
	if (r == device) {
		device = 0;
		return 0;
	}
	return 1;
}

int sim_power_on(void)
{
	if (!simShared)
		sim_init();
	reap();
	simShared->booted = 0;
	simShared->ready = 0;
	simShared->shst = 0;
	simShared->shutdownReq = 0;
	simShared->cmdHead = 0;
	simShared->cmdTail = 0;
	memset(simShared->cpl, 0, sizeof simShared->cpl);
	fflush(NULL);
	pid_t pid = fork();
	if (pid < 0) {
		perror("[sim] fork");
		exit(2);
	}
	if (pid == 0) {
		sim_device_main();
		_exit(0);
	}
	device = pid;
	long t0 = nowMs();
	// 부팅은 모든 사용자 블록을 지우므로 명령보다 오래 걸릴 수 있다
	while (!simShared->ready) {
		if (!sim_running()) {
			fprintf(stderr, "[sim] 펌웨어가 부팅 중에 끝남\n%s\n", sim_log_tail(2000));
			return 0;
		}
		if (nowMs() - t0 > timeoutMs() * 6) {
			fprintf(stderr, "[sim] 펌웨어가 시간 안에 준비되지 않음\n%s\n", sim_log_tail(2000));
			reap();
			return 0;
		}
		pause_us(200);
	}
	return 1;
}

void sim_power_cut(void)
{
	reap();
	simShared->ready = 0;
}

int sim_shutdown(void)
{
	if (!sim_running())
		return 0;
	simShared->shutdownReq = 1;
	long t0 = nowMs();
	while (simShared->shst != 2) {
		if (!sim_running() || nowMs() - t0 > timeoutMs()) {
			fprintf(stderr, "[sim] 정상 종료가 끝나지 않음\n%s\n", sim_log_tail(2000));
			reap();
			return 0;
		}
		pause_us(200);
	}
	reap();
	simShared->ready = 0;
	return 1;
}

static sim_status submit(int admin, const uint32_t dwIn[16], void *buf, size_t len, int toDevice)
{
	sim_status s = { 0 };
	if (!sim_running()) {
		s.crashed = 1;
		return s;
	}
	if (len > SIM_SLOT_BYTES) {
		fprintf(stderr, "[sim] 버퍼가 너무 큼: %zu\n", len);
		exit(2);
	}
	unsigned slot = nextSlot++ % SIM_SLOTS;
	uint32_t dw[16];
	memcpy(dw, dwIn, sizeof dw);
	uint32_t prpL = slot * SIM_SLOT_BYTES;
	dw[0] = (dw[0] & 0xFFFFu) | (slot << 16);
	dw[6] = prpL;
	dw[7] = SIM_PCIE_H;
	dw[8] = 0;
	dw[9] = 0;
	uint8_t *h = simHostMem + (size_t)slot * SIM_SLOT_BYTES;
	memset(h, 0xCC, SIM_SLOT_BYTES);
	if (buf && len && toDevice)
		memcpy(h, buf, len);
	sim_cpl_slot *c = &simShared->cpl[slot];
	memset(c, 0, sizeof *c);
	unsigned opc = dw[0] & 0xFF;
	if (!admin && (opc == 0x01 || opc == 0x02))
		c->expectBlocks = (dw[12] & 0xFFFF) + 1;
	c->state = SIM_CPL_PENDING;
	uint32_t tail = simShared->cmdTail;
	unsigned i = tail % SIM_CMD_RING;
	simShared->cmd[i].qid = admin ? 0 : 1;
	simShared->cmd[i].slot = (uint16_t)slot;
	memcpy(simShared->cmd[i].dw, dw, sizeof dw);
	__atomic_store_n(&simShared->cmdTail, tail + 1, __ATOMIC_RELEASE);
	long t0 = nowMs();
	while (__atomic_load_n(&c->state, __ATOMIC_ACQUIRE) != SIM_CPL_DONE) {
		if (!sim_running()) {
			s.crashed = 1;
			fprintf(stderr, "[sim] 명령 처리 중 펌웨어가 끝남 (opc %02x)\n%s\n", opc, sim_log_tail(1500));
			return s;
		}
		if (nowMs() - t0 > timeoutMs()) {
			s.hung = 1;
			fprintf(stderr, "[sim] 명령 완료가 오지 않음 (opc %02x)\n%s\n", opc, sim_log_tail(1500));
			reap();
			return s;
		}
		pause_us(20);
	}
	s.ok = 1;
	s.sc = (c->statusFieldWord >> 1) & 0xFF;
	s.sct = (c->statusFieldWord >> 9) & 0x7;
	s.dw0 = c->specific;
	if (buf && len && !toDevice)
		memcpy(buf, h, len);
	return s;
}

sim_status sim_admin(const uint32_t dw[16], void *buf, size_t len, int toDevice)
{
	return submit(1, dw, buf, len, toDevice);
}

sim_status sim_io(const uint32_t dw[16], void *buf, size_t len, int toDevice)
{
	return submit(0, dw, buf, len, toDevice);
}

sim_status sim_write(uint64_t lba, unsigned blocks, const void *buf)
{
	uint32_t dw[16] = { 0 };
	dw[0] = 0x01;
	dw[1] = 1;
	dw[10] = (uint32_t)lba;
	dw[11] = (uint32_t)(lba >> 32);
	dw[12] = (blocks - 1) & 0xFFFF;
	return sim_io(dw, (void *)buf, (size_t)blocks * BYTES_PER_NVME_BLOCK, 1);
}

sim_status sim_read(uint64_t lba, unsigned blocks, void *buf)
{
	uint32_t dw[16] = { 0 };
	dw[0] = 0x02;
	dw[1] = 1;
	dw[10] = (uint32_t)lba;
	dw[11] = (uint32_t)(lba >> 32);
	dw[12] = (blocks - 1) & 0xFFFF;
	return sim_io(dw, buf, (size_t)blocks * BYTES_PER_NVME_BLOCK, 0);
}

sim_status sim_flush(void)
{
	uint32_t dw[16] = { 0 };
	dw[0] = 0x00;
	dw[1] = 1;
	return sim_io(dw, NULL, 0, 0);
}

sim_status sim_identify(unsigned cns, void *buf4k)
{
	uint32_t dw[16] = { 0 };
	dw[0] = 0x06;
	dw[1] = 1;
	dw[10] = cns;
	return sim_admin(dw, buf4k, 4096, 0);
}

uint64_t sim_capacity(void)
{
	uint8_t id[4096];
	sim_status s = sim_identify(0, id);
	if (!SIM_SUCCESS(s))
		return 0;
	uint64_t nsze;
	memcpy(&nsze, id, 8);
	return nsze;
}

sim_geometry sim_geo(void)
{
	sim_geometry g;
	g.channels = USER_CHANNELS;
	g.ways = USER_WAYS;
	g.dies = USER_DIES;
	g.blocksPerDie = TOTAL_BLOCKS_PER_DIE;
	g.rowsPerBlock = ROWS_PER_MLC_BLOCK;
	g.pagesPerBlock = USER_PAGES_PER_BLOCK;
	g.bytesPerPage = BYTES_PER_DATA_REGION_OF_PAGE;
	g.spareBytes = BYTES_PER_SPARE_REGION_OF_PAGE;
	g.rowBytes = BYTES_PER_NAND_ROW;
	return g;
}

static size_t rowIndex(unsigned die, unsigned phyBlock, unsigned row)
{
	return (size_t)die * ROWS_PER_DIE + (size_t)phyBlock * ROWS_PER_MLC_BLOCK + row;
}

int sim_row_programmed(unsigned die, unsigned phyBlock, unsigned row)
{
	return simNandProgrammed[rowIndex(die, phyBlock, row)];
}

const uint8_t *sim_row_data(unsigned die, unsigned phyBlock, unsigned row)
{
	return simNandData + rowIndex(die, phyBlock, row) * BYTES_PER_NAND_ROW;
}

const uint8_t *sim_row_spare(unsigned die, unsigned phyBlock, unsigned row)
{
	return sim_row_data(die, phyBlock, row) + BYTES_PER_DATA_REGION_OF_PAGE;
}

uint32_t sim_erase_count(unsigned die, unsigned phyBlock)
{
	return simEraseCount[(size_t)die * TOTAL_BLOCKS_PER_DIE + phyBlock];
}

int sim_find_row(const void *prefix, size_t n, unsigned *die, unsigned *phyBlock, unsigned *row)
{
	// NAND 명령 기록을 뒤에서부터 보며 그 내용이 아직 남아 있는 가장 최근 프로그램을 찾는다
	uint64_t total = simShared->oplogCount;
	uint64_t lo = total > SIM_OPLOG_MAX ? total - SIM_OPLOG_MAX : 0;
	for (uint64_t i = total; i > lo; i--) {
		const sim_op *o = &simOplog[(i - 1) % SIM_OPLOG_MAX];
		if (o->op != SIM_OP_PROGRAM || o->result != 0)
			continue;
		unsigned lun = o->row >= 0x00200000 ? 1 : 0;
		unsigned within = o->row - (lun ? 0x00200000 : 0);
		unsigned blk = lun * TOTAL_BLOCKS_PER_LUN + within / ROWS_PER_MLC_BLOCK;
		unsigned r = within % ROWS_PER_MLC_BLOCK;
		unsigned d = o->ch + o->way * USER_CHANNELS;
		if (sim_row_programmed(d, blk, r) && memcmp(sim_row_data(d, blk, r), prefix, n) == 0) {
			*die = d;
			*phyBlock = blk;
			*row = r;
			return 1;
		}
	}
	return 0;
}

uint64_t sim_op_count(int op)
{
	return simShared->opCount[op];
}

uint64_t sim_oplog_count(void)
{
	return simShared->oplogCount;
}

const sim_op *sim_oplog(uint64_t i)
{
	return &simOplog[i % SIM_OPLOG_MAX];
}

uint32_t sim_violations(void)
{
	return simShared->violations;
}

const char *sim_violation_msg(void)
{
	return simShared->violationMsg;
}

uint32_t sim_bad_mark_writes(void)
{
	return simShared->badMarkWrites;
}

const char *sim_log_tail(size_t n)
{
	static char out[SIM_LOG_BYTES + 1];
	uint32_t end = simShared->logPos;
	if (n > SIM_LOG_BYTES)
		n = SIM_LOG_BYTES;
	if (n > end)
		n = end;
	for (size_t i = 0; i < n; i++)
		out[i] = simShared->log[(end - n + i) % SIM_LOG_BYTES];
	out[n] = 0;
	return out;
}

void sim_factory_bad(unsigned die, unsigned phyBlock)
{
	size_t i = (size_t)die * TOTAL_BLOCKS_PER_DIE + phyBlock;
	simFactoryBad[i] = 1;
	simProgramFail[i] = -1;
	simEraseFail[i] = -1;
}

void sim_fail_program(unsigned die, unsigned phyBlock, int times)
{
	simProgramFail[(size_t)die * TOTAL_BLOCKS_PER_DIE + phyBlock] = times;
}

void sim_fail_erase(unsigned die, unsigned phyBlock, int times)
{
	simEraseFail[(size_t)die * TOTAL_BLOCKS_PER_DIE + phyBlock] = times;
}

void sim_fail_read(unsigned die, unsigned phyBlock, unsigned row, int kind, int times)
{
	for (int i = 0; i < SIM_READ_FAULTS; i++) {
		sim_read_fault *f = &simShared->readFaults[i];
		if (f->kind == 0 || f->remaining == 0) {
			f->die = (int32_t)die;
			f->row = (int32_t)(phyBlock * ROWS_PER_MLC_BLOCK + row);
			f->kind = kind;
			f->remaining = times;
			return;
		}
	}
	fprintf(stderr, "[sim] 읽기 고장을 더 넣을 자리가 없음\n");
	exit(2);
}

void sim_cut_after_programs(int64_t n)
{
	simShared->cutAfterPrograms = n;
}

void sim_cut_after_erases(int64_t n)
{
	simShared->cutAfterErases = n;
}

void sim_set_inbyte(char c)
{
	simShared->inbyteChar = c;
}

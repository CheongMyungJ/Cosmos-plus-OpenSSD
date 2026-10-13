# GreedyFTL (Cosmos+ OpenSSD) 팀 레포

Cosmos+ OpenSSD 보드의 NVMe SSD 펌웨어 [GreedyFTL 3.0.0](https://github.com/Cosmos-OpenSSD/Cosmos-plus-OpenSSD)을 가져와 기능을 더해 가는 레포입니다. 보드 없이 개발할 수 있게 PC에서 도는 호스트 시뮬레이터가 있습니다.

## 폴더

| 폴더 | 내용 |
|---|---|
| `firmware/` | 펌웨어. 보드에서는 Xilinx SDK로 빌드합니다 |
| `firmware/nvme/` | NVMe 명령 처리 (`nvme_main.c`의 주 루프, 관리·입출력 명령, Identify) |
| `host/sim/` | 호스트 시뮬레이터: NAND 모델, NVMe 호스트 제어기 대역, Xilinx 헤더 대역 |
| `host/tests/` | 시뮬레이터 위의 시험 (`test_*.c` 하나가 실행 파일 하나) |
| `docs/design/` | 설계 문서 |

## 빌드와 시험

```bash
make test        # host/tests/test_*.c를 모두 빌드하고 돌린다
make clean
SIM_VERBOSE=1 ./host/build/test_basic   # 펌웨어 로그(xil_printf)를 함께 본다
```

gcc(또는 cc)와 make만 있으면 됩니다. 리눅스 x86-64에서 돕니다.

## 펌웨어 구조 (요약)

- NVMe 명령 → `ReqTransNvmeToSlice`가 슬라이스(16KiB, NVMe 블록 4개) 요청으로 나눔 → `ReqTransSliceToLowLevel`이 데이터 버퍼(DRAM, LRU)를 잡고, 미스면 더러운 버퍼를 NAND에 내리고(`EvictDataBufEntry`) 필요하면 NAND에서 읽음 → `SelectLowLevelReqQ`가 의존성을 보고 NAND 요청 큐에 넣음 → `SchedulingNandReq`가 채널·웨이마다 NAND 명령을 냄
- 주소 변환(`address_translation.c`): 논리 슬라이스 → 가상 슬라이스(다이, 블록, 페이지) 페이지 매핑. 쓰기는 다이를 돌아가며 각 다이의 현재 블록에 이어 씀
- GC(`garbage_collection.c`): 빈 블록이 모자라면 무효 슬라이스가 가장 많은 블록을 골라 유효 데이터를 옮기고 지움
- 표(매핑, 블록, 버퍼, 요청 풀)는 `memory_map.h`가 정한 DRAM 고정 주소에 있음
- 하드웨어 계층: `nsc_driver.c`(NAND 제어기 레지스터), `nvme/host_lld.c`(NVMe 호스트 제어기 레지스터, DMA), `main.c`(보드 초기화)

## 호스트 시뮬레이터

`host/Makefile`이 `firmware/`의 C 파일을 작은 NAND 기하로 빌드하고, 하드웨어 계층 셋 대신 `host/sim/`을 링크합니다. 하드웨어 계층에 함수를 더하면 `host/sim/`에도 같은 함수를 더해야 시뮬레이터가 빌드됩니다.

- **기하:** 2채널 × 2웨이 = 다이 4, 다이마다 LUN 2, LUN마다 블록 16(+예비 2), 블록마다 SLC 페이지 64(MLC 행 128 가운데 LSB 행), 페이지 16KiB + 사용자 spare 256바이트. 이름공간은 약 112MiB(NVMe 블록 4KiB)
- **전원:** `sim_power_on()`마다 펌웨어가 새 프로세스로 부팅합니다. `sim_power_cut()`은 그 프로세스를 바로 죽입니다. DRAM(전역 변수와 `memory_map.h`의 영역)은 사라지고 NAND 내용만 남습니다. `sim_shutdown()`은 정상 종료 알림(CC.SHN)을 보내고 완료(CSTS.SHST=2)를 기다립니다
- **DRAM 주소:** 펌웨어는 주소를 32비트 `unsigned int`로 다룹니다. 시뮬레이터는 `memory_map.h`의 주소 범위를 그 주소에 그대로 잡습니다. 스택이나 전역 변수의 주소를 `unsigned int`로 바꿔 DMA·NAND 버퍼로 넘기면 시뮬레이터에서는 깨집니다(보드에서도 캐시 일관성 문제가 생깁니다). 그런 버퍼는 `memory_map.h`의 영역에 둡니다
- **NAND:** 명령은 즉시 끝납니다. 지운 페이지를 읽으면 0xFF이고 ECC는 통과합니다. 프로그램할 때 사용자 spare의 마지막 8바이트는 ECC 엔진이 CRC로 덮어씁니다. 지우지 않은 행에 다시 프로그램하거나 블록 안에서 앞 행으로 돌아가 프로그램하면 위반으로 셉니다(`sim_violations()`). 원시 읽기(ECC 끔)는 데이터 16KiB 뒤에 spare 영역을 그대로 붙여 줍니다
- **명령:** `sim_write`, `sim_read`, `sim_flush`, `sim_identify`, 아무 명령이나 `sim_io`/`sim_admin`(dword 16개와 데이터 버퍼). 펌웨어의 `ASSERT`는 무한 루프라 시뮬레이터는 시간 제한(`SIM_TIMEOUT_MS`, 기본 5초)으로 `hung`을 돌려줍니다
- **고장 주입과 관찰:** 공장 배드 블록, 프로그램·지우기 실패, 읽기 오류(고칠 수 없음 / 비트 오류 많음), n번째 프로그램 직전 전원 차단. NAND 행 내용, spare, 블록별 지우기 횟수, NAND 명령 기록. `host/sim/sim.h` 참고

## 시험 쓰기

`host/tests/test_<이름>.c`를 만들면 `make test`가 빌드하고 돌립니다. `check.h`의 `RUN(fn)`이 시험마다 새 NAND로 시뮬레이터를 준비합니다.

```c
#include "check.h"

static unsigned char buf[4 * 4096];

static void writes_and_reads(void)
{
	REQUIRE(sim_power_on());
	fill_pattern(buf, 8, 4, 1);
	CHECK(SIM_SUCCESS(sim_write(8, 4, buf)));
	CHECK(SIM_SUCCESS(sim_read(8, 4, buf)));
	CHECK(same_pattern(buf, 8, 4, 1));
}

int main(void)
{
	RUN(writes_and_reads);
	return CHECK_DONE();
}
```

## 라이선스

GPLv3 (`LICENSE`). 원본은 Hanyang University ENC Lab의 Cosmos+ OpenSSD입니다.

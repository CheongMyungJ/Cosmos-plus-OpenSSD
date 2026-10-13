# 호스트 시뮬레이터 빌드와 시험. firmware/의 C 파일을 작은 NAND 기하로 PC에서 빌드한다.
# 하드웨어 계층(main.c, nsc_driver.c, nvme/host_lld.c)은 빌드하지 않고 sim/이 대신한다.
FW := ../firmware
BUILD := build

FW_SRCS := $(filter-out $(FW)/main.c $(FW)/nsc_driver.c $(FW)/nvme/host_lld.c,$(wildcard $(FW)/*.c $(FW)/nvme/*.c))
SIM_SRCS := $(wildcard sim/*.c)
TEST_SRCS := $(wildcard tests/test_*.c)
TESTS := $(patsubst tests/%.c,$(BUILD)/%,$(TEST_SRCS))

# 시뮬레이터 기하: 2채널 x 2웨이, LUN 2, LUN당 블록 16(+예비 2), 블록당 SLC 페이지 64
GEOMETRY := -DROWS_PER_SLC_BLOCK=64 -DROWS_PER_MLC_BLOCK=128 -DMAIN_BLOCKS_PER_LUN=16 \
	-DEXTENDED_BLOCKS_PER_LUN=2 -DUSER_BLOCKS_PER_LUN=16 -DUSER_WAYS=2

CC ?= cc
COMMON := -std=gnu11 -g -O1 -fno-strict-aliasing -fno-pie -fcommon -I sim -I $(FW) -I $(FW)/nvme $(GEOMETRY)
# 펌웨어는 32비트 보드용이라 주소를 unsigned int로 다룬다. 그 경고는 끈다
FW_CFLAGS := $(COMMON) -w
SIM_CFLAGS := $(COMMON) -Wall -Wno-unused-parameter
LDFLAGS := -no-pie

FW_OBJS := $(patsubst $(FW)/%.c,$(BUILD)/fw/%.o,$(FW_SRCS))
SIM_OBJS := $(patsubst sim/%.c,$(BUILD)/sim/%.o,$(SIM_SRCS))

.PHONY: all test clean
all: $(TESTS)

$(BUILD)/fw/%.o: $(FW)/%.c $(wildcard $(FW)/*.h $(FW)/nvme/*.h) Makefile
	@mkdir -p $(dir $@)
	$(CC) $(FW_CFLAGS) -c $< -o $@

$(BUILD)/sim/%.o: sim/%.c $(wildcard sim/*.h $(FW)/*.h $(FW)/nvme/*.h) Makefile
	@mkdir -p $(dir $@)
	$(CC) $(SIM_CFLAGS) -c $< -o $@

$(BUILD)/test_%: tests/test_%.c tests/check.h $(FW_OBJS) $(SIM_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(SIM_CFLAGS) $(LDFLAGS) $< $(FW_OBJS) $(SIM_OBJS) -o $@

test: $(TESTS)
	@set -e; for t in $(TESTS); do echo "== $$t"; ./$$t; done; echo "모든 시험 통과"

clean:
	rm -rf $(BUILD)

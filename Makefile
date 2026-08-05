CC ?= cc
BUILD_DIR ?= build
BUILD_PROFILE ?= portable-O3
OPT_FLAGS ?= -O3

CPPFLAGS := -Iinclude -Isrc -DBUILD_FLAGS=\"$(BUILD_PROFILE)\"
CFLAGS := -std=c11 $(OPT_FLAGS) -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
          -Wstrict-prototypes -MMD -MP $(ARCH_FLAGS)
LDLIBS := -lm -pthread

SOURCES := src/lbm.c src/kernel_aos.c src/kernel_soa_scalar.c \
           src/kernel_soa_auto.c src/kernel_neon.c src/parallel.c \
           src/benchmark_util.c
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
BENCH_OBJECT := $(BUILD_DIR)/benchmark_main.o
TEST_OBJECT := $(BUILD_DIR)/test_lbm.o

CC_VERSION := $(shell $(CC) --version 2>/dev/null | head -n 1)
ifneq ($(findstring clang,$(CC_VERSION)),)
SCALAR_ONLY_FLAGS := -fno-vectorize -fno-slp-vectorize
VECTOR_REPORT_FLAGS := -Rpass=loop-vectorize -Rpass-missed=loop-vectorize
else
SCALAR_ONLY_FLAGS := -fno-tree-vectorize -fno-tree-slp-vectorize
VECTOR_REPORT_FLAGS := -fopt-info-vec-optimized -fopt-info-vec-missed
endif

.PHONY: all test check clean rpi3 unoptimized vector-report help

all: $(BUILD_DIR)/lbm_bench $(BUILD_DIR)/test_lbm

$(BUILD_DIR)/lbm_bench: $(OBJECTS) $(BENCH_OBJECT)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/test_lbm: $(OBJECTS) $(TEST_OBJECT)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_lbm.o: tests/test_lbm.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_aos.o $(BUILD_DIR)/kernel_soa_scalar.o: CFLAGS += $(SCALAR_ONLY_FLAGS)

test check: $(BUILD_DIR)/test_lbm
	./$(BUILD_DIR)/test_lbm

rpi3:
	$(MAKE) clean
	$(MAKE) CC="$(CC)" ARCH_FLAGS="-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard" BUILD_PROFILE="rpi3-armv7-neon-O3" all

unoptimized:
	$(MAKE) clean
	$(MAKE) CC="$(CC)" OPT_FLAGS="-O0" BUILD_PROFILE="portable-O0-appendix" all

vector-report: CFLAGS += $(VECTOR_REPORT_FLAGS)
vector-report: clean all

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "make              Build portable benchmark and tests"
	@echo "make test         Run correctness tests"
	@echo "make rpi3         Build for 32-bit Raspberry Pi 3 with NEON"
	@echo "make unoptimized  Build an O0 compiler-sensitivity appendix"
	@echo "make vector-report  Rebuild with compiler vectorization diagnostics"

-include $(OBJECTS:.o=.d) $(BENCH_OBJECT:.o=.d) $(TEST_OBJECT:.o=.d)

SESI    := /home/dan/dev/priv/sesi/scripts/sesi
# Spec 008 T052 (contracts/build.md): ARCH defaults to the host, whose build is build/vo; another
# architecture is cross-compiled into build-<arch>/vo and runs under qemu (bin/vo).
HOST    := $(shell uname -m)
ARCH    ?= $(HOST)
ifeq ($(ARCH),riscv64)
ARCH_CFLAGS := -march=rv64imafd -mabi=lp64d -mcmodel=medany -mno-relax -msmall-data-limit=0
else ifeq ($(ARCH),x86_64)
ARCH_CFLAGS := -fcf-protection=none
else ifeq ($(ARCH),aarch64)
ARCH_CFLAGS := -mbranch-protection=none
else
$(error unsupported ARCH=$(ARCH): riscv64, x86_64 or aarch64)
endif
CROSS   := $(if $(filter $(ARCH),$(HOST)),,$(ARCH)-linux-gnu-)
SUFFIX  := $(if $(filter $(ARCH),$(HOST)),,-$(ARCH))
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
# Spec 008 (research R3): freestanding C, one rounding per float operation, no libc.
CFLAGS  := -std=gnu11 -O2 -g -ffreestanding -fno-builtin -nostdlib -fno-pie -fno-stack-protector \
	-fno-asynchronous-unwind-tables -fno-strict-aliasing -ffp-contract=off -fno-math-errno \
	-fno-trapping-math $(ARCH_CFLAGS)
CWARN   := -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wvla \
	-Wimplicit-fallthrough -Wundef -Wdouble-promotion -Wformat=2
LIBGCC   = $(shell $(CC) -print-libgcc-file-name)

BUILD ?= build$(SUFFIX)
# Maintainers' builds (spec 003): STRESS=1 collects constantly and poisons reclaimed memory;
# FAULT=<KIND> also drops one kind of root or object scan, to prove the stress suite notices.
ifdef FAULT
BUILD := build-fault-$(FAULT)$(SUFFIX)
CFLAGS += -DGC_FAULT_$(FAULT)=1 -DGC_STRESS_BUILD=1
else ifdef STRESS
BUILD := build-stress$(SUFFIX)
CFLAGS += -DGC_STRESS_BUILD=1
else ifdef INSTRUMENTED
# Spec 008 research R9 (T063): a hosted x86-64 build under ASan and UBSan; the C library is linked for
# the sanitizer runtimes only (src/platform/hosted.c), and the fixed regions move up (ADDR_BASE).
BUILD := build-instrumented
CFLAGS := $(filter-out -nostdlib,$(CFLAGS)) -DVO_HOSTED -DVO_ASAN -fsanitize=address,undefined \
	-fno-sanitize-recover=all -fno-omit-frame-pointer --param asan-globals=0
PLAT_FLAGS := -DVO_HOSTED
else ifdef UBTRAP
# Spec 008 research R9: the C objects trap on undefined behavior and out-of-bounds array access.
BUILD := build-ubtrap$(SUFFIX)
CFLAGS += -fsanitize=undefined,bounds -fsanitize-trap=all
endif
# Spec 008: the toolchain is C plus one platform file (research R5), the only assembly, linted by SESI.
C_SRCS   := $(wildcard src/*.c)
PLATFORM := src/platform/$(ARCH).S
HEADERS  := src/vo.h src/platform/$(ARCH).h Makefile
# `make ARCH=<arch> test` (and the other suites) run that build, through qemu (bin/vo).
ifneq ($(SUFFIX),)
export VO_BIN ?= $(abspath build$(SUFFIX)/vo)
endif
# platform.o first: its vo_data_begin starts .data (the globals the collector scans).
OBJS := $(BUILD)/platform.o $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRCS)) $(BUILD)/embedded.o \
	$(if $(INSTRUMENTED),$(BUILD)/hosted.o)

.PHONY: all check-tools lint lint-c test gc-check oracle baseline asm ubtrap instrumented diff-asm gate-step gate speed speed-check prop-eval refactor-eval stdlib-eval clean

all: $(BUILD)/vo

# FR-003: a static binary, no interpreter or shared libraries.
ifdef INSTRUMENTED
$(BUILD)/vo: $(OBJS)
	$(CC) -no-pie -fsanitize=address,undefined -o $@ $(OBJS)
$(BUILD)/hosted.o: src/platform/hosted.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(CWARN) -I src -c -o $@ $<
# The collector reads free slots by design; it keeps ASan's shadow up to date instead (gc.c, VO_ASAN).
$(BUILD)/gc.o: CFLAGS += -fno-sanitize=address
else
$(BUILD)/vo: $(OBJS)
	$(LD) -static --no-relax -e _start -o $@ $(OBJS) $(LIBGCC)
	@readelf -d $@ | grep -q "no dynamic section" || { echo "$@ has a dynamic section"; exit 1; }
endif

$(BUILD)/platform.o: $(PLATFORM) Makefile | $(BUILD)
	$(CC) $(ARCH_CFLAGS) $(PLAT_FLAGS) -g -c -o $@ $<

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(CWARN) -I src -c -o $@ $<
# memcpy and friends must not be turned back into calls to themselves.
$(BUILD)/vo_c.o: CFLAGS += -fno-tree-loop-distribute-patterns

# Spec 008 research R8: the reference, the catalog and the standard library, embedded as C arrays.
STD_FILES := std/json.vo std/time.vo std/text.vo std/random.vo std/http.vo
$(BUILD)/embedded.c: docs/reference.md docs/errors.md $(STD_FILES) tools/embed.py | $(BUILD)
	python3 tools/embed.py $@ docs/reference.md docs/errors.md $(STD_FILES)
$(BUILD)/embedded.o: $(BUILD)/embedded.c $(HEADERS)
	$(CC) $(CFLAGS) $(CWARN) -I src -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# Spec 008 (contracts/build.md): every tool the C port and its cross checks need, each named when missing.
TOOLS := riscv64-linux-gnu-as riscv64-linux-gnu-ld riscv64-linux-gnu-gcc aarch64-linux-gnu-gcc gcc \
	qemu-riscv64 qemu-aarch64 python3
check-tools:
	@missing=0; for t in $(TOOLS); do command -v $$t >/dev/null || { echo "missing: $$t"; missing=1; }; done; \
	  if [ $$missing = 1 ]; then echo "install: sudo apt install gcc binutils-riscv64-linux-gnu gcc-riscv64-linux-gnu gcc-aarch64-linux-gnu qemu-user python3"; exit 1; fi; \
	  echo "check-tools: all present"

# Spec 008 T065 (FR-005, FR-012, SC-008): the strict-warning C build for every architecture; SESI over
# the RISC-V platform file (the only pack; deliberate breaks marked `sesi: allow` on their prototype
# line, sesi-linter-issues.md #5, are listed as suppressed and do not count); no assembly outside
# src/platform/, and at most 300 lines per platform file.
lint:
	@fail=0; out=$$($(SESI) lint src/platform/riscv64.S . </dev/null 2>&1 | grep -v 'suppressed by sesi: allow'); \
	  if [ -n "$$out" ]; then echo "$$out"; fail=1; fi; \
	  for a in x86_64 aarch64 riscv64; do $(MAKE) -s --no-print-directory ARCH=$$a lint-c || fail=1; done; \
	  stray=$$(git ls-files -co --exclude-standard -- '*.s' '*.S' | grep -v '^src/platform/'); \
	  if [ -n "$$stray" ]; then echo "lint: assembly outside src/platform/: $$stray"; fail=1; fi; \
	  for f in src/platform/*.S; do n=$$(wc -l < $$f); \
	    if [ $$n -gt 300 ]; then echo "lint: $$f has $$n lines (at most 300)"; fail=1; fi; done; \
	  if [ $$fail = 0 ]; then echo "lint: clean (C for x86_64, aarch64 and riscv64; SESI; platform files)"; fi; \
	  exit $$fail
lint-c:
	@for f in $(C_SRCS); do $(CC) $(CFLAGS) $(CWARN) -I src -fsyntax-only $$f || exit 1; done

# The constitution's quality gate: every check that exists so far.
test: all
	tests/run.sh
	tests/run.sh --repeat 3 tests/cases/run tests/cases/effects tests/cases/func tests/cases/generic tests/cases/prop
	@for s in "fuzz.py --iterations 200 --seed 1" apply_fixes.py catalog.py ref_examples.py fmt_corpus.py \
	  migrate.py record_changes.py doc_conformance.py json_corpus.py packages_check.py time_check.py "scope_fuzz.py --iterations 300 --seed 1" gc_bounds.py gc/baseline.py gc/timeout_check.py \
	  steps.py "steps.py --repeat 3 tests/cases/trace tests/cases/replay" "fuzz_inputs.py --iterations 300 --seed 1"; do \
	  f=tests/$${s%% *}; if [ -f $$f ]; then echo "== $$s"; python3 tests/$$s || exit 1; \
	  else echo "== skip $$f (not created yet)"; fi; done
	@if [ -x build-oracle/vo ]; then echo "== diff_engines.py --count 1000 --seed 4"; \
	  python3 tests/diff_engines.py --count 1000 --seed 4 || exit 1; \
	  else echo "== skip diff_engines.py (no build-oracle/vo: run make oracle)"; fi

# The slow memory-safety suite (spec 003 US6): the stress build over the whole corpus, the benchmark
# reference solutions and the fuzzer, then the fault-injection matrix.
gc-check: all
	$(MAKE) STRESS=1
	tests/run.sh --stress
	python3 tests/gc/baseline.py --stress
	python3 tests/fuzz.py --stress --iterations 3000 --seed 9
	python3 tests/gc_faults.py
	python3 tests/diff_engines.py --stress --count 1000 --seed 2
	VO_BIN=$(abspath build-stress/vo) python3 tests/steps.py

# The comparison engine (spec 004): the tree-walking interpreter from the last commit before the
# bytecode engine, for speed figures and differential testing. Built once; `clean` keeps it.
ORACLE_REV := 1fff558

oracle: build-oracle/vo

build-oracle/vo:
	@[ -d build-oracle/src ] || git worktree add --detach build-oracle/src $(ORACLE_REV)
	$(MAKE) -C build-oracle/src
	mkdir -p build-oracle
	cp build-oracle/src/build/vo build-oracle/vo

# The previous spec's binary (spec 006 FR-016): programs must stay within 5% of it, checked with
# `python3 tests/speed.py --baseline build-base/vo`. Built once; `clean` keeps it.
BASE_REV := 1ff166a

baseline: build-base/vo

build-base/vo:
	@[ -d build-base/src ] || git worktree add --detach build-base/src $(BASE_REV)
	$(MAKE) -C build-base/src
	mkdir -p build-base
	cp build-base/src/build/vo build-base/vo

# The last assembly release (spec 008 research R10): the behavioral oracle of the C port and its speed
# baseline. Built once in a worktree; `clean` keeps it.
ASM_REV := bc610cb

asm: build-asm/vo

build-asm/vo:
	@[ -d build-asm/src ] || git worktree add --detach build-asm/src $(ASM_REV)
	$(MAKE) -C build-asm/src
	mkdir -p build-asm
	cp build-asm/src/build/vo build-asm/vo

# Spec 008 research R9: the suite under the build whose C objects trap on undefined behavior.
ubtrap:
	$(MAKE) UBTRAP=1
	VO_BIN=$(abspath build-ubtrap/vo) tests/run.sh

# Spec 008 research R9 (T063, SC-005): the suite under ASan and UBSan (x86-64 hosts).
instrumented:
	$(MAKE) INSTRUMENTED=1
	VO_BIN=$(abspath build-instrumented/vo) tests/run.sh

# Spec 008 research R12: generated programs against the last assembly release.
COUNT ?= 2000
diff-asm: all asm
	python3 tests/diff_asm.py --count $(COUNT) --seed 1

# Spec 008 research R10 (results.md "Deviations"): gate-step after every migration step; gate (adds the
# collector stress suite and the speed check) at every stage boundary and on every step from the
# runtime stage on. Since the VM is C (T047), the speed check is enforced: at most 10% slower than
# build-asm/vo, geometric mean (SC-004).
gate-step: all asm
	@echo "== gate: lint" && $(MAKE) -s lint </dev/null
	@echo "== gate: test" && $(MAKE) -s test
	@echo "== gate: ubtrap" && $(MAKE) -s ubtrap
	@echo "== gate: diff-asm" && python3 tests/diff_asm.py --count 2000 --mutants 2000 --seed 1
	@echo "== gate-step: passed"

# SC-004 compares like with like: the RISC-V build against build-asm/vo, both under qemu.
RV_BUILD := build$(if $(filter riscv64,$(HOST)),,-riscv64)
gate: gate-step
	@echo "== gate: gc-check" && $(MAKE) -s gc-check
	@echo "== gate: speed" && $(MAKE) -s ARCH=riscv64 && \
	  python3 tests/speed.py --baseline build-asm/vo --new $(RV_BUILD)/vo --max-geomean 1.10
	@echo "== gate: passed"

# Spec 004 speed targets (timing depends on machine load, so not part of `make test`).
speed: all oracle
	python3 tests/speed.py

# Spec 008 SC-003 (native hosts: build/vo natively at least 3 times faster than build-asm/vo under qemu,
# no program slower) and SC-004 (the RISC-V build at most 10% slower than build-asm/vo, both under qemu).
speed-check: oracle speed asm
	python3 tests/diff_engines.py --count 10000 --seed 1
	@if [ "$(HOST)" != riscv64 ]; then python3 tests/speed.py --native; fi
	$(MAKE) -s ARCH=riscv64
	python3 tests/speed.py --baseline build-asm/vo --new $(RV_BUILD)/vo --max-geomean 1.10

# Spec 007 US6 (SC-001): the task programs' saved solutions on their tests (written by an isolated
# agent from the statements and `vo doc` output: `python3 tests/stdlib_eval.py --solve`).
stdlib-eval: all
	python3 tests/stdlib_eval.py

# Spec 006 SC-001/SC-002: checked edits and queries on the refactor tasks' starters.
refactor-eval: all
	python3 tests/refactor_eval.py

# Spec 005 SC-001/SC-002: property tests against mutants of the hidden-property benchmark tasks.
prop-eval: all
	python3 tests/prop_eval.py

clean:
	rm -rf build build-stress build-ubtrap build-instrumented build-fault-* build-riscv64 build-*-riscv64 build-aarch64 build-*-aarch64

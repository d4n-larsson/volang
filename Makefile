AS      := riscv64-linux-gnu-as
LD      := riscv64-linux-gnu-ld
ASFLAGS := -march=rv64imafd -mabi=lp64d -g -I src
SESI    := /home/dan/dev/priv/sesi/scripts/sesi

BUILD ?= build
# Maintainers' builds (spec 003): STRESS=1 collects constantly and poisons reclaimed memory;
# FAULT=<KIND> also drops one kind of root or object scan, to prove the stress suite notices.
ifdef FAULT
BUILD := build-fault-$(FAULT)
ASFLAGS += --defsym GC_FAULT_$(FAULT)=1 --defsym GC_STRESS_BUILD=1
else ifdef STRESS
BUILD := build-stress
ASFLAGS += --defsym GC_STRESS_BUILD=1
endif
SRCS := $(wildcard src/*.s)
# The runtime links first, in a fixed order (spec 005): under qemu-user, where hot code falls within
# a 4 KiB page changes its speed by up to 20%, so growth in the parser, checker or compiler must not
# move the VM and the runtime it calls (see also the page offset at the top of src/vm.s).
RUNTIME := vm gc sys interp jmp num text rt_call rt_effects rt_func rt_io rt_list rt_map rt_render rt_str rt_test
RT_OBJS := $(patsubst %,$(BUILD)/%.o,$(RUNTIME))
OBJS := $(RT_OBJS) $(filter-out $(RT_OBJS),$(patsubst src/%.s,$(BUILD)/%.o,$(SRCS)))

.PHONY: all check-tools lint test gc-check oracle baseline-004 speed speed-check prop-eval clean

all: $(BUILD)/vo

$(BUILD)/vo: $(OBJS) Makefile
	$(LD) -static --no-relax -e _start -o $@ $(OBJS)

$(BUILD)/%.o: src/%.s src/defs.inc | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/data.o: docs/reference.md docs/errors.md

$(BUILD):
	mkdir -p $(BUILD)

check-tools:
	@command -v $(AS) >/dev/null && command -v qemu-riscv64 >/dev/null || \
	  { echo "missing toolchain: sudo apt install binutils-riscv64-linux-gnu qemu-user"; exit 1; }

# src/jmp.s (setjmp/longjmp) breaks the calling convention on purpose: see its header and
# sesi-linter-issues.md #5. Every other file must lint clean; deliberate breaks marked
# `sesi: allow` on their prototype line are listed as suppressed and do not count.
LINT_EXEMPT := src/jmp.s

lint:
	@fail=0; for f in $(filter-out $(LINT_EXEMPT),$(SRCS)); do out=$$($(SESI) lint $$f . </dev/null 2>&1 | grep -v 'suppressed by sesi: allow'); \
	  if [ -n "$$out" ]; then echo "$$out"; fail=1; fi; done; \
	  echo "lint: exempt (explained): $(LINT_EXEMPT)"; exit $$fail

# The constitution's quality gate: every check that exists so far.
test: all
	tests/run.sh
	tests/run.sh --repeat 3 tests/cases/run tests/cases/effects tests/cases/func tests/cases/generic tests/cases/prop
	@for s in "fuzz.py --iterations 200 --seed 1" apply_fixes.py catalog.py ref_examples.py fmt_corpus.py \
	  migrate.py record_changes.py "scope_fuzz.py --iterations 300 --seed 1" gc_bounds.py gc/baseline.py gc/timeout_check.py; do \
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

# The comparison engine (spec 004): the tree-walking interpreter from the last commit before the
# bytecode engine, for speed figures and differential testing. Built once; `clean` keeps it.
ORACLE_REV := 1fff558

oracle: build-oracle/vo

build-oracle/vo:
	@[ -d build-oracle/src ] || git worktree add --detach build-oracle/src $(ORACLE_REV)
	$(MAKE) -C build-oracle/src
	mkdir -p build-oracle
	cp build-oracle/src/build/vo build-oracle/vo

# The spec 004 binary (spec 005 FR-024): programs without contracts must stay within 5% of it,
# checked with `python3 tests/speed.py --baseline build-004/vo`. Built once; `clean` keeps it.
BASE004_REV := 71375e5

baseline-004: build-004/vo

build-004/vo:
	@[ -d build-004/src ] || git worktree add --detach build-004/src $(BASE004_REV)
	$(MAKE) -C build-004/src
	mkdir -p build-004
	cp build-004/src/build/vo build-004/vo

# Spec 004 speed targets (timing depends on machine load, so not part of `make test`).
speed: all oracle
	python3 tests/speed.py

speed-check: oracle speed
	python3 tests/diff_engines.py --count 10000 --seed 1

# Spec 005 SC-001/SC-002: property tests against mutants of the hidden-property benchmark tasks.
prop-eval: all
	python3 tests/prop_eval.py

clean:
	rm -rf build build-stress build-fault-*

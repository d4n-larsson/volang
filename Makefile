AS      := riscv64-linux-gnu-as
LD      := riscv64-linux-gnu-ld
ASFLAGS := -march=rv64imafd -mabi=lp64d -g -I src
SESI    := /home/dan/dev/priv/sesi/scripts/sesi

BUILD ?= build
SRCS := $(wildcard src/*.s)
OBJS := $(patsubst src/%.s,$(BUILD)/%.o,$(SRCS))

.PHONY: all check-tools lint test clean

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
# sesi-linter-issues.md #5. Every other file must lint clean.
LINT_EXEMPT := src/jmp.s

lint:
	@fail=0; for f in $(filter-out $(LINT_EXEMPT),$(SRCS)); do out=$$($(SESI) lint $$f . 2>&1); \
	  if [ -n "$$out" ]; then echo "$$out"; fail=1; fi; done; \
	  echo "lint: exempt (explained): $(LINT_EXEMPT)"; exit $$fail

# The constitution's quality gate: every check that exists so far.
test: all
	tests/run.sh
	tests/run.sh --repeat 3 tests/cases/run tests/cases/effects tests/cases/func tests/cases/generic
	@for s in "fuzz.py --iterations 200 --seed 1" apply_fixes.py catalog.py ref_examples.py fmt_corpus.py \
	  migrate.py record_changes.py "scope_fuzz.py --iterations 300 --seed 1"; do \
	  f=tests/$${s%% *}; if [ -f $$f ]; then echo "== $$s"; python3 tests/$$s || exit 1; \
	  else echo "== skip $$f (not created yet)"; fi; done

clean:
	rm -rf build

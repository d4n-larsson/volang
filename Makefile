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
ARCH_CFLAGS := -mbranch-protection=none -mno-outline-atomics # spec 055: atomics inline (no libgcc helpers needing libc)
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
# Spec 019 (constitution 1.4.0): Mbed TLS, vendored unmodified under third_party/, built freestanding
# with only the compiler's own headers and third_party/shim, with its upstream's warnings (not vo's
# strict set), configured by src/tls_config.h and src/tls_crypto_config.h. The C files are the ones
# listed in third_party/mbedtls/VENDOR.md.
MBED      := third_party/mbedtls
MBED_SRCS := $(addprefix $(MBED)/,$(shell sed -n 's/^- `\(.*\.c\)`$$/\1/p' $(MBED)/VENDOR.md))
MBED_DIRS := $(MBED)/include $(MBED)/tf-psa-crypto/include $(MBED)/tf-psa-crypto/drivers/builtin/include \
	$(MBED)/tf-psa-crypto/core $(MBED)/tf-psa-crypto/drivers/builtin/src $(MBED)/tf-psa-crypto/platform \
	$(MBED)/tf-psa-crypto/utilities $(MBED)/tf-psa-crypto/extras $(MBED)/tf-psa-crypto/dispatch $(MBED)/library
MBED_CONF := -DMBEDTLS_CONFIG_FILE='"tls_config.h"' -DTF_PSA_CRYPTO_CONFIG_FILE='"tls_crypto_config.h"'
MBED_FLAGS = -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I third_party/shim -I src \
	$(addprefix -I ,$(MBED_DIRS)) $(MBED_CONF) -U__ARM_NEON
# vo's own TLS code sees the library's headers as system headers (no strict warnings from them).
TLS_FLAGS = -isystem third_party/shim $(addprefix -isystem ,$(MBED_DIRS)) $(MBED_CONF)
MBED_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(MBED_SRCS))
# Spec 020 (constitution 1.4.1): CORE-MATH's correctly rounded functions, vendored under third_party/
# with one recorded patch, built like Mbed TLS: freestanding, the compiler's own headers and
# third_party/shim, upstream warnings, -ffp-contract=off (CFLAGS). Each file finds its own dint.h.
CM       := third_party/core-math
CM_SRCS  := $(addprefix $(CM)/,$(shell sed -n 's/^- `\(.*\.c\)`$$/\1/p' $(CM)/VENDOR.md))
CM_FLAGS  = -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I third_party/shim
CM_OBJS  := $(patsubst %.c,$(BUILD)/%.o,$(CM_SRCS))
# Spec 026 (constitution 1.5.0): SQLite, vendored unmodified (public domain), built like Mbed TLS:
# freestanding, the compiler's own headers and third_party/shim, upstream warnings, -ffp-contract=off
# (CFLAGS). Its OS layer is vo's (src/sqlite_vfs.c); the options are research R2 of specs/026-sqlite.
SQL       := third_party/sqlite
SQL_SRCS  := $(addprefix $(SQL)/,$(shell sed -n 's/^- `\(.*\.c\)` sha256 .*$$/\1/p' $(SQL)/VENDOR.md))
SQLITE_OPTS := -DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_WAL \
	-DSQLITE_TEMP_STORE=3 -DSQLITE_ENABLE_MEMSYS5 -DSQLITE_ZERO_MALLOC -DSQLITE_DEFAULT_MEMSTATUS=0 \
	-DSQLITE_OMIT_DEPRECATED -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_UNTESTABLE -DSQLITE_DQS=0 \
	-DSQLITE_OMIT_LOCALTIME -DSQLITE_ENABLE_MATH_FUNCTIONS -DSQLITE_DEFAULT_FOREIGN_KEYS=1
SQL_FLAGS  = -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I third_party/shim $(SQLITE_OPTS)
SQL_OBJS  := $(patsubst %.c,$(BUILD)/%.o,$(SQL_SRCS))
# platform.o first: its vo_data_begin starts .data (the globals the collector scans).
OBJS := $(BUILD)/platform.o $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRCS)) $(BUILD)/embedded.o \
	$(if $(INSTRUMENTED),$(BUILD)/hosted.o) $(MBED_OBJS) $(CM_OBJS) $(SQL_OBJS)

PHONY_TLS := tls-selftest tls-deps
.PHONY: $(PHONY_TLS) quick grammars cm-noasm math-selftest math-vectors site site-serve site-check all check-tools lint lint-c test gc-check oracle baseline asm ubtrap instrumented diff-asm gate-step gate speed speed-check prop-eval refactor-eval stdlib-eval security-check clean

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
# Spec 019: vo's TLS layer includes the library's headers; the vendored files keep their own warnings.
$(BUILD)/tls.o $(BUILD)/tls_libc.o: CFLAGS += $(TLS_FLAGS)
# Spec 026: vo's SQLite code sees sqlite3.h as a system header (no strict warnings from it).
$(BUILD)/sqlite_vfs.o $(BUILD)/sqlite_rt.o $(BUILD)/check_sqlite.o: CFLAGS += -isystem $(SQL) $(SQLITE_OPTS)
$(BUILD)/tls.o $(BUILD)/tls_libc.o: src/tls_config.h src/tls_crypto_config.h
$(BUILD)/third_party/%.o: third_party/%.c src/tls_config.h src/tls_crypto_config.h Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall $(MBED_FLAGS) -c -o $@ $<
$(BUILD)/third_party/sqlite/%.o: third_party/sqlite/%.c third_party/sqlite/sqlite3.h Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -Wno-unused-variable $(SQL_FLAGS) -c -o $@ $<
$(BUILD)/third_party/core-math/%.o: third_party/core-math/%.c third_party/shim/fenv.h Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall $(CM_FLAGS) -I $(dir $<) -c -o $@ $<
# Constitution 1.4.1: no assembly in vendored code. CORE-MATH's inline asm sits behind compiler and
# processor tests (spec 020 research R4); this fails if any of it is compiled for this platform. Only
# the preprocessed lines that come from the vendored files are read (compiler headers have asm).
cm-noasm:
	@for f in $(CM_SRCS); do $(CC) $(CFLAGS) $(CM_FLAGS) -I $$(dirname $$f) -E $$f | \
	  awk -v f=$$f -f tools/cm_noasm.awk || exit 1; done
# memcpy and friends must not be turned back into calls to themselves.
$(BUILD)/vo_c.o: CFLAGS += -fno-tree-loop-distribute-patterns

# Spec 008 research R8: the reference, the catalog and the standard library, embedded as C arrays.
STD_FILES := std/json.vo std/time.vo std/text.vo std/random.vo std/http.vo std/collections.vo std/fs.vo std/process.vo std/math.vo std/bytes.vo std/regex.vo std/sqlite.vo std/web.vo std/option.vo std/crypto.vo std/tcp.vo std/bits.vo std/decimal.vo std/security.vo std/lists.vo std/workers.vo std/log.vo std/metrics.vo
TZ_BLOB := third_party/tzdata/zones.bin # spec 048: made by tools/vendor_tzdata.sh
$(BUILD)/embedded.c: docs/reference.md docs/errors.md $(STD_FILES) $(TZ_BLOB) tools/embed.py | $(BUILD)
	python3 tools/embed.py $@ docs/reference.md docs/errors.md $(STD_FILES) $(TZ_BLOB)
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
# src/platform/, and at most 300 lines per platform file. SESI is not in this repository: where it
# is not installed (CI), `make lint SESI_SKIP=1` runs everything else and says so.
lint:
	@fail=0; if [ -n "$(SESI_SKIP)" ]; then echo "lint: SESI skipped (SESI_SKIP is set)"; else \
	  out=$$($(SESI) lint src/platform/riscv64.S . </dev/null 2>&1 | grep -v 'suppressed by sesi: allow'); \
	  if [ -n "$$out" ]; then echo "$$out"; fail=1; fi; fi; \
	  for a in x86_64 aarch64 riscv64; do $(MAKE) -s --no-print-directory ARCH=$$a lint-c || fail=1; done; \
	  stray=$$(git ls-files -co --exclude-standard -- '*.s' '*.S' | grep -v '^src/platform/'); \
	  if [ -n "$$stray" ]; then echo "lint: assembly outside src/platform/: $$stray"; fail=1; fi; \
	  for f in src/platform/*.S; do n=$$(wc -l < $$f); \
	    if [ $$n -gt 300 ]; then echo "lint: $$f has $$n lines (at most 300)"; fail=1; fi; done; \
	  tools/vendor_core_math.sh --verify >/dev/null || fail=1; \
	  tools/vendor_sqlite.sh --verify >/dev/null || fail=1; \
	  if [ $$fail = 0 ]; then echo "lint: clean (C for x86_64, aarch64 and riscv64; SESI; platform files; vendored CORE-MATH and SQLite)"; fi; \
	  exit $$fail
# Spec 019 (FR-012, FR-013, research R11): the vendored cryptography's self-tests and known-answer
# vectors, built from the same sources and configuration as vo for each platform (a static test
# program; its C library only prints) and run natively and under qemu; the three outputs must say
# "all passed" and be identical.
SELFTEST_FLAGS = -std=gnu11 -O2 -static -w -ffp-contract=off -fno-strict-aliasing -DVO_TLS_SELF_TEST \
	-I src $(addprefix -I ,$(MBED_DIRS)) $(MBED_CONF) -U__ARM_NEON
# Spec 019: the second TLS implementation of the interoperability tests (test-only), pinned by hash.
tls-deps:
	python3 -m pip install --quiet --no-deps --require-hashes --target build/py -r tests/tls/requirements.txt
tls-selftest:
	@mkdir -p build-selftest; for a in x86_64 aarch64 riscv64; do \
	  c=$$([ $$a = $(HOST) ] && echo gcc || echo $$a-linux-gnu-gcc); \
	  $$c $(SELFTEST_FLAGS) -o build-selftest/selftest-$$a tests/tls/selftest.c $(MBED_SRCS) || exit 1; \
	  r=$$([ $$a = $(HOST) ] && echo "" || echo qemu-$$a); \
	  $$r build-selftest/selftest-$$a > build-selftest/out-$$a || { cat build-selftest/out-$$a; exit 1; }; done; \
	  cmp -s build-selftest/out-x86_64 build-selftest/out-aarch64 && cmp -s build-selftest/out-x86_64 build-selftest/out-riscv64 \
	  || { echo "tls-selftest: outputs differ between platforms"; exit 1; }; \
	  echo "tls-selftest: $$(grep -c ' ok$$' build-selftest/out-x86_64) checks passed, identical on x86_64, aarch64 and riscv64"
lint-c:
	@for f in $(C_SRCS); do $(CC) $(CFLAGS) $(CWARN) $(TLS_FLAGS) -isystem $(SQL) $(SQLITE_OPTS) -I src -fsyntax-only $$f || exit 1; done
# Spec 020 (research R3): src/math_libm.c's software fma and rounding functions against the x86-64
# instructions, bit for bit (a hosted program; it skips on a CPU without FMA). x86-64 hosts only.
math-selftest:
	@mkdir -p build-selftest
	@if [ $(HOST) = x86_64 ]; then \
	  gcc -std=gnu11 -O2 -mfma -msse4.1 -ffp-contract=off -fno-builtin-floor -fno-builtin-ceil -fno-builtin-round \
	    -fno-builtin-roundeven -fno-builtin-fmax -fno-builtin-fmin -Wall -Wextra -Werror \
	    -o build-selftest/math-selftest tests/math/fma_selftest.c src/math_libm.c && build-selftest/math-selftest; \
	  else echo "math-selftest: skipped (not an x86-64 host)"; fi
# Spec 020 (research R7): regenerates tests/math/vectors.txt.gz (dev only; mpmath pinned by hash).
math-vectors:
	python3 -m pip install --quiet --no-deps --require-hashes --target build/py -r tests/math/requirements.txt
	python3 tools/math_vectors_gen.py

# Between edits: only the tests of the areas a change touches (tools/quick.sh has the table), chosen
# from the files changed since the last commit, or named: `make quick AREA=math,docs`. A shared file
# (vo.h, the VM, the runtime core, this Makefile) selects everything. `make test` still completes a change.
quick: all
	@tools/quick.sh $(AREA)

# Spec 037: after a change to editors/tree-sitter-vo/grammar.js: the generated parser (ABI 14, which
# current Neovim, Helix and Zed load) and the hash tests/grammars.py compares. Needs `npm ci` in editors/.
grammars:
	cd editors/tree-sitter-vo && ../node_modules/.bin/tree-sitter generate --abi 14
	python3 tests/grammars.py --hash

# The constitution's quality gate: every check that exists so far.
test: all
	tests/run.sh
	$(MAKE) -s --no-print-directory tls-selftest
	$(MAKE) -s --no-print-directory math-selftest
	@for a in x86_64 aarch64 riscv64; do $(MAKE) -s --no-print-directory ARCH=$$a cm-noasm || exit 1; done
	@! grep -n 'MBEDTLS_SSL_VERIFY_NONE\|MBEDTLS_SSL_VERIFY_OPTIONAL\|\*flags *=' src/tls.c || \
	  { echo "src/tls.c must never weaken certificate verification (spec 019 FR-006)"; exit 1; }
	tests/run.sh --repeat 3 tests/cases/run tests/cases/effects tests/cases/func tests/cases/generic tests/cases/prop
	python3 tests/run_scripts.py "fuzz.py --iterations 200 --seed 1" "fuzz.py --manifests 200 --seed 1" apply_fixes.py catalog.py ref_examples.py site_examples.py fmt_corpus.py \
	  migrate.py migrate_056.py std_names.py libchange_rows.py message_style.py record_changes.py doc_conformance.py json_corpus.py packages_check.py time_check.py "scope_fuzz.py --iterations 300 --seed 1" "scope_calls.py --iterations 300 --seed 1" gc_bounds.py gc/baseline.py gc/timeout_check.py \
	  steps.py json_stream.py https_interop.py "steps.py --repeat 3 tests/cases/trace tests/cases/replay" "fuzz_inputs.py --iterations 300 --seed 1" \
	  math_vectors.py "bytes_fuzz.py --count 2000 --runs 200" "format_diff.py --quick" "regex_diff.py --quick" \
	  "regex_fuzz.py --count 20000" "sqlite_scope.py --quick" "sqlite_fuzz.py --count 1000" sqlite_corpus.py structure_corpus.py "serve_workers.py --quick" "web_fuzz.py --quick" effect_sets_records.py \
	  "const_equiv.py --count 300" const_records.py build_examples.py crypto_vectors.py mcp.py grammars.py lsp.py tcp.py stream.py package_fetch.py package_git.py package_graphs.py strings.py bits.py decimals.py web_security.py iteration.py workers.py zones.py "scope_paths.py --count 300 --seed 1" postgres.py "recorded_tests.py --quick" "operations.py --quick"
	@echo "== examples"; for m in examples/*/main.vo examples/*/*/main.vo; do d=$$(dirname $$m); \
	  (cd $$d && $(CURDIR)/bin/vo check main.vo >/dev/null && \
	  if [ -d recordings/tests ]; then $(CURDIR)/bin/vo test --replay recordings/tests main.vo >/dev/null; else $(CURDIR)/bin/vo test main.vo >/dev/null; fi && for r in recordings/*.jsonl; do [ -f "$$r" ] || continue; \
	  $(CURDIR)/bin/vo replay "$$r" >/dev/null; [ $$? -lt 64 ] || exit 1; done) || { echo "example $$d fails"; exit 1; }; done
	@if [ -x build-oracle/vo ]; then echo "== diff_engines.py --count 1000 --seed 4"; \
	  python3 tests/diff_engines.py --count 1000 --seed 4 || exit 1; \
	  else echo "== skip diff_engines.py (no build-oracle/vo: run make oracle)"; fi

# The slow memory-safety suite (spec 003 US6): the stress build over the whole corpus, the benchmark
# reference solutions and the fuzzer, then the fault-injection matrix.
gc-check: all oracle
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
# Since spec 009 the language changes again, so mutated sources (`diff_asm.py --mutants`, the spec 008
# porting check of the front end) disagree with build-asm/vo by design; generated programs, which use none
# of the new features, are still compared. Malformed input stays covered by fuzz.py in `make test`.
gate-step: all asm
	@echo "== gate: lint" && $(MAKE) -s lint </dev/null
	@echo "== gate: test" && $(MAKE) -s test
	@echo "== gate: ubtrap" && $(MAKE) -s ubtrap
	@echo "== gate: diff-asm" && python3 tests/diff_asm.py --count 2000 --seed 1
	@echo "== gate: tasks determinism" && python3 tests/tasks_determinism.py --runs 20
	@echo "== gate: files and processes determinism" && tests/run.sh --repeat 20 tests/cases/fs tests/cases/process && \
	  python3 tests/steps.py --repeat 20 tests/cases/fs_steps tests/cases/process_steps
	@echo "== gate: agent testing determinism" && tests/run.sh --repeat 20 tests/cases/test tests/cases/coverage tests/cases/mutate && \
	  python3 tests/steps.py --repeat 20 tests/cases/test_steps
	@echo "== gate: agent testing scale" && python3 tests/test_scale.py && python3 tests/mutate_eval.py
	@echo "== gate: https determinism" && tests/run.sh --repeat 20 tests/cases/https && \
	  python3 tests/steps.py --repeat 20 tests/cases/https_steps
	@echo "== gate: https interop, fuzz, speed" && $(MAKE) -s --no-print-directory tls-deps && \
	  python3 tests/https_interop.py --require-tlslite && python3 tests/tls_fuzz.py && python3 tests/https_scale.py
	@echo "== gate: math determinism and speed" && tests/run.sh --repeat 20 tests/cases/math && \
	  python3 tests/math_speed.py
	@echo "== gate: bytes determinism, fuzz and scale" && tests/run.sh --repeat 20 tests/cases/bytes && \
	  python3 tests/steps.py --repeat 20 tests/cases/bytes_steps && python3 tests/bytes_fuzz.py && \
	  python3 tests/bytes_scale.py
	@echo "== gate: sleep determinism and timing" && tests/run.sh --repeat 20 tests/cases/time && \
	  python3 tests/steps.py --repeat 20 tests/cases/sleep_steps && python3 tests/sleep_check.py
	@echo "== gate: hover documentation speed" && python3 tests/hover_speed.py
	@echo "== gate: format speed" && python3 tests/format_speed.py
	@echo "== gate: regex speed" && python3 tests/regex_speed.py
	@echo "== gate: sqlite speed" && python3 tests/sqlite_speed.py
	@echo "== gate: structure rules speed" && python3 tests/structure_speed.py
	@echo "== gate-step: passed"

# SC-004 compares like with like: the RISC-V build against build-asm/vo, both under qemu.
RV_BUILD := build$(if $(filter riscv64,$(HOST)),,-riscv64)
gate: gate-step
	@echo "== gate: gc-check" && $(MAKE) -s gc-check
	@echo "== gate: serve_workers_speed" && python3 tests/serve_workers_speed.py
	@echo "== gate: web_speed" && python3 tests/web_speed.py
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

# Spec 015: the documentation site (build/site), built with pinned MkDocs in build/site-venv (created once:
# needs the network the first time). site-check: links, agent files, search lookups, contrast, install steps.
SITE_VENV := build/site-venv
export NO_MKDOCS_2_WARNING := 1
export DISABLE_MKDOCS_2_WARNING := true
$(SITE_VENV)/.ok: site/requirements.txt site/pygments_vo/pyproject.toml site/pygments_vo/pygments_vo.py
	python3 -m venv $(SITE_VENV)
	$(SITE_VENV)/bin/pip install -q -r site/requirements.txt
	$(SITE_VENV)/bin/pip install -q --force-reinstall --no-deps ./site/pygments_vo
	touch $@

site: all $(SITE_VENV)/.ok
	$(SITE_VENV)/bin/python tools/site.py
	$(SITE_VENV)/bin/mkdocs build -q -f build/mkdocs.gen.yml
	$(SITE_VENV)/bin/python tools/site.py --agents

site-serve: site
	$(SITE_VENV)/bin/mkdocs serve -f build/mkdocs.gen.yml

site-check: site
	$(SITE_VENV)/bin/python tests/site_lexer.py
	python3 tests/site_agents_files.py
	python3 tests/site_search.py
	python3 tests/site_contrast.py
	python3 tests/site_examples.py --start

# Spec 013: the security review's reproductions that need timing, helper processes or long paths
# (tests/security/); every finding's other cases run in `make test` (tests/cases/security/).
security-check: all
	@for t in tests/security/*.py; do python3 $$t $(abspath build/vo) || exit 1; done

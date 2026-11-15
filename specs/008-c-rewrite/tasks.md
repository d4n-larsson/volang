---

description: "Task list for vo-lang spec 008: the toolchain in C (native builds)"
---

# Tasks: The Toolchain in C (Native Builds)

**Input**: Design documents from `specs/008-c-rewrite/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R14), data-model.md, contracts/build.md, contracts/platform-layer.md, contracts/port-protocol.md, quickstart.md; constitution 1.3.0.

**Tests**: The behavior is already specified by the existing suite (Principle IV), and this feature adds no behavior. So there are no new golden cases. The new test tools are the gate and the differential against the assembly release (`tests/diff_asm.py`), the cross-platform replay check and the instrumented builds. Every migration step must pass the unchanged suite.

**Organization**: one phase per user story. The order is by dependency, not priority:
- **US3** (the stepwise port, P2) comes first, because every other story needs the C toolchain it produces.
- **US1** (native x86-64, P1) needs US3.
- **US2** (identical on every platform, P1) needs US1's native build plus the other platform layers.
- **US4** (speed, P2) and **US5** (instrumented build, P3) need US1.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependencies on incomplete tasks).
- **[Story]**: US1–US5 from spec.md.

## Conventions

**Every port task follows [contracts/port-protocol.md](contracts/port-protocol.md):**
1. Audit the assembly (R13 hardware behavior, R7 recursion limits).
2. Write `src/<name>.c` with the same global symbols and the prototypes from the assembly's prototype comments. Use the `src/vo.h` structs, and register heap references held across an allocation with `GC_ROOT`/`GC_UNROOT` (R4).
3. Delete `src/<name>.s`, or the ported functions of a large file.
4. Run `make gate`.
5. Add one line to the migration log in `results.md`: file, assembly lines removed, C lines added, gate result, speed ratio, audit notes.

**Rules for every port task:**
- A port task changes no golden file, reference, catalog or contract (FR-017). A changed expected output is a bug, except a memory figure under R4, which is recorded.
- Large files may be ported in parts (functions grouped by caller). Each part is gated and logged on its own; the task is done when the `.s` file is gone.
- C style:
  - `-std=gnu11` and the R3 flags, with zero warnings.
  - No libc; `long`/`unsigned long` for 64-bit words, `unsigned __int128` where assembly used `mulhu`.
  - Comments keep the assembly's intent and `ponytail:` markers.
- **Carried over:** commit only when the user asks; append SESI false positives and false negatives to `sesi-linter-issues.md`; run `make lint` with `</dev/null`.

---

## Phase 1: Setup

- [X] T001 Set `ASM_REV` in `Makefile` to the commit where spec 007 was merged into `main` (the last assembly release), and run `make gc-check` on it once (skipped at merge time, spec 007 results.md "Deferred"). The branch `008-c-rewrite` and `.specify/feature.json` already exist.
- [X] T002 Install and check the build tools: `gcc-riscv64-linux-gnu`, `gcc-aarch64-linux-gnu`, `binutils-aarch64-linux-gnu`, `qemu-user` (contracts/build.md). Extend `make check-tools` in `Makefile` so it names each missing tool.
- [X] T003 `make asm` in `Makefile`: builds `build-asm/vo` from `ASM_REV` (the same mechanism as `make baseline`, with a worktree under `build/`). Record its speed in `specs/008-c-rewrite/results.md`: the `tests/speed.py` times under qemu on this machine.
- [X] T004 [P] Create `specs/008-c-rewrite/results.md` with these sections:
  - the SC-001–SC-009 table (all "open");
  - "Migration log" (the table from data-model.md);
  - "Audit findings (R13)";
  - "Memory figures (R4)";
  - "Recursion limits (R7)";
  - "Deviations".

---

## Phase 2: Foundational (Blocking Prerequisites)

**⚠️ CRITICAL:** no port step can start until the mixed build links and the gate runs.

- [X] T005 [P] `tools/gen_defs.py`: reads `src/defs.inc` (`.equ NAME, VALUE` lines, decimal and hex, comments) and writes `build/defs.h` with `#define NAME VALUE`, values in parentheses, `UL` suffix for hex above 2^31. The `Makefile` regenerates it when `defs.inc` changes (R14).
- [X] T006 [P] `tools/gen_protos.py`: collects the C prototype comments (`# <type> name(<args>)` followed by `.globl name`) from every remaining `src/*.s`, and writes `build/protos.h` with `extern` declarations. Comments of the form `sesi: allow ...` are stripped. Functions whose prototype cannot be expressed (handlers, `_start`) are skipped with a comment. Regenerated on every build (R14).
- [X] T007 `src/vo.h`, the shared C declarations:
  - `#include "defs.h"` and `"protos.h"`;
  - `typedef`s for the 64-bit word;
  - structs for tokens, AST nodes, types, effect sets, FX records, file records, vectors, strings, string builders, heap object headers, lists, maps, compiled functions, instructions, frames and recording events;
  - every field checked with `_Static_assert(offsetof(...) == <defs.inc name>, ...)` (data-model.md "Shared layouts");
  - `GC_ROOT(&p)` / `GC_UNROOT(n)` over `gc_roots`/`gc_roots_n` (data-model.md "Root stack": 1 Mi entries reserved at `ADDR_ROOTS`; reaching the reservation is T0003);
  - `NORETURN` and `RETURNS_TWICE` attribute macros.
- [X] T008 Root-stack scanning in `src/gc.s` (research R4, data-model.md "Root stack"):
  - `ADDR_ROOTS` = `0x22_8000_0000` in `src/defs.inc` (between the mark stack's 64 MiB and the value stack), 1 Mi entries (8 MiB) mapped at startup like the value stack, with `gc_roots_n` in `.bss`.
  - `gc_collect` marks from every `*gc_roots[i]` in addition to its current roots.
  - Under `GC_STRESS_BUILD`:
    - a non-zero `gc_roots_n` when `vm_run` returns to `run_main` or the test runner is T0003;
    - the high-water mark is printed to stderr at exit when `VO_ROOTS_HWM=1` is set, and `make gc-check` fails if any case pushes it above 131,072 entries (1/8 of the reservation).
  
  This is an assembly change: SESI lint, then `make gc-check`.
- [X] T009 The mixed build in `Makefile`:
  - C rules for `src/*.c` with `riscv64-linux-gnu-gcc` and the R3 flags (RV64: `-march=rv64imafd -mabi=lp64d -mcmodel=medany -mno-relax -msmall-data-limit=0`);
  - the strict warning set with `-Werror`;
  - `-I build -I src`;
  - linked by `riscv64-linux-gnu-ld` with the assembly objects as today, keeping the `RUNTIME` first-link order for the runtime files that are still assembly;
  - `SRCS` covers `.s` and `.c`, and a file present in both forms is a build error.
  
  Add a pilot `src/vo_c.c` holding `memcpy`, `memmove`, `memset`, `memcmp` (R3), and check that `make test` still passes.
- [X] T010 The per-edit check for C (FR-012, constitution 1.3.0): `tools/cc_check.sh`, a PostToolUse hook entry in `.claude/settings.json` next to the SESI hook (matcher `Write|Edit`).
  - When the edited file is `src/*.c` or `src/vo.h`, it compiles that file (or every C file, for the header) with the RV64 flags and the strict warning set to `-fsyntax-only`.
  - It reports each warning or error with file and line, and exits 2 (a blocking hook error) when there is one, as the SESI hook does.
  - Other files pass through untouched.
  - The script must not depend on `build/` existing: it regenerates `build/defs.h` and `build/protos.h` first if they are missing.
- [X] T011 `make ubtrap` in `Makefile`: `build-ubtrap/vo`, the same build with `-fsanitize=undefined,bounds -fsanitize-trap=all` on the C objects (R9). Together with `make gc-check`, this is the instrumented build that constitution 1.3.0 requires while assembly remains. The suite runs under it via `VO_BIN`. A trap is an illegal-instruction exit, which the gate reports as a failure with the case name.
- [X] T012 [P] `tests/diff_asm.py` (R12):
  - generates programs with `tests/gen_programs.py`;
  - runs each on `build-asm/vo` (qemu) and on `--new` (default `build/vo`), and compares stdout, stderr and exit status;
  - takes `--count N --seed S [--platform riscv64|aarch64|x86_64]`;
  - writes disagreements to `build/diff-failures/<seed>/`;
  - exits 1 on any disagreement;
  - `make diff-asm COUNT=N` in `Makefile`.
- [X] T013 `make gate` in `Makefile` (R10). It runs, in order, stopping at the first failure:
  1. `make lint`
  2. `make test`
  3. `make gc-check`
  4. `make ubtrap` with `tests/run.sh` under it
  5. `make diff-asm COUNT=2000`
  6. `python3 tests/gc/baseline.py` (703 bench2 runs)
  7. `python3 tests/speed.py --baseline build-asm/vo` (reported, not enforced, until T047)
  
  It prints one summary line per stage. Run it on the pilot build: everything passes.

**Checkpoint:** the mixed toolchain builds, C and assembly call each other, and the gate passes with no assembly ported yet.

---

## Phase 3: User Story 3 - The migration never breaks the tree (Priority: P2, done first)

**Goal:** every assembly file except the platform layer is replaced by C, one gated step at a time (FR-018, R1, R2).

**Independent test:** each migration-log line in `results.md` names a gate that passed. Re-running `make gate` on any recorded step's commit passes.

### Stage 1: leaf code

- [X] T014 [P] [US3] Port `src/ast.s` (vectors and node construction) to `src/ast.c`. This is the first real step: it settles the conventions of `vo.h` on the simplest file.
- [X] T015 [P] [US3] Port `src/text.s` to `src/text.c`: memory and string helpers, the byte builder, UTF-8, JSON escaping and message templates. `mem_copy` and `mem_eq` stay as named symbols, implemented with the T009 functions.
- [X] T016 [US3] Port `src/num.s` to `src/num.c`. Audit (R13): integer division and remainder by zero and of the minimum integer by -1; float-to-integer conversion; the explicit rounding mode at `num.s:380` (`fcvt.l.d … rne`, round half to even), written without libm and without SSE4.1, for example with the 2^52 add-and-subtract on values below 2^52; `text_to_float`'s 15-digit path and `scale10`, which must give the same bits; `float_to_text`'s formatting of `-0.0`, `inf` and NaN, ignoring the NaN sign and payload.
- [X] T017 [P] [US3] Port `src/sha256.s` to `src/sha256.c` (32-bit rotations on `uint32_t`; the vectors from spec 007 T020 still pass through `tests/cases/packages`).
- [X] T018 [US3] Port `src/stdprim.s` to `src/stdprim.c`. The big-integer routines use `unsigned __int128` for `mulhu`. Check `exact_to_double`'s and `exact_digits`' ties-to-even rounding against `tests/json_corpus.py`. `std_only_ok` and `std_float_text` keep their prototypes.
- [X] T019 [US3] `tools/embed.py` (R8): writes `build/embedded.c` from `docs/reference.md`, `docs/errors.md` and `std/*.vo`, with the same symbol names, NUL terminators and `std_files` table (`{path, text, len}`, zero-terminated) as `src/data.s`. Delete `src/data.s`, and make `build/embedded.o` depend on the embedded files.

### Stage 2: front end

- [X] T020 [US3] Port `src/lex.s` to `src/lex.c` (16-byte tokens; byte classification as in the assembly; invalid UTF-8 E0104).
- [X] T021 [US3] Port `src/parse.s` to `src/parse.c`, in parts. Keep the explicit 5,000-level limit (E0115). The native-stack guard stays as a last resort, at a threshold the counter always reaches first (R7). Record in `results.md` that `tests/cases/check/nesting.vo` reports at the same position.
- [X] T022 [US3] Port `src/diag.s` to `src/diag.c`: the file table, diagnostics, sorting (the same stable order), rendering, summaries and `toolchain_error`.
- [X] T023 [US3] Port `src/fmt.s` to `src/fmt.c`. If its recursion follows nesting, it needs the same limit as the parser (R7).

### Stage 3: checker

- [X] T024 [P] [US3] Port `src/types.s` to `src/types.c` (interning and memo tables in the same order).
- [X] T025 [P] [US3] Port `src/effects.s` to `src/effects.c` (effect sets with scopes; `es_render` byte-identical).
- [X] T026 [US3] Port `src/check.s` to `src/check.c`, in parts.
- [X] T027 [US3] Port `src/check_expr.s` to `src/check_expr.c`, in parts. The builtin tables (`builtin_names`, `builtin_sigs`, `B_COUNT`) stay data in the same order.
- [X] T028 [P] [US3] Port `src/check_func.s` to `src/check_func.c`.
- [X] T029 [P] [US3] Port `src/check_generic.s` to `src/check_generic.c`.
- [X] T030 [US3] Port `src/check_effects.s` to `src/check_effects.c` (`builtin_effects` stays a 64-byte table).
- [X] T031 [US3] Port `src/loader.s` to `src/loader.c`: imports, `std_lookup`, `std_runtime_grants`, cycle detection and `str_join2`.
- [X] T032 [P] [US3] Port `src/packages.s` to `src/packages.c` (manifest parsing, `pkg_hash` over `walk_dir`/`sort_paths`, `vo package`).
- [X] T033 [US3] Port `src/effects_record.s` to `src/effects_record.c` (`find_up`, `find_up_dir`, `rec_relpath` with `@NAME/` paths).

**Checkpoint:** run `make diff-asm COUNT=10000`. Expected: 0 disagreements, recorded in `results.md`.

### Stage 4: commands

- [X] T034 [P] [US3] Port `src/query.s` to `src/query.c`, in parts.
- [X] T035 [P] [US3] Port `src/edit.s` to `src/edit.c`, in parts.
- [X] T036 [P] [US3] Port `src/doc.s` to `src/doc.c`.
- [X] T037 [US3] Port `src/replay.s` to `src/replay.c`. Recording writes stay allocation-free (spec 006). Event kinds and fields are unchanged, so recordings are interchangeable.
- [X] T038 [US3] Port `src/testrun.s` to `src/testrun.c`. `jmp_save` is declared `RETURNS_TWICE`, and every local used after it returns twice is `volatile`.
- [X] T039 [US3] Port `src/main.s` to `src/main.c`: options, path expansion, `walk_dir`, `sort_paths`, `write_file_safely` and dispatch.

### Stage 5: compiler

- [X] T040 [US3] Port `src/compile.s` to `src/compile.c`, in parts. Instruction words still hold handler addresses read from `vm_ops` (the assembly VM's table), and the code arena stays at `ADDR_CODE`.

### Stage 6: runtime

- [X] T041 [P] [US3] Port `src/rt_str.s`, `src/rt_list.s` and `src/rt_map.s` to C (FNV-1a with the fixed seed; growth policies byte-identical, since sizes drive memory figures). The page pins in `rt_list.s`/`rt_map.s` disappear: record the qemu speed ratio before and after.
- [X] T042 [P] [US3] Port `src/rt_render.s`, `src/rt_io.s` and `src/rt_test.s` to C. `rt_render` recursion over nested values needs a counter if it is depth-limited (R7). `rt_test`'s recovery goes through `jmp_restore`.
- [X] T043 [US3] Port `src/rt_func.s` and `src/rt_call.s` to C. `btab` becomes a table of function pointers or a `switch` with the same ids, and `B_LAST` bounds it.
- [X] T044 [US3] Port `src/rt_effects.s` and `src/net.s` to C (grant checks; clock, random, env, sockets and DNS through `vo_syscall*`).
- [X] T045 [US3] Port `src/prop.s` to `src/prop.c`. The generated values that the assembly kept alive through conservative roots (`pr_args`, `pr_best`, `pr_cand` and the generator's locals) are registered with `GC_ROOT` (R4).
- [X] T046 [US3] Port `src/interp.s` to `src/interp.c`:
  - Frames entered from native code, runtime errors and reports, timeouts, equality, closures, `run_main`, test bodies.
  - R7: before porting, measure on `build-asm/vo`, for each builtin that calls back (`map`, `filter`, `fold`, `any`, `all`, `sort_by`, and the property runner), the nesting depth at which a program recursing only through that builtin reports R0005. Write these programs as step or golden cases in `tests/cases/run/reentry_<builtin>.vo` with expected output from `build-asm/vo`, so the depth is pinned. Then implement the per-builtin re-entry limits (a table indexed by builtin id; mixed recursion stops at the first exceeded limit), and record the table in `results.md` "Recursion limits (R7)". Record any mixed-recursion depth that differs from the assembly release as a deviation.

### Stage 7: engine

- [X] T047 [US3] Port `src/vm.s` and `src/trace.s` to `src/vm.c` (R6):
  - one `vm_run` with `&&label` handlers;
  - `vm_ops` and the tracing table published by an init call before `compile_program`;
  - pc, frame and slots as locals;
  - the integer arithmetic checks explicit (R13).
  
  The page pin at the top of `vm.s` goes. From this task on, `make gate` enforces `tests/speed.py --baseline build-asm/vo` at 10% geometric mean (SC-004).
- [X] T048 [US3] Port `src/gc.s` to `src/gc.c`:
  - mark-sweep over the same pages, page table and mark stack at the same addresses;
  - roots: the value stack, `gc_roots`, the globals, the permanent arena;
  - no native-stack or register scanning (R4);
  - `STRESS` and `FAULT=<KIND>` keep working, with the native-stack fault kind removed and `tests/gc/fault_targets` updated.
  
  Run `make gc-check` and the full gate. Compare every case that prints memory figures with `build-asm/vo`, and record any difference in `results.md` "Memory figures (R4)".
- [X] T049 [US3] Split `src/sys.s` and `src/jmp.s`: the C parts (system-call wrappers, buffered output, file reading, the arena allocator, `map_fixed`) go to `src/sys.c`. `_start`, `vo_syscall0`–`vo_syscall6` and `jmp_save`/`jmp_restore` go to `src/platform/riscv64.S`, with numbers in `src/platform/riscv64.h` (contracts/platform-layer.md, at most 300 lines, SESI-linted). Delete `gc_spill_registers` and the `RUNTIME` link order in `Makefile`.
- [X] T050 [US3] Confirm that no `src/*.s` remains. Then make `src/vo.h` the source of the constants, with `defs.inc` reduced to what `riscv64.S` uses or generated from `vo.h`, and remove `tools/gen_protos.py` and `build/protos.h` from the build. Run the full gate and `make diff-asm COUNT=10000` (0 disagreements). Record US3's independent test in `results.md`.

**Checkpoint:** the whole toolchain is C plus one RISC-V platform file. It still runs under qemu, and every step was gated.

---

## Phase 4: User Story 1 - vo runs natively on an ordinary machine (Priority: P1) 🎯 MVP

**Goal:** a native x86-64 build from a clean checkout that passes everything (FR-002–FR-004, FR-015).

**Independent test:** on x86-64 Linux, run `git clean -xdf && make && make test` with no qemu involved for `build/vo`. Everything passes.

- [X] T051 [US1] `src/platform/x86_64.S` and `src/platform/x86_64.h` (contracts/platform-layer.md):
  - `_start` maps `ADDR_STACK` and switches `rsp`;
  - `vo_syscall0`–`vo_syscall6` (`syscall`, with `r10` for the fourth argument);
  - `jmp_save`/`jmp_restore` (`rbx`, `rbp`, `r12`–`r15`, `rsp`, the return address);
  - no signal trampoline: the toolchain installs no signal handlers (research R5);
  - at most 300 lines.
  
  System-call numbers come from the x86-64 table, restricted to the calls listed in research R5.
- [X] T052 [US1] `Makefile`: `ARCH` defaults to the host (`uname -m`). `CC`, flags and platform file are chosen per architecture: native `gcc` on x86-64, without the RV64 flags. `build/vo` is the host build. `make ARCH=riscv64` keeps the current cross build as `build-riscv64/vo`. Check that `readelf -d build/vo` shows no dynamic section (FR-003).
- [X] T053 [US1] `bin/vo`: reads the ELF machine field of `${VO_BIN:-build/vo}` and runs it directly when it matches the host, otherwise through `qemu-riscv64` or `qemu-aarch64` (contracts/build.md). Check that `make test`, `tests/steps.py` and every tool that launches `bin/vo` work for both kinds.
- [X] T054 [US1] Run the full suite natively (`make test`, `make gc-check` natively, `tests/gc/baseline.py`), and fix every disagreement. Expect the x86-64-only trap cases (division, R13) and the NaN sign (R13). Record the results in `results.md` (SC-001 for x86-64).
- [X] T055 [US1] Time a clean native build: `git clean -xdf && time make` must be under 1 minute (SC-006). Record it. If it is over, add parallel build support (`make -j`) and record that.

**Checkpoint:** `vo` is native on x86-64 (the MVP of this spec).

---

## Phase 5: User Story 2 - Same results on every platform and against the assembly version (Priority: P1)

**Goal:** byte-identical behavior on x86-64, ARM64 and RISC-V, and against `build-asm/vo` (FR-001, FR-008–FR-010).

**Independent test:**
- `make diff-asm COUNT=10000` on each platform: 0 disagreements.
- `tests/cross_replay.py` between every pair: identical output.

- [X] T056 [P] [US2] `src/platform/aarch64.S` and `src/platform/aarch64.h`: `_start`, `vo_syscall0`–`vo_syscall6` (`svc #0`, `x8`), `jmp_save`/`jmp_restore` (`x19`–`x30`, `sp`, `d8`–`d15`), at most 300 lines. The system-call numbers come from the generic table shared with RISC-V. `make ARCH=aarch64` builds `build-aarch64/vo` with `aarch64-linux-gnu-gcc`.
- [X] T057 [US2] Run the suite on `build-aarch64/vo` and `build-riscv64/vo` (`VO_BIN=... make test`, `tests/steps.py`, `tests/gc/baseline.py`) through `bin/vo`'s qemu path. Fix every disagreement, and record SC-001 for both platforms in `results.md`.
- [X] T058 [P] [US2] `tests/cross_replay.py` (SC-007):
  - for every replay step case in `tests/cases/replay`, record with `--record BUILD_A` and replay the recording with `--replay BUILD_B`;
  - compare the replay's output with the case's expected output;
  - the default runs all ordered pairs of `build/vo`, `build-aarch64/vo` and `build-riscv64/vo`.
- [X] T059 [US2] Run `make diff-asm COUNT=10000` with `--platform x86_64`, `aarch64` and `riscv64`. Expected: 0 disagreements on each. Also add `--mode float` (arithmetic chains over floats: `*`, `/`, `+`, `-` mixed, values near overflow, underflow, `0.0 / 0.0`, `str` of every result) and `--mode intedge` (division and remainder with the minimum integer, -1 and 0 as operands; overflow at both ends) to `tests/gen_programs.py`, and run 10,000 programs of each mode with `--platform` x86_64, aarch64 and riscv64. Expected: 0 disagreements. Record SC-002 in `results.md`.
- [X] T060 [US2] Write golden cases only where the cross-platform check found a gap the suite did not cover, for example a NaN-producing expression printed with `str`, or a minimum-integer division. These are new tests of old behavior; their expected output comes from `build-asm/vo`, not from the C build. Record each in `results.md`.

**Checkpoint:** three platforms, one behavior.

---

## Phase 6: User Story 4 - Faster native runs (Priority: P2)

**Goal:** SC-003 and SC-004.

**Independent test:** `make speed-check` natively, and `tests/speed.py --baseline build-asm/vo --new build-riscv64/vo`.

- [X] T061 [US4] `tests/speed.py`: a `--native` mode that times `tests/speed/*.vo` on `build/vo` natively against `build-asm/vo` under qemu on the same machine. It fails if the geometric-mean speedup is below 3.0 or any program is slower (SC-003). `make speed-check` uses it on native hosts.
- [X] T062 [US4] Measure SC-003 and SC-004 and record them. If SC-004 fails, profile the RV64 VM under qemu. Try the R6 `switch` dispatch (keeping it if it meets SC-004, which removes the GNU extension), check `-O2` against `-O3`, and inline the hot runtime calls, until it passes. Record each attempt in `results.md`.

---

## Phase 7: User Story 5 - Cheaper and safer to change (Priority: P3)

**Goal:** the ASan + UBSan build passes the whole suite, and C changes have per-edit checks (FR-011, FR-012, SC-005).

**Independent test:**
- `make instrumented` runs the full suite with 0 reports.
- Introducing an out-of-bounds write in a scratch copy is reported with its function and line.

- [X] T063 [US5] `make instrumented` in `Makefile` (R9): `build-instrumented/vo` is a hosted x86-64 build of the same sources.
  - `-fsanitize=address,undefined`, linked with the system libc for the sanitizer runtimes only.
  - A `main()` in `src/platform/hosted.c` calls `vo_main`.
  - The platform layer stays the same, with the system calls still raw.
  - `ADDR_*` regions move by a build constant above ASan's shadow (`0x1000_0000_0000`).
  - The suite runs under it via `VO_BIN`.
- [X] T064 [US5] ASan poisoning hooks in `src/gc.c`, under `#ifdef VO_ASAN`: poison free slots, page tails and swept objects with `__asan_poison_memory_region`, and unpoison on allocation. Then run the full suite under `build-instrumented/vo` and fix every report. Check the detector with a canary (US5 scenario 2): in a scratch copy, make one runtime function write one word past a list's data, run a case that reaches it, and require an ASan report naming that function and line. Record SC-005 and the canary result in `results.md`.
- [X] T065 [US5] `make lint` in `Makefile`:
  - the strict-warning C build for all three architectures, with zero warnings;
  - SESI over `src/platform/riscv64.S`;
  - a check that no `.s`/`.S` file exists outside `src/platform/` and that each platform file is at most 300 lines (FR-005, SC-008).
  
  Check that the SESI PostToolUse hook covers `src/platform/*.S` (the C hook is T010).

---

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T066 [P] Update the documentation (FR-016):
  - `README.md`: build and run natively; tools; cross builds.
  - `docs/roadmap.md`: "Running natively" is done, and the next specs start from C.
  - Any text that tells users to use qemu, including the language reference's run instructions, if any. The rest of the reference and the error catalog stays unchanged (SC-009), shown by `git diff --stat docs/` and recorded.
- [X] T067 [P] Update `specs/008-c-rewrite/contracts/*.md` and `quickstart.md` where the implementation settled details (flags, script options), and check that every quickstart command runs.
- [X] T068 Run the final full gate on each platform: `make gate` natively, the suites under the ARM64 and RISC-V builds, `make instrumented`, `make diff-asm COUNT=10000` on all three, `tests/cross_replay.py`, the speed checks. Fill the SC-001–SC-009 table in `results.md` with the measured values, and list every deviation (R4 memory figures, if any).
- [X] T069 Remove what only the assembly needed: `tests/gc/fault_targets` entries for native-stack scanning, the `RUNTIME` comment in `Makefile`, and `.claude` skill references to writing RV64 assembly for toolchain code (the `sesi-riscv64` skill stays for the platform layer). Update the project memory notes if they name assembly-only workflows.

---

## Dependencies & Execution Order

- **Setup (Phase 1)** → **Foundational (Phase 2)** → **US3 (Phase 3)** → **US1 (Phase 4)** → US2, US4, US5 → **Polish**.
- **Within US3**, the stages run in order: each stage's callers are ported after their callees.
  - Within a stage, [P] tasks touch different files and can be ported in parallel, but each is gated on its own. Merge one at a time, re-running the gate after each merge.
  - `sys`/`jmp` (T049) needs every other port (T014–T048).
  - `gc` (T048) needs every heap-touching file ported (T041–T047).
- **US1** needs T050.
- **US2** needs US1: x86-64 is the reference host for the differential.
- **US4** needs US1 (T061) and T047.
- **US5** needs US1 and T048 (the poisoning hooks live in `gc.c`).
- **Polish** comes after US1, US2, US4 and US5.

### Parallel opportunities

- T004, T005, T006 and T012 in parallel.
- Stage 1: T014, T015 and T017, then T016, T018 and T019.
- Stage 3: T024, T025, T028, T029 and T032, while T026, T027, T030, T031 and T033 run in sequence (shared checker state).
- Stage 4: T034, T035 and T036.
- Stage 6: T041 and T042.
- After US1: T056 and T058 alongside T061.
- T066 and T067.

## Parallel Example: Stage 3

```text
Track A: T024 (types) → T026 (check) → T027 (check_expr) → T030 (check_effects) → T031 (loader) → T033 (effects_record)
Track B: T025 (effects), T028 (check_func), T029 (check_generic), T032 (packages)  # each gated and merged on its own
```

## Implementation Strategy

1. Phases 1–2: the oracle (`build-asm/vo`), the mixed build, the shared headers, the root stack and the gate. Nothing ported yet.
2. US3, stages 1–7: one gated file at a time, logged in `results.md`. The toolchain is releasable after every step.
3. US1: the x86-64 platform layer, which gives a native toolchain (the MVP).
4. US2: ARM64 and the cross-platform evidence. US4: speed. US5: ASan.
5. Polish: documentation and the final gate on all platforms.

Stopping points: after any US3 step, the tree is green and still runs under qemu. After US1, `vo` is native on x86-64.

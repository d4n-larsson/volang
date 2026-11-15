# Implementation Plan: The Toolchain in C (Native Builds)

**Branch**: `008-c-rewrite` | **Date**: 2026-11-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/008-c-rewrite/spec.md`

## Summary

**1. A mixed toolchain, one file at a time** (research R1, R2, R14):
- C compiled for RV64 links with the remaining assembly, since both share the calling convention and data layouts.
- `defs.inc` stays the source of offsets; C structs are checked against it with static assertions.
- Each migration step ports one file, deletes its assembly and passes the full gate before the next step starts.
- Order: leaf code, front end, checker, commands, compiler, runtime, then the engine (VM, collector, startup). The native x86-64 and ARM64 builds come last.

**2. Determinism that does not depend on the compiler** (R4, R7, R13):
- Precise native roots: an explicit root stack replaces conservative scanning of the C stack once the collector is ported.
- Explicit counters for every documented recursion limit.
- Audited replacements for everything RISC-V hardware decided: division, shifts, float conversion and NaN bits.
- `-ffp-contract=off`, so every float operation rounds to IEEE double.

**3. A small platform layer** (R5): `_start` (the fixed 64 MiB stack), raw system calls, `jmp_save`/`jmp_restore`. One file per architecture, at most 300 lines, using only system calls common to all three.

**4. The VM keeps its design** (R6): direct-threaded dispatch with GCC labels as values over the same 32-byte instructions. A `switch` is the fallback if it is fast enough.

**5. The oracle and the gates** (R9–R12):
- `build-asm/vo` (the last assembly release) is the reference for a generated-program differential (2,000 programs per step, 10,000 at milestones) and the speed baseline.
- UB traps from the first step; ASan + UBSan on a hosted x86-64 variant once native.
- The suite, the stress build and the bench2 reference runs at every step.

## Technical Context

**Language/Version**: C11 with GNU extensions (`-std=gnu11`: labels as values, `returns_twice`), freestanding, no C library. Per-architecture platform layers in assembly (`.S`). Today's RV64 assembly during the mixed phase.

**Primary Dependencies**:
- None at run time: Linux system calls only.
- Build: GCC 13 (`gcc` native, `gcc-riscv64-linux-gnu`, `gcc-aarch64-linux-gnu`), binutils, `make`, Python 3 (embedding and header generation scripts).
- Test: `qemu-user` (`qemu-riscv64`, `qemu-aarch64`), the existing Python test tools, SESI `riscv64` for the RISC-V platform layer and the remaining assembly.

**Storage**: N/A (the files the toolchain reads and writes are unchanged).

**Testing**:
- Per edit: a PostToolUse hook compiles an edited `src/*.c` with the strict warning set, beside the SESI hook for assembly (FR-012).
- The existing suite, unchanged: `make test`, `make gc-check`, `tests/gc/baseline.py` (703 bench2 runs), fuzzers, the determinism and engine checks.
- New:
  - `make gate` (a step's full gate)
  - `tests/diff_asm.py` (differential against `build-asm/vo`)
  - `make ubtrap`
  - `make instrumented` (stage 8)
  - `tests/cross_replay.py` (SC-007)
  - `make ARCH=…` cross builds

**Target Platform**: Linux user space on x86-64, ARM64 and RISC-V 64. Static ELF, no interpreter. The non-host architectures are tested under `qemu-user`.

**Project Type**: compiler and runtime for a programming language, as a CLI. This is a reimplementation with identical behavior.

**Performance Goals**:
- Natively on x86-64: at least 3 times faster than `build-asm/vo` under qemu (geometric mean of `tests/speed`), and no program slower (SC-003).
- The RV64 C build under qemu: at most 10% slower than `build-asm/vo` (SC-004).
- Native build from a clean checkout: under 1 minute (SC-006).

**Constraints**:
- Build dependencies: a C compiler, `make` and Python 3 (FR-002).
- No behavior change of any kind (FR-001, FR-017). The only allowed difference is FR-001's exception: memory figures that precise roots make smaller, identical across platforms (R4), recorded.
- No C library. At most 300 lines of assembly per platform.
- Zero warnings under the strict set. UB traps clean at every step.

**Scale/Scope**:
- 45 assembly files (about 53,000 lines) become an estimated 16,000–20,000 lines of C in about 42 files, in about 45 migration steps over 8 stages.
- 3 platform layers (about 150–250 lines each), 5 developer scripts (`embed.py`, `gen_defs.py`, `gen_protos.py`, `cc_check.sh`, `diff_asm.py`), 1 cross-replay script.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* Evaluated against constitution 1.3.0.

| Principle / constraint | Assessment |
|---|---|
| I. Agent-first design | **PASS.** No language or CLI change (FR-017). Agents see the same tool. |
| II. Machine-readable feedback | **PASS.** Every diagnostic, record and schema is byte-identical (FR-001), and no code changes meaning. |
| III. Determinism | **PASS.** Strengthened: results become identical across three platforms. This is the reason for precise roots (R4), explicit recursion counters (R7), the hardware-behavior audit (R13) and `-ffp-contract=off` (R3). Exit paths and reports are unchanged. |
| IV. Test-first through the CLI | **PASS.** The behavior is already specified by the golden and step cases, which every step must pass unchanged. New cases are not needed, because no behavior is new. The differential and cross-platform checks add coverage, not behavior. |
| V. Single source of truth, minimal surface | **PASS.** The reference, the catalog and the contracts are unchanged. `defs.inc` stays the single source of layouts during the mixed phase (R14). `ponytail:` comments carry over with their code. |
| Freestanding C, no libc, static ELF | **PASS.** R3, R5. The instrumented variant links libc only for sanitizer runtimes and is never shipped (R9). |
| Platforms x86-64, ARM64, RV64 | **PASS.** Stage 8; contracts/build.md. |
| Assembly only in the platform layer or not-yet-ported files, SESI-linted | **PASS.** Platform layers at most 300 lines each. The RISC-V one and the remaining assembly are linted by SESI. x86-64 and ARM64 have no SESI pack, which the constitution allows ("where a pack exists"). |
| No reliance on UB; explicit arithmetic checks | **PASS.** R13 audit, UB-trap build. |
| IEEE double per operation | **PASS.** `-ffp-contract=off`, SSE2 on x86-64. |
| Zero warnings; instrumented build | **PASS.** The strict warning set applies from step 1, checked on every edit by a hook (tasks T010) and by `make lint`. The instrumented build follows constitution 1.3.0's phasing: the UB-and-bounds trap build plus the collector stress build while assembly remains, ASan + UBSan from the first native build (stage 8). |
| Developer scripts only for building and testing | **PASS.** `tools/*.py` generate C and headers at build time; nothing ships. |
| GNU extensions under "C (C11)" | **PASS.** `gnu11` is C11 plus extensions. The two used are named in R3. The `switch` fallback (R6) removes the larger one if speed allows. |

Post-design re-check (after Phase 1): unchanged, all PASS. The data model adds no observable state; the contracts are developer-facing only.

## Project Structure

### Documentation (this feature)

```text
specs/008-c-rewrite/
├── plan.md              # This file
├── research.md          # R1-R14
├── data-model.md        # shared layouts, root stack, platform interface, migration log, build variants
├── quickstart.md        # validating the port
├── contracts/
│   ├── build.md          # make targets, bin/vo, dependencies
│   ├── platform-layer.md # the per-architecture assembly interface
│   └── port-protocol.md  # the procedure of one migration step
├── results.md           # migration log and success criteria (written during implementation)
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/
├── *.c                  # one C file per former assembly file, same name (lex.c, parse.c, ..., vm.c, gc.c)
├── vo.h                 # shared C declarations: structs for the defs.inc layouts, GC_ROOT, helpers
├── defs.inc             # offsets and constants until stage 8 (T050 folded them into vo.h)
├── *.s                  # assembly not yet ported (shrinks to nothing)
└── platform/
    ├── riscv64.S  riscv64.h   # _start, syscalls, jmp_save/restore; syscall numbers
    ├── x86_64.S   x86_64.h
    └── aarch64.S  aarch64.h
tools/
├── embed.py             # docs/*.md + std/*.vo -> build/embedded.c (R8)
├── gen_defs.py          # src/defs.inc -> build/defs.h (R14; removed in T050)
├── gen_protos.py        # prototype comments of remaining .s -> build/protos.h (R14; removed in T050)
└── cc_check.sh          # per-edit C check (PostToolUse hook, FR-012)
tests/
├── diff_asm.py          # differential against build-asm/vo (R12)
├── cross_replay.py      # SC-007
└── (everything else unchanged)
Makefile                 # mixed build; ARCH=; asm, gate, ubtrap, instrumented targets (contracts/build.md)
bin/vo                   # native or qemu, by the binary's architecture
```

**Structure Decision**: the single-project layout stays. C files take the names of the assembly files they replace, so `git log --follow`-style history and the module map from earlier specs stay recognizable. The only new directories are `src/platform/` and `tools/`.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| The mixed phase keeps conservative native-stack scanning until the collector is ported, while C code already registers roots | Assembly code still relies on its `s` registers being scanned. | Precise roots from step 1 would require rewriting the root handling of all remaining assembly first. |
| Two sources of layout truth during the mixed phase (`defs.inc` and C structs) | Both languages need the offsets. | Generated structs alone cannot express unions and variable-length tails readably; static assertions make the hand-written structs safe. |
| GNU extension (labels as values) | It keeps the VM's direct-threaded dispatch, which the 10% speed bound (SC-004) is likely to need. | Plain `switch`: kept as the measured fallback (R6). |

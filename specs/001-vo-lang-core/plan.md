# Implementation Plan: vo-lang Core Language and Toolchain

**Branch**: `001-vo-lang-core` | **Date**: 2026-11-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-vo-lang-core/spec.md`

## Summary

vo-lang is a small, statically typed language in which every construct has exactly one spelling and every statement starts with a keyword or a call. It ships as one executable, `vo`, with these commands: `check`, `run`, `test`, `fmt`, `explain`, and `reference`. Every command can emit JSON Lines output.

The toolchain is written in RV64IMAFD assembly as a static Linux user-space ELF with no libc. On x86 hosts it runs under `qemu-riscv64`. Every value (`int`, `float`, pointer) fits in one 64-bit register.

The pipeline is: lexer → recursive-descent parser → AST (allocated in an arena) → checker (names, types, exhaustiveness, result handling) → tree-walking interpreter over the checked AST. The language reference and the error catalog are markdown files, embedded into the binary with `.incbin`. That makes them the single source of truth for `vo reference` and `vo explain`.

## Technical Context

**Language/Version**: RISC-V assembly (RV64IMAFD + Zicsr, lp64d calling convention), GNU as syntax. Every edit is checked by the SESI riscv64 pack (`.sesi/packs`).

**Primary Dependencies**: None at runtime: raw Linux syscalls, no libc. For building: `binutils-riscv64-linux-gnu` (`as -march=rv64imafd -mabi=lp64d`, `ld`). For running on x86: `qemu-user` (`qemu-riscv64`).

**Storage**: Source and data files on the local filesystem only.

**Testing**: Golden-file tests (`tests/run.sh`: run `vo` on each case, diff stdout/stderr/JSONL/exit code), a malformed-input fuzzer (`tests/fuzz.py`), a reference-example checker (`tests/ref_examples.py`, FR-030), and an agent benchmark (`bench/`, SC-001/SC-008). Python 3.12 is used for dev scripts only. It is not part of the product.

**Target Platform**: Linux user space on 64-bit RISC-V. Developers on x86_64 run it through `qemu-riscv64` via the `bin/vo` wrapper.

**Project Type**: Compiler front end plus interpreter, delivered as a CLI.

**Performance Goals**: Check plus run startup for a 1,000-line program in under 1 s under qemu (SC-004). Tree-walking is enough for this. No throughput target.

**Constraints**: Deterministic output (FR-009). No undefined behavior (FR-008). Every failure ends in a structured report (SC-007). Reference of 15k tokens or less (SC-002).

**Scale/Scope**: About 9–12k lines of assembly across ~12 source files. Six user stories. About 40 error codes.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluated against constitution **v1.1.0** (2026-11-13).

| Gate | Status | Evidence / obligation carried into tasks |
|---|---|---|
| I. Agent-first design | PASS | grammar.md: one spelling per construct, first-token statements, no implicit conversion, shadowing, or overloading. Builtins like `len` and `slice` accept several built-in types; they are fixed language primitives, not user overloading |
| II. Machine-readable feedback | PASS | output-schema.md: versioned JSONL; every diagnostic has code, span, `text`, and fixes; error-codes.md is append-only |
| III. Determinism and defined behavior | PASS | research §3 (checked arithmetic), §5 (insertion-ordered maps), §8–9 (timeout, stack); `now_ms` is the only nondeterministic builtin |
| IV. Test-first through the CLI | PASS | research §12. **Obligation**: in tasks.md, each golden case task must come before the implementation it covers, and every catalog code needs a triggering case |
| V. Single source of truth, minimal surface | PASS | `docs/` embedded via `.incbin`; token budget checked by `ref_examples.py`. **Obligation**: the shortcuts in Complexity Tracking get `ponytail:` comments in code |
| Implementation constraints | PASS | RV64IMAFD/lp64d, no libc, SESI `riscv64` pack enabled; Python only in `tests/` and `bench/` |
| Workflow and quality gates | PASS | `make test` runs golden cases, `--repeat 3` determinism, a 200-iteration fuzz, `apply_fixes.py`, `catalog.py`, `ref_examples.py` and `fmt_corpus.py`. Full-scale runs happen in tasks T082 and T084 |

**Post-design re-check**: PASS. The Phase 1 contracts add no dependencies and no requirements outside the spec.

### Requirement → component map

| Requirements | Component / contract |
|---|---|
| FR-001–003, FR-016 | `parse.s`, [contracts/grammar.md](contracts/grammar.md) |
| FR-004–007, FR-013 | `check.s` |
| FR-008, FR-025 | `interp.s` (inline checked arithmetic), `rt_*.s`; runtime error schema in [contracts/output-schema.md](contracts/output-schema.md) |
| FR-009 | map with insertion order and no random seed (research §5); time and randomness only through explicit builtins |
| FR-010 | arena allocator, no `free` exposed (research §4) |
| FR-011–015 | `rt_*.s` builtins, [contracts/grammar.md](contracts/grammar.md) §Builtins |
| FR-017–020 | `main.s`, `rt_io.s` (stdin timeout), [contracts/cli.md](contracts/cli.md) |
| FR-021–024 | `diag.s`, [contracts/error-codes.md](contracts/error-codes.md) |
| FR-026–027 | `testrun.s` |
| FR-028 | `fmt.s` |
| FR-029–030 | `docs/reference.md` (embedded), `tests/ref_examples.py` |

## Project Structure

### Documentation (this feature)

```text
specs/001-vo-lang-core/
├── plan.md              # This file
├── research.md          # Phase 0: decisions and rationale
├── data-model.md        # Phase 1: tokens, AST, types, values, reports
├── quickstart.md        # Phase 1: build + validation scenarios
├── contracts/
│   ├── cli.md           # commands, flags, exit statuses
│   ├── output-schema.md # JSONL records: diagnostic, runtime_error, test, summary
│   ├── grammar.md       # EBNF, lexical rules, builtins
│   └── error-codes.md   # initial stable code catalog
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
Makefile                 # assemble + link build/vo; `make test`
bin/vo                   # sh wrapper: exec build/vo natively or via qemu-riscv64
src/
├── defs.inc             # shared .equ constants: token/node/type kinds, layouts, syscalls, exit statuses
├── main.s               # argv parsing, command dispatch, exit statuses
├── sys.s                # syscalls, file I/O, clock, arena allocator, buffered output
├── num.s                # int/float <-> text (research §3, §11)
├── text.s               # byte strings, UTF-8 validation, column counting, JSON escaping
├── lex.s                # tokens with spans; comments as tokens
├── ast.s                # pointer vectors and node construction
├── parse.s              # recursive descent -> AST; parse diagnostics with recovery
├── loader.s             # imports: loading, cycle detection, binding, missing-import scan
├── check.s              # scopes, declarations, statements, exhaustiveness, result use, returns
├── check_expr.s         # expression typing, builtin signatures, fix suggestions
├── interp.s             # tree-walking evaluator, call frames, step/time/stack guards
├── jmp.s                # setjmp/longjmp for `try` and test recovery (lint-exempt, explained)
├── rt_call.s            # builtin dispatch
├── rt_io.s              # I/O builtins (stdin reads wait with ppoll against the deadline)
├── rt_str.s             # string builtins and conversions
├── rt_list.s            # lists, stable sort
├── rt_map.s             # insertion-ordered hash maps
├── rt_render.s          # values rendered in vo literal syntax (test failures)
├── rt_test.s            # expect / expect_eq
├── fmt.s                # AST -> canonical text
├── diag.s               # diagnostic collection, sorting, cap, human + JSONL rendering, fixes
├── testrun.s            # test discovery, isolation, results
└── data.s               # .incbin docs/reference.md and docs/errors.md
docs/
├── reference.md         # the language reference (FR-029), printed by `vo reference`
└── errors.md            # error catalog, one section per code, read by `vo explain`
tests/
├── run.sh               # golden runner (+ --repeat N for SC-006)
├── cases/{cli,lex,parse,check,run,test,fmt,multi}/   # *.vo + .cmd/.in/.out/.err/.code
├── fuzz.py              # malformed-input fuzzer (SC-007)
├── apply_fixes.py       # applies every suggested fix, re-checks, measures fix coverage (SC-003)
├── catalog.py           # runs every catalog Example/Fixed; checks every code has a golden case
├── ref_examples.py      # checks every example in docs/reference.md (FR-030) and the token budget
├── fmt_corpus.py        # formatter idempotence + unchanged behavior (SC-005)
└── perf.sh              # 1,000-line timing (SC-004) and large-input edge cases
bench/
├── tasks/NN-name/       # task.md + stdin/expected pairs (SC-001, SC-008)
├── run_agent.sh         # runs a headless agent with only `vo reference` as context
└── summarize.py         # SC-001 / SC-008 metrics
```

**Structure Decision**: Single project. One executable, so there is one `src/` directory with one assembly file per pipeline stage. The docs live in `docs/` because they are product inputs (embedded into the binary), not just documentation.

## Complexity Tracking

No constitution violations to justify. The deliberate shortcuts below are recorded in research.md, each with its ceiling and upgrade path:

| Shortcut | Ceiling | Upgrade path |
|---|---|---|
| Arena allocation, no GC (research §4) | Allocation-heavy long runs hit the memory limit (reported as R0006) | Copying collector over the same object layouts |
| Tree-walking interpreter (research §1) | ~50–100× slower than native; irrelevant at agent task sizes | Bytecode VM behind the same checker |
| Float printing to 15 significant digits (research §11) | Not the shortest round-trip representation | Ryu/Grisu port |

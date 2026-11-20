# Implementation Plan: Typed Errors

**Branch**: `032-typed-errors` (work on `main`, as for specs 001–031) | **Date**: 2026-11-20 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/032-typed-errors/spec.md`

## Summary

`result[T, E]`: the result type gets its error type as an optional second argument, `str` when left out,
stored so that every existing type stays exactly what it was (research R1). The error type reaches
`match`, rendering, tasks, generated inputs and test reports through the one function they already share
(R8). `try` compares error types (E0412, R5); `map_err` is the one conversion (R6). `std.web` gains
`typed(op, answer)` beside the prefix form (R9); `std.option` adds `Option[T]` (R10).

| Part | File | What |
|---|---|---|
| Type | `src/check.c`, `src/types.c`, `src/check_generic.c`, `src/vo.h` | B of `TY_RESULT`, `ty_result_e`, `err_type`, naming, unify, substitute; E0334 |
| Syntax | `src/parse.c`, `src/fmt.c` | `result[T, E]` |
| Checking | `src/check_expr.c`, `src/check.c` | `err` against the error type, `try` (E0412), `case_payload` |
| Builtin | `src/check_expr.c`, `src/rt_call.c` | `map_err` |
| Runtime and reports | `src/rt_render.c`, `src/task.c`, `src/prop.c`, `src/rt_test.c` | through `case_payload`; checked, not rewritten |
| Library | `std/web.vo`, `std/option.vo` (new) | `Problem`, `typed`; `Option`, helpers |
| Docs | `docs/reference.md`, `docs/errors.md`, `site/docs/guide/`, `docs/roadmap.md`, `README.md` | R12 |
| Examples | `examples/notes-api`, `examples/expenses` | `NoteError`, a typed parse error |
| Tests | `tests/cases/typed_errors/`, `tests/cases/web/`, `tests/cases/prop/`, `tests/cases/tasks/` | R13 |

## Technical Context

**Language/Version**: C (C11, freestanding) for the toolchain; vo for the library and examples.

**Primary Dependencies**: none.

**Storage**: none; recordings and the effects record unchanged.

**Testing**: golden cases written first; `make quick` between edits, `make test` once per change; the
gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64, byte-identical.

**Project Type**: compiler toolchain (CLI) with its standard library.

**Performance Goals**: check time within 5% (SC-005); a typed `err` costs what a `str` one does (one
variant object either way).

**Constraints**: every existing program byte-identical (FR-003); one conversion form; no implicit
conversion; reference ≤ 15,000 tokens (+≤ 300 here); schema 1.

**Scale/Scope**: about 300 lines of C, 80 of vo library, two catalog entries, two examples.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One form for the type (an optional second argument), one conversion (`map_err`), no implicit conversion, no hidden control flow: `try` across error types is refused, not converted. | Pass |
| II. Machine-readable feedback | E0334 and E0412 with spans and messages naming both types; E0401's fix covers error variants. | Pass |
| III. Determinism | No new nondeterminism; error values render, compare and transfer as variants do. | Pass |
| IV. Test-first | Golden cases per scenario and code before the code; existing corpus unchanged (SC-001). | Pass |
| V. Single source of truth, minimal surface | Reference, catalog and library docs together; +≤ 300 tokens; `std.web` keeps one routing model (typed operations become ordinary ones); `Option` is a library type, no builtin changes (YAGNI). | Pass |

Re-checked after Phase 1: unchanged. The one spec deviation (std.web's error type per operation, not per
app) is recorded in research R9.

## Project Structure

### Documentation (this feature)

```text
specs/032-typed-errors/
├── plan.md
├── research.md          # R1–R13
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── syntax.md        # result[T, E], ok/err/match, try, map_err, main and tests
│   ├── diagnostics.md   # E0334, E0412, reused codes
│   └── library.md       # std.web typed/Problem, std.option
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/check.c, src/check_expr.c, src/check_generic.c, src/types.c, src/parse.c, src/fmt.c, src/rt_call.c,
src/rt_render.c, src/task.c, src/prop.c, src/rt_test.c, src/vo.h
std/web.vo, std/option.vo
docs/reference.md, docs/errors.md, site/docs/guide/, docs/roadmap.md, README.md
examples/notes-api, examples/expenses
tests/cases/typed_errors/, tests/cases/web/, tests/cases/prop/, tests/cases/tasks/
```

**Structure Decision**: no new C file; the error type lives in the type it belongs to. One new library
file, `std/option.vo`, as each module is one file.

## Order of work

1. The type (R1–R3) with naming, generics and formatting; the corpus unchanged.
2. `err`, `match` and `try` (R4, R5); E0334, E0412 with catalog entries.
3. `map_err` (R6).
4. Reports, tasks, properties (R8): cases first, code only where a consumer bypasses `case_payload`.
5. `std.web` `typed` and `Problem`, `std.option`; the notes API and expenses examples.
6. Reference, guide, roadmap, README; `make test` on three platforms; results.

## Complexity Tracking

No constitution violations.

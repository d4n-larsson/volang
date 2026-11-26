# Implementation Plan: Variant Cases With Fields, and `match` on Values

**Branch**: `060-case-fields-value-match` (work on `main`, as for specs 001–059) | **Date**: 2026-11-26 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/060-case-fields-value-match/spec.md`

## Summary

A variant case may declare fields; it then carries a hidden record (research R1), so the runtime, JSON,
equality, ordering, keys, test diffs and property tests work unchanged, and only the parser, the checker
and the tools learn the new form (R2, R3, R7). A value prints in construction syntax (R4, a spec
correction). `match` accepts `int` and `str` scrutinees with literal or constant arms and a required
`else`, compiled with existing opcodes (R6). The chain warning of US3 is dropped: no chain in the repository
qualifies (R8). Ships as 0.6.0; two payload records migrate (R9); the reference stays
word-neutral (R10).

| Part | File | What |
|---|---|---|
| Parsing | `src/parse.c`, `src/vo.h` | field cases in `parse_members`; `Type.case {` literal with the `{ name :` lookahead; `Type.case` types; value arms and `N_ELSE_ARM` (R2, R6) |
| Hidden records | `src/check.c`, `src/check_expr.c`, `src/check_generic.c` | the synthetic record per field case: define, resolve, annotations, exports, generic parameters; literals through the record checks; E0344; type names (R1, R3) |
| Value match | `src/check.c`, `src/compile.c`, `src/check_structure.c` | arms by node kind; E0402/E0301/E0345/E0414/E0415; `cmatch` with `OPC_JNE_K`/`OP_EQ`; coverage points (R6) |
| One-field cases | `src/check.c` | E0216 with its fix (R5) |
| Printing | `src/rt_render.c` | construction syntax for field cases (R4) |
| Tools | `src/fmt.c`, `src/doc.c`, `src/query.c`, `src/edit.c`, `src/lsp.c` | R7 |
| Editors | `editors/`, `site/pygments_vo` | R7 |
| Repository | `std/time.vo`, `examples/expenses/report.vo`, `src/libchange.c`, tests | R9 |
| Docs | `docs/reference.md`, `docs/errors.md`, `docs/stability.md`, `CHANGELOG.md`, `site/docs/guide/` | R10 |
| Tests | `tests/cases/case_fields/`, `tests/cases/value_match/` (new) | per scenario and code |

## Technical Context

**Language/Version**: C (C11, freestanding) for the toolchain; vo for std, examples and tools.

**Primary Dependencies**: none.

**Storage**: none. JSON of field cases equals that of record payloads; recordings, `vo.effects` and
`vo.packages` unchanged; schema 1 unchanged.

**Testing**: golden cases first and failing (`tests/cases/case_fields/`, `tests/cases/value_match/`);
`apply_fixes.py`, `catalog.py`, `ref_examples.py`, `site_examples.py`, `doc_conformance.py`,
`fmt_corpus.py`, `fuzz.py`, `grammars.py`, `lsp.py`, `diff_engines.py`; `make quick` between edits (now
parallel), `make test` once per change; ARM64 and RISC-V `make test` at the end; the gate runs nightly.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64, byte-identical.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: no run-time change for existing programs (same bytecode); a value match compiles
to a sequence of compares, as `if` chains do; `vo check` times within 5% of today.

**Constraints**: one form per situation (one value: `(T)`; two or more: fields; dispatch on a value:
`match` with `else`); no new opcode (the assembly backends stay untouched); no node flag is free (R6);
the reference has no headroom (R10).

**Scale/Scope**: about 900 lines of C (parser 150, hidden records 250, value match 200, fixes and codes
120, printing 40, tools 140); 5 new codes; 2 payload records migrated.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One form per situation, decided by a count the author knows (one value or several) and by the scrutinee's type; a case with one field is rejected with a fix (E0216), an `else` on an exhaustive match is rejected with a fix (E0415). Every statement still starts with a keyword. No shadowing, no implicit conversion. | Pass |
| II. Machine-readable feedback | Five new codes with catalog entries; E0216, E0414, E0415 have one-edit fixes; repeats reuse E0402's fix. A field-case literal reuses the record codes (E0304, E0305). | Pass |
| III. Determinism | No run-time representation change; property generation and shrinking go through the record paths; output identical on three platforms. | Pass |
| IV. Test-first | Golden cases per scenario and per code before the code; JSON equality of migrated payloads proven by a golden case (FR-005). | Pass |
| V. Single source, minimal surface | Reference, catalog and contracts change together; reference word-neutral (R10). Left out: destructuring arms, ranges and several values per arm, `match` on float or bool; US3 dropped (R8). | Pass |
| VI. Backend network services | **Write**: typed errors and messages (the normal error type since spec 032) carry several values without a payload record per case; dispatch on status codes, commands and header values is checked for repeats and has a visible default. **Review**: one place declares an error case's fields; a value match shows every handled value and the default. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/060-case-fields-value-match/
├── plan.md
├── research.md          # R1–R11
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── syntax.md
│   └── diagnostics.md
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/parse.c, src/vo.h, src/check.c, src/check_expr.c, src/check_generic.c, src/check_structure.c,
src/compile.c, src/rt_render.c, src/fmt.c, src/doc.c, src/query.c, src/edit.c, src/lsp.c, src/libchange.c
editors/vscode, editors/tree-sitter-vo, editors/vim, editors/zed, site/pygments_vo
std/time.vo, examples/expenses/report.vo
docs/reference.md, docs/errors.md, docs/stability.md, CHANGELOG.md, site/docs/guide/
tests/cases/case_fields/, tests/cases/value_match/
```

**Structure Decision**: no new source file; the hidden records live with the variant code in `check.c`
and `check_expr.c`.

## Order of work

1. **Value match (US2)** first: parser, checker, compiler, coverage, fmt; independent of case fields and
   smaller, so it lands and is tested on its own.
2. **Case fields (US1)**: parser (declaration, literal with lookahead, type), hidden records, literals,
   read-only fields, type names, printing, JSON/prop golden cases.
3. **E0216 and E0415 fixes**, catalog entries.
4. **Tools and editors** (R7).
5. **Repository** (R9): `Repeated`, `DateProblem`, libchange row; golden cases.
6. **Docs** (R10): reference word-neutral, guide, catalog, stability, changelog; version 0.6.0.
7. `make test` on three platforms, `results.md`.

## Complexity Tracking

| Choice | Simpler alternative | Why not |
|---|---|---|
| Hidden record per field case (R1) | a new value kind with named slots | equality, hashing, order, JSON, test diffs and property tests would each need a second implementation |
| `N_ELSE_ARM` node kind (R6) | a node flag | all 16 flag bits are taken (`vo.h:357-374`) |
| Lookahead `{ name :` after `Type.case` (R2) | a keyword or `new` before field literals | the spec builds field cases without `new`, and `Type.case {` followed by a block exists in std |

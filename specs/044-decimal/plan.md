# Implementation Plan: Decimal (exact decimal numbers)

**Branch**: `044-decimal` (work on `main`, as for specs 001–043) | **Date**: 2026-11-22 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/044-decimal/spec.md`

## Summary

One new library module, `std/decimal.vo`, written entirely in vo. No C changes beyond registering the
module (`Makefile`, `src/doc.c`): no new builtin, opcode, syntax or diagnostic code.

- **No primitive**: a prototype of the invoice run of SC-003 in pure vo takes 91 ms for 100,000 lines
  (bound 1 s), so nothing is added for speed (R1).
- **Overflow** needs no code: `int` `+`, `-` and `*` already stop with R0001, and `/` with R0002, so
  exact operations are written directly (R2).
- **Division** is the one place where an intermediate value can exceed 64 bits. It is done as long
  division, a digit at a time, on the remainder, with a slow path for divisors so large that
  `remainder * 10` would overflow (R3). `round_to` divides by a power of ten of at most 18 digits and
  needs nothing special.
- **`to_float`** is the existing internal `__text_float` on the decimal's text: one correct rounding
  (R4).
- **Invalid places** are `requires` clauses: R0012 at the call (R5).

| Part | File | What |
|---|---|---|
| Library | `std/decimal.vo` (new) | `Decimal`, `Rounding`, the twenty-two functions |
| Registration | `Makefile` (`STD_FILES`, test list), `src/doc.c` (`doc_modules`), `tools/quick.sh` (area `decimal`) | the module in the embedded library and `vo doc` |
| Example | `examples/expenses/report.vo` and its tests | amounts as `Decimal`; `money` removed |
| Docs | `docs/reference.md` (one row), `site/docs/guide/standard-library.md` (section), `CHANGELOG.md`, `docs/roadmap.md`, `docs/review-2026-11/README.md` | |
| Tests | `tests/decimals.py` (new, in `make test`), `tests/cases/run/decimal*`, `tests/cases/check/decimal_pure`, `tests/cases/doc/all*.out` (module listing) | R6 |

## Technical Context

**Language/Version**: vo for the module; C only for the two registration lines.

**Primary Dependencies**: none new.

**Storage**: none.

**Testing**: golden cases for the acceptance scenarios, the edge cases and each way of stopping
(R0001, R0002, R0012); `tests/decimals.py` compares 10,000 random cases per function with Python's
`decimal` (SC-001), checks `split_evenly` and `allocate` (SC-002) and times the invoice run (SC-003);
the module's doc examples run under `tests/doc_conformance.py`. `make quick AREA=decimal` between
edits, `make test` once; the gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: 100,000 invoice lines in under 1 s on x86-64 (SC-003); the prototype: 91 ms.

**Constraints**: every result exact or the program stops; reference under 15,000 tokens (14,848 now;
one row of about 25 tokens).

**Scale/Scope**: about 350 lines of vo, 300 of tests, 60 of guide.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. One form per construct | No syntax. One function per operation; no default rounding. `==` and `equal` differ on 1.5 and 1.50: recorded in the spec's Assumptions and stated in the doc comments of `Decimal`, `equal` and `normalize`. | Pass |
| II. Machine-readable feedback | No new code: R0001, R0002 and R0012 as they are; `parse_decimal`, `to_int`, `split_evenly` and `allocate` return results whose messages quote the input. | Pass |
| III. Determinism and defined behaviour | Integer arithmetic only, except `to_float`, which is one correctly rounded conversion. | Pass |
| IV. Test-first | Golden cases and `tests/decimals.py` before the functions they cover. | Pass |
| V. Minimal surface, reference budget | No builtin; one reference row. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/044-decimal/
├── plan.md
├── research.md          # R1–R7
├── data-model.md
├── quickstart.md
├── contracts/
│   └── decimal.md       # the types, the functions, the errors
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
std/decimal.vo (new), Makefile, src/doc.c, tools/quick.sh
examples/expenses/report.vo
docs/reference.md, site/docs/guide/standard-library.md, CHANGELOG.md, docs/roadmap.md
tests/decimals.py (new), tests/cases/run/, tests/cases/check/, tests/cases/doc/
```

**Structure Decision**: one vo file; no C file is added or changed in substance.

## Order of work

1. Register an empty module; `tests/decimals.py` and golden cases written and failing.
2. Types, `decimal`, `from_int`, `parse_decimal`, `decimal_text`, `grouped_text`.
3. `add`, `sub`, `mul`, `negate`, `absolute`, `total`; `compare`, `equal`, `is_zero`, `is_negative`,
   `normalize`.
4. `round_to`, then `div` (R3); `to_int`, `to_float`.
5. `split_evenly`, `allocate`.
6. The expenses example; docs; three architectures; `make test`; results.

## Complexity Tracking

Nothing beyond the constitution's principles.

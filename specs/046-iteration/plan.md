# Implementation Plan: Iteration (two-variable `for`, list helpers)

**Branch**: `046-iteration` (work on `main`, as for specs 001–045) | **Date**: 2026-11-22 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/046-iteration/spec.md`

## Summary

A second variable in `for`, one new instruction, one new warning, and a library module written in vo.

- **Syntax tree**: a flag and the unused range-end field hold the second name; no node grows (R1).
- **Lists and deques**: the loop runs once per index the list had at its start and reads each
  element at its turn, so every two-variable loop visits what was there when it began (R2).
- **Maps** loop over the keys they had at the start and read each value with one new instruction that
  skips a removed key (R3). This holds the key list while the loop runs, as `for k in keys(m)` does
  today; walking the map in place does not survive the map growing in the body. The spec's "no list
  of keys" is amended to "no more than today's form".
- **One warning with a fix**, W0403, for `for k in keys(m) { let v = m[k];` and for
  `for i in 0..len(xs) { let x = xs[i];`. Both rewrites keep the meaning (R2, R4), so one form
  remains.
- **`std.lists`**: eight functions in vo. The descending sort is the builtin `sort_by` between two
  reversals; for that the builtin accepts a key of an `ordered` type parameter, one condition in the
  checker (R5). Measured: 26 ms for 200,000 ints, against 12 ms ascending.
- **Reference**: 95 tokens free; obsolete text is replaced (R6).

| Part | File | What |
|---|---|---|
| Parser | `src/parse.c`, `src/vo.h` | the second binding, `NF_PAIR`, `FOR_RANGE` |
| Checker | `src/check.c`, `src/check_func.c` | types of the two variables, E0301 with its fix, W0403; `sort_by` with a key of an `ordered` type parameter |
| Compiler, VM | `src/compile.c`, `src/vm.c`, `src/vo.h`, `src/rt_coll.c` or `src/rt_map.c` | `OPC_SNAPSHOT` on a map; `OPC_FOR_VALUE` (map by key, list by index); `fl_stmt` |
| Other readers | `src/check_structure.c`, `src/fmt.c` | `FOR_RANGE`; printing |
| Library | `std/lists.vo` (new), `Makefile`, `src/doc.c`, `tools/quick.sh` | R5 |
| Grammars | `editors/tree-sitter-vo/grammar.js` and its generated parser, highlight tests | R7 |
| Moved code | `std/*.vo`, `examples/*/*.vo` | R9 |
| Docs | `docs/reference.md`, `docs/errors.md` (W0403), `site/docs/guide/` (loops; `std.lists`), `CHANGELOG.md`, `docs/roadmap.md`, `docs/review-2026-11/README.md` | |
| Tests | `tests/iteration.py` (new), golden cases in `run`, `check`, `fmt` and the tool areas, `tests/cases/doc/all*.out` | R8 |

## Technical Context

**Language/Version**: C11 (freestanding) for the toolchain; vo for `std.lists`.

**Primary Dependencies**: none new.

**Storage**: none.

**Testing**: golden cases per scenario; `tests/iteration.py` with generated programs against a model
of the meaning, the helpers against Python, and a speed and memory measurement; the grammar checks;
the catalog check for W0403. `make quick AREA=iteration` between edits, `make test` once; the gate
only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: a map loop no slower and no larger than the `keys` form (SC-002).

**Constraints**: reference under 15,000 tokens (14,905 now); no existing program changes behaviour;
deterministic on the three architectures.

**Scale/Scope**: about 150 lines of C, 150 of vo, 350 of test script, 12 golden cases, about 20 loops
moved in `std/` and `examples/`.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. One form per construct | Adds a form for a loop that needs both parts; W0403 with its fix moves both old patterns to it, and `std/` and `examples/` are moved. `by` is not added. | Pass |
| II. Machine-readable feedback | E0301 reused with a fix; W0403 in the catalog with an example and a fix. | Pass |
| III. Determinism and defined behaviour | Changing a map in its loop is defined (contracts/iteration.md) and tested against a model on three architectures. | Pass |
| IV. Test-first | Golden cases and `tests/iteration.py` before the code they cover. | Pass |
| V. Minimal surface, reference budget | One instruction, one warning, no builtin; helpers in the library; the reference stays under budget by replacing text. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/046-iteration/
├── plan.md
├── research.md          # R1–R9
├── data-model.md
├── quickstart.md
├── contracts/
│   └── iteration.md     # syntax, meaning, diagnostics, std.lists
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/parse.c, src/check.c, src/compile.c, src/vm.c, src/vo.h, src/fmt.c, src/check_structure.c, src/rt_map.c
std/lists.vo (new), std/*.vo and examples/ (moved loops), Makefile, src/doc.c, tools/quick.sh
editors/tree-sitter-vo/
docs/reference.md, docs/errors.md, site/docs/guide/, CHANGELOG.md, docs/roadmap.md
tests/iteration.py (new), tests/cases/
```

**Structure Decision**: changes in place; one new library file and one new test script.

## Order of work

1. Golden cases and `tests/iteration.py` written and failing.
2. Parser, flag, `FOR_RANGE`; formatter; checker (types, E0301).
3. Compiler and VM: lists and deques; then maps (`OPC_SNAPSHOT`, `OPC_FOR_VALUE`).
4. W0403 and its fix; the catalog.
5. Grammars; the other tools' cases.
6. `std.lists`.
7. Move `std/` and `examples/`; reference and guide; three architectures; `make test`; results.

## Complexity Tracking

Nothing beyond the constitution's principles.

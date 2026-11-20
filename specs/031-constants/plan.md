# Implementation Plan: Constants

**Branch**: `031-constants` (work on `main`, as for specs 001–030) | **Date**: 2026-11-20 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/031-constants/spec.md`

## Summary

A top-level item `const name = expr;` (optionally typed and exported) whose value the checker works out
from literals, other constants and operators (research R1, R3, R4). Where a constant is used, the checker
folds it into the syntax tree as a literal (R2), so the compiler, the effects machinery and every check
that reads a literal work on it unchanged; the positions that take literals accept constant expressions
(R5). No new instruction, no run-time state.

| Part | File | What |
|---|---|---|
| Parsing | `src/parse.c`, `src/vo.h` | the `const` item (`N_CONST`), `export const`; expressions where scopes and annotation arguments took string tokens; E0122 for `const` as a statement |
| Evaluation | `src/check_const.c` (new), `src/vo.h`, `src/vm.c` | `const_eval`, `const_fold`; the integer arithmetic helpers shared with the VM; E0212, E0213, E0332, E0333 |
| Checking | `src/check.c`, `src/check_expr.c` | `SYM_CONST`, the evaluation pass, identifiers folded to literals with `NF_CONST`, E0208 wording, W0202 |
| Literal positions | `src/check_effects.c`, `src/check_annot.c`, `src/check_format.c`, `src/check_regex.c`, `src/check_sqlite.c`, `src/check_web.c` | fold before the kind test; messages "a literal or a constant"; the E0504 note |
| Imports | `src/loader.c` | `file_defines` learns `const NAME` (E0204) |
| Formatting | `src/fmt.c` | the `const` item; scope order |
| Tools | `src/query.c`, `src/edit.c`, `src/doc.c`, `src/mutate.c`, `src/rt_test.c` | refs, rename, type-at with the value, impact, doc; no mutation of folded constants; constants among a failing check's operands |
| Docs | `docs/reference.md`, `docs/errors.md`, `site/docs/guide/`, `docs/roadmap.md`, `README.md` | R12 |
| Examples | `examples/*` | directories and repeated literals as constants |
| Tests | `tests/cases/const/`, `tests/cases/fmt/`, `tests/cases/query/`, `tests/const_equiv.py`, `tests/const_records.py` | R13 |

## Technical Context

**Language/Version**: C (C11, freestanding) for the toolchain; vo for examples and the standard library.

**Primary Dependencies**: none.

**Storage**: none. The effects record's format and the JSON schema version (1) are unchanged; `type_at`
gains an optional `value` field and `doc` records a new `item` value, `"const"`.

**Testing**: golden cases (written first, failing), `apply_fixes.py`, `catalog.py`, `ref_examples.py`,
`site_examples.py`, `doc_conformance.py`, `fmt_corpus.py`, `fuzz.py`, two new scripts
(`tests/const_equiv.py`, `tests/const_records.py`); `make quick AREA=front,query,testing,docs` between
edits, `make test` once per change, `make gate` before the results are written.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64, byte-identical.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: `vo check` of a 5,000-line program within 5% of today (SC-006); no run-time cost: a
use compiles to the instruction a literal compiles to.

**Constraints**: one form (`const name = expr;`); no new reserved word; reference ≤ 15,000 tokens (+≤ 450
here, from about 14,017); programs without constants and their records byte-identical (SC-002); schema 1.

**Scale/Scope**: about 650 lines of C (parse 90, `check_const.c` 220, checker and literal positions 150,
fmt 40, tools 150), six catalog entries, five examples, two test scripts.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One form, shaped like `let`; the item starts with a word, as every item does. No implicit conversion in constant expressions; no shadowing (E0203); naming by the existing rule. One rule for literal positions ("a literal or a constant expression") instead of a list. `const` is not reserved, so no program changes meaning. | Pass |
| II. Machine-readable feedback | Six new codes with span, text, message and catalog entries; fixes where one correction exists (E0122, W0202, and the reused E0207, E0204, E0209); no code reused or redefined (E0315, E0321, E0324, E0330 keep their meaning, wording widened). Cascades suppressed: a constant in error is an error value. Schema 1: one optional field added. | Pass |
| III. Determinism | Values are worked out at check time with the VM's arithmetic through shared helpers; floats with the same operations and flags; nothing depends on the machine or the order of files beyond the dependency order already fixed. Overflow and division by zero in a constant are check errors, never silent. | Pass |
| IV. Test-first | A golden case per scenario and per code before the code; every new reference and guide example executed; the equivalence script compares constants with literals at every position. | Pass |
| V. Single source of truth, minimal surface | Reference, catalog and contracts change together; +≤ 450 tokens. Left out on purpose (YAGNI): calls in values, lists/records/variants as constants, constants inside functions, generic constants. One new source file, justified below. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/031-constants/
├── plan.md
├── research.md          # R1–R13
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── syntax.md        # the const item, constant expressions, literal positions, canonical form
│   ├── diagnostics.md   # new codes, reused codes, reworded messages, fixes
│   └── tools.md         # query, edit, impact, doc, type-at JSON, reports with values
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/check_const.c (new), src/parse.c, src/check.c, src/check_expr.c, src/check_effects.c,
src/check_annot.c, src/check_format.c, src/check_regex.c, src/check_sqlite.c, src/check_web.c,
src/loader.c, src/fmt.c, src/query.c, src/edit.c, src/doc.c, src/mutate.c, src/rt_test.c, src/vm.c,
src/vo.h
docs/reference.md, docs/errors.md, docs/roadmap.md, README.md, site/docs/guide/
examples/expenses, examples/notes-api, examples/helpdesk, examples/release-check, examples/status-board
tests/cases/const/, tests/cases/fmt/, tests/cases/query/, tests/const_equiv.py, tests/const_records.py
```

**Structure Decision**: one new file, `src/check_const.c`, for evaluation and folding: it is a unit with
two entry points used from six files, and `check_expr.c` is already over 1,100 lines. Everything else goes
where its neighbours are (items in `parse.c` and `check.c`, tools beside the code for effect sets).

## Order of work

Each step ends with its golden cases passing and `make quick`; the steps follow the spec's priorities, and
the feature is usable after step 3.

1. **The item and plain use (US1)**: parse `const`, `SYM_CONST`, `check_const.c` with literals, constants
   and operators, the evaluation pass, identifiers folded; `vo fmt` for the item. Shared integer helpers
   moved out of `vm.c` first, with the suite unchanged.
2. **Mistakes (US2)**: E0212, E0213, E0332, E0333, E0122, E0208 wording, W0202; naming, duplicates and
   shadowing cases; catalog entries.
3. **Files (US3)**: import, export, E0204 and E0209 cases, packages, `vo doc`.
4. **Literal positions (US4)**: scopes (parser and `fx_build_set`), annotations, then `format`, `regex`,
   `sql` and routes, then E0503 targets; reworded messages; `tests/const_equiv.py`.
5. **Review and tools (US5)**: the E0504 note, `query refs`/`type-at`, `edit rename`, `impact`, operands in
   failing checks, mutation sites.
6. **Docs and examples (US6)**: reference, guide, roadmap, README; examples and `tests/const_records.py`.
7. `make test` on three platforms, `make gate`, `results.md`; SC-008's agent run scheduled with the user.

## Complexity Tracking

No constitution violations.

# Implementation Plan: Application Structure Standards

**Branch**: `027-structure-standards` (work on `main`, as for specs 001–026) | **Date**: 2026-11-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/027-structure-standards/spec.md`

## Summary

Five structure rules in `vo check`, warnings W0701–W0705, computed from what the checker already knows
(statements, the effects each call requires, the call graph), plus W0706 for an `@allow` that silences
nothing. `@allow("W07NN", "reason")`, a second annotation beside `@spec`, accepts a warning on one
declaration and is counted in the summary. `vo test ENTRY` runs the tests of every program file the
entry loads, so tests can live next to their code. A reference section (≤ 250 tokens) and a site guide,
"Structuring a program", state the standard with a before-and-after example.

Thresholds come from measuring the examples, std, golden cases and the two agent-written programs
(research R1–R5): effectful functions with 15 or more effect-free statements (W0701), files over 400
code lines (W0702), functions over 50 statements (W0703), 6 duplicated sibling statements (W0704).

| Part | File | What |
|---|---|---|
| Rules | `src/check_structure.c` (new) | the five measures and W0706, run after a clean check in `vo check` mode; related locations; sorting with the other diagnostics |
| Effect per statement | `src/check_effects.c` | a per-statement "requires effects" bit recorded while effects are computed (research R4) |
| Annotations | `src/check.c`, `src/parse.c` | `@allow`: arguments (E0121), placement (E0117), the name (E0116), the accepted set per declaration |
| Summary | `src/diag.c` | `(N accepted)` in the text summary, `"accepted"` in JSON |
| `vo test` | `src/main.c`, `src/testrun.c` | the program files of every entry, each once, for running, listing, selection, coverage and mutation |
| Catalog | `docs/errors.md` | W0701–W0706 and E0121 entries; E0116 and E0117 updated |
| Reference | `docs/reference.md` | a "Structure" section; `vo test` paragraph; annotations paragraph |
| Guide | `site/docs/guide/structure.md` (new), site nav | the standard with a worked example |
| Std | `std/json.vo` | two `@allow` with reasons (research R9) |

## Technical Context

**Language/Version**: C (C11, freestanding) for the checker and CLI; vo for examples and std.

**Primary Dependencies**: none new.

**Storage**: none.

**Testing**:
- golden cases `tests/cases/structure/` (`.cmd` `check --json {}`): each rule triggered and not
  triggered at its threshold, effect-only duplicates, three copies, tests excluded, std and packages
  excluded, programs with errors, `@allow` accepted, unused, misplaced, with an empty reason and a
  non-structure code;
- golden cases `tests/cases/test/` for `vo test main.vo` running an imported file's tests, `--list`,
  `--test` across files, and no double run with `vo test .`;
- `tests/structure_corpus.py`: the rules on the examples (no warning), the site's and reference's examples
  (no warning), std (only accepted ones), seeded violations (every one reported, SC-003) and 10
  well-structured programs (none reported);
- `tests/structure_speed.py`: `vo check` time with and without the rules on the largest example and a
  5,000-line generated program (SC-004, `make gate`);
- `tests/apply_fixes.py` (W0706), `tests/catalog.py`, `tests/ref_examples.py`, `tests/site_examples.py`;
- `make quick` between edits, `make test` once per change.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64 (the rules are platform-independent).

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: `vo check` at most 10% slower with the rules (SC-004).

**Constraints**: deterministic diagnostics (FR-009); reference +250 tokens at most (SC-005); no
configuration of thresholds (FR-008).

**Scale/Scope**: about 600 lines of C, 6 catalog entries, one guide page.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One syntactic form: `@allow("CODE", "reason")`, the annotation form `@spec` already uses. No optional spellings, no configuration. | Pass |
| II. Machine-readable feedback | Seven new codes (W0701–W0706, E0121), none reused or redefined; every warning has span, text, one-sentence message, related locations; a fix where exactly one correction is unambiguous (W0706). The summary gains a field (`accepted`, present when not zero), no field removed. | Pass |
| III. Determinism | Rules are pure functions of the source; sorting as for all diagnostics; no thresholds from the environment. | Pass |
| IV. Test-first through the CLI | Each code gets golden cases before its implementation; `apply_fixes.py` covers W0706. | Pass |
| V. Single source of truth, minimal surface | Catalog, reference and contracts updated together; the reference grows by at most 250 tokens; no extract-function edit (YAGNI, a later feature). | Pass |

Re-checked after Phase 1: unchanged. The `vo test` behavior change is documented in `contracts/cli.md` and
the reference in the same change.

## Project Structure

### Documentation (this feature)

```text
specs/027-structure-standards/
├── plan.md              # this file
├── research.md          # thresholds and decisions (R1–R12)
├── data-model.md        # rules, statements, runs, annotations
├── quickstart.md        # validation scenarios
├── contracts/
│   ├── codes.md         # W0701–W0706, E0121; E0116, E0117 changes
│   ├── diagnostics.md   # messages, spans, related locations, fix
│   └── cli.md           # vo check summary, vo test program files, @allow
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/check_structure.c        # new: the rules
src/check_effects.c          # per-statement effect bit
src/check.c, src/parse.c     # @allow
src/diag.c                   # accepted count in the summary
src/main.c, src/testrun.c    # vo test: every program file's tests
docs/errors.md, docs/reference.md
site/docs/guide/structure.md, site/mkdocs.yml
std/json.vo
tests/cases/structure/, tests/cases/test/
tests/structure_corpus.py, tests/structure_speed.py
Makefile                     # test and gate steps; quick area `structure`
```

**Structure Decision**: one new checker file for the rules, next to the other per-feature checkers
(`check_regex.c`, `check_sqlite.c`); everything else is small changes to existing files.

## Complexity Tracking

No constitution violations.

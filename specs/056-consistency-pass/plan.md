# Implementation Plan: Consistency Pass

**Branch**: `056-consistency-pass` (work on `main`, as for earlier specs) | **Date**: 2026-11-25 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/056-consistency-pass/spec.md`

## Summary

Group 3 of the inconsistency audit, as a breaking minor version (0.2.0). One table of library changes
drives a new diagnostic, E0214 `library_changed`, and its fixes (R1). These are the changes:

- **Name clashes:** renames.
- **"Nothing found":** lookups return `Option`.
- **Conventions:** argument order, time limits, units, typed reads.
- **Removed duplicates.**

A use of the old form gets E0214 and a fix that keeps the program's behavior, so applying fixes migrates
it. The language changes reuse existing codes:
- builtins cannot be shadowed (E0203);
- `bool` and variants are ordered (no diagnostic: more programs check);
- `[]` takes an expected type;
- `Shape.empty()` is E0411;
- literal spellings are E0105;
- `vo fmt` writes effect arguments with `uses`.

Messages follow one style (R13). Everything std, the examples, the packages, the site and the tests use
is migrated by the same fixes, and a migration script proves it on the pre-spec copies (R15).

| Part | Files | What |
|---|---|---|
| Change table, E0214 | `src/libchange.c` (new), `src/loader.c` (imports), `src/check_expr.c` (uses, retyped calls), `src/vo.h` | R1 |
| Shadowing | `src/check.c` (`new_local`) | R4 |
| Order | `src/check_generic.c` (`ty_ordered_d`), `src/rt_coll.c` (`value_cmp`), `src/check_expr.c` (ordering operators on variants and bool) | R5 |
| `[]`, `S.empty()` | `src/check_expr.c` (`check_list`), `src/parse.c`, `src/check_expr.c` (variant cases) | R12 |
| Literals | `src/lex.c` | R12 |
| Effect arguments | `src/fmt.c`, `src/check_generic.c` (named set without `uses`) | R12 |
| Messages | `src/*.c` (texts only); `src/check_annot.c` (E0119 for every annotation); `src/build.c` (T0002) | R13 |
| Library | `std/lists.vo`, `json.vo`, `tcp.vo`, `metrics.vo`, `sqlite.vo`, `http.vo`, `random.vo`, `regex.vo`, `collections.vo`, `workers.vo`, `web.vo`, `time.vo`, `security.vo`, `log.vo` (uses), `src/tcp.c` and `src/workers.c` (limits), `src/sqlite_*.c` (prefix) | R2 to R11 |
| Migration of the repository | `examples/`, `packages/postgres/`, `tests/cases/`, `site/docs/` | the fixes themselves |
| Docs | `docs/reference.md`, `docs/errors.md` (E0214; E0203, E0105, E0119, E0306, E0329, E0411 updates), `CHANGELOG.md`, `docs/roadmap.md`, `docs/stability.md` (0.2.0 note), the audit file, the guide pages naming changed items | R14 |
| Tests | `tests/cases/library/` (new), `tests/cases/check/shadow_builtin*`, `tests/cases/run/order_*`, `tests/std_names.py` (new), `tests/libchange_rows.py` (new: every change-table row), `tests/migrate_056.py` (new), `tests/message_style.py` (new), `tests/migrate.py` (3 rounds, E0214) | SC-001 to SC-007 |

## Technical Context

**Language/Version**: C11 freestanding (toolchain); vo (std, examples, packages); Python 3 (tests).

**Primary Dependencies**: none new.

**Storage**: N/A.

**Testing**:
- Golden cases come first (constitution IV): `tests/cases/library/` and the language cases.
- `tests/migrate_056.py`, `tests/std_names.py` and `tests/message_style.py`.
- `make quick` between edits, `make test` once at the end, on three architectures for the order cases.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain and standard library.

**Performance Goals**: no measurable change (the variant order adds a branch to `value_cmp` only for
variant types).

**Constraints**:
- No new syntax.
- Codes are only added (E0214); no code changes meaning.
- The JSON schema is unchanged.
- The reference stays within 15,000 tokens: about 13 left, and the additions are estimated at +30 to
  +50 tokens. They are measured, then paid for by moving detail to the site (R14).
- The frozen v1 corpus is not edited.

**Scale/Scope**:
- about 47 table rows (contracts/api.md: 16 renames, 4 order swaps, 20 lookups, 2 removals, 5 units);
- about 150 import lines to migrate across examples, tests, std, site and packages (counted for each
  renamed item in research);
- about 20 language and message changes;
- C: about 300 lines (the table and E0214 with its fixes being most of it);
- vo: the std edits;
- tests: about 40 golden cases.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | Removes the guesses that cost an agent a round trip: clashing imports, three failure forms, hidden builtins, unsortable keys, two argument orders. One way per job is restored (duplicates removed). | Pass |
| II. Machine-readable feedback | One new stable code (E0214) with a fix for every change that has one; existing codes extended, none reused for another meaning; catalog entries updated. | Pass |
| III. Determinism | The variant order is defined by declaration; no new nondeterminism; order cases checked on three architectures. | Pass |
| IV. Test-first | Golden cases for each change before the code; the migration script on the pre-spec copies. | Pass |
| V. Minimal surface | No syntax; fewer names (duplicates removed); conventions stated once in the reference; cost measured and paid for. | Pass |
| VI. Backend network services | Time limits, headers, the database and TCP read alike, so service code is easier to write and review. | Pass |

Re-checked after Phase 1: unchanged. Breaking changes are allowed before 1.0 under
`docs/stability.md`'s rules, which FR-001 to FR-003 implement.

## Project Structure

### Documentation (this feature)

```text
specs/056-consistency-pass/
├── plan.md
├── research.md          # R1–R15
├── data-model.md        # the library change table, ordered types, limits, units
├── quickstart.md
├── contracts/
│   └── api.md           # every change with its fix; the diagnostics
└── tasks.md             # /speckit-tasks
```

**Structure Decision**: one new C file (`src/libchange.c`) holds the change table and builds E0214 and
its fixes, so the table is the single source the catalog check and the changelog also read. Every other
change is local to the file that owns the behavior.

## Complexity Tracking

| Item | Why needed | Simpler alternative rejected because |
|---|---|---|
| A table-driven E0214 with fixes of five kinds (rename, remove, swap, option, unit) | `docs/stability.md`: a breaking change needs a diagnostic and, where one correction exists, a fix | A note in the changelog only: programs and agents would fail with E0205 and E0301 and no way forward |

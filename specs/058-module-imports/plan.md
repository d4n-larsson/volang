# Implementation Plan: Module Imports

**Branch**: `058-module-imports` (work on `main`, as for specs 001–057) | **Date**: 2026-11-25 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/058-module-imports/spec.md`

## Summary

`import std.json;` binds the module name `json`, and its items are used as `json.parse_json`. The parser
folds `m.n` into one name whenever `m` is a module of the file, which it knows because imports precede
items (research R1); the loader binds each module's items under their qualified names (R2), so every
existing lookup, message and check works unchanged. Programs written for 0.3 still check, with one E0215
per file whose single-edit fix rewrites the imports and every use (R6). Types in messages and fixes print
in the importing file's terms (R7). Ships as 0.4.0 with the repository migrated by its own fixes (R11).

| Part | File | What |
|---|---|---|
| Parsing | `src/parse.c`, `src/vo.h` | module-name set from the imports; qualified names in expressions, types, `new`, cases, `uses`; `NF_QUAL` |
| Loading and binding | `src/loader.c`, `src/packages.c` | path meaning and old-form detection (R3); `SYM_MODULE`; items under `m.n`; E0202, E0205 wording |
| Checking | `src/check.c`, `src/check_expr.c`, `src/check_effects.c` | E0201 qualify fix and messages; E0203 for module names; W0203; `ty_name_in` (R7) |
| Migration | `src/loader.c`, `src/libchange.c`, `src/diag.c` | E0215 and its region fix; E0214 at qualified uses; text summary of long fixes |
| Missing imports | `src/loader.c` (`find_import`, `import_insert_at`) | E0204 suggests module imports |
| Tools | `src/query.c`, `src/edit.c`, `src/lsp.c`, `src/doc.c` | qualified targets; rename of the item part; hover/definition on the module part; `use:` line |
| Formatting | `src/fmt.c` | qualified names printed from the folded text (imports already sorted) |
| Editors | `editors/` | qualified types and calls in three grammars |
| Migration of the repo | `std/`, `packages/postgres/`, `examples/`, `tools/vo_fix.vo`, `site/docs/`, `docs/` | by fixes (R11) |
| Docs | `docs/reference.md`, `docs/errors.md`, `docs/stability.md`, `CHANGELOG.md`, `README.md`, `site/docs/guide/` | R12, R14 |
| Tests | `tests/cases/imports/` (new), `tests/cases/imports_v03/` (new), `tests/migrate_058.py` (new), `tools/vo_fix.vo` (4 rounds) | R11 |

## Technical Context

**Language/Version**: C (C11, freestanding) for the toolchain; vo for std, packages, examples and tools.

**Primary Dependencies**: none.

**Storage**: none. `vo.effects`, `vo.packages` and recordings are unchanged; JSON schema 1 gains one
optional field (`use` in `vo doc --json`).

**Testing**: golden cases written first and failing (`tests/cases/imports/`, `tests/cases/imports_v03/`);
`apply_fixes.py`, `catalog.py`, `ref_examples.py`, `site_examples.py`, `doc_conformance.py`,
`fmt_corpus.py`, `fuzz.py`, `editors/check.js`, `migrate.py`, `migrate_056.py`, new `migrate_058.py`;
`make quick AREA=front,query,docs` between edits, `make test` once per change, `make gate` before results.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64, byte-identical.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: `vo check` of the largest example and of the generated 8,000-item program within 5%
of today (SC-006). No run-time change: bytecode is identical.

**Constraints**: one form (per-name imports are rejected with a fix, not kept); no `import ... as`; no
shadowing; schema 1 (one edit per fix, R6); reference ≤ 15,000 tokens with ~13 to spare today (R12).

**Scale/Scope**: about 700 lines of C (parser 120, loader and binding 150, migration fix 180, checker 80,
`ty_name_in` 40, tools 130); 2 new codes; 690 golden-case sources and every vo file in the repository
migrated by fixes.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One form: per-name imports become an error with a fix, not a second spelling. Every statement still starts with a keyword or is a call (`web.on_get(...)` is a call). No shadowing: module names join the rule (E0203). No implicit behavior: a name's module is visible at every use. | Pass |
| II. Machine-readable feedback | Two new codes (E0215, W0203) with catalog entries and fixes; widened codes keep their meaning; no code reused. One edit per fix (schema 1); the long fix is summarized in text only. Cascades: an old import still binds its item, so the rest of the file checks. | Pass |
| III. Determinism | No run-time change. Fix text is built from the reference log in source order; output identical on three platforms. | Pass |
| IV. Test-first | Golden cases per scenario and per code before the code; every reference and guide example executed; `migrate_058.py` proves the 690 migrated cases keep their meaning. | Pass |
| V. Single source, minimal surface | Reference, catalog and contracts change together; the reference size is stated (R12). Left out (YAGNI): aliases, re-exports, directory modules, completion in the LSP. | Pass |
| VI. Backend network services | **Review**: a service file's dependencies shrink from 48 lines to one per module, and each call says which module it reaches (`sqlite.exec`, `web.request`), so a reviewer sees in the body what touches the database or the web layer. **Write**: an agent adds no import line per new name and stops hitting E0204 for each one. | Pass |

Re-checked after Phase 1: unchanged. The one constitutional tension, "explicit verbosity preferred",
favors this change: qualified names are more explicit at the use site than bare imported names.

## Project Structure

### Documentation (this feature)

```text
specs/058-module-imports/
├── plan.md
├── research.md          # R1–R14
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── syntax.md        # grammar, paths, canonical form
│   ├── diagnostics.md   # E0215, W0203, widened codes, fixes
│   └── tools.md         # query/edit targets, lsp, doc, fmt
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/parse.c, src/loader.c, src/packages.c, src/check.c, src/check_expr.c, src/check_effects.c,
src/libchange.c, src/diag.c, src/query.c, src/edit.c, src/lsp.c, src/doc.c, src/fmt.c, src/vo.h
editors/vscode, editors/tree-sitter-vo, editors/vim
std/*.vo, packages/postgres/*.vo, examples/*, tools/vo_fix.vo
docs/reference.md, docs/errors.md, docs/stability.md, CHANGELOG.md, README.md, site/docs/
tests/cases/imports/, tests/cases/imports_v03/, tests/migrate_058.py
```

**Structure Decision**: no new source file. Parsing goes in `parse.c`, binding and the migration fix in
`loader.c` beside `bind_imports` and `find_import`, which already build import text and insertion points.

## Order of work

Each step ends with its golden cases passing and `make quick`; the feature is usable after step 3.

1. **Parser (R1)**: module-name set, qualified names in every position; parse-only golden cases.
2. **Binding (R2, R3)**: new path meaning, `SYM_MODULE`, items under `m.n`, old form still binding;
   E0202, E0203, E0205, E0209 cases.
3. **Checker (R4, R5, R7)**: E0201 fixes and messages, E0204, W0203, `ty_name_in`; the new form works end
   to end (US1).
4. **Migration (R6)**: E0215 and its region fix, E0214 at qualified uses, the text summary; `vo_fix` to 4
   rounds; `tests/cases/imports_v03/` (US2).
5. **Repository**: migrate `std/`, `packages/postgres`, `tools/vo_fix.vo`, examples, then golden cases
   with `migrate_058.py`; `make test`.
6. **Tools (R8)**: query/edit targets, rename, lsp, doc; editor grammars (US3).
7. **Docs (R12, R14)**: reference, catalog, guide, stability, changelog, README; budget check.
8. `make test` on three platforms, `make gate`, check-time measurements, `results.md`; migrate the
   voledger and vositegen kits with the user.

## Complexity Tracking

No constitution violations.

| Choice | Simpler alternative | Why not |
|---|---|---|
| One-edit region fix for E0215 (R6) | one fix per import and per use | the 50-diagnostic cap makes that 4+ rounds for a service file, and agents apply one edit per fix |
| `ty_name_in` (R7) | keep printing bare type names | fixes would insert `Json` where the file must write `json.Json`, breaking "every fix checks clean" |

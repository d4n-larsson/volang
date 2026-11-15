---

description: "Task list for vo-lang spec 010: visibility with `export`"
---

# Tasks: Visibility with `export`

**Input**: Design documents from `specs/010-export-visibility/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R8), data-model.md, contracts/language.md,
contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution, Principle IV): every story starts with golden cases that fail on the
current binary; both new codes get a catalog entry and a case.

**Organization**: one phase per user story in spec priority order (US1, US2 P1; US3 P2; US4 P3). US2 and
US3 need the keyword from US1.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- C with the strict warning set on all three architectures (`make lint`); declarations in `src/vo.h`.
- Golden cases: `tests/cases/<dir>/<name>.vo` (multi-file cases as a directory with `main.vo`) and
  `.out`/`.err`/`.code`/`.cmd`; outputs are accepted only after checking them against
  contracts/language.md.
- **Regression rule:** single-file programs keep byte-identical results; a changed expected output of an
  existing multi-file case is accepted only when its diff is the migration (`export ` on a declaration
  line, spans on that line 7 columns later).

---

## Phase 1: Setup

- [X] T001 Create `specs/010-export-visibility/results.md` (SC table SC-001…SC-006 "open"; sections per
  story, Deviations, Follow-ups).

---

## Phase 2: User Story 1 - A file keeps its helpers to itself (Priority: P1)

**Goal:** the keyword, E0209 and E0210 (FR-001-FR-004, research R1-R4).

**Independent test:** `tests/run.sh tests/cases/multi tests/cases/parse` passes, including the new cases;
`python3 tests/catalog.py` and `python3 tests/apply_fixes.py` pass.

- [X] T002 [P] [US1] Golden cases in `tests/cases/multi/`: `export_ok/` (exported fn, record and variant
  imported, built, matched; a private helper used by the same file's function and test: runs),
  `not_exported/` (E0209 with the fix; `--json` variant `not_exported_json`), `not_exported_twice/` (two
  files import the same private item: one E0209 with a related note), `private_type/` (E0210 for a
  parameter, a result and a record field; `--json` variant), and in `tests/cases/parse/`:
  `export_misplaced.vo` (`export test`, `export import`, `export` in a block, `export export fn`),
  `export_as_name.vo` (a local named `export`). Confirm they fail now.
- [X] T003 [US1] `src/vo.h` `T_EXPORT 47`, `T_KW_LAST 47`, `NF_EXPORT 512`; `src/lex.c` `"export"`;
  `src/parse.c` `parse_item`: `export` + `fn`/`record`/`variant` sets `NF_EXPORT` and starts the span at
  `export`, else the contract's syntax error; `src/fmt.c` prints `export `.
- [X] T004 [US1] `src/loader.c` `bind_imports`: E0209 (message per contracts/language.md; fix inserting
  `export ` at the declaration's start unless the file is a library or package file; still binds the
  name); one diagnostic per declaration with related notes (R4), shared with E0210.
- [X] T005 [US1] `src/check.c`: E0210 after `resolve_item` for exported items (parameters, result, fields,
  case payloads, through `list`/`map`/`result`/function types/type arguments), fix exporting the type.
- [X] T006 [US1] `docs/errors.md` E0209 `not_exported`, E0210 `private_type_exported` (example, fix);
  `tests/catalog.py` reads 010's `contracts/codes.md`; `docs/reference.md`: `export` in the keyword list,
  the Programs-in-several-files paragraph and its example (`export` in `geometry.vo`).

---

## Phase 3: User Story 2 - The standard library shows only its interface (Priority: P1)

**Goal:** `std/` exports its documented items; packages follow the rule; `vo doc` lists exported items
(FR-005-FR-007, R5, R6).

**Independent test:** `tests/run.sh tests/cases/doc tests/cases/packages tests/cases/json` and
`python3 tests/doc_conformance.py`, `python3 tests/packages_check.py` pass.

- [X] T007 [P] [US2] Golden cases: `tests/cases/check/std_internal.vo` (`import std.json.parse_value;` and
  `import std.http.exchange;`: E0209 without a fix), a package case in `tests/cases/packages/` (exported
  item imports, private item E0209 without a fix; `vo doc PKG.module` lists only the exported items).
  Confirm they fail now.
- [X] T008 [US2] `std/*.vo`: `export` on every item with a doc comment (48); `src/doc.c` `doc_file` lists
  items with `NF_EXPORT` (a missing description prints empty); `tests/doc_conformance.py` checks exported
  = documented for `std/`; `tests/packages_check.py` and `tests/fuzz_inputs.py` package sources gain
  `export`; `vo doc` output for `std/` stays byte-identical.

---

## Phase 4: User Story 3 - Existing programs move to the rule (Priority: P2)

**Goal:** every multi-file program in the repository migrated by the fix's edit (FR-008, R7).

**Independent test:** `make test` passes; every entry program of `bench2/`, `tests/` and `examples/` has
no E0209/E0210.

- [X] T009 [US3] One-off migration (scratch script, not committed): add `export ` before each declaration
  named by a sibling or package import in `bench2/`, `tests/` and `examples/`; check every entry program
  for E0209/E0210; re-accept golden outputs only where the diff is the migration (regression rule).
- [X] T010 [US3] `tests/speed.py --bench2`: strip `export ` from the sources for the assembly oracle;
  note in results that `bench2/` changed once (spec Assumptions).

---

## Phase 5: User Story 4 - Tools and editors know the keyword (Priority: P3)

**Goal:** formatter, queries, edits and the IntelliJ plugin (FR-009, FR-011, R8).

**Independent test:** `tests/run.sh tests/cases/fmt tests/cases/query tests/cases/edit`; `./gradlew test`
in `../volang-intellij-plugin`.

- [X] T011 [P] [US4] Golden cases: `tests/cases/fmt/export.vo` (spacing normalized to `export fn`),
  `tests/cases/edit/rename_exported/` (renaming an exported function keeps `export` and renames imports),
  `tests/cases/query/refs_exported/` (refs list the declaration and the imports).
- [X] T012 [US4] IntelliJ plugin: `export` in `VoTokens.KEYWORDS`, a lexer test, version 0.1.4, build.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T013 Measure the reference growth (≤ 250 tokens, SC-005); full gate: `make gate`,
  `make ARCH=aarch64 test`, `make ARCH=riscv64 test`, `make instrumented`, `make diff-asm COUNT=10000`;
  run `prop-eval`, `refactor-eval`, `stdlib-eval`; fill `results.md`.

---

## Dependencies & Execution Order

- Setup → US1 → (US2, US3, US4 in any order) → Polish. US3 needs US1's keyword; US2 needs US1's E0209.
- Within a story: cases first (T002, T007, T011), then implementation.

### Parallel opportunities

- T002, T007 and T011 write different case directories; T012 is in another repository.

## Implementation Strategy

MVP: US1 (keyword, E0209, E0210) and US2 (library privacy); then the migration, the tools, the gate.

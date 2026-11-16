---

description: "Task list for vo-lang spec 012: spec annotations on tests"
---

# Tasks: Spec Annotations on Tests

**Input**: Design documents from `specs/012-spec-annotations/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R6), data-model.md, contracts/language.md,
contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution, Principle IV): every story starts with golden cases that fail on the
current binary; every new code gets a catalog entry and a case.

**Organization**: Setup, the shared foundation (lexing, parsing, storing annotations), then one phase per
user story in spec order (US1-US3 P1, US4 P2), then polish.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- C with the strict warning set on all three architectures (`make lint`); declarations in `src/vo.h`.
- Golden cases in `tests/cases/spec/` (`.vo` with `.out`/`.err`/`.code`/`.cmd`), checked by hand against
  contracts/language.md before they are accepted.
- **Regression rule:** programs without annotations keep byte-identical results.

---

## Phase 1: Setup

- [X] T001 Create `specs/012-spec-annotations/results.md` (SC table SC-001…SC-005 "open"; sections per
  story, Deviations, Follow-ups); add `012-spec-annotations` to the spec list in `tests/catalog.py`.

---

## Phase 2: Foundational (blocks every story)

- [X] T002 `src/vo.h`: token `T_AT` (`@`) and its spelling in `src/parse.c`'s token table; `Fx` gains
  `specs` (a list of annotations: name, IDs, span), `FX_SIZE` 136 with its `CHECK_OFFSET`; `src/lex.c`
  lexes `@`. `src/parse.c` `parse_item`: any number of `@NAME(STRING, ...)` in front of an item, kept on the
  item's side record (`item_fx`) without changing the item's span; an annotation followed by a comment or
  the end of the file is E0117; inside a block `@` is the usual statement error. Every existing case
  unchanged.

---

## Phase 3: User Story 1 - A test says which requirements it proves (Priority: P1)

**Goal:** `@spec` accepted on tests and properties; every mistake reported with its code and fix (FR-001-
FR-004, research R1, R2).

**Independent test:** `tests/run.sh tests/cases/spec` passes; `python3 tests/catalog.py` and
`python3 tests/apply_fixes.py` pass.

- [X] T003 [P] [US1] Golden cases in `tests/cases/spec/`: `annotated.vo` (`@spec` on a test and a prop with
  several IDs: checks clean, `vo test` output identical to the same file without annotations),
  `unknown.vo` (`@spce`: E0116 with the fix `spec`; `@deprecated`: E0116 without a fix),
  `bad_ids.vo` (`"FR 2"`, `""`, `"a//b"`, `@spec()`: E0118 each), `misplaced.vo` (`@spec` on a `fn`, a
  `record`, an `import`: E0117), `misplaced_end.vo` (`@spec("A")` at the end of the file: E0117),
  `repeated.vo` (two `@spec` on one test: E0119 with the merged fix; `@spec("A", "A")`: W0101 with the fix),
  `all_mistakes.vo` (every mistake above in one file: one check reports all of them, SC-003), each
  `--json`. Confirm they fail now.
- [X] T004 [US1] `src/check.c`: validate annotations for every item: E0116 (closest of the known names by
  edit distance ≤ 2), E0117 (item not `test`/`prop`), E0118 (ID not `PART(/PART)*`, PART
  `[A-Za-z0-9_-]+`; or no IDs), E0119 (merge fix: one annotation with all IDs in order), W0101 (fix: the
  annotation without the repeated IDs); all reported in one pass.
- [X] T005 [US1] `docs/errors.md`: E0116 `unknown_annotation`, E0117 `misplaced_annotation`, E0118
  `invalid_spec_id`, E0119 `repeated_annotation`, W0101 `repeated_spec_id` (examples, fixes);
  `docs/reference.md`: annotations and `@spec` (≤ 200 tokens with US2-US4's lines), the E01xx range
  (E0101 to E0119).

---

## Phase 4: User Story 2 - Test reports carry the requirement IDs (Priority: P1)

**Goal:** `spec` in test records, IDs on failure lines, the per-ID summary (FR-005, R3).

**Independent test:** `tests/run.sh tests/cases/spec` (the report cases) passes; every existing `test`
case unchanged.

- [X] T006 [P] [US2] Golden cases `tests/cases/spec/report.vo` (`.cmd` `test {}`: annotated tests passing
  and failing, an unannotated one: failure lines with IDs, the summary lines) and `report_json.vo`
  (`test --json {}`: `spec` fields sorted, `spec_result` records). Confirm they fail now.
- [X] T007 [US2] `src/testrun.c`: `"spec":[...]` (sorted) in annotated tests' records; ` [ID, ID]` at the
  end of a not-passing test's line; after the summary, one line (JSON: `spec_result` record) per ID,
  sorted, only when at least one test is annotated.

---

## Phase 5: User Story 3 - Which requirements are covered (Priority: P1)

**Goal:** `vo query specs ENTRY` (FR-006, FR-007, R4).

**Independent test:** `tests/run.sh tests/cases/query` passes.

- [X] T008 [P] [US3] Golden cases in `tests/cases/query/`: `specs` (a program of two files whose tests
  reference FR-001 twice, FR-002 and US1/AC1: text), `specs_json`, `specs_none` (no annotations: empty,
  exit 0), and a step case `specs_package/` (a package whose file has an annotated test: not listed; the
  program's own annotated test is). Confirm they fail now.
- [X] T009 [US3] `src/query.c`: the `specs` query (every file the entry loads except standard-library and
  package files; IDs sorted; per ID its tests in source order: name, kind, file, line); usage text and the
  reference's command list.

---

## Phase 6: User Story 4 - Formatting and editors (Priority: P2)

- [X] T010 [P] [US4] `tests/cases/fmt/annotations.vo` and `.expected.vo` (spacing, IDs sorted, the
  annotation moved to its own line, a doc comment kept above it).
- [X] T011 [US4] `src/fmt.c`: print each annotation on its own line above its item, IDs sorted.
- [X] T012 [US4] IntelliJ plugin (`../volang-intellij-plugin`): `@NAME` is one `ANNOTATION` token
  (`METADATA` color, Nord scheme entry), a lexer test, version 0.1.7, build.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T013 Annotate every test and property of the examples (SC-001: `examples/expenses/main.vo`,
  `examples/status-board/main.vo`, `examples/status-board/service/main.vo`, `examples/notes-api/main.vo`,
  17 in all) with requirement IDs from a short requirement list added to each README; `vo query specs` on
  each lists 100% of them with the right tests; a 10-requirement demonstration with 2 untested (SC-002) in
  results.
- [X] T014 Agent check (SC-004: 3 isolated runs annotating a small program's tests from a requirement
  list); reference growth (≤ 200 tokens, SC-005); the full gate (`make gate`, ARM64 and RISC-V suites,
  `make instrumented`, `make diff-asm COUNT=10000`); fill `results.md`.

---

## Dependencies & Execution Order

- Setup → Foundational (T002) → US1 → (US2, US3, US4) → Polish.
- Within a story: cases first, then implementation.

### Parallel opportunities

- T003, T006, T008, T010 write different case files; T012 is in another repository.

## Implementation Strategy

MVP: T002 and US1 (annotations accepted and validated); then the reports, the query, formatting, and the
examples and gate.

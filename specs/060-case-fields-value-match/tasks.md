---

description: "Task list for vo-lang spec 060: Variant Cases With Fields, and match on Values"
---

# Tasks: Variant Cases With Fields, and `match` on Values

**Input**: Design documents from `specs/060-case-fields-value-match/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R11), data-model.md, contracts/syntax.md,
contracts/diagnostics.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's golden cases land first and fail before its code.

**Organization**:
- Setup: test wiring, the results file, the codes table, baselines.
- Foundational: the arm and literal shapes both stories touch in the parser and in every `N_MATCH`
  walker (research R6).
- US2 (P1) `match` on `int` and `str`, done first (smaller, independent); US1 (P1) cases with fields.
- User Story 3 is dropped (research R8).
- Polish: tools, editors, repository migration, docs, version, three platforms.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: `tests/cases/value_match/<name>.vo` and `tests/cases/case_fields/<name>.vo`, with
  `.cmd` (`check --json {}` for diagnostics; none to run), `.out`, `.code`. Accept with
  `tests/run.sh --accept PATH` only after reading the diff; never accept an output before the catalog
  entry of its codes exists (the diagnostic `name` comes from the catalog).
- **Catalog**: each new code gets its entry in `docs/errors.md` (a broken and a fixed example) and its row
  in `specs/060-case-fields-value-match/contracts/codes.md`, in the task that implements it; the spec is
  listed in `tests/catalog.py` from T003 with only implemented codes in the table. Messages:
  `contracts/diagnostics.md`.
- **Fixes**: every fix's result must check clean: `python3 tests/apply_fixes.py` (its case list includes
  `value_match` and `case_fields` from T001).
- **No existing program changes meaning**: one-value cases, record payloads and variant matches are
  untouched; existing expected outputs change only where a task says so (E0345 replaces today's E0301
  for a non-matchable scrutinee; R4's printing for migrated payloads).
- **Reference budget**: `python3 tests/ref_examples.py` after every reference change; 11,538 words at
  the start, the maximum: word-neutral (R10).
- **Between edits**: `make quick AREA=match` (T001); `make test` once per change; never two suites at once.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: a new area `match` (cases `value_match case_fields check parse fmt run query
  edit doc json prop`; scripts `apply_fixes.py`, `catalog.py`, `ref_examples.py`, `fmt_corpus.py`,
  `lsp.py`, `doc_conformance.py`), listed among the known areas in the "unknown area" message;
  `tests/apply_fixes.py` lists `value_match` and `case_fields` among its case folders.
- [X] T002 [P] `specs/060-case-fields-value-match/results.md`: SC-001–SC-005 (pending; SC-002 dropped),
  Verification, Deviations (start with FR-005's printing and US3, research R4 and R8).
- [X] T003 [P] `specs/060-case-fields-value-match/contracts/codes.md` in the format of spec 059's (an
  empty table, rows added by the tasks that implement codes); `tests/catalog.py` lists spec 060.
- [X] T004 [P] Baselines in `results.md`: the reference's word count (11,538), `vo check` times (best of
  5) of `examples/notes-api/main.vo`, `examples/expenses/main.vo` and the 8,000-item program of
  `tests/perf.sh`; the JSON written for `time.LocalError.repeated(...)` and
  `ExpenseError.bad_date(...)` values (saved to `tests/cases/case_fields/json_baseline.out` for T030).

---

## Phase 2: Foundational (blocking)

- [X] T005 Audit, then guard, every place that reads an arm as a name: `src/check.c` (`:882` ok/err
  pre-scan, `:979`, `:1007`), `src/fmt.c:793`, `src/check_structure.c:443-446` (`norm`), the other
  `N_MATCH` walkers (`check_structure.c:167, 202, 438, 604, 926`; `compile.c:378, 433`;
  `vm.c:1133`): each branches on `arm->kind` and the arm's `a.n` kind, so a value arm or `N_ELSE_ARM`
  (T007) is skipped or handled, never read as a `Str*`. `make test`: no change.
- [X] T006 [P] `tests/fuzz.py`: noise for the new forms (`case 200 {`, `case -1 {`, `case "a" {`,
  `else {`, `case x.y {`, `name { a: int; b: str; }`, `T.c { a: 1, b: 2 }`, `T.c {`, `T.c { a`), well
  formed and broken; `python3 tests/fuzz.py --iterations 200 --seed 1` passes.

---

## Phase 3: User Story 2 - `match` on an `int` or `str` (Priority: P1)

**Goal**: dispatch on a value with literal or constant arms and a required `else`, checked for repeats
and types, compiled with existing opcodes.

**Independent Test**: quickstart step 2 prints `not found slow down other`; each US2 acceptance scenario
is a golden case.

- [X] T007 [US2] Golden cases first, `tests/cases/value_match/`: `ints.vo` (run: literal arms, `-1`,
  `0x10`, a constant arm, `else`; a function returning from every arm needs no `return` after it),
  `strs.vo` (run: string arms, a constant, `else`, byte comparison: `"É"` vs `"é"` distinct),
  `no_else.vo` (check: E0414 with its fix, scenario 1), `repeat.vo` (check: `case 200` twice, and a
  constant equal to a literal: E0402 with the removal fix, scenario 2), `wrong_type.vo` (check: `case
  "200"` on an `int`: E0301, scenario 3), `scrutinee.vo` (check: `match` on a `float`, `bool`, `bytes`,
  list, map and record: E0345, scenario 4), `else_on_variant.vo` (check: E0415 with its fix),
  `const_unknown.vo` (check: an arm name that is no constant of the type: E0201). Fail today.
- [X] T008 [US2] `src/parse.c`, `src/vo.h`: `N_ELSE_ARM` (new node kind, documented: C body); in
  `parse_stmt`'s match (`parse.c:1068-1103`) an arm is `case` followed by an int literal (with a leading
  `-`), a string literal, or a name (`case name`, `case name(...)`, a constant; a module constant `m.name`
  through the spec 058 qualified-name path), then a block; `else` + block ends the arms; anything after
  `else` is E0110.
- [X] T009 [US2] `src/check.c` (`check_match` `:877`, `check_arm` `:1000`, `report_missing_cases`
  `:1050`): an `int`/`str` scrutinee checks value arms (a literal of the type, or a name resolving to a
  constant of the type through `scope_find`/`const_eval`, else E0201; another type E0301; equal values
  E0402 with today's fix); no `else`: E0414, fix inserting `else {\n}` before the match's `}`; `else` on
  a variant or `result` match: E0415, fix removing the arm; any other scrutinee: E0345 (replaces today's
  E0301 "`match` needs a variant or result value", `check.c:893`; update the golden cases that expect
  it); the match terminates when every arm and `else` do. Catalog entries and codes rows for E0345,
  E0414, E0415.
- [X] T010 [US2] `src/compile.c` (`cmatch` `:370-388`): a value match compiles to the scrutinee in a
  register, then per arm `OPC_JNE_K` (an int in 32 bits) or `OPC_JNE` (other ints) or `OP_EQ` + `OPC_JF`
  (strings), the body, a jump to the end; `else` last; each arm and `else` a coverage point (`COV_CASE`);
  no new opcode. Accept T007's run cases; `tests/diff_engines.py` unchanged (generated programs do not
  use the new form).
- [X] T011 [P] [US2] `src/fmt.c`: value arms (`case 200 {`, `case "a" {`, `case -1 {`, `case limit {`) and
  `else {` print in canonical form (contracts/syntax.md); golden case `tests/cases/fmt/value_match.vo`.
- [X] T012 [US2] Coverage and mutation: `vo test --coverage` lists an unrun value arm and an unrun `else`
  (golden case `tests/cases/value_match/coverage.cmd`); `--mutate` behaves as for `if` chains (no new
  mutants required; state the outcome in `results.md`).

**Checkpoint**: value matches work end to end; nothing else changed.

---

## Phase 4: User Story 1 - A case with several values (Priority: P1)

**Goal**: a variant case declares two or more fields, built `V.case { ... }`, bound read-only, behaving
as a record payload everywhere at run time.

**Independent Test**: quickstart step 1 prints `a.txt:3: bad number` and
`{"malformed":{"path":"a.txt","line":3,"reason":"bad number"}}`; US1 acceptance scenarios 1–3 are golden
cases; the reference's `Shape` example runs without `Size` (T031).

- [X] T013 [US1] Golden cases first, `tests/cases/case_fields/`: `basic.vo` (run: declaration, literal in
  any order, binding, reads, `format`), `missing_field.vo` (check: E0305), `unknown_field.vo` (check:
  E0304), `read_only.vo` (check: `set m.line = 4;` E0344), `equality_keys_order.vo` (run: `==`, a map key,
  `sort` of a list of values, scenario 3, compared with the same program written with a payload record
  in `equality_keys_order_record.vo`: identical output), `json.vo` (run: `to_json` / `from_json` round
  trip, identical to the record-payload form), `annotations.vo` (run: `@range` and `@size` on case fields
  limit `from_json` as on records), `generic.vo` (run: `variant Tree[T] { leaf; node { left: Tree[T];
  value: T; right: Tree[T]; } }` built and matched, type arguments inferred), `type_name.vo` (run: a
  function taking `LoadError.malformed` as a parameter type), `no_new.vo` (check: `new
  LoadError.malformed { ... }` is E0201 naming `LoadError.malformed { ... }`), `block_after_case.vo`
  (run: `if e == T.empty {` followed by a block still parses as today, research R2), `print.vo` (test: a
  failing `expect_eq` on field-case values prints `LoadError.malformed { path: "a", ... }`, research R4).
- [X] T014 [US1] `src/parse.c`, `src/vo.h`: in `parse_members(N_CASE_DECL)` (`parse.c:1403-1458`) a case
  name followed by `{` parses field declarations with annotations (the record path, `parse.c:1437`
  allowing them); `N_CASE_DECL.c` holds a synthetic `N_RECORD` (name `Variant.case`, the fields, the
  variant's type parameters copied) and `b` a type node naming it; in `parse_primary` (`parse.c:697-716`,
  and the module form `:616-631`) `Type.case` followed by `{` and then a name and `:` parses a field
  literal (an `N_VCASE` whose C is a synthetic `N_NEW_RECORD` spanning the braces); in `parse_type`
  (`parse.c:491-494`) `Type.case` and `Type[args].case`. Golden parse cases pass.
- [X] T015 [US1] `src/check.c` (`define_item` `:1073`, `resolve_item_types` `:1119-1131`,
  `check_field_annotations` `:1130`, `check_export_types` `:1344-1358`): the hidden records are defined,
  resolved, their annotations checked and their export follows the variant's (no E0210 for them); a
  field case with one field is E0216 (T019).
- [X] T016 [US1] `src/check_expr.c` (`check_vcase` `:1229-1278`), `src/check_generic.c` (`vcase_generic`
  `:716-743`): a field-case literal goes through `check_new_record` (E0304, E0305, repeated fields,
  generic inference from fields and context, spec 059 lambdas in fields); `check_set` (`check.c:621-651`)
  refuses a field of a hidden record: E0344 (catalog entry, codes row); `new` naming a hidden record:
  E0201 with the message of contracts/diagnostics.md.
- [X] T017 [US1] `src/check.c` (`ty_name_into` `:184-188`): a hidden record's type prints
  `LoadError.malformed`, a generic one `Tree[int].node`; hover and `type-at` show it.
- [X] T018 [US1] `src/rt_render.c` (`:137-165`): a variant value whose payload is a hidden record prints
  `LoadError.malformed { path: "a", line: 3, reason: "x" }`; a hidden record printed alone (a bound value
  in a trace) prints `LoadError.malformed { ... }` too. Accept T013's run and print cases.
- [X] T019 [US1] E0216 one_field_case in `src/check.c` (contracts/diagnostics.md): its fix rewrites the
  declaration to `name(T);` (research R5); golden case `tests/cases/case_fields/one_field.vo`; catalog
  entry, codes row; `python3 tests/apply_fixes.py`.
- [X] T020 [US1] Property tests (FR-007, SC-004): golden case `tests/cases/case_fields/prop.vo` (a
  `prop` over a variant with a field case fails on one; the counterexample prints in construction syntax,
  shrunk, identical for `--seed 1` twice); no code change expected (`prop.c` reads payload types), fix if
  it fails.

**Checkpoint**: both stories complete.

---

## Phase 5: Polish & Cross-Cutting Concerns

- [X] T021 [P] `src/fmt.c` (`fm_members` `:837-869`, item printer `:970-978`, expressions): field cases
  print like record fields (annotations included); field-case literals follow the record literal's
  layout; golden case `tests/cases/fmt/case_fields.vo`; `tests/fmt_corpus.py` passes.
- [X] T022 [P] `src/doc.c` (`:191-213`, `:292-298`, item loops `:411, 770, 922`): signatures print
  `malformed { path: str; line: int; reason: str; }`; hidden records are not listed as items; `vo doc`
  of a std variant with a field case (after T029) shows them; `doc_conformance.py` passes.
- [X] T023 `src/query.c` (`q_resolve` `:322-372`, `q_resolve_qualified` `:384-418`, `q_members`
  `:114-136`, `QK_TYPES`, `q_decl_type` `:1259-1267`): TARGET `Variant.case.field` (and
  `m.Variant.case.field`) keyed `(hidden record, field index)`; refs and rename of a case field (golden
  cases `tests/cases/query/case_field_refs.cmd`, `tests/cases/edit/rename_case_field.cmd`).
- [X] T024 `src/edit.c` (`ed_add_field` `:302-318`, `ed_field_decl_change` `:213-241`): `vo edit
  add-field ENTRY Variant.case FIELD: TYPE = DEFAULT` adds the field to the case and to every literal; the
  new field goes before the closing `}` (also fixing records declared on one line); golden cases.
- [X] T025 [P] `src/lsp.c` (`:925-932`): document symbols list a field case's fields as children;
  `tests/lsp.py` passes.
- [X] T026 [P] Editors: tree-sitter (`case_declaration` :106, `match_arm` :234-238, `variant_value` :273,
  `qualified_type` :160; `make grammars`), TextMate, Vim, Zed queries (a copy of tree-sitter's),
  `site/pygments_vo`; `python3 tests/grammars.py` passes.
- [X] T027 Repository (research R9): `std/time.vo` `LocalError.repeated(Repeated)` becomes a field case
  (`Repeated` removed); an E0214 library-change row in `src/libchange.c` for `time.Repeated` with its
  message; `tests/cases/library/error_texts.vo` and `tests/migrate_056.py`'s expected texts follow.
- [X] T028 Repository: `examples/expenses/report.vo` `ExpenseError.bad_date(DateProblem)` becomes a
  field case (`DateProblem` removed); the example's tests and recordings give the same results (the
  printed failure text changes to construction syntax, R4: update `tests/migrate_056.py`'s CHANGED entry).
- [X] T029 SC-001 and FR-005: the JSON of the migrated `repeated` and `bad_date` values equals T004's
  baseline (`tests/cases/case_fields/json_baseline.vo` prints them; its `.out` is T004's file).
- [X] T030 `make test`.
- [X] T031 `docs/reference.md` (R10): the Variants paragraph and example (`rect { w: float; h: float; }`,
  `Shape.rect { w: 2.0, h: 1.5 }`, `case rect(r)`, `r.w * r.h`, no `Size`), the `match` table row and the
  details line say both new forms in the words they use now; `python3 tests/ref_examples.py`: words
  before and after stated in `results.md`, at most 11,538; tighten other paragraphs if needed, listed in
  `results.md`.
- [X] T032 [P] `site/docs/guide/records-and-variants.md` (case fields), the guide's control-flow page
  (value `match`), `docs/stability.md`, `CHANGELOG.md` (0.6.0: both forms, the codes, `time.Repeated`
  removed); `site_examples.py` passes.
- [X] T033 Version 0.6.0 (`src/build.c`, `src/main.c`, `tests/cases/cli/version*.out`,
  `tests/cases/build/options/expected/out`, `site/docs/tools/version.md`, `site/docs/start/install.md`,
  `docs/stability.md`); `make site-check`.
- [X] T034 SC measurements in `results.md`: SC-001 (2 of 2 payload records removed, JSON identical),
  SC-003 (codes, cases, catalog), SC-004 (prop), SC-005 (reference words), check times (T004's baseline).
- [ ] T035 `make test` on x86-64, then `make test ARCH=aarch64` and `ARCH=riscv64` under qemu
  (byte-identical); `results.md` complete; `docs/roadmap.md` row for 060.

---

## Dependencies & Execution Order

- **Setup (T001–T004)**: first; T002–T004 in parallel.
- **Foundational (T005–T006)**: T005 before any story; T006 in parallel.
- **US2 (T007–T012)**: after Phase 2. T007 → T008 → T009 → T010; T011 and T012 after T010.
- **US1 (T013–T020)**: after Phase 2; independent of US2 except `src/check.c` (do US2 first, as planned).
  T013 → T014 → T015 → T016 → T017 → T018; T019 after T015; T020 after T018.
- **Polish (T021–T035)**: T021, T022, T025, T026 in parallel after the stories; T023 → T024; T027 → T028
  → T029 → T030; T031 and T032 after T030; T033 before T034; T035 last.

## Parallel Example: User Story 1

```text
T013 golden cases (all files at once)
→ T014 parser → T015 hidden records → T016 literals and read-only → T017 type names → T018 printing
   T019 (E0216) beside T016–T018 once T015 is in
```

## Implementation Strategy

- **MVP**: Phases 1–3 (US2). Value matches are self-contained and land with no change to existing
  programs. Validate with quickstart step 2.
- **Then US1** (Phase 4): the larger story; validate with quickstart step 1 and the JSON/prop cases.
- **Then Polish**: tools and editors, the two payload records, the reference within its budget, docs,
  0.6.0, three platforms.

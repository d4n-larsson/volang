---

description: "Task list for vo-lang spec 027: Application Structure Standards"
---

# Tasks: Application Structure Standards

**Input**: Design documents from `specs/027-structure-standards/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R12), data-model.md, contracts/codes.md,
contracts/diagnostics.md, contracts/cli.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases land first and fail before its code.

**Organization**:
- Setup: test tooling and the results file.
- Foundational: the per-statement effect bit, the rules' frame (`src/check_structure.c`, run by `vo check`
  after a clean check), `@allow` (E0116, E0117, E0121, the accepted set, W0706) and the summary count:
  every rule reports through it.
- US1 (P1) pure core W0701; US2 (P1) sizes W0702/W0703 and `@allow` end to end; US3 (P2) duplication
  W0704; US4 (P3) test placement W0705 and `vo test` running every program file (FR-014); US5 (P1)
  documentation, last because it documents every rule.
- Polish: corpus and speed checks, golden churn review, verification on three platforms.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases** go in `tests/cases/structure/` as check cases (`.cmd` `check --json {}`); a multi-file
  case is a directory with `main.vo`, `main.cmd`, `main.out` and the imported files (as
  `tests/cases/multi/app/`). `vo test` cases go in `tests/cases/test/` or, with several commands, in
  `tests/cases/test_steps/<name>/steps`.
- **Thresholds** (data-model.md): R = 15 effect-free statements, F = 400 code lines, S = 50 statements,
  D = 6 sibling statements. Fixed constants in `src/check_structure.c`, named, each with the research item
  that chose it.
- **Messages, spans, related locations, fixes**: exactly as contracts/diagnostics.md.
- **Only `vo check`** reports structure warnings, only for program files (not std, not packages), only
  when the program has no errors (research R8, FR-007).
- **Between edits**: `make quick AREA=structure`. `make test` once per change; never two suites at once.
- **GC safety**: the rules allocate (diagnostics, hash tables); root what is held across allocations, and
  run the stress build over `tests/cases/structure` before marking a C task done.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: area `structure` (cases `structure`, `test`, `test_steps`, `check`; scripts
  `apply_fixes.py`, `catalog.py`, `structure_corpus.py`), and the file mapping `src/check_structure.c`,
  `tests/structure_*.py`, `tests/structure_seeds/*` → `structure`.
- [X] T002 [P] `tests/catalog.py` reads `specs/027-structure-standards/contracts/codes.md`;
  `tests/apply_fixes.py` adds `structure` to its directories.
- [X] T003 [P] `specs/027-structure-standards/results.md`: SC-001–SC-007 (pending), Verification,
  Thresholds as measured (research R1–R5 numbers, then the exact ones from the implementation), Deviations.

---

## Phase 2: Foundational (blocking)

**Purpose**: everything the rules share: effects per statement, the rule frame, `@allow`, the summary.

- [X] T004 Golden cases (fail first) `tests/cases/structure/allow_unknown` (`@alow("W0703", "x")` above a
  `fn`: E0116 with the fix `allow`), `allow_misplaced` (`@allow` above a `record`: E0117),
  `allow_args` (`@allow("W0703")`, `@allow("W0501", "x")`, `@allow("W0703", "")`: E0121 with the three
  messages of contracts/diagnostics.md), `allow_ok` (`@allow` and `@spec` above one `test`, in either
  order: no error).
- [X] T005 `src/check.c` `check_annotations`: the closed set is `spec` and `allow` (E0116's message names
  both; its fix replaces a name within two letters of either); `@allow` above a `fn`, `test` or `prop`
  (E0117 otherwise; `@spec` keeps its rule); E0121 when the arguments are not exactly two strings, the code
  is not W0701–W0705, or the reason is empty; record each valid `@allow` on its declaration (code, the
  annotation node, a used flag). Make T004 pass.
- [X] T006 (done in `src/check_structure.c` instead of `src/check_effects.c`: computed after checking from
  each call's builtin id, declaration, generic bindings or function-value type, since user functions'
  effects are only final after the file's fixpoint) a per-statement flag "requires effects" (data-model.md "Statement":
  "its own expressions (the condition, iterable or scrutinee of a compound statement; the whole statement
  otherwise) contain a call whose required effects are not empty: an effectful builtin, a function with
  declared effects, an effect-generic function whose effects at that call are not empty, or a function
  value whose type has effects. `todo(...)` and creating an anonymous function do not require effects"),
  set while `fx_call` / `fx_value_call` compute a call's effects, on the innermost enclosing statement.
- [X] T007 `src/check_structure.c` (new) and `src/vo.h`: `structure_check(int entry, Vec *files)`, called in
  `src/main.c` where `rec_review` runs (check mode, no errors), over the program files only; a statement
  walk shared by the rules (counts at every depth, anonymous function bodies included); `st_report(code,
  file, start, end, message, related)` that drops a warning an `@allow` on its declaration accepts (marking
  it used, counting it) and emits the rest; after all rules, W0706 for every unused `@allow`, with the fix
  removing its line including the newline (contracts/diagnostics.md W0706).
- [X] T008 `src/diag.c`: the check summary `E errors, W warnings` gains ` (A accepted)` when A > 0; the JSON
  summary gains `"accepted": A` only when A > 0 (contracts/cli.md); accepted warnings are not counted in W.

**Checkpoint**: annotations check; `vo check` runs an (empty) rule set; every existing golden case
unchanged (`make quick`).

---

## Phase 3: User Story 1 — Pure core (Priority: P1) 🎯 MVP

**Goal**: W0701 on effectful functions with 15 or more effect-free statements.

**Independent Test**: a `main` with 15 effect-free statements gets W0701 with related locations; with 14,
or with its logic moved into pure functions, none.

- [X] T009 [P] [US1] Golden cases `tests/cases/structure/effectful_logic` (`main` with 15 effect-free
  statements among prints: W0701 at `main`, three related stretches longest first), `effectful_logic_14`
  (14: none), `effectful_thin_shell` (the same logic in pure functions, `main` calling them: none),
  `effectful_dispatch` (a `match` whose arms print: arms' effectful statements not counted),
  `effectful_lambda` (an anonymous function created but not called counts effect-free; called with
  effects, effectful), `effectful_generic` (an effect-generic call with no effects at that call is
  effect-free), `effectful_test` (a test with 20 effect-free statements: none).
- [X] T010 [US1] `src/check_structure.c`: W0701 (R = 15; not for tests and properties); the message and the
  three longest effect-free stretches (data-model.md "Effect-free stretch": maximal sequences of
  consecutive effect-free siblings, descending into effect-free compound statements; ties by position) as
  related locations "$k statements that use no effect". Make T009 pass.

**Checkpoint**: US1 cases pass; the voledger program gets W0701 at `main`.

---

## Phase 4: User Story 2 — Sizes, and accepting a warning (Priority: P1)

**Goal**: W0702 and W0703; `@allow` end to end; std accepts its two.

**Independent Test**: 401 code lines warn and 400 do not; 51 statements warn and 50 do not; an `@allow`
moves the warning to the summary's accepted count, and an unused one is W0706 with its fix.

- [X] T011 [P] [US2] Golden cases `tests/cases/structure/file_too_long` (401 code lines plus blank and
  comment lines: W0702 at 1:1 with "401 code lines"), `file_400` (none), `function_too_long` (51
  statements, some inside an anonymous function: W0703 at the name), `function_50` (none), `test_too_long`
  (a test with 51: W0703 at `test "name"`), `allow_accepted` (`@allow("W0703", "reason")`: no W0703;
  summary `0 errors, 0 warnings (1 accepted)`, JSON `"accepted": 1`), `allow_file` (`@allow("W0702", ...)`
  above the first declaration), `allow_unused` (W0706 with the fix; `tests/apply_fixes.py` applies it),
  `with_errors` (a program with an error and 60-statement function: only the error), `std_excluded`
  (a small program importing `std.json`: no W0702 for std).
- [X] T012 [US2] `src/check_structure.c`: W0702 (F = 400 code lines: not blank and not only a comment;
  at line 1, zero width; accepted by an `@allow` on the file's first declaration) and W0703 (S = 50, at the
  name; for a test or property its keyword and name). Make T011 pass.
- [X] T013 [US2] `std/json.vo`: `@allow("W0702", "one module per import path: std.json is one file")` above
  its first declaration and `@allow("W0703", "one scanner, read top to bottom")` above `parse_number`;
  `vo check std/json.vo` from the repository shows `0 errors, 0 warnings (2 accepted)`; `vo doc std.json`
  output unchanged (`tests/cases/doc/*`).

**Checkpoint**: US1 and US2 pass; the voledger program gets W0702.

---

## Phase 5: User Story 3 — Duplication (Priority: P2)

**Goal**: W0704 for copied runs of 6 or more statements, with the effect-parameter advice for effect-only
copies.

**Independent Test**: two functions differing only in local names and literals: W0704 on the later with
the earlier as related; differing in a call: none; differing only in effects: the effect-parameter message.

- [X] T014 [P] [US3] Golden cases `tests/cases/structure/duplicate_renamed` (6 statements copied with other
  local names and literals: W0704 on the later run, related "the first copy"), `duplicate_5` (5: none),
  `duplicate_call` (a different call target in one statement: none), `duplicate_three` (three copies: two
  warnings, both pointing at the first), `duplicate_effects` (two functions taking `Database[uses
  file.read, file.write]` and `Database`, otherwise equal: the effect-parameter message on the later
  function's name), `duplicate_tests` (the same 8 statements in two tests: none), `duplicate_files/`
  (a multi-file case: the copy in an imported file is reported, the earlier one by file then position).
- [X] T015 [US3] `src/check_structure.c`: W0704 per data-model.md "Duplicate run": runs of D = 6
  consecutive sibling statements in non-test functions of the program; normal form = tokens with "each
  local variable and parameter replaced by its position of first appearance in the run and each literal by
  its kind (`int`, `float`, `str`, `bool`)"; a hash table (FNV-1a) over normal forms, equality confirmed by
  comparing tokens; overlapping windows merged into maximal runs; each later maximal run reported once at
  its first statement; effect-only copies of whole functions (signatures equal after erasing `uses`
  clauses and effect arguments) get the effect-parameter message at the function's name. Make T014 pass.

**Checkpoint**: US3 passes; the voledger program's `parse_cardco` is reported if its body repeats
`parse_nordbank` under the exact normalization (record the outcome in results.md).

---

## Phase 6: User Story 4 — Tests next to the code (Priority: P3)

**Goal**: `vo test ENTRY` runs every program file's tests (FR-014); W0705 for tests in the wrong file.

**Independent Test**: tests in an imported `money.vo` run with `vo test main.vo`; a test in `main.vo`
calling only `money.vo` functions gets W0705, and none once moved.

- [X] T016 [P] [US4] Step case `tests/cases/test_steps/program_files/` (`main.vo` importing `money.vo`, both
  with tests): `vo test main.vo` runs both (`money.vo::...` identifiers), `vo test --list main.vo` lists
  both, `vo test --test money.vo::NAME main.vo` runs that one, `vo test .` runs each test once, and a
  program whose imports have no tests runs exactly the tests it ran before.
- [X] T017 [US4] `src/main.c` `cmd_test` and `src/testrun.c`: the test files are the program files every
  entry loads (the entry, then its imports as loaded; not std, not packages), each once across entries;
  running, `--list`, `--coverage`, `--mutate`, `--test`, `--spec`, `--diff` and T0011 use that set.
  Make T016 pass; update affected `tests/cases/test*` goldens after reviewing each change.
- [X] T018 [P] [US4] Golden cases `tests/cases/structure/test_elsewhere/` (multi-file: a test in `main.vo`
  calling only `money.vo` functions: W0705 naming `money.vo`), `test_here/` (the test in `money.vo`:
  none), `test_two_files/` (calls into two other files: none), `test_builtins_only` (no program calls:
  none).
- [X] T019 [US4] `src/check_structure.c`: W0705 (a test or property whose calls to program functions, at
  least one, all go to functions declared in one other program file). Make T018 pass.

**Checkpoint**: all five rules pass their cases.

---

## Phase 7: User Story 5 — The standard, written down (Priority: P1)

**Goal**: the catalog, the reference and the site guide state the standard (FR-010).

**Independent Test**: `vo explain` for each new code; the reference section within budget; the guide's
before-and-after example runs and the after version checks with no warnings.

- [X] T020 [P] [US5] `docs/errors.md`: entries W0701–W0706 and E0121 (description, example, fixed version
  that checks clean), E0116 and E0117 updated; `tests/catalog.py` and the `vo explain` cases pass.
- [X] T021 [P] [US5] `docs/reference.md`: a "Structure" section (≤ 250 tokens with the other changes,
  SC-005): a thin effectful shell over a pure core, modules by concept, tests next to the code, one
  function instead of copies (an effect parameter for effect-only copies), the four limits with their codes,
  and `@allow("W07NN", "reason")`; the `vo test` paragraph ("runs every test and property in the program:
  the file and the files it imports"); the annotations paragraph (`@spec` and `@allow`).
  `tests/ref_examples.py` within budget.
- [X] T022 [P] [US5] `site/docs/guide/structure.md` (new, in the guide nav of `site/mkdocs.yml`):
  "Structuring a program": the principles with reasons, then one program before (one file, logic in
  `main`, a copied block, tests in `main.vo`) with the warnings it gets, and after (`main.vo` a thin
  shell, a pure core module, tests next to the code) checking clean; examples run by
  `tests/site_examples.py` (multi-file examples in the form that script supports, or extend it).
  `site/docs/tools/test.md` and `site/docs/guide/tests.md`: `vo test main.vo` runs the program's tests.
- [X] T023 [US5] `examples/`: `vo check` every example program; restructure any with warnings (expected:
  none, research R1–R5); `README.md` and `docs/roadmap.md` (spec 027 done, with "Not done": an
  extract-function edit, per-project limits).

---

## Phase 8: Polish and verification

- [X] T024 [P] `tests/structure_corpus.py`: the rules on the example programs and the site's and
  reference's runnable examples (no W07xx), std (only accepted ones), `tests/structure_seeds/` (at least 5
  seeded violations per rule: each reported) and `tests/structure_seeds/clean/` (at least 10
  well-structured programs: nothing reported) (SC-002, SC-003); added to `make test` in `Makefile`.
- [X] T025 [P] `tests/structure_speed.py`: `vo check` time with and without the rules (an environment
  switch used only by this script, or the previous build) on the largest example and a generated
  5,000-line program: at most 10% slower, median of 5 (SC-004); added to `make gate`.
- [X] T026 Golden churn: run `tests/run.sh` and `tests/steps.py`; review every changed `check` case (only
  structure warnings may appear) and count them against SC-002's 5% of golden programs; accept.
- [X] T027 SC-001: `vo check` on the voledger agent's first program (a copy in `tests/structure_seeds/`):
  W0702, W0701 at least; record the warnings in results.md. The restructured version and SC-007 come from
  rerunning the experiment (noted in results.md as follow-up).
- [X] T028 `make lint`; `make instrumented`; the stress build over `tests/cases/structure`
  (`tests/run.sh --stress 64 tests/cases/structure`).
- [X] T029 `make test` on x86-64, then `make ARCH=aarch64 test` and `make ARCH=riscv64 test`, one at a time.
- [X] T030 Fill in `results.md`; spec status Implemented; every task [X].

---

## Dependencies & Execution Order

- **Phase 1** → **Phase 2** (T004 → T005; T006, T007 → T008) → user stories.
- **US1** (T009–T010), **US2** (T011–T013), **US3** (T014–T015), **US4** (T016–T019): each after Phase 2,
  independent of each other (all add to `src/check_structure.c`, so implement them one at a time; their
  golden cases can be written in parallel). T013 after T012. T019 after T017 (W0705 advises moving a test
  that `vo test` must then still run).
- **US5** after US1–US4 (it documents them).
- **Phase 8** after everything; T029 last but one.

## Parallel Opportunities

- T002, T003 together.
- The golden cases of every story (T009, T011, T014, T016, T018) once Phase 2 is done.
- T020, T021, T022 together.
- T024, T025 together.

## Implementation Strategy

- **MVP**: Phase 1, Phase 2, US1 (the rule only vo can check), then US2 (the rule that would have caught
  both experiment programs). Validate with the voledger program (T027) before going on.
- Then US3, US4, US5, Polish. Each story leaves every earlier golden case passing.

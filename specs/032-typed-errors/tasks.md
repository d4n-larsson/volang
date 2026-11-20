---

description: "Task list for vo-lang spec 032: Typed Errors"
---

# Tasks: Typed Errors

**Input**: Design documents from `specs/032-typed-errors/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R13), data-model.md, contracts/syntax.md,
contracts/diagnostics.md, contracts/library.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's golden cases land first and fail before its code.

**Organization**:
- Setup: test wiring and the results file.
- Foundational: the error type in `TY_RESULT` (B, 0 for `str`), parsing and naming, with every existing
  program unchanged.
- US1 (P1) declaring and matching typed errors; US2 (P1) `try` across error types and `map_err`; US3 (P2)
  reports, tasks and properties; US4 (P2) `std.web` `typed`; US5 (P3) `std.option`; US6 (P3)
  documentation, examples, tools.
- Polish: speed, three platforms, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: `tests/cases/typed_errors/<name>.vo` (`.cmd` `check --json {}` for diagnostics, `run {}`
  or `test {}` otherwise); accept with `tests/run.sh --accept PATH` after reading the diff.
- **Catalog**: each new code gets its `docs/errors.md` entry (broken and fixed example), a row in
  `specs/032-typed-errors/contracts/codes.md` and `032-typed-errors` in `tests/catalog.py`'s spec list,
  in the task that introduces it. Message texts: contracts/diagnostics.md.
- **Unchanged programs**: no golden case outside the ones this list names may change (FR-003, SC-001).
- **Reference budget**: `python3 tests/ref_examples.py` after each reference change; at most +300 tokens
  (from about 14,255).
- **Between edits**: `make quick AREA=typed_errors` (T001); `make test` once per change, through
  `tools/progress.py run test`; the gate only when asked.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: a new area `typed_errors` (cases `typed_errors`, `check`, `generic`, `prop`,
  `tasks`, `test`, `run`, `web`, `doc`; scripts `catalog.py`, `apply_fixes.py`, `ref_examples.py`,
  `doc_conformance.py`) listed among the known areas; no source file maps to it alone.
- [X] T002 [P] `specs/032-typed-errors/results.md` (SC-001–SC-007 pending, Verification, Deviations, R9's
  deviation already listed) and `specs/032-typed-errors/contracts/codes.md` in the format of spec 031's
  (E0334, E0412 new; E0409's message changed).

---

## Phase 2: Foundational (blocking)

- [X] T003 Golden cases first, `tests/cases/typed_errors/type.vo`: `result[int, str]` and `result[int]`
  used interchangeably (a function declared with one returns a value of the other, `==` between them, a
  list of both) checks and runs; `vo query type-at` on a `result[int, str]` variable shows `result[int]`;
  `result[void, LoadError]` and `result[list[int], LoadError]` parse and check.
- [X] T004 `src/parse.c`: `result[T]` takes an optional `, TYPE` after `T`, into the type node's C;
  `src/fmt.c` prints `result[T, E]` as written ("result[" T ", " E "]").
- [X] T005 `src/check.c`, `src/vo.h`: `ty_result_e(T, E)` (E = 0 or `t_str` gives B = 0, so
  "`result[T]` MUST be exactly `result[T, str]`"); `err_type(t)` (B, or `t_str` for B = 0);
  `resolve_type` builds `ty_result_e` from the node's C; `case_payload(t, 1)` returns `err_type(t)`.
- [X] T006 `src/types.c`, `src/check_generic.c`, `src/check_expr.c`, `src/check_effects.c`,
  `src/check_func.c`, `src/check_structure.c`: every function that recurses into a result's A
  (naming, `ty_has_params`, unify, substitute, `ty_has_func`, `ty_mentions_owner`, `ty_has_tparam`,
  `ty_carries_evar`, the task-result check, the structure fingerprint, the `json` constraint) also
  recurses into B when it is set; unifying B = 0 with a type parameter binds it to `str`; naming prints
  `result[T]` for B = 0 and `result[T, E]` otherwise.
- [X] T007 `src/check.c`: E0334 at the error type node when the error type is `void`, a function type or
  contains one ("the error type of a `result` is `void`, a function type or contains one"); catalog entry;
  `tests/cases/typed_errors/err_type.vo` (`result[int, void]`, `result[int, fn(int)]`,
  `result[int, list[fn(int)]]`).
- [X] T008 Accept T003; `make test` (via `tools/progress.py run test`): every existing case unchanged.

**Checkpoint**: the type exists; nothing uses it yet beyond declarations.

---

## Phase 3: User Story 1 - A function says how it can fail (P1) 🎯 MVP

**Goal**: build, return and match typed errors.

**Independent Test**: quickstart step 1.

- [X] T009 [P] [US1] Golden cases first: `tests/cases/typed_errors/use.vo` (the contracts/syntax.md
  example without `map_err`: `load`, `describe`, a caller matching `case err(e)` on `LoadError`),
  `use_missing_case.vo` (`check --json`: a `match e` leaving out `malformed` is E0401 with the fix that
  inserts it), `use_wrong_err.vo` (`err("missing")` in a function returning `result[int, LoadError]` is
  E0301 naming `LoadError` and `str`), `use_record.vo` (a record error type), `use_generic.vo`
  (`fn first_ok[T, E](rs: list[result[T, E]], fallback: E) -> result[T, E]` called with `str` and
  `LoadError` results), `use_infer.vo` (`let r = err(LoadError.missing("a"));` is E0307 with its fix).
- [X] T010 [US1] `src/check_expr.c`: `err(e)` checks `e` against `err_type(expected)` (today `t_str`),
  with E0301 naming both types; `ok` unchanged.
- [X] T011 [US1] `src/check.c`: the `case err(e)` binding gets `err_type` through `case_payload`; E0401's
  fix for a `match` on an error variant works unchanged (verify with T009).
- [X] T012 [US1] Accept T009; `make quick AREA=typed_errors`.

**Checkpoint**: typed errors work inside one function and its callers that match.

---

## Phase 4: User Story 2 - Errors pass up, and change form, only explicitly (P1)

**Goal**: `try` with the same error type; E0412 otherwise; `map_err`.

**Independent Test**: quickstart step 2.

- [X] T013 [P] [US2] Golden cases first: `tests/cases/typed_errors/try_same.vo` (three functions with
  `LoadError`, `try` through them, the caller matching the error that started at the bottom),
  `try_mismatch.vo` (`check --json`: `try` on `result[str]` in a `LoadError` function; `try` on a
  `LoadError` result in `main`; each E0412 naming both types), `map_err.vo` (the contracts/syntax.md
  example in full; an `ok` passes through unchanged and the function is not called (it prints when
  called); a function with an effect makes the caller need it: E0501 without it), `map_err_bad.vo`
  (`check --json`: a function taking the wrong error type, E0301), `main_typed.vo` (`fn main() ->
  result[void, LoadError]` is E0409 with the message naming `result[void]`).
- [X] T014 [US2] `src/check_expr.c`: `check_try` compares `err_type` of the operand with
  `err_type(ck_ret)` and reports E0412 at the `try` (message of contracts/diagnostics.md); catalog entry;
  E0409's message for a typed `main`.
- [X] T015 [US2] `src/vo.h`, `src/check_expr.c`, `src/rt_call.c`, `src/check_effects.c`: the builtin
  `map_err` (`B_MAP_ERR`, after `B_HTTP_TIMEOUT`; `B_COUNT`, `B_LAST`, the `BU_` enum and its static
  assertion, `builtin_effects` 0): checked with `check_hof` (`r: result[T, E1]`, `f: fn(E1) -> E2`,
  result `result[T, E2]`, the function's effects the caller's); at run time `ok` returns `r`, `err(e)`
  returns `err(f(e))` through the call path `map` uses.
- [X] T016 [US2] `docs/reference.md`: `map_err` in the functions-as-values builtin table;
  `python3 tests/doc_conformance.py` and `tests/cases/doc/all*` (accepted for the new row).

**Checkpoint**: all P1 stories: typed errors usable across layers.

---

## Phase 5: User Story 3 - Failures are reported in full (P2)

**Goal**: typed errors in every report, task result and generated input.

**Independent Test**: quickstart step 4.

- [ ] T017 [P] [US3] Golden cases first: `tests/cases/typed_errors/report_test.vo` (`test`: a failing
  `expect_eq(load(""), ok(1))` shows `err(LoadError.missing("config.txt"))` and its first difference),
  `report_runtime.vo` (a runtime error with a typed-error local: shown in vo syntax),
  `report_trace` (a step case: `vo run --trace` with a function returning a typed error),
  `tests/cases/prop/typed.vo` (a property over `result[int, LoadError]`: a failing case shrunk to a
  minimal typed error), `tests/cases/tasks/typed.vo` (`concurrent_map` returning `result[int, LoadError]`
  values in input order).
- [ ] T018 [US3] `src/rt_render.c`, `src/task.c`, `src/prop.c`, `src/rt_test.c`: verify each goes
  through `case_payload` for the error (T005); fix any that assumes `str` (for example a generator that
  builds an error message directly); the cases of T017 pass.

---

## Phase 6: User Story 4 - std.web maps typed errors in one checked place (P2)

**Goal**: `typed(op, answer)` and `Problem`.

**Independent Test**: quickstart step 5.

- [ ] T019 [P] [US4] Golden cases first: `tests/cases/web/typed.vo` (an app with `NoteError` operations
  through `typed(..., note_problem)` and one prefix-form operation; `handle_request` answers 404 and 409
  with problem bodies identical to the prefix form's, 200 for success, and the prefix operation as
  before), `tests/cases/web/typed_missing.vo` (`check --json`: `note_problem` missing a case is E0401
  with its fix).
- [ ] T020 [US4] `std/web.vo`: `export record Problem { status: int; detail: str; }` and `export fn
  typed[C, I: json, X, E: effects](op: fn(C, I) -> result[Response, X] uses E, answer: fn(X) ->
  Problem) -> fn(C, I) -> result[Response] uses E`, answering `err(x)` with the problem body the prefix
  form builds (the same function), documented with an example; the module comment mentions both forms;
  `python3 tests/doc_conformance.py`.
- [ ] T021 [US4] `examples/notes-api`: `NoteError` (`not_found(int)`, `blank`), `note_problem`, the
  operations through `typed`; its tests and `vo.effects` unchanged in meaning (record updated only if a
  function's effects change; they should not); the README's error paragraph.

---

## Phase 7: User Story 5 - Absence is a value, not a failure (P3)

**Goal**: `std.option`.

**Independent Test**: quickstart step 6.

- [ ] T022 [P] [US5] Golden cases first: `tests/cases/typed_errors/option.vo` (`Option[Note]` returned,
  matched exhaustively, `or_else`, `is_some`, `to_result` into `result[Note, LoadError]`).
- [ ] T023 [US5] `std/option.vo` (new; `export variant Option[T] { some(T); none; }`, `or_else`,
  `is_some`, `to_result`, each documented with an example), registered with the other std modules
  (`Makefile` embed list, `tools/embed.py` inputs, `vo doc` module list); `docs/reference.md`'s module
  table gets a `std.option` row.

---

## Phase 8: User Story 6 - One canonical form, documented where agents read (P3)

**Goal**: reference, guide, examples, tools.

**Independent Test**: `ref_examples.py`, `site_examples.py`, `catalog.py` pass; the reference is under
budget.

- [ ] T024 [P] [US6] `tests/cases/fmt/typed_errors.vo` with `.expected.vo` (`result[int,LoadError]` and
  `result[ void , E ]` formatted to `result[int, LoadError]`, `result[void, E]`; `result[int, str]` kept),
  `.code` 0; `tests/cases/query/typed_errors` (a step case: `vo query refs` on an error case finds its
  `err(...)` uses and `case` arms; `vo edit rename` of the case changes them all).
- [ ] T025 [US6] `docs/reference.md`: in "Results and errors", `result[T, E]`, `err` of the error type,
  `try` with the same error type (E0412 otherwise), `map_err`, `main` and tests with `str` errors, one
  runnable example; the Types table row for `result`; +≤ 300 tokens in all with T016 and T023.
- [ ] T026 [P] [US6] `site/docs/guide/results-and-errors.md`: a section on typed errors (when callers act
  on the kind of failure; a `describe` function for messages; `map_err` at layer boundaries) with checked
  examples; `site/docs/guide/apis.md`: `typed` and `Problem`; `python3 tests/site_examples.py`.
- [ ] T027 [P] [US6] `examples/expenses`: `expenses_from` returns `result[list[Expense], ExpenseError]`
  (`bad_date(str)`, `negative(str)`, `field(str)`), `main` converts it with `map_err` and a `describe`;
  output byte-identical; its tests pass.
- [ ] T028 [P] [US6] `docs/roadmap.md`, `README.md`, `docs/review-2026-11/README.md`: spec 032 in the
  status table and the notes (what was left out: `Option` for builtins, automatic messages, per-app error
  types).

---

## Phase 9: Polish & cross-cutting

- [ ] T029 SC-005: `tests/perf.sh` (check time) and a loop of one million calls returning `err` with a
  variant against the same with a `str`, median of five; figures in results.md.
- [ ] T030 `make test` (via `tools/progress.py run test`), then `make ARCH=aarch64` and `make
  ARCH=riscv64` with `tests/run.sh tests/cases/typed_errors tests/cases/web tests/cases/prop
  tests/cases/tasks` under each (byte-identical outputs); not the gate unless asked.
- [ ] T031 `specs/032-typed-errors/results.md`: SC-001–SC-006 with measurements, Verification,
  Deviations; spec status; SC-007's agent run left for the user to schedule.

---

## Dependencies & Execution Order

- Setup (T001–T002) → Foundational (T003–T008) → US1 (T009–T012) → US2 (T013–T016).
- US3 (T017–T018) needs US1. US4 (T019–T021) needs US1 (typed errors and matching). US5 (T022–T023)
  needs only the Foundational phase. US6: T024 needs the Foundational phase; T025–T028 need US1–US5.
- Polish needs everything.

## Parallel opportunities

- T002 with T001; the golden-case tasks marked [P] (T009, T013, T017, T019, T022, T024) touch only new
  files and can be written ahead of their code.
- US4 (std.web) and US5 (std.option) touch different library files.
- T026, T027 and T028 are documentation and examples in different directories.

## Implementation Strategy

- **MVP**: Phases 1–3 (T001–T012): typed errors declared, returned and matched; nothing existing changes.
- **Usable across a program**: add US2 (T013–T016).
- **Then** US3 (reports) and US4 (std.web), US5 and US6, each testable alone, and the Polish phase.
- Commit after each phase; `make quick AREA=typed_errors` between edits.

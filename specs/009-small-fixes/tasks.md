---

description: "Task list for vo-lang spec 009: small language and toolchain fixes"
---

# Tasks: Small Language and Toolchain Fixes

**Input**: Design documents from `specs/009-small-fixes/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R6), data-model.md, contracts/language.md, quickstart.md

**Tests**: REQUIRED (constitution, Principle IV): every story starts with golden or step cases that fail
on the current binary; every new code gets a catalog entry and a case; reference examples run.

**Organization**: one phase per user story, in spec priority order (US1, US2 P1; US3, US4 P2; US5 P3).
The stories touch different files and are independent.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- C with the strict warning set on all three architectures (`make lint`); declarations in `src/vo.h`.
- Golden cases: `tests/cases/<dir>/<name>.vo` with `.out`/`.err`/`.code` (and `.cmd`); accept outputs only
  after checking them by hand against contracts/language.md. HTTP cases are step cases (`server http`).
- **Regression rule:** after every task that touches `src/` or `std/`, `tests/run.sh` and the affected step
  cases pass; outputs of programs that use none of the new features stay byte-identical.

---

## Phase 1: Setup

- [X] T001 Create `specs/009-small-fixes/results.md` (SC table SC-001…SC-007 "open"; sections per story,
  Deviations, Follow-ups).

---

## Phase 2: User Story 1 - Reading a map with a fallback (Priority: P1)

**Goal:** `get_or(m, key, default)` (FR-001, FR-002, research R1).

**Independent test:** `tests/run.sh tests/cases/run/get_or tests/cases/check/get_or_types tests/cases/check/get_or_redefined` passes.

- [X] T002 [P] [US1] Golden cases: `tests/cases/run/get_or.vo` (present and missing keys; `map[str, int]`,
  `map[int, str]`, a nested map default; the map unchanged afterwards (`len`, `has`); counting words with
  `set m[w] = get_or(m, w, 0) + 1;`), `tests/cases/check/get_or_types.vo` (wrong key type, wrong default
  type, a non-map first argument, two arguments: E0301/E0302), `tests/cases/check/get_or_redefined.vo`
  (`fn get_or(...)`: E0202). Confirm they fail now.
- [X] T003 [US1] `src/check_expr.c`: `B_GET_OR 55`, `B_COUNT 55`, the name, typing like `has` (key against
  `K`, default against `T`, result `T`); `src/vo.h` `B_LAST 55`; `src/rt_call.c` `BU_GET_OR` (the stored
  value or the default, no insertion). Rename the prelude helper in `tests/gen_programs.py` to `lookup_or`.
- [X] T004 [US1] `docs/reference.md` builtins table row for `get_or` (contracts/language.md); check that
  `tests/doc_conformance.py` covers the new row and `tests/ref_examples.py` passes.

---

## Phase 3: User Story 2 - Replay refuses a request it did not record (Priority: P1)

**Goal:** the request line in `http` events, matched on replay (FR-003, FR-004, research R2).

**Independent test:** `python3 tests/steps.py tests/cases/http` passes, including the new cases.

- [X] T005 [P] [US2] Step cases in `tests/cases/http/`: `replay_other_path` (record `GET /plain`, stop the
  server, replay a program asking for `/json`: R0014 naming both requests), `replay_other_method` (record a
  GET, replay a POST to the same path: R0014), `replay_old_recording` (a committed recording whose `http`
  event has no `request` field: replays by host and port, as before). Re-accept `record_replay`'s kept
  recording (it gains the field) after checking it by hand. Confirm the new cases fail now.
- [X] T006 [US2] `src/vo.h` `EV_REQUEST 48`, `EV_SIZE 56`, `Event.request`; `src/replay.c`: take the
  request line from the request text (`METHOD TARGET`), write `"request"` after `port`, read it back
  (`rk_keys`), match it in `io_http_exchange` when the recorded event has one, and describe requests in
  R0014 as `` `GET /v2/status` to `localhost:8000` ``. Update spec 006's `contracts/recording.md` (`http`
  row) and the reference's replay paragraph (requests are matched by host, port, method and target).

---

## Phase 4: User Story 3 - Permission errors name the right function (Priority: P2)

**Goal:** R0010 names the granting function (FR-005, research R3).

**Independent test:** `tests/run.sh tests/cases/effects` and `python3 tests/steps.py tests/cases/http/scope_denied` pass.

- [X] T007 [P] [US3] Golden cases `tests/cases/effects/denied_in_lambda.vo` (a function declaring
  `file.read("data/")` passes an anonymous function that reads `other/x.txt`: R0010 names that function)
  and `denied_in_lambda_json` (the same with `--json`: the report's function field); re-accept
  `tests/cases/http/scope_denied` (now names `main`) after checking it by hand. Confirm the new cases fail now.
- [X] T008 [US3] `src/rt_effects.c`: `rt_denied` names the grant frame (walk up while the frame is not a
  test and its function has no declared effect set, the same condition as `vm_push`); the JSON report uses
  the same name. Every existing R0010 case stays byte-identical.

---

## Phase 5: User Story 4 - `case ok()` for `result[void]` (Priority: P2)

**Goal:** the empty-parentheses arm, E0411 and W0402 (FR-006, FR-007, research R4).

**Independent test:** `tests/run.sh tests/cases/check tests/cases/run` passes; `python3 tests/catalog.py` passes.

- [X] T009 [P] [US4] Golden cases: `tests/cases/run/case_ok_void.vo` (a `result[void]` matched with
  `case ok()` / `case err(e)`, both arms taken), `tests/cases/check/case_ok_value.vo` (`case ok()` on a
  `result[int]`: E0411 with the fix `case ok(_)`), `tests/cases/check/case_empty_variant.vo` (`case none()`
  on a variant case without a value: E0411, fix `case none`; `case one()` on a case with a value: fix
  `case one(_)`), `tests/cases/check/case_ok_void_discard.vo` (`case ok(_)` and `case ok` on a
  `result[void]`: W0402 each, fix `case ok()`, exit 0). Confirm they fail now.
- [X] T010 [US4] `src/vo.h` `NF_EMPTY_ARM`; `src/parse.c`: `case NAME()` sets it; `src/check.c` `check_arm`:
  E0411 and W0402 with their fixes (contracts/language.md); `docs/errors.md` entries for E0411
  `empty_case_parens` and W0402 `void_ok_pattern` (examples and fixes); `docs/reference.md`: one sentence in
  Results and errors (`case ok()` matches the success of a `result[void]`). Run `tests/apply_fixes.py` and
  `tests/catalog.py`.
- [X] T011 [US4] Rewrite every `case ok(_)` / `case ok` on a `result[void]` in `std/`, `examples/`,
  `tests/stdlib_eval/*/` and the reference's examples to `case ok()` (apply the W0402 fix), so the
  project's own code has no warning; golden cases whose expected output now shows W0402 are re-accepted
  only after checking that the warning is the only change.

---

## Phase 6: User Story 5 - Padding with spaces (Priority: P3)

**Goal:** `align_left`, `align_right` (FR-008, research R5).

**Independent test:** `tests/run.sh tests/cases/text` and `python3 tests/doc_conformance.py` pass.

- [X] T012 [P] [US5] Golden case `tests/cases/text/align.vo` (plain and multi-byte text, widths shorter and
  longer than the text, width 0, both functions). Confirm it fails now.
- [X] T013 [US5] `std/text.vo`: `align_left`, `align_right` with doc comments and examples.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T014 Use the new features where they fit (SC-001, SC-007): `examples/expenses` and
  `examples/status-board` (map counting with `get_or`, space padding with `align_*`); their outputs,
  tests and replays stay byte-identical (re-record the status-board demo recording only if its `http`
  events need the new field to replay, which they do not).
- [X] T015 Measure the reference growth (≤ 300 tokens, SC-005) and run the full gate: `make gate`,
  `make ARCH=aarch64 test`, `make ARCH=riscv64 test`, `make instrumented`, `make diff-asm COUNT=10000`
  (SC-006). Fill `results.md` (SC table, deviations: the R4 finding about `case ok`, follow-ups).

---

## Dependencies & Execution Order

- Setup → stories (independent of each other) → Polish.
- Within a story: the cases (T002, T005, T007, T009, T012) first, then the implementation.
- T011 needs T010; T014 needs T003 and T013; T015 needs everything.

### Parallel opportunities

- All five stories touch different files (`check_expr.c`/`rt_call.c`, `replay.c`, `rt_effects.c`,
  `parse.c`/`check.c`, `std/text.vo`); `src/vo.h` and `docs/reference.md` are shared: edit them in sequence.

## Implementation Strategy

MVP: US1 (`get_or`) and US2 (replay matching), the two P1 fixes; then US3, US4, US5; then the examples
and the gate.

---

description: "Task list for vo-lang spec 065: Expiring values"
---

# Tasks: Expiring values

**Input**: Design documents from `specs/065-expiring-values/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/std-time.md,
quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden case lands first and fails before its code.

**Organization**:
- Setup and Foundational: nothing to do. No new file, module, build entry or shared code.
- US1 (P1): the record, `expiring`, the read, and every way around it refused. This is the MVP.
- US2 (P2): `expiring_for` and `left_ms`. Needs US1's record.
- US3 (P3): store and rebuild. No library code; a golden case and guide text. Needs US1.
- Polish: reference row and its cut, guide, changelog, roadmap, the one full suite run.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Signatures, error text, misuse reports**: contracts/std-time.md and data-model.md. Decisions:
  research.md R1 to R9.
- **All library code goes in `std/time.vo`**, in the file's style: a `#` comment per exported item and
  per field, with an indented example line `#     EXPR is VALUE` that `tests/doc_examples.py` runs.
- **`src/` does not change.** `git diff --stat src/` is empty after every task.
- **Golden cases** are `tests/cases/time/NAME.vo` with `NAME.out` and `NAME.code`, and `NAME.cmd` when
  the command is not the default (model: `tests/cases/time/sleep_fixed.*`; for a check error,
  `tests/cases/time/now_missing_effect.*`). Write the `.vo` and the expected files by hand from the
  contract, see the case fail, then implement. Do not `--accept` output you have not compared with the
  contract.
- **No existing golden case may change**, except cases that print the reference or the documentation
  of `std.time`.
- `make quick AREA=time` between edits; `make test` once, in T013; the gate is not run. Never two
  suites at once.

---

## Phase 1: Setup

None.

## Phase 2: Foundational

None.

---

## Phase 3: User Story 1 - A value that must be asked for with the time (P1) 🎯 MVP

**Goal**: `time.Expiring[T]` exists, is made with `time.expiring(value, expires_at)`, and gives its
value only through `e.value_at(now)`.

**Independent test**: quickstart sections 1 and 2.

- [X] T001 [P] [US1] Write the golden case `tests/cases/time/expiring.vo` (+ `.out`, `.code` 0): a
  function `header(token: time.Expiring[str], now: int) -> result[str]` with no `uses` clause that
  returns `ok("Bearer " + try token.value_at(now))`; `main` (`uses console`) wraps `"secret"` with
  `time.expiring("secret", 1000)` and prints `header` at 0, 999, 1000 and 5000 through a `match`.
  Expected lines: `Bearer secret` twice, `expired at 1000, read at 1000`,
  `expired at 1000, read at 5000`. Also print: a read with a negative time (valid), `e.expires_at`,
  a record wrapped and read back (one field printed), and a nested
  `time.expiring(time.expiring(1, 5), 9)` read twice (each read peels one). See it fail (E0209 or the
  unknown-name report).
- [X] T002 [P] [US1] Write the golden case `tests/cases/time/expiring_misuse.vo` (+ `.out`, `.code`
  65): one line per row of the misuse table in contracts/std-time.md, all in one file so one pass
  reports them all: an `Expiring[str]` assigned to a `let s: str` (E0301); `e.value_at(5);` as a
  statement (E0404 with the fix `try `); `e == e` (E0308); `str(e)` and `format("{}", e)` (E0301);
  `json.to_json(e)` (E0328); and `concurrent_map([1, 2], 2, fn(n) => time.expiring(n, 5))` (expected
  E0314; the prototype did not reach this one, so confirm the code here and correct the contract's
  last table row and research R1 if it differs). Add a `--json` twin `expiring_misuse_json` with a
  `.cmd` so the E0404 fix is shown as an edit.
- [X] T003 [US1] In `std/time.vo` add, after the `DateTime` record: `export record Expiring[T]` with
  the fields `expires_at: int` ("Milliseconds since 1970-01-01T00:00:00Z. The first moment at which
  the value is no longer valid.") and `value_at: fn(int) -> result[T]` ("Given a time: `ok(value)`
  when the time is before `expires_at`, otherwise `err("expired at E, read at N")`."); the record's
  comment says the value can only be read with a time, that it cannot be compared, turned into JSON or
  sent to a worker because it holds a function, and that one built by hand answers with whatever
  function the program gave (R5). Add a private generic helper that returns `ok(value)` when
  `now < expires_at` and otherwise `err(format("expired at {}, read at {}", expires_at, now))`, and
  `export fn expiring[T](value: T, expires_at: int) -> Expiring[T]` whose `value_at` is
  `fn(now) => HELPER(value, expires_at, now)`. Comment examples, verbatim from the contract:
  `expiring("t", 1000).value_at(999) == ok("t") is true` and
  `expiring("t", 1000).value_at(1000) == err("expired at 1000, read at 1000") is true`. Add "and
  values that expire" to the module's first comment line.
- [X] T004 [US1] Run `make quick AREA=time`; T001 and T002 pass with the hand-written expected files.
  Run `python3 tests/std_names.py` (no name exported by two modules) and
  `python3 tests/doc_examples.py` (the two examples run).

**Checkpoint**: the MVP. A token held as `time.Expiring[str]` cannot be used without a time.

---

## Phase 4: User Story 2 - Good for a duration, and how long is left (P2)

**Goal**: `time.expiring_for(value, now, ms)` and `time.left_ms(e, now)`.

**Independent test**: quickstart section 3.

- [X] T005 [P] [US2] Write the golden case `tests/cases/time/expiring_for.vo` (+ `.out`, `.code` 0,
  `.cmd` running `vo test` if the default command is `run`; model: `tests/cases/time/sleep_test_hour.*`):
  a `test` with `let t = time.expiring_for("secret", 1000, 500);` and `expect_eq` on `t.expires_at`
  (1500), `time.left_ms(t, 1200)` (300), `time.left_ms(t, 1500)` (0), `time.left_ms(t, 9000)` (0, never
  negative), `t.value_at(1200 + 300)` (`err("expired at 1500, read at 1500")`: the margin is added to
  the time, no separate function), and on a value wrapped with `ms` 0 and with `ms` -1 read at the
  wrapping time (both expired). See it fail.
- [X] T006 [P] [US2] Write the golden case `tests/cases/time/expiring_overflow.vo` (+ `.out`, `.code`
  70): `main` calls `time.expiring_for(1, 9223372036854775807, 1)`; expected R0001 "Integer overflow"
  with the trace through `expiring_for` (model for the files: `tests/cases/time/sleep_overflow.*`).
- [X] T007 [US2] In `std/time.vo` add `export fn expiring_for[T](value: T, now: int, ms: int) ->
  Expiring[T]` returning `expiring(value, now + ms)` with no check on `ms` (R6), and
  `export fn left_ms[T](e: Expiring[T], now: int) -> int` returning `e.expires_at - now` when
  `now < e.expires_at`, else 0. Comment examples, verbatim from the contract:
  `expiring_for("t", 1000, 500).expires_at is 1500` and `left_ms(expiring("t", 1000), 400) is 600`.
  The comment of `expiring_for` says that `ms` of 0 or less gives a value that is already expired.
- [X] T008 [US2] Run `make quick AREA=time`; T005 and T006 pass.

---

## Phase 5: User Story 3 - Keeping an expiring value somewhere else (P3)

**Goal**: the store-and-rebuild pattern is shown to work and that replay is unaffected. No library code.

**Independent test**: quickstart sections 4 and 5.

- [X] T009 [P] [US3] Write the golden case `tests/cases/time/expiring_stored.vo` (+ `.out`, `.code` 0):
  a record `Stored { token: str; expires_at: int; }` built from `try t.value_at(now)` and
  `t.expires_at`, written with `json.to_json` and read back with `json.from_json`, then
  `time.expiring(s.token, s.expires_at)`; print the reads of the original and of the rebuilt value at
  0, 1499 and 1500 and show they are the same three lines each. Passes at once (it uses only US1);
  that is expected.
- [X] T010 [P] [US3] Add a recorded run under `tests/cases/replay/` (read how `tests/run.sh` runs
  `tests/cases/replay/all_inputs_clock` first and copy its layout): `main` with `uses console, clock`
  wraps a value with `time.expiring_for(v, now_ms(), 100)`, calls `time.sleep(200)`, reads at
  `now_ms()` and prints the result. Recorded with a fixed `--clock-ms` so the expected output is
  stable; the replay prints the same bytes with the same exit status (SC-003).

---

## Phase 6: Polish

- [X] T011 In `docs/reference.md`, append to the `std.time` row of the library table, verbatim:
  ``; `Expiring[T]` (`expiring`, `expiring_for`, `left_ms`): a value read only with a time``. Run
  `python3 tests/ref_examples.py`; if the budget (words × 1.3 ≤ 15,000) fails, cut the same number of
  words elsewhere in the reference without removing a fact, and write down which sentence was cut and
  the count before and after for T014.
- [X] T012 [P] In `site/docs/guide/standard-library.md`, at the end of the `std.time` section, add a
  subsection "Values that expire": two sentences on why (the value and its expiry moment travel
  together, and the checker makes every reader state a time), one executed example in the page's form
  (program plus `console` block with a fixed `--clock-ms`): fetch a token (a stand-in function that
  returns a string and a lifetime of 1,800,000 ms), keep it as `time.Expiring[str]`, and a function
  that reuses it while `value_at(now + 60000)` is `ok` and fetches again otherwise. Then the
  store-and-rebuild pattern of T009 in a few lines, and one sentence each for: no `==`, no JSON, not
  sent to a worker. Run `python3 tests/site_examples.py`.
- [X] T013 Run `make test` once. Then `git diff --stat src/` (empty) and `git status --short`
  (only the files this list names).
- [X] T014 [P] Add an entry under "Unreleased" in `CHANGELOG.md` ("**Values that expire (spec 065).**":
  the four names, that the read takes a time and returns a result, the three things an `Expiring`
  cannot do, and the reference's token count before and after) and a "065: expiring values
  (implemented)" paragraph in `docs/roadmap.md` in the form of its neighbours, including what was
  chosen against (a type-system feature, private fields, a module of its own) and "Not done": a cache,
  automatic refresh, use inside `std.security`. Set `**Status**` in
  `specs/065-expiring-values/spec.md` to `Implemented (DATE; not released)`.
- [ ] T015 SC-002, only when the user says to run it (it spends their tokens): five isolated runs of
  `claude -p` with no tools, an empty working directory and no user settings, each given the reference
  and the `std.time` page and asked for the "fetch a token, reuse it until it expires, fetch again"
  function; count the runs whose first answer passes `vo check`. Record the result in
  `specs/065-expiring-values/results.md`.

---

## Dependencies

- T001, T002 → T003 → T004 (US1).
- US2 (T005–T008) needs T003. US3 (T009, T010) needs T003; T010 also needs T007 (`expiring_for`).
- T011, T012 need T007. T013 after everything but T014 and T015. T015 needs T011 and T012.

## Parallel opportunities

- T001 and T002 (two case files).
- T005, T006 and T009 once T003 is in.
- T012 and T014 beside each other; T011 alone (it may need a cut that T014 reports).

## Implementation strategy

1. **MVP**: T001–T004. The type is usable and guarded; stop here if the rest is not wanted.
2. US2 (T005–T008), then US3 (T009, T010): each adds cases that pass on their own.
3. Polish in order: T011, T012, T013, T014. T015 on request.

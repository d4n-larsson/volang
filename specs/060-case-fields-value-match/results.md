# Results: Variant Cases With Fields, and `match` on Values

## Setup

- T004 baseline (commit `102e928`, x86-64): the reference is 11,538 words (~14,999 tokens of 15,000).
  `vo check`, best of 5: `examples/notes-api/main.vo` 17 ms, `examples/expenses/main.vo` 13 ms, the
  8,000-item program of `tests/perf.sh` 167 ms. JSON of the two payload records' cases:
  `time.LocalError.repeated(new time.Repeated { first: 0, second: 3600000 })` writes
  `{"repeated":{"first":0,"second":3600000}}` and reads back equal
  (`tests/cases/case_fields/json_baseline.vo`, whose output T029 keeps);
  `report.ExpenseError.bad_date(new report.DateProblem { date: "2026-02-30", reason: "no day 30" })`
  writes `{"bad_date":{"date":"2026-02-30","reason":"no day 30"}}`.

## SC-001

Pending.

## SC-002

Dropped with User Story 3 (research R8).

## SC-003

Pending.

## SC-004

Pending.

## SC-005

Pending.

## Verification

- US2 (T007–T012): value matches on `int` and `str` check, run and format; coverage lists each unrun
  value arm and an unrun `else` as branches (`tests/cases/value_match/coverage`); `vo test --mutate`
  makes no mutant of a value arm, as of a variant arm today (arms compile to compare-and-jump, not through
  the expression path that mutation rewrites). The tree-sitter grammar's `match_arm` and `else_arm` were
  extended with US2 (part of T026), since the grammar check parses every `.vo` file of the repository.

## Deviations

- FR-005 said a case with fields prints "with the same text" as the same case carrying a record; that
  text names the record, which no longer exists, so a field case prints in its construction syntax
  (`LoadError.malformed { path: "a", line: 3, reason: "x" }`); JSON is unchanged (research R4, spec
  amended).
- User Story 3 (the `if`/`else if` chain warning, FR-012, SC-002) dropped: no chain in `std/` or
  `examples/` qualifies (research R8, decided with the user on 2026-11-26).

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

2 of 2 payload records removed: `std.time.Repeated` (now the fields of `LocalError.repeated`) and
`examples/expenses/report.vo` `DateProblem` (now the fields of `ExpenseError.bad_date`). JSON identical
to T004's: `{"repeated":{"first":0,"second":3600000}}` (`tests/cases/case_fields/json_baseline`, output
unchanged) and `{"bad_date":{"date":"2026-02-30","reason":"no day 30"}}` (a probe program importing
`report`, as for T004), each reading back equal. The example's tests pass (5 of 5).

## SC-002

Dropped with User Story 3 (research R8).

## SC-003

Five new codes, each with a catalog entry (example and fixed version, `tests/catalog.py`), a codes row
and golden cases: E0216 (`case_fields/one_field`, fix checked by `tests/apply_fixes.py`), E0344
(`case_fields/read_only`), E0345 (`value_match/scrutinee`), E0414 (`value_match/no_else`, fix), E0415
(`value_match/else_on_variant`, fix). Golden cases: 17 in `tests/cases/case_fields/`, 9 in
`tests/cases/value_match/`, `fmt/case_fields`, `fmt/value_match`, `query/case_field_refs`,
`query/case_field_unknown`, `edit/rename_case_field`, `edit/add_field_case`, `library/repeated_fields`.

## SC-004

`tests/cases/case_fields/prop`: a `prop` over a variant with a field case fails, and the counterexample
prints in construction syntax, shrunk (`e = LoadError.malformed { path: "", line: 50 }`, 70 steps);
`--seed 1` twice gives identical output (timings masked). No change to `prop.c`.

## SC-005

The reference is 11,535 words (~14,995 tokens), 3 fewer than T004's 11,538. The Variants paragraph and
example (no `Size`; `rect { w: float; h: float; }`, `Shape.rect { w: 2.0, h: 1.5 }`), the `match` table
row and its details line say both forms. Tightened to pay for them: the First-token rule refers to the
keyword list at the top instead of repeating it, and "for example `vo explain E0301`" in the agent loop
was dropped.

Check times, best of 5, against the baseline binary (`102e928`) in the same session: notes-api 14 ms
(baseline 15), expenses 7 ms (5), the 8,000-item program of `tests/perf.sh` 280 ms (289): within noise.
(T004's 167 ms for the large program was measured in a different session; the same-session comparison
is the one that counts.)

## Verification

- US2 (T007–T012): value matches on `int` and `str` check, run and format; coverage lists each unrun
  value arm and an unrun `else` as branches (`tests/cases/value_match/coverage`); `vo test --mutate`
  makes no mutant of a value arm, as of a variant arm today (arms compile to compare-and-jump, not through
  the expression path that mutation rewrites). The tree-sitter grammar's `match_arm` and `else_arm` were
  extended with US2 (part of T026), since the grammar check parses every `.vo` file of the repository.
- US1 (T013–T020): field cases parse, check, run, print in construction syntax, compare, key, order,
  serialize and shrink through the hidden record (`tests/cases/case_fields/`, 18 cases); E0216 and E0344
  have catalog entries; `new Variant.case { ... }` is E0201 naming the field literal. The tree-sitter
  grammar's `case_declaration`, `variant_value` (with a declared conflict for `T.case {` then a block)
  and `named_type` (`Tree[int].node`) were extended with US1 (part of T026), for the same reason.

- Polish (T021–T035): fmt prints field cases and field literals (`tests/cases/fmt/case_fields`);
  `vo doc` shows `repeated { first: int; second: int; }` and gives a field case's `"fields"` in JSON;
  hidden records are not listed as items; `vo query`/`vo edit rename` take `Variant.case.field` (and
  `m.Variant.case.field`), with a suggestion for a misspelled field; `vo edit add-field` takes
  `Variant.case` with no code change (the hidden record is found by name); LSP document symbols nest a
  case's fields (`tests/lsp.py`). TextMate, Vim, Zed and Pygments needed no change: the new forms are
  made of tokens they already color (`tests/grammars.py`, `tests/site_lexer.py`).
  `tests/doc_conformance.py` accepts the `}` closing a field case. Version 0.6.0.

## Deviations

- FR-005 said a case with fields prints "with the same text" as the same case carrying a record; that
  text names the record, which no longer exists, so a field case prints in its construction syntax
  (`LoadError.malformed { path: "a", line: 3, reason: "x" }`); JSON is unchanged (research R4, spec
  amended).
- User Story 3 (the `if`/`else if` chain warning, FR-012, SC-002) dropped: no chain in `std/` or
  `examples/` qualifies (research R8, decided with the user on 2026-11-26).
- `time.Repeated`'s E0214 row (kind `LC_FIELDS`) has no fix: a use is `LocalError.repeated(new
  Repeated { ... })`, whose rewrite spans the case's parentheses, and no program in the repository's
  migration corpus names it. The message names the case to build. `tests/libchange_rows.py` lists the row
  as skipped.
- `tests/migrate_056.py`'s CHANGED entry needed no update: it migrates the example as it was at
  `8c8db3e`, which declares its own `DateProblem`.
- `vo edit add-field` on a record declared on one line (R7) was not changed: `vo fmt` never writes one.
- `new Variant.case { ... }` reports E0201 with its own message ("`LoadError.malformed` is a case's
  fields: build it with `LoadError.malformed { ... }`, without `new`.").

# Data Model: Agent-Friendly Testing

All of these live in one `vo test` process (or its forked children) and in its reports. Nothing is
stored on disk.

## Test entry (`--list`, selection, reports)

| Field | Type | Rule |
|---|---|---|
| id | str | `FILE::NAME`: FILE as reports print it, NAME the test's name (R1); unique per run |
| name | str | the declared name |
| test_kind | `test` \| `property` | `prop` items are `property` |
| file, start, end | position | the whole item |
| doc | str | the `#` lines directly above (above `@spec`), `# ` removed, joined by `\n`; `""` if none |
| uses | str | the declared clause in canonical form, `""` if none |
| spec | list of str | `@spec` IDs in source order |

Order: files in the order of PATH arguments (directories expanded sorted), items in source order. This
is also the run order.

## Selection

`selected(t) = filter(t) ∧ (no --test ∨ id ∈ tests) ∧ (no --spec ∨ ∃ s ∈ t.spec: s = X ∨ s starts with X + "/") ∧ (no --diff ∨ t ∈ impact_tests(diff))`

Validation: every `--test` ID names a test of the loaded program, and every `--spec` X matches some
test; otherwise T0011. Selection happens before any test runs.

## Test result (the `test` record, extended)

| Field | Present | Rule |
|---|---|---|
| id | always | as the entry |
| status | always | `pass`, `fail`, `error`, `timeout` (unchanged) |
| cause | status ≠ pass | one of the R7 values; derived from status, runtime code and the innermost frame |
| output | something printed | standard output of the test and its tasks, at most 64 KiB, then the cut note |
| failure.operands | failing `expect` on a comparison | `{text, value}` for each operand that is not a literal |
| failure.difference | failing `expect_eq` on a non-scalar or a string | `{path, expected, actual}` (+ `character`) |

State per test: the capture buffer is emptied when the test starts (and before a property's final
counterexample run). The frame flag `in_test` is set by `capture_trace` and read by the report.

## Coverage point

| Field | Rule |
|---|---|
| index k | assigned at compile time in compile order, < 65,536 |
| node | the block, arm, loop or declaration (its span is reported) |
| point | `function`, `lambda`, `then`, `else`, `if_false`, `case`, `loop` |
| function | the enclosing named function |
| hit | byte in a shared anonymous mapping, set to 1 by `OPC_COVER k` |

Points exist only for files with `std == 0` and `pkg == 0`, and only with `--coverage` or `--mutate`.
Report order: by file id, then start offset, then kind order as listed.

## Reach (mutation baseline)

A byte matrix `reach[test_index][function_index]`, shared like the hits, set by function-entry points
while test `test_index` runs during the baseline only; mutant children compile without points and
never write it. `tests_reaching(f) = { t | reach[t][f] = 1 }`, in run order.

## Mutant

| Field | Rule |
|---|---|
| node | the binary, unary, boolean or integer node changed |
| change | one row of the R10 table |
| function | the enclosing named function (a lambda's change belongs to its enclosing function) |
| text / mutated | source text of the node, and the same with the operator token or literal replaced |
| tests | `tests_reaching(function)` |
| result | `caught` (a test failed, errored or timed out, or the child was killed at its limit) or `survived` |

Generation order: file id, start offset, R10 row. The first `--max-mutants` are tried; the rest count
as `not_tried`. Functions not reached by any selected test produce no mutants.

States of a mutation run: baseline (tests run with reach) → refused (T0012, any baseline test not
passing) or mutating (one child per mutant, sequential) → summary.

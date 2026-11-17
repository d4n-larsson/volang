# Contract: Codes

## New

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| W0602 | `test_checks_nothing` | 0 (warning) | no | a `test` or `prop` whose body (nested blocks and lambdas included) has no `expect` or `expect_eq` call, no call to a function with `ensures` (its contract is the check; found in analysis of the contracts guide) and no hole; span: the test's name; message: "Test `NAME` checks nothing: it has no `expect` or `expect_eq`, so it can only fail by a runtime error." |
| W0603 | `constant_check` | 0 (warning) | no | `expect` of a literal, `not` of a literal or a comparison of two literals, or `expect_eq` of two literals (a literal: a number, string or bool constant, or a list, record or case made only of literals); span: the check; message: "This check always has the same outcome: it does not depend on the program." |
| T0011 | `unknown_test` | 64 (usage) | no | `vo test --test ID` naming no test, or `--spec ID` that no test names; message: "No test has the identifier `ID`." / "No test names `ID` in `@spec`.", plus "Closest: `A`, `B`, `C`." when there are candidates (up to three, by edit distance, ties in run order) |
| T0012 | `mutation_needs_passing_tests` | 1 (a test did not pass) | no | `vo test --mutate` when a selected test does not pass without changes; printed after the baseline's test records; message: "Mutation testing needs passing tests: N of the selected tests did not pass. Fix them first." |

Catalog entries (docs/errors.md): W0602 with a test that only prints and its fixed version with
`expect_eq`; W0603 with `expect_eq(3, 3)` and the version comparing a call's result; T0011 with a
misspelled identifier and the listed closest one; T0012 with a failing test and the order of work
(fix the test, then mutate).

Golden cases: `tests/cases/test/checks_nothing`, `tests/cases/test/constant_check`,
`tests/cases/test/unknown_id`, `tests/cases/test/unknown_spec`, `tests/cases/mutate/baseline_fails`,
plus `explain` cases for each.

## Existing codes, new triggers

| Code | New trigger |
|---|---|
| T0001 | `--list`, `--coverage`, `--mutate`, `--test`, `--spec` or `--max-mutants` with another command; `--list`, `--coverage` and `--mutate` combined; `--max-mutants` without `--mutate` or outside 1-100000 |
| T0007 | `vo test --diff FILE` with a diff that does not apply (as for `vo impact`) |
| T0003 | more than 65,536 coverage points in one run (research R8) |

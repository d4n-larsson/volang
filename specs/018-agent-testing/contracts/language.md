# Contract: Reference text

The language does not change: no new syntax, keywords or builtins visible to programs. The reference
(`docs/reference.md`) changes in four places. Shipped: +547 tokens (11,687 to 12,234), limit 700 (FR-018).

## Tests section, after "The exit status is 1 if any test did not pass."

> Each test has an identifier `FILE::NAME` (`main.vo::doubles`). `--list` checks and lists the selected tests (identifier, kind, location, `@spec` IDs, effects, the comment above) without running anything. `--test ID` runs exactly that test, `--spec ID` the tests naming `ID` (or `ID/...`), `--diff FILE` the tests `vo impact` reports. Options combine; an empty selection passes. A failing `expect` on a comparison shows each operand (for `and`, the false part); a failing `expect_eq` on larger values shows the first difference. What a test prints is kept with its report: under a test that did not pass, and as `output` in JSON. Tests share the working directory and run in listed order, so a test can see files another wrote.

Example (`vo-test` block, checked):

```vo-test
fn clamp(x: int, hi: int) -> int {
    if x > hi {
        return hi;
    }
    return x;
}

test "clamps" uses console {
    let limit = 5;
    print("checking");
    expect(clamp(7, limit) < limit);
}
```

```output
FAIL clamps (0ms)
  main.vo:11:5: `expect` failed: `clamp(7, limit) < limit` is false.
    clamp(7, limit): 5
    limit:           5
  output:
    checking
0 passed, 1 failed
```

## New subsection "Coverage and mutation testing", after the `@spec` paragraph

> `vo test --coverage` lists what the selected tests never ran in the program's own files: functions, branches (also an `if` whose condition was never false), `match` cases, loop bodies, then totals. `vo test --mutate` runs the tests (they must pass, T0012), then changes the functions they reach one step at a time (`<` to `<=`, `+` to `-`, `and` to `or`, `true` to `false`, dropping `not`, `n` to `n + 1`) and reruns the tests that reach it. Each change no test notices is a check you are missing: `SURVIVED main.vo:2:12 in is_adult: \`age >= 18\` -> \`age > 18\``. A failing, erroring or much slower test counts as noticing. Survivors are candidates: a change can behave like the original. `--max-mutants N` (default 500) caps the changes, `--diff` limits them to changed functions; neither option changes the exit status. W0602 is a test with no check, W0603 a check that cannot depend on the program.

## Command line

The `vo test` line becomes:

```text
vo test      [--json] [--timeout SEC] [--seed N] [--clock-ms N] [--max-memory MiB] [--mem-stats] [--filter TEXT] [--test ID] [--spec ID] [--diff FILE] [--list | --coverage | --mutate [--max-mutants N]] PATH...
```

## Machine-readable output

The **test** paragraph adds: "`id`; unless the status is `pass`, `cause`: one of `check_failed`,
`too_few_inputs`, `error_in_code`, `error_in_test`, `requires_broken_by_test`, `requires_broken_by_code`,
`ensures_broken`, `hole`, `outside_grant`, `timeout`, `out_of_memory`; `output` when the test printed;
`failure.operands` (`{text, value}`) and `failure.difference` (`{path, expected, actual}`, `character`
for strings)."

"Other kinds" adds: "`test_entry` (`--list`: `id`, `name`, `test_kind`, `file`, `start`, `end`, `doc`,
`uses`, `spec`), `coverage` (`file`, `start`, `end`, `point`, `function`), `coverage_summary`
(`functions`, `functions_run`, `branches`, `branches_run`), `mutant` (`file`, `start`, `end`,
`function`, `text`, `mutated`, `tests`) and `mutation_summary` (`mutants`, `caught`, `survived`,
`not_tried`)". The test summary adds `selected` (and `listed` for `--list`).

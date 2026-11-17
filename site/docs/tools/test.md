# vo test

`vo test` checks programs and runs every `test` and `prop` in them, reporting each as passed, failed, errored or timed out.

## Synopsis

```text
vo test [--json] [--filter TEXT] [--test ID] [--spec ID] [--diff FILE] [--timeout SEC] [--seed N]
        [--clock-ms N] [--max-memory MiB] [--mem-stats] [--trace FILE] [--trace-limit N]
        [--list | --coverage | --mutate [--max-mutants N]] PATH...
```

`PATH` is a `.vo` file or a directory (every `*.vo` file under it, recursively, in sorted order). A file
with tests needs no `main`. Each test runs with the grant of its own `uses` clause (see
[Tests](../reference/tests.md) and [Effects](../guide/effects.md)); `vo run` ignores tests.

The files are checked first, in test mode. If any has an error, the diagnostics and a summary are
printed, no test runs, and the status is 65. Otherwise the warnings are printed, then the tests run in
file order.

## Options

| Option | Meaning |
|---|---|
| `--json` | Report one JSON object per line (see [JSON output](json-output.md)). |
| `--filter TEXT` | Run only the tests and properties whose name contains `TEXT`. |
| `--test ID` | Run only the test with identifier `ID` (`FILE::NAME`, as `--list` shows it); repeatable. An unknown one is [T0011](../errors/T0011.md). |
| `--spec ID` | Run only the tests whose `@spec` names `ID` or an ID starting with `ID/`; repeatable. |
| `--diff FILE` | Run only the tests and properties that `vo impact` reports for the unified diff `FILE` (`-` is stdin). |
| `--list` | Check, then list the selected tests (identifier, kind, location, `@spec` IDs, effects, documentation comment) without running anything. |
| `--coverage` | After the tests, list the functions, branches, `match` cases and loop bodies of the program's own files that no selected test ran. |
| `--mutate` | After the tests (which must pass, [T0012](../errors/T0012.md)), change the code one small step at a time and list the changes no test notices. |
| `--max-mutants N` | With `--mutate`: try at most `N` changes (1 to 100000, default 500). |
| `--timeout SEC` | The time limit of **each** test, in whole seconds (default 10, `0`: none). A property's limit covers all its cases. |
| `--seed N` | The seed of `random_int` in every test, and of the inputs a `prop` generates. Default 1. |
| `--clock-ms N` | `now_ms` returns `N`, `N+1`, ... in every test. Default 0. |
| `--max-memory MiB` | The live-data limit of each test, 1 to 65536 MiB (default 512); more is [R0006](../errors/R0006.md). |
| `--mem-stats` | After each test, report its peak live size, total allocated and collections (a line on stdout, or a `memory` record after the `test` record with `--json`). |
| `--trace FILE` | Write a trace of every test's calls to `FILE` (a `test` record before each test, and before each case of a property). |
| `--trace-limit N` | With `--trace`: at most `N` records (1 to 100000000, default 100000); the rest are counted. |

Without `--seed` and `--clock-ms`, every test starts from seed 1 and clock 0, so a test run is
reproducible. `--record` is not available: only `vo run` records.

## Output

Results go to stdout. What a test prints is kept with its report: shown under a test that did not pass,
dropped for a passing one, and in the `output` field with `--json`. In text, one line per test:

- `PASS name (Nms)`; for a property, `PASS name (100 cases, Nms)`.
- `FAIL name (Nms)`, then the failed check's place and message: for a comparison in `expect`, each
  operand and its value; for `expect_eq`, `expected:`, `actual:` and, for larger values, the first
  difference (`first difference at [412].name: ...`). A property adds `counterexample: x = ...` lines (the input shrunk to a small one that
  fails the same way) and `shrunk in N steps`.
- `ERROR name (Nms)` for a runtime error, with the error and the call stack with values.
- `TIMEOUT name (Nms)` when the test ran past `--timeout` ([R0009](../errors/R0009.md)).
- A test with `@spec` IDs shows them after a failing line: `[FR-001]`. Before the summary, each ID gets
  a line: `spec FR-001: 2 tests, 1 not passing` (or `all passing`).
- The last line counts the results: `2 passed, 1 failed`, plus `N errored` and `N timed out` when there are any.

With `--json`, each test is a `test` record (`id`, `name`, `status`, `file`, `start`, `uses`, `ms`, `spec`,
the property's `prop`, `cases`, `discarded`, `shrinks`, `counterexample`, `output` when it printed, and
`cause` and `failure` unless it passed), each
`@spec` ID a `spec_result` record, and the last record a `summary` with `passed`, `failed`, `errored` and
`timed_out`:

```json
{"v":1,"kind":"test","id":"main.vo::clamps high values","name":"clamps high values","status":"fail","file":"main.vo","start":{"line":17,"col":1},"uses":"","ms":0,"spec":["FR-001"],"cause":"check_failed","failure":{"file":"main.vo","start":{"line":18,"col":5},"end":{"line":18,"col":36},"message":"`expect_eq` failed: the values differ.","expected":"10","actual":"0"}}
{"v":1,"kind":"spec_result","id":"FR-001","tests":1,"passed":0}
{"v":1,"kind":"summary","command":"test","errors":0,"warnings":0,"truncated":0,"files":1,"passed":0,"failed":1,"errored":0,"timed_out":0,"selected":1}
```

`ms` is the only field that varies from run to run. Values in `expected` and `actual` are written in vo
syntax (`"abc"`, `[1, 2]`, `ok(1)`).

## Exit statuses

| Status | When |
|---|---|
| 0 | Every test that ran passed (also when `--filter` matched none). |
| 1 | A test failed, errored or timed out. |
| 64 | Usage error ([T0001](../errors/T0001.md)). |
| 65 | A file has check errors; no test ran. |
| 66 | A path cannot be read ([T0002](../errors/T0002.md)). |
| 74 | The trace cannot be written ([T0009](../errors/T0009.md)). |

A runtime error or a timeout in a test is that test's result, not the command's status: the other tests
still run.

## Example

`clamp` has a bug in its second branch. The two tests prove requirement `FR-001`; the property checks
100 generated inputs:

```vo-test
fn clamp(x: int, lo: int, hi: int) -> int {
    if x < lo {
        return lo;
    }
    if x > hi {
        return lo;
    }
    return x;
}

@spec("FR-001")
test "clamps low values" {
    expect_eq(clamp(-5, 0, 10), 0);
}

@spec("FR-001")
test "clamps high values" {
    expect_eq(clamp(50, 0, 10), 10);
}

prop "stays in range" (x: int) {
    let c = clamp(x, 0, 10);
    expect(c >= 0 and c <= 10);
}
```

```output
PASS clamps low values (0ms)
FAIL clamps high values (0ms) [FR-001]
  main.vo:18:5: `expect_eq` failed: the values differ.
    expected: 10
    actual:   0
PASS stays in range (100 cases, 0ms)
spec FR-001: 2 tests, 1 not passing
2 passed, 1 failed
```

With the bug fixed, the whole run passes; `--filter` picks tests by name, `--seed` changes the inputs a
property tries, and `vo query specs` lists which tests prove which requirement without running them:

```vo
fn clamp(x: int, lo: int, hi: int) -> int {
    if x < lo {
        return lo;
    }
    if x > hi {
        return hi;
    }
    return x;
}

@spec("FR-001")
test "clamps low values" {
    expect_eq(clamp(-5, 0, 10), 0);
}

@spec("FR-001")
test "clamps high values" {
    expect_eq(clamp(50, 0, 10), 10);
}

prop "stays in range" (x: int) {
    let c = clamp(x, 0, 10);
    expect(c >= 0 and c <= 10);
}
```

```console
$ vo test main.vo
PASS clamps low values (0ms)
PASS clamps high values (0ms)
PASS stays in range (100 cases, 0ms)
spec FR-001: 2 tests, all passing
3 passed, 0 failed
$ vo test --filter high main.vo
PASS clamps high values (0ms)
spec FR-001: 1 test, all passing
1 passed, 0 failed
$ vo test --seed 42 --filter range main.vo
PASS stays in range (100 cases, 0ms)
1 passed, 0 failed
$ vo query specs main.vo
FR-001  main.vo:12:1 test "clamps low values"
FR-001  main.vo:17:1 test "clamps high values"
1 requirement ID
```

A test that hits a runtime error is reported as `ERROR` with the call stack and its values, and the
remaining tests still run:

```vo-test
fn first(xs: list[int]) -> int {
    return xs[0];
}

test "first of empty" {
    expect_eq(first(new list[int]), 0);
}
```

```output
ERROR first of empty (0ms)
  main.vo:2:12: runtime error[R0003]: Index 0 is out of bounds for length 0.
    at first (main.vo:2:12)
        xs: list[int] = []
    at test "first of empty" (main.vo:6:15)
0 passed, 0 failed, 1 errored
```

The error is [R0003](../errors/R0003.md). Writing tests, properties and `@spec` is covered in the
language guide's [Tests](../guide/tests.md) and [Properties](../guide/properties.md).

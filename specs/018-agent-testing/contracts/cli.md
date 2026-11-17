# Contract: `vo test` options and reports

## Command line

```text
vo test [--json] [--timeout SEC] [--seed N] [--clock-ms N] [--max-memory MiB] [--mem-stats]
        [--filter TEXT] [--test ID]... [--spec ID]... [--diff FILE]
        [--list | --coverage | --mutate [--max-mutants N]] PATH...
```

| Option | Meaning |
|---|---|
| `--list` | check, then list the selected tests and properties; nothing is compiled or run |
| `--test ID` | only the test with identifier `FILE::NAME` (repeatable: any of them) |
| `--spec ID` | only tests whose `@spec` names `ID` or an ID starting with `ID/` (repeatable) |
| `--diff FILE` | only the tests and properties `vo impact` reports for the unified diff (`-` is stdin) |
| `--coverage` | after the tests, list the functions and branches of the program's own files that no selected test ran |
| `--mutate` | after the tests (which must all pass), change the code one small step at a time and list changes no test catches |
| `--max-mutants N` | with `--mutate`: try at most N changes (default 500, 1-100000) |

Selection: `--filter`, `--test`, `--spec` and `--diff` combine as an intersection; repeated `--test`
or `--spec` are a union. An empty selection is a normal run with nothing selected (exit 0). Errors:
T0011 (unknown identifier or spec ID), T0007 (diff does not apply), T0001 (option misuse, see
codes.md).

Exit statuses are unchanged: 0, 1 (a test did not pass, or T0012), 64, 65, 66, 70 never (tests report
runtime errors as `error`), 124 never at top level, 125.

## Listing (`--list`)

Text:

```text
main.vo::doubles (test, main.vo:5:1)
  Doubling keeps the sign.
main.vo::distance is symmetric (property, main.vo:12:1) spec FR-003 uses console
2 tests
```

JSON, one record per entry then the summary:

```json
{"v":1,"kind":"test_entry","id":"main.vo::doubles","name":"doubles","test_kind":"test","file":"main.vo","start":{"line":5,"col":1},"end":{"line":8,"col":2},"doc":"Doubling keeps the sign.","uses":"","spec":[]}
{"v":1,"kind":"summary","command":"test","errors":0,"warnings":0,"truncated":0,"files":1,"listed":2}
```

`test_kind` is `"test"` or `"property"`; `doc` is `""` without a comment; `uses` is the clause as
written (canonical order) or `""`; `spec` lists IDs in source order.

## Test records (additions)

Every `test` record gains `"id"`. A record that is not `pass` gains `"cause"` (research R7) and, when the
test wrote to standard output, `"output"` (at most 64 KiB, then `... N more bytes not shown`).

`failure` gains, when they apply:
- `"operands": [{"text": "len(xs)", "value": "7"}, {"text": "limit", "value": "5"}]` (a failing
  comparison in `expect`; literals are left out);
- `"difference": {"path": "[412].name", "expected": "\"bob\"", "actual": "\"bab\""}` (a failing
  `expect_eq` on a non-scalar or long string; a missing side is `null`; for strings `"path": ""` and
  `"character": N`).

```json
{"v":1,"kind":"test","id":"main.vo::limits","name":"limits","status":"fail","file":"main.vo","start":{"line":4,"col":1},"uses":"","ms":0,"cause":"check_failed","failure":{"file":"main.vo","start":{"line":6,"col":5},"end":{"line":6,"col":30},"message":"`expect` failed: `len(xs) < limit` is false.","operands":[{"text":"len(xs)","value":"7"},{"text":"limit","value":"5"}]},"output":"checking 7 items\n"}
```

Text report of a failure (additions in the indented lines):

```text
FAIL limits (0ms)
  main.vo:6:5: `expect` failed: `len(xs) < limit` is false.
    len(xs): 7
    limit:   5
  output:
    checking 7 items
```

```text
FAIL people (1ms)
  main.vo:9:5: `expect_eq` failed: the values differ.
    expected: [new Person { name: "ann", age: 3 }, ...]
    actual:   [new Person { name: "ann", age: 3 }, ...]
    first difference at [412].name: expected "bob", actual "bab"
```

A missing element reads `actual (none) (lengths: actual 3, expected 4)`; a string difference
`first difference at character 120: expected "...", actual "..."` (excerpts of up to 40 characters).

Passing tests' output is not shown in text mode. The summary line is unchanged; with an empty selection
it is `0 tests selected`.

The JSON summary of a run gains `"selected"` (the number of tests and properties selected).

## Coverage (`--coverage`)

After the test records and the `@spec` summary, before the summary:

```text
not run: main.vo:12:1 function helper
not run: main.vo:20:12 else in classify
not run: main.vo:25:9 case in classify
not run: main.vo:3:5 if_false in clamp
coverage: 4 of 5 functions, 9 of 12 branches
```

```json
{"v":1,"kind":"coverage","file":"main.vo","start":{"line":20,"col":12},"end":{"line":22,"col":6},"point":"else","function":"classify"}
{"v":1,"kind":"coverage_summary","functions":5,"functions_run":4,"branches":12,"branches_run":9}
```

`point` is one of `function`, `lambda`, `then`, `else`, `if_false`, `case`, `loop`. Functions count
`function` and `lambda` points; branches count the rest. The span is the block (for `if_false`: the
whole `if` statement; for `function`: the declaration). The text names a function point
`function NAME` and every other point `POINT in NAME`. Only files outside the standard library and
packages are counted.

## Mutation (`--mutate`)

After the baseline's test records (printed as usual) and the `@spec` summary:

```text
SURVIVED main.vo:2:12 in is_adult: `age >= 18` -> `age > 18` (2 tests ran)
mutation: 12 changes, 11 caught, 1 survived, 0 not tried
survivors are candidates: a change can behave exactly like the original
```

```json
{"v":1,"kind":"mutant","file":"main.vo","start":{"line":2,"col":12},"end":{"line":2,"col":21},"function":"is_adult","text":"age >= 18","mutated":"age > 18","tests":["main.vo::adults","main.vo::children"]}
{"v":1,"kind":"mutation_summary","mutants":12,"caught":11,"survived":1,"not_tried":0}
```

The last line of the text report (the candidates note) appears only when something survived. If a
selected test did not pass: T0012 after its records, no mutation runs, exit 1.

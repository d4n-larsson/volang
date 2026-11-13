# Contract: Structured Output (schema version 1)

With `--json`, every report is a single line holding one JSON object. Every object has the fields `"v": 1` and `"kind"`. Fields may be added within version 1, but no existing field is ever removed or renamed. Consumers must ignore any field they don't know.

A **position** is `{"line": L, "col": C}`. Both values are 1-based, and `col` counts Unicode code points. Each span runs from `start` (inclusive) to `end` (exclusive).

## `diagnostic`

| Field | Type | Required | Notes |
|---|---|---|---|
| code | string | yes | `E####` or `W####` |
| name | string | yes | snake_case catalog name, e.g. `type_mismatch` |
| severity | `"error"` \| `"warning"` | yes | |
| file | string | yes | path as given on the command line or resolved via import |
| start, end | position | yes | |
| text | string | yes | exact source text covered by the span (may be empty) |
| message | string | yes | one sentence, no trailing newline |
| expected, found | string | no | e.g. types `"int"`, `"str"`, or tokens `"';'"` |
| fixes | array of fix | yes | may be empty |
| related | array of `{file, start, end, message}` | yes | may be empty |

A **fix** is `{"file", "start", "end", "replacement"}`. When `start == end`, the fix is an insertion. A diagnostic contains no overlapping fixes. Applying a diagnostic's first fix is expected to resolve that diagnostic (SC-003).

```json
{"v":1,"kind":"diagnostic","code":"E0201","name":"undefined_name","severity":"error","file":"main.vo","start":{"line":4,"col":11},"end":{"line":4,"col":16},"text":"totl","message":"`totl` is not defined.","fixes":[{"file":"main.vo","start":{"line":4,"col":11},"end":{"line":4,"col":16},"replacement":"total"}],"related":[{"file":"main.vo","start":{"line":2,"col":9},"end":{"line":2,"col":14},"message":"`total` is defined here."}]}
```

## `runtime_error`

| Field | Type | Notes |
|---|---|---|
| code, name | string | `R####`, e.g. `division_by_zero` |
| message | string | |
| file, start, end | | the expression that failed |
| trace | array of `{function, file, start}` | innermost frame first |

```json
{"v":1,"kind":"runtime_error","code":"R0002","name":"division_by_zero","message":"Division by zero.","file":"main.vo","start":{"line":7,"col":14},"end":{"line":7,"col":19},"trace":[{"function":"average","file":"main.vo","start":{"line":7,"col":14}},{"function":"main","file":"main.vo","start":{"line":12,"col":5}}]}
```

## `test`

| Field | Type | Notes |
|---|---|---|
| name | string | |
| status | `"pass"` \| `"fail"` \| `"error"` \| `"timeout"` | |
| file, start | | location of the `test` declaration |
| ms | integer | wall-clock duration (the only nondeterministic field) |
| failure | object | present unless the status is `pass`: `{file, start, end, message, expected?, actual?}`. For `error`, `message` holds the runtime error and `code` is added |

`expected` and `actual` contain values rendered in vo literal syntax (`"abc"`, `[1, 2]`, `new Point { x: 1, y: 2 }`, `Shape.circle(1.0)`, `ok(1)`). Maps have no literal syntax, so they use the display form `{k: v, …}`, listed in insertion order.

## `toolchain_error`

This record is for T codes, which have no source span.

| Field | Type | Notes |
|---|---|---|
| code, name | string | `T0001` usage_error, `T0002` cannot_read_input, `T0003` internal_error |
| message | string | for T0001, this includes the usage summary |
| path | string | optional; the input that could not be read (T0002) |

```json
{"v":1,"kind":"toolchain_error","code":"T0002","name":"cannot_read_input","message":"Cannot read `does/not/exist.vo`: no such file.","path":"does/not/exist.vo"}
```

## `explain`, `reference`, `version`

- `explain`: `{"v":1,"kind":"explain","code":"E0401","name":"non_exhaustive_match","fixable":true,"text":"<the catalog section as markdown>"}`
- `reference`: `{"v":1,"kind":"reference","text":"<docs/reference.md>"}`
- `version`: `{"v":1,"kind":"version","version":"0.1.0","schema":1}`

## `fmt`

`{"v":1,"kind":"fmt","file":"a.vo","changed":true}`. One record per file processed.

## `summary`

The final record of every `check`, `test` and `fmt` run, and of `run` when it fails before the program starts:

```json
{"v":1,"kind":"summary","command":"check","errors":3,"warnings":1,"truncated":0,"files":2}
```

`test` adds `passed`, `failed`, `errored`, `timed_out`. `fmt` adds `changed`. `truncated` is the number of diagnostics left out because of the cap of 50.

## Human format (default)

```
main.vo:4:11: error[E0201]: `totl` is not defined.
   4 |     print(str(totl));
     |               ^^^^
  fix: replace with `total`
  note: main.vo:2:9: `total` is defined here.
```

A runtime error prints a `trace:` list with one `  at function (file:line:col)` line per frame. The test runner prints `PASS name` or `FAIL name` lines, a failure block for each failed test, and a summary line.

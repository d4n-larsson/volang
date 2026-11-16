# JSON output

With `--json`, every command reports in JSON lines, one object per line, and traces and recordings use the same format; this page lists every record kind with a real example.

## Rules

- `--json` works with every command, anywhere after the command name (`vo check main.vo --json` too). It
  switches `vo`'s own reports to JSON; the program's output is never wrapped.
- Every record is one line and starts with `{"v":1,"kind":...}`: `v` is the schema version, `kind` says
  which record it is.
- A position is `{"line": L, "col": C}`, both 1-based, with `col` counted in characters (Unicode code
  points). A span runs from `start` (inclusive) to `end` (exclusive). `file` is the path as given on the
  command line or as resolved through imports.
- Values (arguments, locals, `expected`, `actual`) are strings in vo syntax: `"abc"`, `[1, 2]`,
  `new Point { x: 1, y: 2 }`, `Shape.circle(1.0)`, `ok(1)`, maps as `{k: v}`, function values as their
  type `<fn(int) -> int>`. They are shortened to 200 bytes, ending in `...`.
- Reports go to stdout, except for `vo run` and `vo replay`, whose reports go to stderr (stdout belongs
  to the program). A usage error is JSON when `--json` appears anywhere before `--`, even if the rest of
  the command line could not be read.
- **Ignore fields you do not know.** Read records by `kind` and skip kinds you do not handle.

The reference's [Machine-readable output](../reference/machine-readable-output.md) section is the short
version of this page.

## Versions

The schema version is 1, and `vo version --json` reports it as `"schema":1` (see [vo version](version.md)).
Within version 1:

- fields and record kinds may be **added**;
- no field is ever **removed or renamed**.

So a reader written for version 1 keeps working with every later `vo` of version 1, as long as it ignores
the fields and kinds it does not know. Fields marked "optional" below are left out when they do not apply.

## Reports

| Kind | From | Fields |
|---|---|---|
| `diagnostic` | `check`, `run`, `test`, `fmt`, `effects`, `query`, `impact`, `edit`, `doc` | `code`, `name`, `severity`, `file`, `start`, `end`, `text`, `message`, optional `expected` and `found`, `fixes`, `related` |
| `summary` | the last record of most commands | `command`, `errors`, `warnings`, `truncated`, `files`, and the command's counts |
| `runtime_error` | `run`, `replay` (stderr) | `code`, `name`, `message`, `file`, `start`, `end`, `trace`, and extra fields for some codes |
| `toolchain_error` | every command | `code`, `name`, `message`, optional `path` and `related` |
| `test` | `test` | `name`, `status`, `file`, `start`, `uses`, `ms`, optional `spec`, property fields, `failure` |
| `spec_result` | `test` | `id`, `tests`, `passed` |
| `memory` | `run`, `test`, `replay` with `--mem-stats` | `command`, optional `test`, `peak_live`, `allocated`, `collections`, `limit` |
| `fmt` | `fmt` | `file`, `changed` |
| `capabilities` | `effects` | `entry`, `file`, `uses` |
| `effect_change` | `effects --update` | `change`, `file`, `function`, `effects`, `before`, `after`, `via`, optional `old_function` |
| `ref` | `query callers`, `query refs` | `target`, `role`, `file`, `start`, `end`, `text`, optional `in` |
| `type_at` | `query type-at` | `file`, `start`, `end`, `text`, `type`, optional `name` and `declaration` |
| `spec` | `query specs` | `id`, `tests` |
| `impact` | `impact` | `name`, `role`, optional `file`, `start` and `via` |
| `edit` | `edit` | `file`, `start`, `end`, `text`, `replacement` |
| `replay` | `replay` (stderr) | `program`, `changed` |
| `response_changed` | `replay` of a server (stderr) | `request`, `recorded_status`, `status`, `recorded_body`, `body` |
| `doc` | `doc` | `module`, `name`, `item`, `signature`, `description`, optional `example`, and per item kind `tparams`, `params`, `returns`, `effects`, `fields`, `cases` |
| `package` | `package` | `name`, `change`, `directory`, optional `hash` and `old_hash` |
| `explain` | `explain` | `code`, `name`, `fixable`, `text` |
| `reference` | `reference` | `text` |
| `version` | `version` | `version`, `schema` |

### diagnostic

An error or warning found by the checker. `severity` is `"error"` or `"warning"`; `text` is the source in
the span. A fix is `{file, start, end, replacement}`, an insertion when `start == end`; applying the first
fix resolves the diagnostic, and the fixes of one diagnostic never overlap. A related place is
`{file, start, end, message}`. See [vo check](check.md).

```json
{"v":1,"kind":"diagnostic","code":"E0301","name":"type_mismatch","severity":"error","file":"main.vo","start":{"line":4,"col":11},"end":{"line":4,"col":16},"text":"total","message":"Expected `str`, found `int`.","expected":"str","found":"int","fixes":[{"file":"main.vo","start":{"line":4,"col":11},"end":{"line":4,"col":16},"replacement":"str(total)"}],"related":[]}
```

### summary

The last record of `check`, `test`, `fmt`, `effects`, `query`, `impact`, `edit` and `doc`, and of `run`
when it stops before the program starts. `errors` and `warnings` count the program's diagnostics; at most
50 diagnostics are listed and `truncated` counts the rest; `files` counts the files checked or loaded.

```json
{"v":1,"kind":"summary","command":"check","errors":1,"warnings":1,"truncated":0,"files":1}
```

The commands add their own counts: `test` adds `passed`, `failed`, `errored`, `timed_out`; `fmt` adds
`changed`; `query` adds `results`; `impact` adds `changed`, `added`, `removed`, `affected`, `tests`; `edit`
adds `changes`, `changed`, `written`; `doc` adds `items`.

```json
{"v":1,"kind":"summary","command":"test","errors":0,"warnings":0,"truncated":0,"files":1,"passed":0,"failed":1,"errored":0,"timed_out":0}
```

### runtime_error

A run stopped by an error, on stderr. `trace` lists the frames innermost first:
`{function, file, start, locals}`, where `locals` are `{name, type, value}` of the parameters and of the
locals declared before the failing statement. A timeout ([R0009](../errors/R0009.md)) has no `locals`.

```json
{"v":1,"kind":"runtime_error","code":"R0002","name":"division_by_zero","message":"Division by zero.","file":"main.vo","start":{"line":2,"col":12},"end":{"line":2,"col":25},"trace":[{"function":"average","file":"main.vo","start":{"line":2,"col":12},"locals":[{"name":"total","type":"int","value":"10"},{"name":"count","type":"int","value":"0"}]},{"function":"main","file":"main.vo","start":{"line":7,"col":15},"locals":[]}]}
```

[R0010](../errors/R0010.md) (an effect outside the grant) adds `effect`, `target`, `allowed` and
`declared_by`:

```json
{"v":1,"kind":"runtime_error","code":"R0010","name":"capability_denied","message":"file.read(\"data/../secret.txt\") is not allowed: `load` declares file.read(\"data/\").","file":"main.vo","start":{"line":2,"col":12},"end":{"line":2,"col":27},"effect":"file.read","target":"data/../secret.txt","allowed":["data/"],"declared_by":"load","trace":[{"function":"load","file":"main.vo","start":{"line":2,"col":12},"locals":[{"name":"path","type":"str","value":"\"data/../secret.txt\""}]},{"function":"main","file":"main.vo","start":{"line":6,"col":15},"locals":[]}]}
```

[R0006](../errors/R0006.md) (out of memory) adds `in_use` and `limit`, in bytes:

```json
{"v":1,"kind":"runtime_error","code":"R0006","name":"out_of_memory","message":"The program holds 1.7 MiB of live data, over the 1 MiB limit.","file":"big.vo","start":{"line":4,"col":17},"end":{"line":4,"col":22},"in_use":1703968,"limit":1048576,"trace":[{"function":"grow","file":"big.vo","start":{"line":4,"col":17},"locals":[{"name":"n","type":"int","value":"30"},{"name":"s","type":"str","value":"\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..."}]},{"function":"main","file":"big.vo","start":{"line":10,"col":15},"locals":[]}]}
```

The value of `s` shows the 200-byte limit on values.

### toolchain_error

A T code: a problem with the command line or the toolchain's inputs, which have no source span. `path`
names an input that could not be read; `related` lists the declarations of an ambiguous target.

```json
{"v":1,"kind":"toolchain_error","code":"T0002","name":"cannot_read_input","message":"Cannot read `nope.vo`.","path":"nope.vo"}
{"v":1,"kind":"toolchain_error","code":"T0005","name":"ambiguous_target","message":"`helper` is declared in several files; name one with FILE:LINE:COL.","related":[{"file":"util/a.vo","start":{"line":1,"col":11},"end":{"line":1,"col":17}},{"file":"util/b.vo","start":{"line":1,"col":11},"end":{"line":1,"col":17}}]}
```

### test

One per test or property, from [vo test](test.md). `status` is `"pass"`, `"fail"`, `"error"` or
`"timeout"`; `start` is the test's declaration; `uses` its grant (`""` for none); `ms` the time taken, the
only field that varies between runs; `spec` its `@spec` IDs, sorted. A property adds `prop` (`true`),
`cases`, `discarded` (inputs `assume` rejected, when any), and on failure `shrinks` and `counterexample`
(`[{name, value}]`). Unless the status is `pass`, `failure` is `{file, start, end, message, expected?,
actual?}`, plus `code` for an error or timeout and `trace` for an error.

```json
{"v":1,"kind":"test","name":"doubles negatives","status":"fail","file":"main.vo","start":{"line":11,"col":1},"uses":"","ms":0,"spec":["FR-001"],"failure":{"file":"main.vo","start":{"line":12,"col":5},"end":{"line":12,"col":30},"message":"`expect_eq` failed: the values differ.","expected":"-3","actual":"-2"}}
{"v":1,"kind":"test","name":"half of an even number doubles back","status":"pass","file":"p.vo","start":{"line":5,"col":1},"uses":"","ms":1,"prop":true,"cases":100,"discarded":102}
```

### spec_result

One per `@spec` ID after the tests, before the summary: how many tests name it and how many passed.

```json
{"v":1,"kind":"spec_result","id":"FR-001","tests":2,"passed":1}
```

### memory

With `--mem-stats`: peak live data, total allocated (bytes), collections and the limit. For `vo test`,
one per test, right after its `test` record, with `test` naming it.

```json
{"v":1,"kind":"memory","command":"run","peak_live":432,"allocated":432,"collections":0,"limit":536870912}
{"v":1,"kind":"memory","command":"test","test":"loops","peak_live":1072,"allocated":1072,"collections":0,"limit":536870912}
```

### fmt

One per file [vo fmt](fmt.md) processed, changed or not.

```json
{"v":1,"kind":"fmt","file":"main.vo","changed":true}
```

### capabilities

From [vo effects](effects.md): the grant of `main` and of each test.

```json
{"v":1,"kind":"capabilities","entry":"test \"nothing to save\"","file":"main.vo","uses":""}
```

### effect_change

From `vo effects --update`, one per change written to `vo.effects`. `change` is `added`, `removed`,
`widened`, `narrowed` or `renamed` (a rename adds `old_function`); `via` lists the calls that brought the
effect in, `{function, calls, file, start}`.

```json
{"v":1,"kind":"effect_change","change":"added","file":"main.vo","function":"main","effects":"clock","before":"uses console","after":"uses console, clock","via":[{"function":"main","calls":"label","file":"main.vo","start":{"line":5,"col":11}}]}
```

### ref

From `vo query callers` and `vo query refs` (see [vo query](query.md)). `role` is `declaration`, `call`,
`value`, `type`, `import` or `use`; `in` is the enclosing function, `test "name"` or `prop "name"`, absent
at top level.

```json
{"v":1,"kind":"ref","target":"tax","role":"call","file":"pricing.vo","start":{"line":14,"col":38},"end":{"line":14,"col":41},"text":"tax","in":"total"}
```

### type_at

From `vo query type-at`. `name` and `declaration` (a span) only when the position is on a name.

```json
{"v":1,"kind":"type_at","file":"main.vo","start":{"line":6,"col":9},"end":{"line":6,"col":14},"text":"items","type":"list[Item]","name":"items","declaration":{"file":"main.vo","start":{"line":6,"col":9},"end":{"line":6,"col":14}}}
```

### spec

From `vo query specs`, one per ID: the tests and properties that name it, `{name, kind, file, start}`.

```json
{"v":1,"kind":"spec","id":"FR-001","tests":[{"name":"total adds tax","kind":"test","file":"main.vo","start":{"line":11,"col":1}}]}
```

### impact

From [vo impact](impact.md). `role` is `changed`, `added`, `removed` (no `file` or `start`), `affected`,
`test` or `ignored` (a file of the diff outside the program, its path as `name`); `via` names the function
whose call brought it in.

```json
{"v":1,"kind":"impact","name":"test \"total adds tax\"","role":"test","file":"main.vo","start":{"line":11,"col":1},"via":"total"}
```

### edit

From [vo edit](edit.md), one per change: a fix plus `text`, the source replaced (empty for an insertion).

```json
{"v":1,"kind":"edit","file":"main.vo","start":{"line":11,"col":33},"end":{"line":11,"col":33},"text":"","replacement":", depth: 1"}
```

### replay and response_changed

From [vo replay](replay.md), on stderr, before the run when the program differs from the recorded one,
and for each server response that changed.

```json
{"v":1,"kind":"replay","program":"main.vo","changed":true}
{"v":1,"kind":"response_changed","request":"GET /x","recorded_status":200,"status":200,"recorded_body":"hi /x","body":"hello /x"}
```

### doc

From [vo doc](doc.md), one per item. `item` is `fn`, `record`, `variant`, `builtin` or `module`.

```json
{"v":1,"kind":"doc","module":"std.text","name":"pad_left","item":"fn","signature":"fn pad_left(s: str, width: int, fill: str) -> str","tparams":[],"params":[{"name":"s","type":"str"},{"name":"width","type":"int"},{"name":"fill","type":"str"}],"returns":"str","effects":[],"description":"s padded on the left with fill (one character) to width characters.","example":"pad_left(\"7\", 3, \"0\") is \"007\""}
{"v":1,"kind":"doc","module":"builtins","name":"len","item":"builtin","signature":"(x: str | list[T] | map[K, T]) -> int","description":"for `str`: bytes, not characters"}
```

### package

From [vo package](package.md). `change` is `added`, `updated` (with `old_hash`), `unchanged` or `removed`.

```json
{"v":1,"kind":"package","name":"money","change":"unchanged","directory":"vendor/money","hash":"sha256:1e5b240df8daf715941d8af2629ab6404c7f9d1d369e493f0e9eb2d01fc162c3"}
```

### explain, reference, version

From [vo explain](explain.md), [vo reference](reference.md) and [vo version](version.md). `fixable` is
`true` when the entry's `Fixable` line says `yes` or `sometimes`; `text` is Markdown.

```json
{"v":1,"kind":"explain","code":"T0004","name":"unknown_target","fixable":false,"text":"## T0004 unknown_target\n\nA query or edit names something that is not in the program.\n\n..."}
{"v":1,"kind":"reference","text":"# vo-lang Reference\n\n..."}
{"v":1,"kind":"version","version":"0.1.0","schema":1}
```

(`text` shortened here.)

## Traces

`vo run --trace FILE`, `vo test --trace FILE` and `vo replay --trace FILE` write these records, in the
order things happen. Only vo functions are traced (named functions, lambdas, `main`, tests and
properties), not builtins. The same program, input and options give the same bytes: there are no times.
The rules are in the reference's [Traces and replay](../reference/traces-and-replay.md).

| Kind | Fields | When |
|---|---|---|
| `call` | `depth` (1 for `main` or a test body), `function`, `file` and `start` of the call site (absent at depth 1), `args` (`[{name, value}]`) | a function starts |
| `return` | `depth`, `function`, `value` (absent for `void`) | a function returns, also through `try` |
| `error` | `code`, `name`, `message`, `file`, `start` | a runtime error ends the run or the test; the active calls get no `return` |
| `test` | `name` (`test "x"` or `prop "x"`) | `vo test`, before each test and before each case of a property |
| `end` | `records` (written), `omitted` (counted after `--trace-limit`), `status` (`vo run` and `vo replay` only) | always last |

From `vo run`, a run that divided by zero:

```json
{"v":1,"kind":"call","depth":1,"function":"main","args":[]}
{"v":1,"kind":"call","depth":2,"function":"average","file":"main.vo","start":{"line":7,"col":15},"args":[{"name":"total","value":"10"},{"name":"count","value":"0"}]}
{"v":1,"kind":"error","code":"R0002","name":"division_by_zero","message":"Division by zero.","file":"main.vo","start":{"line":2,"col":12}}
{"v":1,"kind":"end","records":3,"omitted":0,"status":70}
```

From `vo test`, with the `test` records and `return` values:

```json
{"v":1,"kind":"test","name":"test \"doubles\""}
{"v":1,"kind":"call","depth":1,"function":"test \"doubles\"","args":[]}
{"v":1,"kind":"call","depth":2,"function":"double","file":"main.vo","start":{"line":7,"col":15},"args":[{"name":"x","value":"4"}]}
{"v":1,"kind":"return","depth":2,"function":"double","value":"8"}
{"v":1,"kind":"return","depth":1,"function":"test \"doubles\""}
{"v":1,"kind":"end","records":5,"omitted":0}
```

A failing `expect` ends a test without a `return` or `error` record: it is not a runtime error.

## Recordings

`vo run --record FILE` writes a header, one record per input the program received, in order, and an
`end` record; [vo replay](replay.md) feeds them back. Text values are JSON strings, with control bytes as
`\u00XX`; a value that is not valid UTF-8 has every byte from 0x80 as `\u00XX` too and `"bytes":true` on
its record.

| Kind | Fields |
|---|---|
| `recording` | `program` (the entry file as given to `vo run`), `fingerprint` (16 hex digits over the program's files), `seed` (the seed used), `clock_ms` (the `--clock-ms` value or `null`), `args` |
| `read_line` | `result` `"ok"` with `value`, or `"err"` with `value` `"eof"` |
| `read_all` | `value` |
| `read_file` | `path`, `result` `"ok"` with `value`, `"err"` with `value` (the error), or `"denied"` |
| `write_file` | `path`, `result` `"ok"`, `"err"` with `value`, or `"denied"` (the content is not recorded) |
| `env` | `name`, `result` `"ok"` with `value`, or `"err"` |
| `now_ms` | `value` |
| `http` | `host`, `port`, `request` (`"GET /status"`), `result` `"ok"` with `value` (the raw response) or `"err"` with `value` |
| `listen` | `address`, `result` `"ok"` with `value` or `"err"` with `value` (the reason) |
| `request` | `value`: the raw request a server received (`""`: the server was stopped) |
| `response` | `value`: the raw response the server sent |
| `end` | `status`: the exit status of the run |

`random_int` needs no records: it is fixed by `seed`. A run that read stdin, a variable and the clock,
and wrote a file:

```json
{"v":1,"kind":"recording","program":"main.vo","fingerprint":"6d85c3c8c5b1f859","seed":147262998034322,"clock_ms":5,"args":[]}
{"v":1,"kind":"read_all","value":"a\u000ab\u000a"}
{"v":1,"kind":"env","name":"LIMIT","result":"ok","value":"3"}
{"v":1,"kind":"write_file","path":"out/copy.txt","result":"ok"}
{"v":1,"kind":"now_ms","value":5}
{"v":1,"kind":"end","status":0}
```

A run that read a line and a file:

```json
{"v":1,"kind":"read_line","result":"ok","value":"Ada"}
{"v":1,"kind":"read_file","path":"greeting.txt","result":"ok","value":"hello\u000a"}
```

An HTTP client, then a server that answered one request and was stopped:

```json
{"v":1,"kind":"http","host":"localhost","port":8765,"request":"GET /x","result":"ok","value":"HTTP/1.1 200 OK\u000d\u000aContent-Length: 5\u000d\u000aConnection: close\u000d\u000a\u000d\u000ahi /x"}
{"v":1,"kind":"listen","address":"8765","result":"ok","value":5}
{"v":1,"kind":"request","value":"GET /x HTTP/1.1\u000d\u000aHost: localhost:8765\u000d\u000aConnection: close\u000d\u000aUser-Agent: vo\u000d\u000a\u000d\u000a"}
{"v":1,"kind":"response","value":"HTTP/1.1 200 OK\u000d\u000aContent-Length: 5\u000d\u000aConnection: close\u000d\u000a\u000d\u000ahi /x"}
{"v":1,"kind":"request","value":""}
{"v":1,"kind":"end","status":0}
```

A recording that is cut short (a run killed by a signal leaves no `end`) or misses what an event needs is
[T0008](../errors/T0008.md) when replayed.

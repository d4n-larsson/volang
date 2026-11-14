# Contract: Reports

All changes are additive within schema 1 (Principle II). Existing outputs of programs without holes, properties or contracts are byte-identical.

## W0601 diagnostic

**Text** (`vo check main.vo`). The `in scope:` line comes after the usual snippet. With nothing in scope it reads `in scope: nothing`.

```text
main.vo:2:5: warning[W0601]: Hole: `"parse the header"` needs a value of type `int`.
   2 |     todo("parse the header");
     |     ^^^^^^^^^^^^^^^^^^^^^^^^
    in scope: line: str
0 errors, 1 warning (1 hole)
```

**JSON** (`vo check --json`). `expected` already exists; `scope` is new.

```json
{"v":1,"kind":"diagnostic","code":"W0601","name":"hole","severity":"warning","file":"main.vo","start":{"line":2,"col":5},"end":{"line":2,"col":29},"text":"todo(\"parse the header\")","message":"Hole: `\"parse the header\"` needs a value of type `int`.","expected":"int","scope":[{"name":"line","type":"str"}],"fixes":[],"related":[]}
```

## Check summary

```json
{"v":1,"kind":"summary","command":"check","errors":0,"warnings":1,"truncated":0,"files":1,"holes":1}
```

- `holes` appears in `check` summaries only when N > 0, so the summaries of programs without holes are unchanged (FR-027).
- **Text:** the summary line is unchanged when there are no holes. Otherwise ` (1 hole)` / ` (N holes)` is appended to it.

## Runtime errors

R0011, R0012 and R0013 use the existing `runtime_error` record and text format.

| Code | Name | Message | Span | Trace starts at |
|---|---|---|---|---|
| R0011 | `hole_reached` | ``Hole reached: parse the header.`` | the `todo(...)` call | the function containing the hole |
| R0012 | `precondition_failed` | ``Precondition `n >= 0` of `isqrt` does not hold.`` | the call expression in the caller | the caller |
| R0013 | `postcondition_failed` | ``Postcondition `ret * ret <= n` of `isqrt` does not hold.`` | the `return`, `try`, or closing `}` | the function |

The condition is quoted as its source text, exactly as written, with whitespace runs collapsed to single spaces.

## `vo test`: properties

**Passing.** The text line shows the case count; the JSON adds `prop` and `cases`.

```text
PASS reverse twice (100 cases, 14ms)
```

```json
{"v":1,"kind":"test","name":"reverse twice","status":"pass","file":"main.vo","start":{"line":9,"col":1},"ms":14,"prop":true,"cases":100}
```

**Failing.** The existing failure lines, then one `counterexample` line per parameter, then the shrink count.

```text
FAIL dedupe keeps distinct (37 cases, 52ms)
  main.vo:14:5: `expect_eq` failed: the values differ.
    expected: 4
    actual:   3
    counterexample: xs = [0, 1, 2, 3]
    shrunk in 23 steps
```

```json
{"v":1,"kind":"test","name":"dedupe keeps distinct","status":"fail","file":"main.vo","start":{"line":12,"col":1},"ms":52,"prop":true,"cases":37,"shrinks":23,"counterexample":[{"name":"xs","value":"[0, 1, 2, 3]"}],"failure":{"file":"main.vo","start":{"line":14,"col":5},"end":{"line":14,"col":38},"message":"`expect_eq` failed: the values differ.","expected":"4","actual":"3"}}
```

- **`cases`** counts the accepted cases run, including the failing one.
- **`discarded`** appears when `assume` discarded any inputs.
- **Shrinking cut short:** when the deadline stopped shrinking, `"shrink_stopped":true` is added, and the text says `shrinking stopped at the time limit after K steps`.
- **Runtime errors:** a property failing with a runtime error has status `error`. Its failure record is the usual one, with `code`, plus the counterexample.
- **Timeouts:** a property that times out while not shrinking has status `timeout`, with the running input as `counterexample` and `"shrinks":0`.

**Too many discards.** Status `fail`, with no counterexample:

```text
FAIL small inputs only (12 cases, 40ms)
  main.vo:20:1: too many inputs were discarded by `assume` (12 of 1000 accepted)
```

**Memory.** `--mem-stats` gives one memory report per property, covering all its cases.

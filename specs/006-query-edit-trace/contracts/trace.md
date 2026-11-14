# Contract: Trace Files

`--trace FILE` writes JSON lines (schema 1). Records appear in execution order. Only program functions (named functions, lambdas, `main`, tests and properties) are traced; builtins are not, but program functions they call (`map(xs, f)`) are.

| kind | Fields | When |
|---|---|---|
| `call` | `depth` (1 = `main` or the test body), `function`, `file`/`start` of the call site (absent for `main` and test bodies), `args`: `[{name, value}]` | a function starts |
| `return` | `depth`, `function`, `value` (absent for `void`) | a function returns, including through `try` (the value is the propagated `err(...)`) |
| `test` | `name` (`test "x"` / `prop "x"`) | `vo test`, before a test's calls; for a property, before each case |
| `error` | `code`, `name`, `message`, `file`/`start` (when the error has a place) | a runtime error ends the run or the test; the calls without a `return` were active (they get no `return` record) |
| `end` | `records` (written), `omitted` (counted after the limit), `status` (exit status; `vo run` only) | always last |

```json
{"v":1,"kind":"call","depth":1,"function":"main","args":[]}
{"v":1,"kind":"call","depth":2,"function":"fact","file":"main.vo","start":{"line":6,"col":19},"args":[{"name":"n","value":"2"}]}
{"v":1,"kind":"call","depth":3,"function":"fact","file":"main.vo","start":{"line":3,"col":16},"args":[{"name":"n","value":"1"}]}
{"v":1,"kind":"return","depth":3,"function":"fact","value":"1"}
{"v":1,"kind":"return","depth":2,"function":"fact","value":"2"}
{"v":1,"kind":"return","depth":1,"function":"main"}
{"v":1,"kind":"end","records":6,"omitted":0,"status":0}
```

- Deterministic: no timings; the same program, input and options give the same bytes (FR-014).
- `--trace-limit N` (default 100000): after N records, records are counted in `omitted` instead of written; `end` is always written.
- Values are shortened to 200 bytes (`...`); function values are their type (`<fn(int) -> int>`).
- `exit(n)` ends the trace with `end` (`status` n), without `return` records for the active calls.
- Contract checks (spec 005) are part of the function: a `requires` failure gives `error` right after the `call`.
- A failing `expect`/`expect_eq` ends a test without a `return` or `error` record (it is not a runtime error).

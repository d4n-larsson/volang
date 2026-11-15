# Contract: Language and Toolchain Changes (spec 009)

## `get_or`

```text
get_or(m: map[K, T], key: K, default: T) -> T     pure; the value for key, or default; m is unchanged
```

Builtins table row: `| get_or | \`(m: map[K, T], key: K, default: T) -> T\` | the default when the key is missing; never adds it |`.
A user function named `get_or` is E0202.

## `case ok()`

| Pattern | On `result[void]` | On a result with a value | On a variant case |
|---|---|---|---|
| `case ok()` | the success arm | E0411, fix `case ok(_)` | E0411, fix `case NAME(_)` or `case NAME` |
| `case ok(_)` | W0402, fix `case ok()` | as before | as before |
| `case ok` | W0402, fix `case ok()` | as before | as before |

- **E0411 `empty_case_parens`**: "`case NAME()` matches only the success of a `result[void]`; write
  `case NAME(_)` to ignore the value." (or "`case NAME`" for a case without a value).
- **W0402 `void_ok_pattern`**: "Match the success of a `result[void]` with `case ok()`, as it is built."

## Replay of HTTP exchanges

- `vo run --record` writes `"request":"METHOD TARGET"` in each `http` event.
- `vo replay`: a request whose host, port, method or target differs from the next recorded exchange is
  R0014: "The program makes an HTTP request `GET /v2/status` to `localhost:8000`, but the recording has an
  HTTP request `GET /status` to `localhost:8000` next: ...". Events without `request` match by host and
  port only.

## R0010

The message and the report's function name the grant frame's function (data-model.md): for a denial inside
`std.http.get` called from `main`, "net(\"localhost\") is not allowed: `main` declares console,
net(\"example.invalid\")." Denials in functions that declare their own effects are unchanged.

## `std.text`

```text
fn align_left(s: str, width: int) -> str    # s, then spaces up to width characters
fn align_right(s: str, width: int) -> str   # spaces, then s
```

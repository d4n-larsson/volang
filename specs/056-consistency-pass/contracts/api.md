# Contract: Consistency Pass

Every change below is a row of the library-change table (data-model.md). A use of the old form reports
E0214 `library_changed`, with the fix in the last column. "vo 0.2.0" is the version in every message.

## Renamed (FR-004, FR-010, FR-013)

| Old | New | Fix |
|---|---|---|
| `std.lists.contains` | `has_item` | rename |
| `std.json.field` | `member` | rename |
| `std.tcp.close` | `close_conn` | rename |
| `std.tcp.describe` | `tcp_error_text` | rename |
| `std.tcp.send` | `write_all` (`write` is a builtin) | rename |
| `std.metrics.add` | `add_to` | rename |
| `std.sqlite.get` | `value_column` (parameters are named `column`) | rename |
| `std.sqlite.get_int` | `int_column` | rename |
| `std.sqlite.get_float` | `float_column` | rename |
| `std.sqlite.get_str` | `text_column` | rename |
| `std.sqlite.get_bytes` | `bytes_column` | rename |
| `std.http.header` | `response_header` (and Option, below) | rename, then wrap |
| `std.http.serve_bytes_workers` | `serve_workers_bytes` | rename |
| `std.random.shuffle` | `shuffled` | rename |
| `std.regex.replace_all` | `replace_matches` (and order, below) | rename and swap |
| `std.workers.WorkerError.timeout` | `timed_out` | rename the case |

## Argument order (FR-010)

| Old | New | Fix |
|---|---|---|
| `is_match(re, s)` | `is_match(s, re)` | swap |
| `first_match(re, s)` | `first_match(s, re)` (and Option) | swap, then wrap |
| `all_matches(re, s)` | `all_matches(s, re)` | swap |
| `split_on(re, s)` | `split_on(s, re)` | swap |
| `replace_all(re, s, r)` | `replace_matches(s, re, r)` | rename and swap |

## Option instead of result (FR-005)

Each returns `Option[T]` (`some(v)` / `none`). The fix wraps the call as `to_result(CALL, OLD_ERROR)` (the
old error text: `"empty"`, `"none"`, `"no match"`, ``"no header `NAME`"``) and
adds `import std.option.to_result;` when needed, so the program behaves as before.

- `std.http`: `response_header`, `request_header`
- `std.collections`: `first_key`, `last_key`, `key_at_or_above`, `key_at_or_below`, `key_after`,
  `key_before`, `first`, `last`, `at_or_above`, `at_or_below`, `after`, `before`, `peek`, `front`,
  `back`
- `std.regex`: `first_match`

Unchanged: `lists.index_of` (`-1`, as the builtin `find`: its result is used as a number, so no fix could
keep a program's behavior); `pop`, `pop_front` (`result`, `err("empty")`); `json.member` (`result`: not an object, or no
member); the builtin `find` (`-1`).

## Removed (FR-014)

| Old | Fix |
|---|---|
| `std.web.clock_ms` | a call: `now_ms()`; a value: `fn() -> int { return now_ms(); }` |
| `std.time.now` | the same |

## Units (FR-009)

| Old | New | Fix |
|---|---|---|
| `fixed_zone(east)` (minutes) | `fixed_zone(east_ms)` | a literal `n` becomes `n * 60000` as a literal; another expression `e` becomes `minutes(e)` |
| `security` `lifetime_s` (field) | `lifetime_ms` | rename; a literal value `n` becomes `n * 1000`, another expression `seconds(e)` |
| `security` `hsts_seconds` (field) | `hsts_ms` | the same |
| `web.Cookie.max_age_s` (field) | `max_age_ms` | the same |
| tz records' duration fields (`seconds`, ...) | `..._s` (still seconds) | rename |

## Time limits (FR-008)

Every `timeout_ms` (and `with_deadline`'s `ms`) takes 0 to 600,000; 0 means 600,000. Out of range:
`err("invalid: the time limit must be 0 to 600000 ms")` in the module's error form. No diagnostic: only
the meaning of 0 in `http.send` and the accepted range change (changelog).

## Resources and errors (FR-015 to FR-017)

- `tcp.close_conn(c)` and `close_listener(l)` need no effect.
- `tcp_error_text(e: TcpError) -> str`, `worker_error_text(e: WorkerError) -> str` and
  `local_error_text(e: LocalError) -> str` (std.time).
- sqlite messages have no `error: ` prefix.

## Typed reads and parse errors (FR-011, FR-012)

- `float_column` converts an INTEGER; a missing column is ``err("no column `NAME`")``.
- Parse errors are `invalid KIND: INPUT[; DETAIL]` for values and `invalid KIND at POSITION: DETAIL` for
  documents. These are text changes only (changelog).

## Language (FR-006, FR-007, FR-018 to FR-021)

| Construct | Before | After |
|---|---|---|
| A local, parameter or binding named like a builtin | accepted; later E0303 | E0203 "`len` is a builtin: give the local another name." Fix: `len_value` here and at its uses |
| `<`, `sort`, `min`, `T: ordered` on `bool`, variants | E0308, E0311, E0313 | ordered: `false < true`; cases in declaration order, then payload |
| A `K: key` parameter sorted | E0311 | allowed |
| `[]` where a list type is expected | E0306 (fix: `new list[T]`) | accepted |
| `Shape.empty()` | parse error, no fix | E0411, fix removes `()` |
| `1E9`, `1e+9`, `00012` | accepted | E0105, fix `1e9`, `12` |
| `1e-400` | silently `0.0` | E0105 (out of range) |
| `-0x8000000000000000` | checks; R0001 at run time | E0105 |
| `Db[Storage]` (a named set without `uses`) | E0327 | accepted; `vo fmt` writes `Db[uses Storage]` |
| `Db[D]` | kept by `vo fmt` | `vo fmt` writes `Db[uses D]` |

## Diagnostics

| Code | Name | Status |
|---|---|---|
| E0214 | `library_changed` | new; error; fixable: sometimes (no fix where none keeps behavior) |
| E0203 | `shadowed_name` | also for builtins |
| E0105 | (existing) | also non-canonical numeric spellings, `1e-400`, `-0x8000000000000000` |
| E0119 | `repeated_annotation` | every annotation, with a fix removing the repeat |
| E0306 | (existing) | only where no list type is expected |
| E0329 | (existing) | no longer for a repeated `@size` |
| E0411 | (existing) | also `Shape.empty()` |
| T0002 | (existing) | also an unreadable `--base` toolchain |

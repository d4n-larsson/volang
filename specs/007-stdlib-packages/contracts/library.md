# Contract: The Standard Library

Every item is imported by its full path, one per import: `import std.json.parse_json;`. Signatures below are what `vo doc` prints. `T` is a type parameter. Effects: functions without `uses` are pure; library effects need the same effect (with any scope) at the caller, and scopes are checked at run time against the caller's grant (research R6).

## std.json

```text
variant Json { null; boolean(bool); integer(int); number(float); text(str); array(list[Json]); object(map[str, Json]); }
fn parse_json(s: str) -> result[Json]                      # RFC 8259; err "line L, column C: ..." ; depth <= 512
fn write_json(j: Json) -> result[str]                       # compact; err for a NaN or infinite number
fn write_json_pretty(j: Json) -> result[str]                # 2-space indent, one member per line
fn field(j: Json, key: str) -> result[Json]            # err "not an object" / "no field `key`"
fn at(j: Json, i: int) -> result[Json]                 # err "not an array" / "index i out of range"
fn as_text(j: Json) -> result[str]
fn as_int(j: Json) -> result[int]                      # integer only
fn as_float(j: Json) -> result[float]                  # integer or number
fn as_bool(j: Json) -> result[bool]
fn as_array(j: Json) -> result[list[Json]]
fn as_object(j: Json) -> result[map[str, Json]]
fn text_field(j: Json, key: str) -> result[str]        # field + as_text; errors name the field
fn int_field(j: Json, key: str) -> result[int]
fn float_field(j: Json, key: str) -> result[float]
fn bool_field(j: Json, key: str) -> result[bool]
```

- Numbers: an integer literal that fits in `int` is `integer`; anything else is `number`. Writing `number` uses 17 significant digits (round-trips exactly); `integer` is written as is.
- Strings: `\uXXXX` (with surrogate pairs) decodes to UTF-8; an unpaired surrogate or invalid UTF-8 is an error. Output escapes `"`, `\`, and control characters (`\n`, `\t`, `\r`, `\b`, `\f`, else `\u00XX`); other characters are written as they are.
- Duplicate keys: the last value wins, at the first key's position.

## std.time

```text
record DateTime { year: int; month: int; day: int; hour: int; minute: int; second: int; ms: int; weekday: int; }
fn from_ms(ms: int) -> DateTime                        # UTC; weekday 1 = Monday ... 7 = Sunday
fn to_ms(d: DateTime) -> result[int]                   # err for an invalid date or time (weekday ignored)
fn date(year: int, month: int, day: int) -> result[int]  # midnight UTC of that day
fn format_time(ms: int) -> str                              # 2026-11-14T09:30:00Z, with .mmm when ms is not 0
fn parse_time(s: str) -> result[int]                        # YYYY-MM-DDTHH:MM:SS[.fff](Z|+HH:MM|-HH:MM), or YYYY-MM-DD
fn is_leap(year: int) -> bool
fn days_in_month(year: int, month: int) -> int
fn seconds(n: int) -> int                              # n * 1000 (durations are milliseconds)
fn minutes(n: int) -> int
fn hours(n: int) -> int
fn days(n: int) -> int
fn now() -> int uses clock                             # now_ms(): real time, fixed by --clock-ms
```

`to_ms`, `date` and `parse` accept years 1–9999; `from_ms` is defined for every `int` (years outside that range included), so it never fails.

## std.text

```text
fn replace(s: str, old: str, replacement: str) -> str       # every occurrence, left to right; old "" is R0007
fn starts_with(s: str, prefix: str) -> bool
fn ends_with(s: str, suffix: str) -> bool
fn contains(s: str, sub: str) -> bool
fn repeat(s: str, n: int) -> str                       # "" when n <= 0
fn pad_left(s: str, width: int, fill: str) -> str      # width in characters; fill is one character (R0007 otherwise)
fn pad_right(s: str, width: int, fill: str) -> str
fn lines(s: str) -> list[str]                          # split on \n, a trailing \r dropped; no final empty line
fn words(s: str) -> list[str]                          # split on runs of ASCII whitespace; no empty words
```

## std.random

```text
fn unit() -> float uses random                         # in [0, 1), 53 random bits
fn pick[T](xs: list[T]) -> result[T] uses random       # err "empty list"
fn shuffle[T](xs: list[T]) -> list[T] uses random      # a shuffled copy
```

## std.http

```text
record Header { name: str; value: str; }
record Response { status: int; headers: list[Header]; body: str; }
fn get(url: str) -> result[Response] uses net
fn post(url: str, content_type: str, body: str) -> result[Response] uses net
fn header(r: Response, name: str) -> result[str]       # first header with that name, case-insensitive
```

- URLs: `http://HOST[:PORT][/PATH][?QUERY]`; `https://` is `err "https is not supported"`; anything else that does not parse is `err`. Nothing is sent for an `err` URL.
- One request per connection (`Connection: close`); redirects are returned, not followed; bodies up to 16 MiB.
- The host is checked against the caller's `net` scope at run time: outside it is R0010.
- Errors (`err` values): connection refused, unknown host, malformed response, too large. There is no separate timeout per request: a server that stops answering is stopped by the run's `--timeout` (R0009).

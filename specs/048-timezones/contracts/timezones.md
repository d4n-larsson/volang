# Contract: time zones in std.time

`Z` is `Zone`. An instant is an `int` of milliseconds since 1970 UTC. All functions are pure except
`local_zone`.

```text
export record Zone { name: str; starts: list[int]; offsets: list[int]; names: list[str]; rules: list[ZoneRule]; }
export record ZoneRule { std_offset: int; std_name: str; dst_offset: int; dst_name: str; has_dst: bool; start: RuleDay; end: RuleDay; }
export record RuleDay { kind: int; month: int; week: int; day: int; seconds: int; }
export record Repeated { first: int; second: int; }
export variant LocalError { invalid(str); skipped(int); repeated(Repeated); }
```

| Function | Result | Notes |
|---|---|---|
| `load_zone(name: str) -> result[Z]` | the embedded zone of that exact name | error `unknown time zone: NAME` |
| `zone_names() -> list[str]` | every name `load_zone` accepts, sorted by bytes | |
| `tzdata_version() -> str` | the release, such as `2026f` | |
| `fixed_zone(east: int) -> Z` | always `east` minutes east of UTC (`minutes` is taken: the duration function); name and abbreviation `+HH:MM` or `-HH:MM` | R0012 outside -1439 to 1439 |
| `zone_from_bytes(name: str, data: bytes) -> result[Z]` | the zone of a TZif file, version 2 or later | error `invalid zone data: REASON`; never stops |
| `local_zone() -> Z uses clock` | the machine's zone; UTC (name `UTC`) when none is found or the clock is fixed | never fails |
| `offset_at(z: Z, ms: int) -> int` | milliseconds east of UTC at that instant | |
| `abbreviation_at(z: Z, ms: int) -> str` | `"CEST"`, `"+0545"` as the data has it | printable ASCII, no spaces |
| `in_zone(z: Z, ms: int) -> DateTime` | `from_ms(ms + offset_at(z, ms))` | |
| `from_local(z: Z, d: DateTime) -> result[int, LocalError]` | the one instant at which the zone shows `d` | `invalid`, `skipped`, `repeated`; `weekday` is ignored |
| `format_in(z: Z, ms: int) -> str` | `YYYY-MM-DDTHH:MM:SS[.fff]` then `+HH:MM`, `-HH:MM`, or with `:SS` when the offset has seconds | zero offset is `+00:00` |

Changed: `parse_time` also accepts an offset `+HH:MM:SS` or `-HH:MM:SS` (seconds 00 to 59). Everything
it accepted before gives the same result.

Laws, checked by the tests:

- `parse_time(format_in(z, ms)) == ok(ms)` for years 1 to 9999;
- `from_local(z, in_zone(z, ms))` is `ok(ms)` or `err(repeated(r))` with `ms` one of `r.first`,
  `r.second`; never `skipped`;
- `load_zone(n)` is `ok` exactly for `n` in `zone_names()`.

## Builtins (for `std/time.vo` only)

| Builtin | Effect | Result |
|---|---|---|
| `__tz(op: int, name: str) -> result[bytes]` | none | `op` 1: the zone's TZif bytes, `err("unknown")`; `op` 2: the index text of `zones.bin` (header and name lines) |
| `__local_tz() -> bytes` | clock | `NAME`, a newline, TZif bytes; or no bytes |

## The data file `third_party/tzdata/zones.bin`

```text
VOTZ1 <release> <count>\n
<name> <offset> <length>\n        count lines, sorted by the bytes of name
<data>                            the distinct TZif files; offset counts from the first byte of data
```

Numbers are decimal. Names have no space. Two names may give the same offset and length.

## `vo version`

```text
vo 0.1.0 schema 1
tzdata 2026f
```

```json
{"v":1,"kind":"version","version":"0.1.0","schema":1,"tzdata":"2026f"}
```

## Recording

A call of `local_zone` writes one event of kind `local_tz` whose value is the bytes `__local_tz`
returned (`"bytes":true`, as for `entropy`). `vo replay` gives them back and reads neither `TZ` nor
`/etc/localtime`. A recording made before this change has no such event: a program that now calls
`local_zone` asks for a different input (R0014), as for any new input.

## Unchanged

No syntax, effect, diagnostic code or runtime code. `DateTime`, `from_ms`, `to_ms`, `date`,
`format_time`, the duration functions, `now` and `sleep`.

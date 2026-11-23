# Data model: Time zones

Instants are milliseconds since 1970-01-01T00:00:00Z. Inside a zone everything is in seconds, as in
the data.

## Zone

| Field | Type | Rule |
|---|---|---|
| `name` | `str` | the name it was made with |
| `starts` | `list[int]` | the instants (seconds) of the listed changes, strictly ascending |
| `offsets` | `list[int]` | seconds east of UTC; `len(starts) + 1` items: `offsets[0]` before the first change, `offsets[i + 1]` from `starts[i]` |
| `names` | `list[str]` | the abbreviation for each item of `offsets` |
| `rules` | `list[ZoneRule]` | none or one: what holds from the last listed change on |

An instant `t` falls in segment `i`, the number of `starts` at or before `t`. When `i` is the last
segment and there is a rule, the rule answers; otherwise `offsets[i]` and `names[i]`.

The record is open, as every vo record is. A zone built by hand that breaks the rules above may stop
a lookup with R0003; the functions that make zones never produce one.

## ZoneRule

The rule line of a zone file (`CET-1CEST,M3.5.0,M10.5.0/3`), read once.

| Field | Type | Rule |
|---|---|---|
| `std_offset`, `dst_offset` | `int` | seconds east of UTC (the line writes them west-positive) |
| `std_name`, `dst_name` | `str` | abbreviations |
| `has_dst` | `bool` | false: only the standard fields are used |
| `start`, `end` | `RuleDay` | when summer time starts (read in standard time) and ends (read in summer time) |

Summer time holds at `t` when, with both days taken in the year of `t` in UTC and turned into
instants: `start <= t < end` if `start < end`, otherwise not `end <= t < start` (southern zones).

## RuleDay

| Field | Type | Rule |
|---|---|---|
| `kind` | `int` | 0: `Mm.w.d`; 1: `Jn` (day 1 to 365, 29 February never counted); 2: `n` (day 0 to 365) |
| `month`, `week`, `day` | `int` | for kind 0: month 1 to 12, week 1 to 5 (5 is the last), weekday 0 (Sunday) to 6; for kinds 1 and 2 `day` is the day number |
| `seconds` | `int` | time of day, -167 h to 167 h; 7200 when the line gives none |

## LocalError and Repeated

```text
export record Repeated { first: int; second: int; }
export variant LocalError { invalid(str); skipped(int); repeated(Repeated); }
```

`skipped` carries the instant of the change, in milliseconds; `Repeated` the two instants, `first`
before `second`.

## Limits checked by `zone_from_bytes`

| What | Limit |
|---|---|
| version byte | `2` or above |
| leap-second records | 0 |
| time types | 1 to 256; offset within ±93,600 s; abbreviation inside the table, ended by a zero byte, printable ASCII without spaces (bytes 33 to 126) |
| listed changes | at most 100,000; strictly ascending; within ±2^62 s; type index below the number of types |
| every table | inside the bytes |
| rule line | between two newlines at the end; empty, or the grammar of ZoneRule |

## Embedded database

One file, `zones.bin`: release name, 597 names, 344 distinct zones (2026f), 423,550 bytes. Names are unique; an
alias is a second name with the same bytes.

# Data Model: Regular Expressions

## Regex (vo record, `std.regex`)

| Field | Type | Notes |
|---|---|---|
| `pattern` | `str` | the pattern text, as written (after string escapes) |

Made by `regex(LITERAL)` (pattern checked at check time) or `parse_regex(s)` (checked at run time, `err`
on a bad pattern). A record built by hand with `new Regex { pattern: ... }` is allowed; a bad pattern
then stops the program with R0007 at its first use. Immutable in practice (the field is never changed by
the library); comparable with `==` (by pattern text).

## Match (vo record, `std.regex`)

| Field | Type | Notes |
|---|---|---|
| `text` | `str` | the matched text (`groups[0]`) |
| `start` | `int` | byte offset of the match in the searched text |
| `end` | `int` | byte offset after the match; `slice(s, m.start, m.end) == m.text` |
| `groups` | `list[str]` | group 0 the whole match, then groups 1..n by opening parenthesis; `""` for a group that did not take part |
| `took_part` | `list[bool]` | whether each group took part in the match (index as `groups`) |
| `named` | `map[str, str]` | every named group's text by name (`""` when it did not take part), in group order |

## Program (C, `src/regex.c`, per compiled pattern)

| Item | Holds |
|---|---|
| instructions | `CHAR`, `CLASS` (sorted code point ranges, negation folded in), `ANY`, `ANY_NL`, `SPLIT x y`, `JMP x`, `SAVE n`, `ASSERT kind` (`^`, `$`, multiline `^`/`$`, `\b`, `\B`), `MATCH` |
| groups | number of capture groups (group 0 implied) and their names |
| first-byte set | 256 bits, or "any" |
| anchored | the program starts with non-multiline `^` |
| flags | applied at compile time: `(?i)` expands letters into two-case classes; `(?s)` turns `.` into `ANY_NL`; `(?m)` selects the multiline assertions |

Limits (research R9): pattern 10,000 bytes, repetition count 1,000, program 20,000 instructions, nesting
200, groups 1,000.

### Compile errors

| Field | Meaning |
|---|---|
| `start`, `end` | byte range in the pattern text |
| `code` | E0319 (syntax or limit) or E0320 (unsupported feature) |
| `message` | one sentence (contracts/diagnostics.md) |
| `fix` | optional replacement for the range (`(?P<` → `(?<`) |

### Compile cache

16 slots keyed by pattern bytes, round-robin replacement, buffers reused per slot.

## Primitive encodings (research R4)

| Call | Result |
|---|---|
| `__re_compile(p)` | `ok("name1\n\nname3")`: group names for groups 1..n (`""` unnamed), or `err(message)` |
| `__re(1, p, s)` | `[]` no match, `[1]` a match |
| `__re(2, p, s)` | `[]`, or `[s0, e0, s1, e1, …, sn, en]` (`-1, -1` for a group that did not take part) |
| `__re(3, p, s)` | the records of every match, concatenated (each `2 × (n + 1)` ints) |

## Replacement (vo, `std/regex.vo`)

Parsed left to right: `$$` → `$`; `$` digits (all following digits) → that group; `${digits}` or
`${name}` → that group; any other `$` → R0007 "Invalid replacement: `$` must be followed by a group
number, `{name}` or `$`." at the call; a group the pattern does not have → R0007 "The pattern has no
group N." (check-time E0322 for literals, research R5).

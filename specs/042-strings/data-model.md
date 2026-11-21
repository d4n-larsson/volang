# Data Model: Strings

## String literals

| Form | Text | Escapes | Lines |
|---|---|---|---|
| `"..."` | decoded | `\n` `\t` `\\` `\"` `\r` `\u{H}`…`\u{HHHHHH}` | one |
| `` `...` `` | as written, CRLF read as LF | none | any; no backtick inside |

Canonical spelling: raw only when the text has a backslash, a `"` or a line break (W0102, `vo fmt`).

`\u{...}`: 1 to 6 hex digits (either case); a Unicode scalar value: 0 to D7FF or E000 to 10FFFF;
not 9, A, D, 22 or 5C, which have short escapes.

An effect scope has no control character (below U+0020, or U+007F).

## Float literals

`DIGITS [. DIGITS] [(e|E) [+|-] DIGITS]`, with a fraction or an exponent (or both); at most 17
significant digits before the exponent; a finite value.

## `for` subjects

list, range, the collections of spec 014, and now `str`: the variable is each code point as a `str`.

## Case table (`src/case_table.h`)

Two sorted arrays of pairs (from, to), for upper and for lower: code points in U+0080–U+024F and
U+0370–U+052F whose simple mapping is one code point other than itself. About 700 pairs, under 6 KiB.

## Library

```text
std.text.char_count(s: str) -> int          # code points
std.text.equal_fold(a: str, b: str) -> bool # lower(a) == lower(b)
```

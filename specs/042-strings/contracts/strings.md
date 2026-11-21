# Contract: string and number literals, `for` over text, case

## Literals

| Source | Value |
|---|---|
| `"a\r\n"` | `a`, carriage return, line feed |
| `"\u{0}"` | one NUL byte |
| `"\u{e5}"`, `"\u{1F600}"` | `å`, 😀 |
| `` `\d+"x"` `` | the nine characters `\d+"x"` |
| a backtick string over three lines | those lines with their line breaks (each one line feed, also in a file with CRLF line ends), nothing trimmed |
| `1e9`, `1.5e-3`, `2E+2` | floats 1000000000.0, 0.0015, 200.0 |

## Diagnostics

| Code | When | Fix |
|---|---|---|
| E0102 | a quoted string not closed on its line (unchanged); a raw string not closed in the file | the quote (unchanged); none |
| E0103 | an escape other than the six (so also `\0`); `\u` without `{`, without digits, with more than 6, with a non-hex digit, naming a surrogate or a value above 10FFFF; `\u{...` without `}`; `\u{...}` of a character that has a short escape | none; the `}` when the digits are valid; the short escape |
| E0506 | an effect scope with a control character | none |
| E0105 | an exponent without digits (`1e`, `1e+`); other letters after a number, more than 17 digits, an overflowing value (unchanged) | none |
| W0102 `raw_string_not_needed` | a raw string whose text has no backslash, no `"` and no line break | the quoted string |

## `for c in s`

`s: str`; `c: str`, one code point per step, in order; nothing for `""`. `break`, `continue` and
`return` as in any `for`. Memory does not depend on `len(s)`.

## Case

`upper(s)`, `lower(s)`: ASCII as before; letters of Latin-1 Supplement, Latin Extended-A and -B,
Greek and Coptic, Cyrillic and Cyrillic Supplement by their simple (one code point) mapping; every
other character unchanged, including `ß` and other letters whose only mapping is several characters.

## Formatter

Keeps every literal as written, except a raw string that W0102 applies to, which becomes quoted.
Never changes or re-indents the inside of a raw string.

# Research: Strings

## R0. The constitution

Principle I asks for one syntactic form per construct. The constitution's workflow section provides
for what this plan does: "Complexity beyond these principles MUST be justified in the plan's Complexity
Tracking table, including the simpler alternative that was rejected and why." The raw string form is
recorded there, was accepted by the project owner on 2026-11-21, and needs no amendment. (The analysis
report's finding D1 said the constitution had no such clause; that was wrong.)

## R1. Raw strings in the lexer and parser

**Decision**: a backtick starts a raw string that ends at the next backtick; every byte between is its
text, line breaks included; it must be valid UTF-8 (as the whole file must). A carriage return
directly before a line feed is dropped when the parser takes the text (the lexer already treats a
carriage return as white space, so files with CRLF line ends check today; a literal's value must not
depend on them), and `lit_source_offset` skips it. The token is `T_STRING`,
as for a quoted string; the parser's string case looks at the token's first byte and, for a backtick,
takes the text as it is instead of decoding escapes. From the AST on there is no difference, so
constants, effect scopes, annotations and checked literals accept it without change. The node keeps
the source span, which is how the formatter, `vo query` and renames know it is raw.

An unclosed raw string is E0102 at the opening backtick ("This raw string is not closed: add a
backtick"), and the token runs to the end of the file. No fix is offered (where it should end is not
known). A backtick cannot be written inside a raw string.

**Alternatives considered**: `r"..."` (one more prefix rule, and still needs an escape for `"`, which
is the character JSON and HTML are full of); triple quotes (three characters to type and to lex, and
the same question about `"`); indentation stripping (a rule to explain, and a way for the value to
differ from what is written).

## R2. One spelling per text

**Decision**: a raw string is canonical only when its text contains a backslash, a `"` or a line
break. Otherwise `vo fmt` rewrites it as a quoted string (the text needs no escape by definition), and
`vo check` reports W0102 `raw_string_not_needed` with that fix. A quoted string is never rewritten as
raw: `"\\d"` stays valid and canonical (it is what every existing program has).

So for a given text: no backslash, quote or line break: only the quoted form is canonical; otherwise
both forms can express it and both are left alone. The asymmetry is deliberate: forcing raw wherever it
is possible would rewrite every existing string with a `\n` in it.

## R3. Escapes, and positions inside literals

**Decision**: the lexer accepts `\r` and `\u{H}` to `\u{HHHHHH}`; the parser decodes them (`\u` to
UTF-8). There is no `\0`: a NUL is `\u{0}`. A `\u{...}` whose character has a short escape (9, A, D,
22, 5C) is E0103 with that escape as its fix, so that no character has two escapes. A printable
character may be written as itself or as `\u{...}`; which is readable depends on the character, and
the formatter leaves it. `\u` without braces, with no or more than six digits, with a non-hex digit, naming a
surrogate (D800 to DFFF) or a value above 10FFFF is E0103 with a message for that case; a missing
closing brace is fixed by inserting it when the digits are valid.

`format`, `regex`, `sql` and route patterns report positions inside a literal. Today each maps an
offset in the value to the source by walking the literal and counting two source bytes per escape. With
`\u{...}` (up to ten source bytes for up to four value bytes) and raw strings (one for one) that
walk moves into one function, `lit_source_offset(node, value_offset)`, used by all four.

## R4. `for c in s`

**Decision**: `check_for` accepts a subject of type `str`; the loop variable is `str`. The compiler
emits `OPC_FOR_STR` (the string in the iterator slot, a byte offset 0 beside it) and
`OPC_FOR_STR_STEP`, which at each step takes the UTF-8 sequence at the offset (1 to 4 bytes: a `str`
is always valid), binds the variable to it and advances. One-byte characters use the shared
one-character strings (the review's performance fix), so an ASCII text allocates nothing; others
allocate one short string per step, which the collector reclaims. The string is held in the iterator
slot for the whole loop, so reassigning the variable in the body does not affect it.

The reference interpreter (`src/interp.c`) gets the same loop, and `tests/gen_programs.py` generates
it so that `tests/diff_engines.py` compares the two.

`std.text.char_count(s)` counts the bytes that are not continuation bytes, in vo, with `byte_at`;
`equal_fold(a, b)` is `lower(a) == lower(b)`.

## R5. Case mapping

**Decision**: `tools/gen_case.py` reads Python's `unicodedata` and writes `src/case_table.h`: for the
code points U+0080 to U+024F (Latin-1 Supplement, Latin Extended-A and -B) and U+0370 to U+052F
(Greek and Coptic, Cyrillic and its supplement), the pairs whose `upper()` or `lower()` is a single
code point different from the source. The file is checked in and the test regenerates and compares
it. `rt_case` keeps its byte loop for ASCII and, at a lead byte, decodes the code point, looks it up
(binary search over about 700 pairs), and appends the result, whose UTF-8 length may differ (`ı` to
`I`, `Ⱥ` to U+2C65).

Characters with only a multi-code-point mapping (`ß`, `ŉ`, `ǰ`, the Greek letters with dialytika and
tonos) are left unchanged; final sigma is not treated specially: `lower("Σ")` is `σ` everywhere.

## R6. Exponent literals

**Decision**: after the digits (and an optional fraction), `e` or `E`, an optional sign and at least
one digit make the token a `T_FLOAT`; `1e9` is a float without a fraction. `1e` and `1e+` are E0105
("An exponent needs digits: write `1e9`"). The value is computed by the conversion `parse_float`
uses, which already reads exponents and rounds correctly; the 17-significant-digit rule applies to the
digits before the exponent; a value that overflows is E0105 as today. `lx_number_suffix` keeps
reporting other letters after a number.

## R7. Diagnostics

| Code | Change |
|---|---|
| W0102 `raw_string_not_needed` (new) | a raw string whose text needs no rawness; fix: the quoted form |
| E0102 `unterminated_string` | also an unclosed raw string, with its own message and no fix |
| E0103 `invalid_escape` | lists the six escapes; messages for the malformed `\u` cases; `\u{...}` of a character with a short escape, with that escape as the fix |
| E0506 (invalid scope) | also an effect scope with a control character (it could not be written on one line of `vo.effects`) |
| E0105 `number_out_of_range` | the "no exponent" message is replaced by the one for an exponent without digits |
| E0308 (indexing a string) | its hint names `for c in s` beside `slice`, `chars` and `byte_at` |

## R8. The reference budget

**Decision**: additions (about 110 tokens): the Strings bullet rewritten for the six escapes and the
raw form; `for c in s` in the loop row and its note; `upper`/`lower` row "Latin, Greek and Cyrillic";
exponent literals in the number line; two `std.text` names. Trims (at least 70 tokens), none removing a
rule: the sentence that explains doubling backslashes for `regex` (no longer the advice); the Strings
bullet's list of what does not exist, shortened; the `chars` uses that `for c in s` replaces in two
examples; repeated wording in the library table rows for `std.http` (`get_bytes`, `post_bytes`,
`send_bytes`, `serve_bytes`, `serve_bytes_workers` become "`*_bytes` forms"). Measured with
`tests/ref_examples.py` after each edit.

## R9. Tests

- Golden cases: `lex/` (each escape, each malformed `\u`, unclosed raw, raw with `#`, `"` and
  keywords, exponent forms and their errors), `fmt/` (raw kept, raw rewritten, multi-line raw not
  re-indented, escapes and exponents kept), `run/` (values of each literal; `for c in s` with
  `break`, `continue`, `return`, reassignment, empty and multi-byte text), `format/`, `regex/`,
  `sqlite/` and `web/` (a raw literal and a `\u` literal with an error after it: the position is
  exact), `check/` (W0102 with its fix applied by `tests/apply_fixes.py`), `text/` (`char_count`,
  `equal_fold`, `upper`/`lower` samples).
- `tests/strings.py` (in `make test`): case mapping for every code point 0 to 0x10FFFF against
  Python (simple mappings in the covered blocks; identity elsewhere), and `src/case_table.h` equal to
  the generator's output; 10,000 random exponent literals against `float()`; `for c in s` against
  `chars` on random text, on 10 MiB under `--max-memory 16`, and its speed ratio (reported; the bound
  of 3 is checked on x86-64 only).
- `tests/grammars.py`: new lines in a corpus file; the tree-sitter parser regenerated.
- `tests/diff_engines.py`: the generator emits the new loop, escapes, raw strings and exponents.
- SC-001: escaped strings in `examples/`, `std/` doc examples and the site are rewritten as raw where
  that removes a doubled backslash or an escaped quote, at least 20 places, outputs unchanged.

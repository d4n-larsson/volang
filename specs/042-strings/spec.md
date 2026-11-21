# Feature Specification: Strings (escapes, raw text, characters, case)

**Feature Branch**: `042-strings`

**Created**: 2026-11-21

**Status**: Implemented (gate not run)

**Input**: User description: "Strings, the review's language item 4 (docs/review-2026-11/language.md,
'Strings'), the next open item after typed errors, constants and map keys. Its four parts: read a
string in place (`byte_at` exists since the performance fixes; `for c in s` over code points without
building a list does not); raw and multi-line strings (SQL, regular expressions, JSON fixtures and HTML
all fight the escape rules: the reference itself says 'write `\\d` for `\d`'); Unicode (`upper` and
`lower` are ASCII only, `len` counts bytes, the only character count is `len(chars(s))`); and exponent
literals (`1e9`, `1.5e-3`). Also from spec 039: there is no `\r` escape, so every line protocol writes
`chr(13)`. Must keep: one way to write each thing (constitution I), every statement identifiable from
its first token, the reference under 15,000 tokens (about 14,960 now: this feature must shorten the
reference elsewhere to make room), the three editor grammars held to the toolchain (spec 037), fixes
on diagnostics, determinism, three architectures. String interpolation stays out (`format` covers it).
Hex and binary literals and `_` separators belong to the review's item 5 (bitwise operations), not
here."

Background: the text a vo program most often contains is text with its own syntax inside: a regular
expression, an SQL statement, a JSON fixture, an HTML fragment, a protocol line. vo has one string
form with four escapes, so each of these is written with doubled backslashes and escaped quotes, on
one line, and a carriage return cannot be written at all. Reading text has the opposite problem: to
look at the characters of a string a program first copies them into a list. And `upper("åäö")`
returns `"åäö"`. None of this stops a program from being written; all of it makes agents produce
escaping mistakes that the checker then has to catch.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Write text that has its own syntax (Priority: P1)

A developer writes a regular expression, an SQL statement, a JSON fixture or a few lines of HTML as it
is, without doubling backslashes or escaping quotes, over several lines when it has several lines.

**Why this priority**: it removes the most common source of string mistakes in generated code.

**Independent Test**: a program holding a regex with `\d`, a JSON object with quotes and a three-line
SQL statement in raw strings prints the same as the escaped one-line versions; `regex(...)` and
`sql(...)` accept and check raw literals.

**Acceptance Scenarios**:

1. **Given** a raw string, **When** the program runs, **Then** its value is exactly the characters
   between its delimiters: a backslash is a backslash, a `"` is a `"`, a line break is a line break.
2. **Given** a raw string as the argument of `regex`, `sql`, `format` or a route, **When** checked,
   **Then** it is a literal there exactly as a quoted string is (same checks, same errors, positions
   inside the literal still exact).
3. **Given** a raw string as a constant's value, an effect scope or an annotation argument, **Then**
   it is accepted wherever a string literal is.
4. **Given** an effect scope (raw or quoted) whose text has a control character (a line break, a
   carriage return, a NUL), **Then** E0506: a scope is written on one line of `vo.effects`.
5. **Given** a raw string that would be the same text as an ordinary string (no backslash, no `"`,
   one line), **When** formatted with `vo fmt`, **Then** it becomes the ordinary string; and an
   ordinary string is never rewritten as raw. So each text has one canonical spelling.
6. **Given** a raw string that is not closed, **When** checked, **Then** one error at its opening
   delimiter, and the rest of the file is still checked sensibly.
7. **Given** text that itself contains the raw delimiter, **Then** it is written as an ordinary
   string with escapes; the error for an unintended early close says so.

---

### User Story 2 - Every character can be written (Priority: P1)

A developer writes a carriage return, an escape character, a NUL or any Unicode character in an
ordinary string without `chr(...)` and `+`.

**Why this priority**: every network protocol line ends in a carriage return and a line feed; spec
039's own example needed a helper function for it.

**Independent Test**: `"\r\n"` is two bytes, 13 and 10; `"\u{1b}[0m"` starts with byte 27; `"\u{e5}"`
equals `"å"`; `"\u{0}"` is one NUL byte; each invalid form has an error with a fix or a clear message.

**Acceptance Scenarios**:

1. **Given** `\r` in a string, **Then** it is a carriage return. A NUL is written `\u{0}`; there is
   no `\0`, so that no character has two escapes.
2. **Given** `\u{H}` with 1 to 6 hexadecimal digits naming a Unicode scalar value, **Then** it is
   that character in UTF-8.
3. **Given** `\u{...}` naming a surrogate, a value above 10FFFF, no digits, more than 6 digits, or
   without braces, **Then** an error naming what is wrong.
4. **Given** any other escape (`\x`, `\a`, `\'`, `\0`), **Then** E0103 as today, its message listing
   the escapes that now exist.
5. **Given** `\u{...}` naming a character that has a short escape (`\u{a}`, `\u{d}`, `\u{9}`,
   `\u{22}`, `\u{5c}`), **Then** E0103 with the short escape as its fix: one escape per character.
6. **Given** `vo fmt`, **Then** escapes are kept as written (a string is never re-spelled between
   `\u{e5}` and `å`: a printable character may be written either way, since which is readable depends
   on the character).

---

### User Story 3 - Look at a string's characters without copying it (Priority: P2)

A developer loops over the characters of a string, or counts them, without building a list of
one-character strings first.

**Why this priority**: `for c in chars(s)` allocates a list as long as the text; on a megabyte of text
that is the difference between working and not.

**Independent Test**: `for c in s` visits the same strings as `for c in chars(s)`; a loop over a
10 MiB string runs within a small memory limit; `char_count(s)` equals `len(chars(s))`.

**Acceptance Scenarios**:

1. **Given** `for c in s` with `s` a `str`, **Then** `c` is each code point as a one-character `str`,
   in order; an empty string never enters the loop.
2. **Given** `break`, `continue` and `return` inside, **Then** they work as in any `for`.
3. **Given** a 10 MiB string, **When** looped over with a memory limit far below what `chars` would
   need, **Then** the program finishes.
4. **Given** `for b in some_bytes`, **Then** unchanged: it is an error today (its message suggests
   `to_ints` or indexes) and stays one; this story adds only `str`.
5. **Given** `std.text.char_count(s)`, **Then** the number of code points, without allocating.

---

### User Story 4 - Case and comparison beyond ASCII (Priority: P2)

A developer upper-cases, lower-cases or compares without case text in European languages and gets what
a reader expects.

**Why this priority**: `upper("åäö") == "åäö"` is a wrong answer in a language that guarantees valid
UTF-8.

**Independent Test**: `upper` and `lower` agree with Python's `str.upper`/`str.lower` on every code
point of the covered ranges whose mapping is a single code point; `equal_fold("STRASSE", "strasse")`
is true.

**Acceptance Scenarios**:

1. **Given** text in the Latin (including Latin-1 Supplement and Latin Extended-A and -B), Greek and
   Cyrillic blocks, **When** passed to `upper` or `lower`, **Then** each letter with a single-code-point
   mapping is mapped; everything else is unchanged.
2. **Given** a character whose full mapping is more than one code point (`ß`, `ŉ`, the Greek final
   sigma rule), **Then** it is left unchanged by `upper`/`lower`: the documented simple mapping.
3. **Given** `std.text.equal_fold(a, b)`, **Then** true when the two are equal after `lower` of each.
4. **Given** ASCII text, **Then** results are exactly as before.

---

### User Story 5 - Exponent literals (Priority: P3)

A developer writes `1e9`, `1.5e-3` or `6.02e23` as a float literal.

**Independent Test**: each literal equals the value `parse_float` gives for the same text; `1e9` is a
`float`, not an `int`.

**Acceptance Scenarios**:

1. **Given** digits, an optional fraction, `e` or `E`, an optional sign and digits, **Then** a `float`
   literal with that value, correctly rounded.
2. **Given** `1e` or `1e+` with no digits after, **Then** an error with the literal's span.
3. **Given** a value that overflows to infinity, **Then** E0105 as for other out-of-range literals.
4. **Given** `vo fmt`, **Then** the literal is kept as written.

---

### User Story 6 - Documented, highlighted, within budget (Priority: P2)

The reference describes the new forms and still fits its budget; the three editor grammars and the
site's highlighter colour them; `vo explain` covers the new and changed codes.

**Independent Test**: `python3 tests/ref_examples.py` under 15,000 tokens; `tests/grammars.py` passes
over every `.vo` file including new ones with raw strings and escapes; `vo explain` for each code.

**Acceptance Scenarios**:

1. **Given** the reference, **Then** it states the raw form, the escapes, `for c in s`, the case rule
   and exponent literals, and is under 15,000 tokens after shortening other text.
2. **Given** a `.vo` file with raw strings containing `"` and `#`, **Then** every grammar tokenizes it
   as the toolchain does (a `#` inside a raw string is not a comment).

### Edge Cases

- A raw string containing a `#`, `//`, or what looks like a keyword: all text.
- A raw string's first and last line breaks: kept exactly (no trimming, no indentation stripping);
  the formatter never re-indents inside one.
- A raw string in a file with CRLF line ends: each carriage return directly before a line feed is
  dropped from the value, so the same program means the same on every machine. A carriage return
  elsewhere in a raw string is kept.
- An empty raw string: allowed, and formatted to `""`.
- A raw string in an `expect_eq` report or a diagnostic's quoted text: shown as the value, escaped as
  any string is shown.
- `len` still counts bytes; `slice` and `find` still work in bytes. Only the new function counts code
  points.
- `for c in s` while `s` is reassigned in the body (`set s = ...`): the loop goes on over the string
  it started with.
- Characters outside the covered blocks (Armenian, Georgian, Cherokee, Deseret): unchanged by
  `upper`/`lower`, documented.
- `upper`/`lower` on a recording made before this change: programs that upper-cased non-ASCII text
  print something else now. It is a breaking change, listed first in the changelog.
- `1e9` where an `int` is expected: a type error as for `1.0`; `int(1e9)` converts.
- A number directly followed by a letter other than an exponent (`1x`, `2em`): the error it is today.
- Existing programs: every program that checks today still checks and prints the same, except
  non-ASCII `upper`/`lower`.

## Requirements *(mandatory)*

### Functional Requirements

**Raw strings**

- **FR-001**: The language MUST have one raw string form: text between a pair of delimiters, with no
  escapes, in which line breaks are allowed. Its type is `str` and its value the text as written,
  with a line break always one line feed.
- **FR-002**: A raw string MUST be accepted everywhere a string literal is (expressions, constants,
  effect scopes, annotations, and the checked-literal arguments of `format`, `regex`, `sql` and
  routes), with diagnostics inside it positioned exactly. An effect scope with a control character
  is E0506, however it is written.
- **FR-003**: `vo fmt` MUST rewrite a raw string as an ordinary one when the text has no backslash, no
  `"` and no line break, and MUST leave all other strings as written; a warning with that fix MUST
  exist for `vo check`, so that each text has one canonical spelling.
- **FR-004**: An unclosed raw string MUST be one error at its start.

**Escapes**

- **FR-005**: Ordinary strings MUST accept `\r` and `\u{H...}` (1 to 6 hex digits, a Unicode scalar
  value) in addition to `\n`, `\t`, `\\` and `\"`. Every other escape stays E0103; a malformed `\u`
  has its own message; a `\u{...}` for a character that has a short escape is E0103 with that escape
  as its fix.

**Characters**

- **FR-006**: `for NAME in EXPR` MUST accept a `str` and visit its code points as one-character
  strings without building a list; memory use MUST NOT depend on the string's length.
- **FR-007**: `std.text` MUST provide `char_count(s) -> int` and `equal_fold(a, b) -> bool`.

**Case**

- **FR-008**: `upper` and `lower` MUST apply Unicode simple (single code point) case mappings for the
  Basic Latin, Latin-1 Supplement, Latin Extended-A, Latin Extended-B, Greek and Coptic, Cyrillic and
  Cyrillic Supplement blocks, and leave every other character unchanged. Results MUST be valid UTF-8 and deterministic.

**Numbers**

- **FR-009**: Float literals MUST accept an exponent part (`e` or `E`, optional sign, digits), with the
  rounding and range rules of today's float literals.

**Fit**

- **FR-010**: Every new or changed diagnostic MUST have a stable code, a catalog entry and, where one
  correction exists, a fix. No existing code changes meaning.
- **FR-011**: The reference MUST describe all of this and stay under 15,000 tokens; text elsewhere in
  it is shortened as needed, without removing a rule.
- **FR-012**: The TextMate, tree-sitter and Vim grammars, the site's highlighter and the formatter
  MUST handle the new forms; `tests/grammars.py` MUST pass.
- **FR-013**: `vo query type-at`, `vo edit rename`, hover, traces and recordings MUST work on programs
  using the new forms (a rename never touches text inside a raw string).
- **FR-014**: Three architectures; the bytecode engine and the reference interpreter agree.

### Key Entities

- **Raw string**: a literal whose text is taken as written.
- **Escape**: `\n`, `\t`, `\\`, `\"`, `\r`, `\u{...}`.
- **Simple case mapping**: a table from one code point to one code point.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The repository's examples and tests that hold a regular expression, SQL, JSON or HTML in
  a string (at least 20 places) are rewritten as raw strings with no change in output, and every doubled
  backslash in them is gone.
- **SC-002**: `for c in s` over a 10 MiB string runs under a 16 MiB memory limit, and is faster than
  `for c in chars(s)` over the same text (target: 3 times; the measured ratio is recorded).
- **SC-003**: `upper` and `lower` agree with a reference implementation on 100% of the code points in
  the covered blocks that have a simple mapping, and change no code point outside them.
- **SC-004**: Every float literal with an exponent in a 10,000-literal corpus has the value the
  reference parser gives.
- **SC-005**: The grammar check passes over every `.vo` file in the repository, including at least 30
  new lines with raw strings and new escapes.
- **SC-006**: The reference is under 15,000 tokens; the whole suite passes on three architectures;
  every existing golden case is unchanged except those that print E0103's message.

## Assumptions

- **The raw delimiter is the backtick**: `` `text` ``, as the review suggests. It is unused in vo
  today, familiar from Go and JavaScript, and rare in SQL, regular expressions, JSON and HTML. There is
  no way to write a backtick inside one: such text uses an ordinary string.
- **No indentation stripping** in multi-line raw strings: what is between the delimiters is the value.
- **One canonical spelling** is kept by the formatter and a warning (FR-003), which is how the
  constitution's "one form per construct" is honoured with two string literals.
- **`\u{...}` with braces only**, no `\xNN` (it would produce bytes that are not text), no `\uNNNN`
  and no `\0`.
- **`upper` and `lower` change**, they do not gain siblings: one way to change case. ASCII-only was
  documented, so this is a breaking change to the library, not the correction of a defect: programs
  that upper- or lower-cased Latin, Greek or Cyrillic text print something else. The stability policy
  asks for a diagnostic with a breaking change; none is possible here (the call is the same and both
  results are valid), so the changelog's breaking-changes list is the notice. Stated as the exception.
- **Simple mappings only**: `ß` stays `ß`. Full case mapping and locale rules (Turkish `i`) are out.
- **`len` stays bytes.** `char_count` is the character count.
- Out of scope: string interpolation, a character type, grapheme clusters, normalization, collation,
  hex and binary integer literals and digit separators (item 5), `for` over `bytes` if it does not
  exist today.

# Feature Specification: Regular Expressions

**Feature Branch**: `025-regular-expressions`

**Created**: 2026-11-19

**Status**: Implemented

**Input**: User description: "regular expressions"

Background: a gap of the roadmap's "Planned after 017" table ("Regular expressions: `std.text` has
`find`, `split`, `replace` only; linear-time matching only (no backtracking)"). Today an agent that must
validate a date, pull fields out of a log line, or collapse runs of whitespace writes a hand-made scanner
with `chars`, `find` and `slice`: long, and where agents make most of their off-by-one mistakes.

### How other languages do it

| Language | Form | Engine | What agents get wrong |
|---|---|---|---|
| Python | `re.search(r"\d+", s)`, `re.match`, `re.fullmatch`, `re.sub(r"(\w+)", r"\1", s)` | backtracking | `match` anchors at the start but not the end; `$` also matches before a final newline; catastrophic backtracking on `(a+)+b`; `\1` in a non-raw string |
| JavaScript | `/\d+/g`, `s.replace(re, "$1")`, `matchAll` | backtracking | the `g` flag's `lastIndex` state; `$1` vs `\1`; catastrophic backtracking |
| Go | `regexp.MustCompile(`\d+`)`, `FindStringSubmatch`, `ReplaceAllString(s, "${1}")` | RE2, linear time | `$1x` read as a group named `1x`; no lookaround or backreferences (by design) |
| Rust | `Regex::new(r"\d+")?`, `captures`, `replace_all(s, "$1")` | linear time | the same `$1x` trap; no lookaround |
| Java | `Pattern.compile("\\d+")`, `matcher.find()`, `replaceAll("$1")` | backtracking | double escaping in string literals; `matches()` is a full match |

From an agent's point of view:
- **Patterns from Python and JavaScript are what agents write**, and the common subset (classes, `\d`
  `\w` `\s`, quantifiers including lazy ones, groups, alternation, anchors, `\b`) works almost
  everywhere. Lookaround and backreferences are rare in generated code, and they are exactly what a
  linear-time engine cannot run.
- **"Match" means three things** (search anywhere, anchored at the start, whole string). Agents mix them
  up. One meaning, "anywhere", plus `^`/`$` for anchoring, removes the ambiguity.
- **Backtracking engines can hang on a pattern that looks harmless.** An agent will not notice; a
  linear-time engine makes it impossible, which also makes user-supplied patterns safe.
- **Escaping is the first wall**: vo strings have no raw form, so `"\d+"` is an error (E0103) today, with
  no fix. Every agent will write it first.
- **A compiled value with its own type** catches the most common silent bug, swapped arguments (pattern
  and text are both strings), and makes a bad literal pattern a check error instead of a runtime one.

What this spec must keep:
- **Principle I**: one meaning per operation, one syntax per feature (no alternative spellings inside
  patterns where a fix can point to the one form).
- **Principle II**: a bad pattern written as a literal is a check-time diagnostic pointing into the
  pattern, with a fix where one correction is unambiguous.
- **Principle III**: every match takes time linear in the text; results are identical on every platform.
- **Principle V**: the reference budget (about 12,900 of 15,000 tokens used); details live in `vo doc`.

## Clarifications

### Session 2026-11-19

- Q: API shape? → A: A compiled `Regex` value (as in Go, Rust and Java): `regex("...")` for literal
  patterns, checked at check time; `parse_regex(s) -> result[Regex]` for patterns from data. Every
  operation takes the `Regex` first.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–024: AI coding agents that write vo programs from the reference
and `vo doc`, and the people who review and run those programs.

### User Story 1 - Validate and extract (Priority: P1)

An agent writes a program that reads log lines like `2026-11-19 12:04:55 ERROR disk full (sda1)`, keeps
the lines whose level is `ERROR` or `WARN`, and prints the date, the level and the device in parentheses.
It also checks that a user-entered ID has the form `ABC-1234` before using it.

**Why this priority**: validating a format and pulling fields out of a line are the two reasons most
programs reach for regular expressions.

**Independent Test**: golden cases that match, fail to match and extract groups (numbered and named),
compared byte for byte with expected output.

**Acceptance Scenarios**:

1. **Given** the pattern `^[A-Z]{3}-\d{4}$`, **When** it is tested against `ABC-1234`, `ABC-12345` and
   `abc-1234`, **Then** only the first matches.
2. **Given** the pattern `(\d{4})-(\d{2})-(\d{2}) \S+ (ERROR|WARN)`, **When** the first match in the log
   line above is taken, **Then** group 0 is the whole matched text, groups 1–4 are `2026`, `11`, `19`
   and `ERROR`, and the match's start and end are byte offsets usable with `slice`.
3. **Given** a named group `(?<level>ERROR|WARN)`, **When** a line matches, **Then** the group's text can
   be read by its name.
4. **Given** a pattern with an optional group that did not take part in the match (`(a)?b` against
   `b`), **When** the groups are read, **Then** that group is the empty string, and the match also says
   that it did not take part.
5. **Given** a line with no match, **When** the first match is asked for, **Then** the program gets a
   value it must handle (no crash, no sentinel offset).

---

### User Story 2 - Pattern mistakes are caught before the program runs (Priority: P1)

An agent writes `"\d+"` (an invalid vo escape), a pattern with an unclosed group, a lookahead `(?=...)`,
a backreference `\1`, or Python's named group `(?P<name>...)`. `vo check` reports each at its position
inside the pattern, with a fix where the correction is clear, so the program never fails on a bad
literal pattern at run time.

**Why this priority**: escaping and unsupported features are the first things an agent hits; catching
them at check time, with fixes, keeps the write-check-fix loop short.

**Independent Test**: golden check cases, one per kind of mistake, each with the expected JSON diagnostic
(code, span, message, fix), and the fix applier re-checking each fix.

**Acceptance Scenarios**:

1. **Given** the string `"\d+"` anywhere in a program, **When** checked, **Then** E0103 has the fix
   `"\\d+"` (for any unknown escape, the fix doubles the backslash).
2. **Given** a literal pattern with an unclosed `(`, an unclosed `[`, a quantifier with nothing before it
   (`*a`), or a reversed range (`[z-a]`, `{3,1}`), **When** checked, **Then** a diagnostic points at the
   offending part of the pattern and says what is wrong.
3. **Given** a lookahead, lookbehind, backreference, atomic group or possessive quantifier in a literal
   pattern, **When** checked, **Then** the diagnostic says the feature is not supported because every
   vo pattern runs in linear time, and names the usual alternative (capture and test in code).
4. **Given** `(?P<name>...)`, **When** checked, **Then** the fix is `(?<name>...)`.
5. **Given** a literal replacement text using `\1` or `\g<name>` (Python), **When** checked, **Then**
   the fix is `$1` or `${name}`; a reference to a group the pattern does not have is a check error.
6. **Given** a pattern that is not a literal (built at run time), **When** it is compiled from text,
   **Then** an invalid pattern is an `err` value with the same message, never a crash.

---

### User Story 3 - Search, replace and split (Priority: P2)

An agent collapses runs of whitespace to one space, masks all but the last four digits of card numbers,
reformats dates from `19/11/2026` to `2026-11-19` with group references, lists every hashtag in a post,
and splits a line on commas surrounded by optional spaces.

**Why this priority**: the everyday text-processing operations after matching; each is a loop of
`find`/`slice` today.

**Independent Test**: golden cases for all matches, replace with group references, and split, including
empty matches and overlapping candidates, compared with Python's results.

**Acceptance Scenarios**:

1. **Given** `\s+` and `"a  b\t\tc"`, **When** every match is replaced by `" "`, **Then** the result is
   `"a b c"`.
2. **Given** `(\d{2})/(\d{2})/(\d{4})` and the replacement `$3-$2-$1`, **When** applied to
   `"19/11/2026"`, **Then** the result is `"2026-11-19"`; `$$` is a literal `$`, and `${1}0` separates a
   group number from a following digit.
3. **Given** `#\w+` and `"#vo is #fast, #vo"`, **When** all matches are listed, **Then** they are
   `#vo`, `#fast`, `#vo` in order, each with its offsets.
4. **Given** `\s*,\s*` and `"a , b,c"`, **When** the text is split, **Then** the parts are `a`, `b`, `c`.
5. **Given** a pattern that can match the empty string (`x*` on `"abxd"`), **When** every match is
   replaced by `-`, **Then** the result is `-a-b--d-`, the same as Python 3.7+ and JavaScript.

---

### User Story 4 - Safe on any pattern and any input (Priority: P2)

An agent builds a search tool whose users type their own patterns, and runs it over large log files. A
pattern that would hang a backtracking engine (`(a+)+$` on a long run of `a` followed by `b`) finishes
quickly; a huge or deeply nested pattern is refused with an error instead of exhausting memory.

**Why this priority**: patterns from users are untrusted input (vo's threat model treats input as
hostile); a regex engine is a classic denial-of-service hole.

**Independent Test**: step or golden cases with pathological patterns and inputs, and timed checks of
matching time against input length.

**Acceptance Scenarios**:

1. **Given** `(a+)+$` and a text of 100,000 `a` followed by `b`, **When** matched, **Then** it finishes
   in well under a second, with no match.
2. **Given** twice the text, **When** matched, **Then** it takes about twice as long (linear), not four
   times or more.
3. **Given** a pattern above the size limit (a very long pattern, or counted repetitions whose expansion
   exceeds the limit, like `(a{1000}){1000}`), **When** compiled, **Then** it is an `err` (or a check
   error for a literal) naming the limit.
4. **Given** `--timeout`, **When** a match over a very large text is still running at the deadline,
   **Then** the program stops with R0009 at the call, as for any long builtin.

---

### User Story 5 - Flags (Priority: P3)

An agent matches keywords without regard to case, anchors per line in a multi-line text, and lets `.`
cross newlines.

**Why this priority**: common but each has a workaround (`lower` first, `lines` first).

**Independent Test**: golden cases per flag.

**Acceptance Scenarios**:

1. **Given** `(?i)error`, **When** matched against `Error` and `ERROR`, **Then** both match.
2. **Given** `(?m)^\d+$` and a text of three lines, **When** all matches are listed, **Then** each line
   that is all digits is one match.
3. **Given** `(?s)a.b` and `"a\nb"`, **When** matched, **Then** it matches; without `(?s)` it does not.

### Edge Cases

- **What "match" means**: every operation searches anywhere in the text; `^` and `$` anchor. There is no
  separate "match at the start" or "match the whole text" operation.
- **`$`** matches only at the end of the text (or before a newline under `(?m)`), never before a final
  newline as in Python; `^` likewise only at the start (or after a newline under `(?m)`).
- **Leftmost-first**: among matches starting at the same position, alternatives and quantifiers are
  preferred as Perl, Python and JavaScript prefer them (greedy unless `?` makes them lazy), not the
  longest match.
- **Text is UTF-8**: `.` and a class match one code point, never part of one; offsets are bytes, as for
  `find` and `slice`, so `slice(s, m.start, m.end)` is always the matched text.
- **`\d`, `\w`, `\s`, `\b`** are ASCII (`[0-9]`, `[A-Za-z0-9_]`, ASCII whitespace), as vo's `upper` and
  `lower` are ASCII; `(?i)` folds ASCII letters only. Other letters match only themselves.
- **Empty matches**: a pattern that matches the empty string matches between every two code points and
  at both ends; an empty match right after a non-empty one is kept (Python 3.7+ and JavaScript rules).
- **Empty text, empty pattern**: the empty pattern matches once in the empty text, and at every code
  point boundary of any other text (n + 1 times for n code points).
- **Group numbering**: group 0 is the whole match; groups are numbered by their opening parenthesis, as
  everywhere; non-capturing groups `(?:...)` have no number.
- **Unknown group in a replacement** (`$7` with three groups, `${nme}`): a check error for literals, an
  `err` or R0007 at run time for a replacement built at run time; never silently empty as in Go.
- **Effects**: matching is pure; no effect is needed.
- **Memory**: compiled patterns and results count toward `--max-memory` like any value.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The standard library MUST provide a module `std.regex` with a compiled pattern type and
  operations to test for a match, take the first match, list all matches, replace all matches (with
  group references), and split around matches. No new builtin names.
- **FR-002**: Patterns MUST be compiled into a `Regex` value that every operation takes, so a pattern
  and a text can never be swapped without a type error. Two constructors: `regex(PATTERN)`, whose
  argument must be a string literal (checked at check time, so it cannot fail at run time; any other
  argument is a check error naming `parse_regex`), and `parse_regex(s) -> result[Regex]` for patterns
  from data.
- **FR-003**: Patterns MUST support: literals and escapes of metacharacters; `.`; classes `[...]`, `[^...]`
  with ranges and the shorthands; `\d \D \w \W \s \S`; anchors `^ $ \b \B`; groups `(...)`, `(?:...)`,
  `(?<name>...)`; alternation `|`; quantifiers `* + ? {n} {n,} {n,m}` and their lazy forms (`*?` etc.);
  flags `(?i)`, `(?m)`, `(?s)` (and scoped `(?i:...)`).
- **FR-004**: Backreferences, lookahead, lookbehind, atomic groups, possessive quantifiers, recursion and
  conditionals MUST be rejected with a message saying they are not supported because matching is linear
  time.
- **FR-005**: Matching time MUST be linear in the length of the text for any pattern (bounded by a
  constant that depends only on the pattern), and pattern size after expanding counted repetitions MUST
  be bounded by a documented limit.
- **FR-006**: Semantics MUST be leftmost-first with Perl-style greedy/lazy preference; `$` and `^` as in
  the Edge Cases; ASCII classes and case folding; byte offsets; empty-match handling as Python 3.7+.
- **FR-007**: A match MUST give the matched text, its start and end byte offsets, every group's text and
  whether it took part, and named groups by name.
- **FR-008**: Replacement text MUST use `$n`, `${n}`, `${name}` and `$$`; `$` followed by anything else
  is an error. A literal replacement using `\n` group references or `\g<name>` MUST get a check-time
  fix to the `$` form.
- **FR-009**: A pattern written as a string literal MUST be checked at check time: every syntax error and
  unsupported feature is a diagnostic with a stable code, pointing at the part of the pattern concerned,
  with a fix where one correction is unambiguous (`(?P<name>` → `(?<name>`).
- **FR-010**: A pattern from data MUST be compiled into a `result`: an invalid pattern is an `err` with
  the same message the check would give, never a runtime crash.
- **FR-011**: E0103 (unknown escape in a string) MUST get the fix that doubles the backslash, so
  `"\d+"` becomes `"\\d+"`.
- **FR-012**: Results MUST be identical on x86-64, ARM64 and RISC-V 64 and independent of locale and
  environment.
- **FR-013**: `--timeout` MUST stop a long match at the deadline (R0009 at the call).
- **FR-014**: `vo doc std.regex` MUST document every item with a checked example, including the pattern
  syntax summary and the escaping rule; the reference gets one row in the standard-library table and
  one sentence about writing `\\` in patterns.
- **FR-015**: Every new diagnostic code MUST have a catalog entry with an example and a fix.

### Key Entities

- **Regex**: a compiled pattern; made only by `regex` (literal) or `parse_regex` (data); immutable; can
  be stored in variables, records and lists and reused.
- **Match**: one match: text, start and end byte offsets, groups (by number, group 0 the whole match,
  with a flag for groups that did not take part), named groups by name.
- **Replacement**: text with `$n`, `${n}`, `${name}` and `$$` references.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For 100,000 random (pattern, text) pairs over the supported syntax, the match positions,
  groups, replacements and splits equal Python's `re` (with ASCII classes and `$` as end of text), and are
  identical on all three platforms.
- **SC-002**: Classic catastrophic-backtracking patterns (`(a+)+$`, `(a|a)*b`, `(.*)*x`, nested
  quantifiers) on 100,000-character inputs each finish in under 1 second, and doubling the input at
  most about doubles the time (factor below 2.5).
- **SC-003**: Searching 10 MB of log text for a typical pattern (a date and a level) finishes in under
  2 seconds.
- **SC-004**: Every kind of pattern mistake in User Story 2 is reported at check time with a stable code
  pointing into the pattern; every fix in those cases, applied, gives a program that checks.
- **SC-005**: Every new code has a catalog entry and a golden case; every acceptance scenario is a golden
  case; every `std.regex` item has a `vo doc` example that runs.
- **SC-006**: The reference grows by at most 150 tokens.

## Assumptions

- The module is `std.regex`, imported item by item like the rest of the standard library; its function
  names do not clash with builtins or with `std.text` (`replace`, `split` and `find` are taken), so a
  file can import both.
- The syntax is the common subset of Python, JavaScript, Go and Rust (RE2 syntax without its rarely used
  parts such as Unicode classes `\p{...}`). `(?<name>...)` is the one named-group form; Python's
  `(?P<name>...)` gets a fix.
- No raw string literal is added: patterns are written with `\\` for a backslash, and E0103's new fix
  makes the first mistake one round trip. A raw string syntax would be a second string form.
- Unicode classes (`\p{L}`), Unicode case folding and Unicode `\w`/`\b` are out of scope (ASCII, as the
  rest of vo's text functions); they can be added later.
- Matching operates on `str`, not `bytes`.
- Out of scope: streaming matches over input as it is read, a regex literal syntax (`/.../`), lookaround
  and backreferences (impossible in linear time), and replacement by a function (the agent can loop
  over all matches).

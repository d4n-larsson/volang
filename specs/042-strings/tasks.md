---

description: "Task list for vo-lang spec 042: Strings (escapes, raw text, characters, case)"
---

# Tasks: Strings (escapes, raw text, characters, case)

**Input**: Design documents from `specs/042-strings/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/strings.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases land in `tests/cases/` first and fail
before its code.

**Organization**:
- Setup: the test script, the results file, the area wiring.
- Foundational: the position helper for checked literals (raw strings and `\u` both need it).
- US2 (P1) escapes comes before US1 (P1) raw strings, because it is the smaller lexer change and both
  touch the same lines; US3 (P2) `for c in s`; US4 (P2) case; US5 (P3) exponents; US6 (P2) grammars,
  reference and docs.
- Polish: engines compared, three architectures, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Forms, values, codes, fixes**: contracts/strings.md. Canonical spelling rule: research R2.
- **Golden cases** run through `tests/run.sh` (and `--accept` for expectations, reviewed with `git diff
  tests/cases`); a case for a diagnostic with a fix is also applied by `tests/apply_fixes.py`.
- **Existing programs keep their meaning**: no existing golden case may change except those that
  print E0103's message, the "no exponent" message of E0105, or E0308's hint, and (US4) cases that
  upper- or lower-case non-ASCII text.
- **Both engines**: every run-time change is made in `src/vm.c` and `src/interp.c`; `tests/diff_engines.py`
  must agree.
- **Reference budget**: `python3 tests/ref_examples.py` after every reference edit; it must stay under
  15,000 tokens (about 14,960 now), so trims come before additions (research R8).
- **Between edits**: `make quick AREA=front` or `AREA=text`; `make test` once, through
  `tools/progress.py run test`, reading `build/progress/test.log`; the gate only when asked. Never two
  suites at once.

## Phase 1: Setup

- [X] T001 Create `tests/strings.py` (driver: `--group NAME`, `FAIL` lines, a one-line summary, exit 1 on a failure; `$VO_BIN`) with an empty group list; add it to the test list in `Makefile` and to areas `front` and `text` in `tools/quick.sh`; create `specs/042-strings/results.md` with headings for SC-001 to SC-006

## Phase 2: Foundational

- [X] T002 Golden cases in `tests/cases/format/`, `tests/cases/regex/`, `tests/cases/sqlite/` and `tests/cases/web/` that pin today's positions of an error inside a quoted literal with `\\`, `\"`, `\n` and `\t` before it (one per checker), so that T003 cannot move them
- [X] T003 Add `lit_source_offset(node, value_offset)` (declared in `src/vo.h`, defined beside the string decoding in `src/parse.c`): the source offset of byte `value_offset` of a string literal's value, walking the literal's source span and its escapes; use it in `src/check_format.c`, `src/check_regex.c`, `src/check_sqlite.c` and `src/check_web.c` in place of their own walks; T002's cases unchanged

## Phase 3: User Story 2 - Every character can be written (P1)

**Goal**: `\r` and `\u{...}`; one escape per character.

**Independent Test**: `tests/run.sh lex run`.

- [X] T004 [P] [US2] Golden cases: `tests/cases/run/escapes.vo` (bytes of `"\r\n"`, `"\u{e5}"`, `"\u{1F600}"`, `"\u{0}"`, `"\u{10FFFF}"`, upper-case hex digits, each beside `chr`); `tests/cases/lex/escape_u_*.vo` for each malformed form of contracts/strings.md (no brace, no digits, seven digits, a non-hex digit, `\u{D800}`, `\u{110000}`, a missing `}` with its fix) and `\x41`, `\a`, `\'`, `\0` E0103 with the new list in the message; `\u{a}`, `\u{d}`, `\u{9}`, `\u{22}`, `\u{5c}` each E0103 with the short escape as its fix (applied by `tests/apply_fixes.py`); `tests/cases/fmt/escapes.vo` (kept as written); a `regex` and a `format` literal with `\u{e5}` before an error (position exact)
- [X] T005 [US2] In `src/lex.c`, accept `\r` and `\u{H…}` (1 to 6 hex digits; a scalar value: "0 to D7FF or E000 to 10FFFF"; "not 9, A, D, 22 or 5C, which have short escapes") with the E0103 messages and the two fixes of research R3; in `src/parse.c`, decode them (`\u` to UTF-8); extend `lit_source_offset` for `\u{...}`; update E0103's entry in `docs/errors.md`

## Phase 4: User Story 1 - Write text that has its own syntax (P1) 🎯 MVP with US2

**Goal**: backtick raw strings with one canonical spelling.

**Independent Test**: `tests/run.sh lex fmt run check format regex sqlite web`.

- [X] T006 [P] [US1] Golden cases: `tests/cases/run/raw_strings.vo` (a regex with `\d`, JSON with quotes, three-line SQL: each equal to its escaped quoted form; a raw string with `#`, `//` and keywords inside; an empty one; as a `const`, an effect scope and an annotation argument; the same file saved with CRLF line ends gives the same output); `tests/cases/effects/scope_control.vo` (an effect scope with a line break written raw, and with `\n`, `\r` and `\u{0}` written quoted: each E0506); `tests/cases/lex/raw_unclosed.vo` (E0102 at the backtick, the following items still reported sensibly in a second case where it is closed later); a raw literal in `regex`, `sql`, `format` and a route with an error on its second line (position exact); `tests/cases/query/` and `tests/cases/edit/` cases where a rename does not touch a name inside a raw string and `type-at` works after one; a trace case and a record-and-replay step case whose program prints a multi-line raw string (FR-013)
- [X] T007 [P] [US1] Golden cases: `tests/cases/check/raw_not_needed.vo` (W0102 on `` `plain` `` and on an empty raw string, none on raw strings with a backslash, a `"` or a line break; the fix applied by `tests/apply_fixes.py` gives the quoted form); `tests/cases/fmt/raw_strings.vo` (the same rewrite by `vo fmt`; a multi-line raw string inside a call and inside a list is not re-indented and what follows it is laid out correctly; quoted strings never become raw; formatting twice changes nothing)
- [X] T008 [US1] In `src/lex.c`, a backtick to the next backtick as one `T_STRING` token (line breaks allowed; E0102 with the raw message when the file ends first); in `src/parse.c`, the token's text as the value when it starts with a backtick; a carriage return directly before a line feed dropped from a raw string's value; `lit_source_offset` one-for-one for raw literals, skipping such carriage returns; in `src/effects.c` and `src/check_effects.c`, E0506 for an effect scope with a control character; anything that reads a string token's source (diagnostic excerpts, `src_text`, hover) checked for multi-line tokens
- [X] T009 [US1] In `src/fmt.c`, emit a raw string as written, never re-indenting its inside and measuring the line it ends on from its last line break, and rewrite one with no backslash, `"` or line break as a quoted string; in `src/check.c`, W0102 `raw_string_not_needed` with that fix; add W0102 to `docs/errors.md` and a `contracts/codes.md` row (`specs/042-strings/contracts/codes.md`, registered in `tests/catalog.py`)

## Phase 5: User Story 3 - Look at a string's characters without copying it (P2)

**Independent Test**: `tests/run.sh run text`; `python3 tests/strings.py --group loop`.

- [X] T010 [P] [US3] Golden cases: `tests/cases/run/for_str.vo` (ASCII, two-, three- and four-byte characters, the empty string, `break`, `continue`, `return` from inside, a nested loop over the same string, `set s = ...` in the body not affecting the loop, a string from a call as the subject); `tests/cases/check/for_str_types.vo` (the variable is `str`; `for c in 5` still its error); `tests/cases/text/char_count.vo` (`char_count` and `equal_fold` samples); a coverage case and a trace case over a `for c in s` loop
- [X] T011 [P] [US3] In `tests/strings.py`, group `loop`: `for c in s` gives the same list as `chars(s)` for 200 random texts of mixed widths; a 10 MiB text looped under `--max-memory 16` (SC-002); the time against `for c in chars(s)` on 4 MiB: faster on x86-64 (the check), with the measured ratio reported against the target of 3
- [X] T012 [US3] In `src/check.c`, `check_for` accepts a `str` subject (variable type `str`); in `src/compile.c`, `src/vm.c` and `src/vo.h`, `OPC_FOR_STR` and `OPC_FOR_STR_STEP` per research R4 (the string kept in the iterator slot, a byte offset beside it, shared one-character strings for ASCII); the same loop in `src/interp.c`; the two instructions added to every table in `src/` that lists `OPC_FOR_LIST` (found with `grep -n OPC_FOR_LIST src/`); E0308's hint names `for c in s`
- [X] T013 [US3] In `std/text.vo`, `char_count(s: str) -> int` (bytes that are not continuation bytes, with `byte_at`) and `equal_fold(a: str, b: str) -> bool` (`lower(a) == lower(b)`), with doc comments and examples

## Phase 6: User Story 4 - Case and comparison beyond ASCII (P2)

**Independent Test**: `python3 tests/strings.py --group case`; `tests/run.sh text`.

- [X] T014 [P] [US4] In `tests/strings.py`, group `case`: for every code point 0 to 0x10FFFF except surrogates, `upper` and `lower` of that one character equal Python's when the code point is in U+0080–U+024F or U+0370–U+052F and Python's result is one code point, equal the character itself otherwise outside ASCII, and equal today's result in ASCII (batched: one vo program per 4,096 code points reading them from a file) (SC-003); `src/case_table.h` equals what `tools/gen_case.py` prints; `upper`/`lower` of 1 MiB of ASCII text no slower than 1.2 times the previous build when `build/vo-before` exists (reported otherwise)
- [X] T015 [P] [US4] Golden case `tests/cases/text/case_unicode.vo`: `upper("åäö")`, `lower("ÅÄÖ")`, Greek and Cyrillic words, `ß` and `ŉ` unchanged, `ı` to `I`, a string mixing covered and uncovered scripts, `equal_fold("STRASSE", "strasse")`
- [X] T016 [US4] Create `tools/gen_case.py` (from `unicodedata`: the pairs of data-model.md, two sorted arrays) and the generated `src/case_table.h`; in `src/rt_str.c`, `rt_case` keeps its ASCII byte loop and maps other characters through the table, building the result with a string builder since lengths may change; update the `upper, lower` row of the reference in T020

## Phase 7: User Story 5 - Exponent literals (P3)

**Independent Test**: `tests/run.sh lex run fmt`; `python3 tests/strings.py --group exponents`.

- [X] T017 [P] [US5] Golden cases: `tests/cases/run/exponents.vo` (`1e9`, `1.5e-3`, `2E+2`, `1e0`, `6.02e23`, `1e-320` (a subnormal), each printed and compared with `parse_float` of its text; `1e9` is a `float`); `tests/cases/lex/exponent_bad.vo` (`1e`, `1e+`, `1e400` overflow, 18 significant digits before `e`, `2em` still the suffix error); `tests/cases/fmt/exponents.vo` (kept as written); in `tests/strings.py`, group `exponents`: 10,000 random literals printed by one vo program equal Python's `float()` of the same text (SC-004)
- [X] T018 [US5] In `src/lex.c`, the exponent part of a float literal per research R6 (a `T_FLOAT` with or without a fraction; E0105 for an exponent without digits; the 17-digit rule on the digits before the exponent; overflow as today); in `src/parse.c` or wherever a float token becomes a value, the conversion `parse_float` uses; update E0105's entry in `docs/errors.md` and re-accept the golden cases that printed the "no exponent" message

## Phase 8: User Story 6 - Documented, highlighted, within budget (P2)

**Independent Test**: `python3 tests/grammars.py`; `python3 tests/ref_examples.py`; `make site-check`.

- [X] T019 [P] [US6] Teach the grammars the new forms: `editors/vscode/syntaxes/vo.tmLanguage.json` (a backtick string as a multi-line region with no escapes inside; the escapes `\r` and `\u{...}`; exponent floats), `editors/tree-sitter-vo/grammar.js` with its parser regenerated and its corpus and highlight queries extended, `editors/vim/syntax/vo.vim`, the site's lexer in `tools/site.py`; add a corpus file with at least 30 lines of raw strings (with `"`, `#`, `//` and keywords inside), new escapes and exponents where `tests/grammars.py` reads its inputs (SC-005)
- [X] T020 [US6] In `docs/reference.md`, first the trims of research R8, then the additions (the Strings bullet with the six escapes and the raw form and its canonical rule; `for c in s`; the `upper, lower` row; exponent literals; `char_count`, `equal_fold`), keeping `python3 tests/ref_examples.py` under 15,000 tokens and every example running
- [X] T021 [P] [US6] Site: the strings part of `site/docs/guide/basics.md` or `types.md` (raw strings with a regex and a JSON example, the escapes, exponents), `for c in s` where loops are taught, case mapping in the `std.text` section of `site/docs/guide/standard-library.md`; `CHANGELOG.md` (the additions, and first, as a breaking change, that `upper`/`lower` now change Latin, Greek and Cyrillic letters); `docs/roadmap.md` (status row and a "042" section); the "Fixed since the review" table in `docs/review-2026-11/README.md`
- [X] T022 [US6] SC-001: rewrite as raw strings the quoted strings in `examples/`, the doc examples in `std/*.vo`, the reference and the site whose text has a doubled backslash or an escaped quote, at least 20 places, with outputs unchanged (the suite's examples still pass); use `chr(13)`-free `"\r\n"` in `std/http.vo`'s `crlf` and in the `std.tcp` example of the site

## Phase 9: Polish

- [X] T023 Add the backtick, `\u{`, `}` and `e` after digits to the alphabet of the malformed-input fuzzer (`tests/fuzz.py` or `tests/fuzz_inputs.py`), extend `tests/gen_programs.py` to emit `for c in s`, the new escapes, raw strings and exponent literals, and run `python3 tests/diff_engines.py --count 1000` (needs `make oracle`): 0 disagreements
- [X] T024 Run the golden cases, `tests/strings.py` and `tests/grammars.py` (x86-64 only for the grammars) with `VO_BIN` set to `build-aarch64/vo` and `build-riscv64/vo`; record in `specs/042-strings/results.md`
- [X] T025 Run `make test` once through `tools/progress.py run test`, read `build/progress/test.log`, fix what it finds, and complete `specs/042-strings/results.md` (SC-001 to SC-006, the reference's final token count and what was trimmed, which existing cases changed and why, deviations, what was not verified); set the spec's status

## Dependencies

- Setup → Foundational (T002 before T003). US2 and US1 both need T003 and both edit `src/lex.c` and
  `src/parse.c`: US2 first, then US1. US3, US4 and US5 are independent of each other and of US1 (US5
  also edits `src/lex.c`: after US1). US6 needs all the forms to exist. Polish last.

## Parallel opportunities

- Test tasks within a story (T006 with T007; T010 with T011; T014 with T015); T019 and T021; the
  runtime stories US3 and US4 touch different files and can interleave.

## Implementation strategy

MVP is Setup, Foundational, US2 and US1: every character can be written, and text with its own syntax
is written as it is. Then the loop, case mapping and exponents in any order, then grammars and
documentation, where the reference budget is the tight spot.

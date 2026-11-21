# Results: Strings (escapes, raw text, characters, case)

Measured on the development machine (x86-64), 2026-11-21. The gate was not run.

## Success criteria

| Criterion | Result |
|---|---|
| SC-001: at least 20 escaped strings rewritten as raw | 35 literals in `examples/`, the library's doc examples and the site guide; outputs unchanged (doc examples, site examples and the examples' tests pass) |
| SC-002: `for c in s` on 10 MiB under a memory limit, faster than `chars` | passes under 32 MiB, not 16: `read_file` holds a text twice while it checks it (20 MiB for 10), and the loop adds nothing. 2.2 times faster than `for c in chars(s)` (86 ms against 186 ms); the target was 3 |
| SC-003: `upper` and `lower` against a reference on every code point | passes: all 1,112,064 code points; the mappings equal Python's simple (one code point) mappings in U+0080–U+024F and U+0370–U+052F, ASCII is as before, everything else is unchanged; `src/case_table.h` equals the generator's output |
| SC-004: 10,000 exponent literals | passes: each has the value Python's `float()` gives |
| SC-005: grammars agree with the toolchain | passes: 1,479 files, 258,838 positions, 0 differences between the TextMate and tree-sitter grammars; 182 lines with the new forms in the corpus; the Vim check passes |
| SC-006: reference budget, three architectures | reference about 14,986 of 15,000 tokens; the lexer, run, text, formatter, check, format and regex cases, `tests/strings.py` and the tools step case pass on ARM64 and RISC-V under qemu (the loop is 1.7 and 1.6 times faster than `chars` there) |

`make test` passed on the final code (exit 0; `strings: 3 groups, 6 cases, 0 failures`, `grammars: 7
checks, 0 failures`; the log ends at the last step). It failed twice before that, each time on a
check that pinned the old behaviour: two site pages (`upper("héllo")`, the listing of `std.text`), then
the frozen v1 corpus (`upper` of `wörld`); each was updated and the run repeated. `make site-check`
passes. The malformed-input fuzzer, with backticks, `\u{` and exponents added to its noise: 400
iterations, 0 failures.

## What the reference lost and gained

Gained: the six escapes and the raw form with its rule, `for c in text`, exponent floats, the
`upper, lower` row, `char_count` and `equal_fold`. Shortened, with no rule removed: the advice to double
backslashes for `regex`, the five `*_bytes` names of `std.http`, the list of what strings lack, the
wording of the `for` note, of the `std.http` row and of the streaming sentence. 14 tokens are left.

## Deviations from the plan

- **The formatter writes quoted strings from their value, as it always did**, so it does not keep
  escapes "as written": a printable character is written as itself (`"\u{e5}"` becomes `"å"`) and a
  control character as its escape. That is one spelling per text, which is more than the spec asked.
  An exponent literal is kept, with a lower-case `e` and no `+`.
- **Values shown to the user write control characters as escapes** (`\r`, `\u{1b}`): traces, runtime
  error reports and `expect_eq`. Before, a carriage return in a shown value was written raw. This is
  the main reason existing cases changed.
- **`parse_float` accepts an exponent too**, so that a literal and its text read alike. The plan
  assumed it already did.
- **There is no second engine to update**: programs run only as bytecode, and the "oracle" of
  `tests/diff_engines.py` is an older revision of the toolchain, which cannot read the new syntax. The
  program generator was therefore not extended; the two instructions are covered by the golden cases.
- **The position helper** is `str_lit_decode`, shared by the parser and the existing `lit_decode` of
  `format`, `regex` and `sql`. Routes keep their own rule (offsets are trusted only when the literal
  has no escapes), which is right for raw strings too.
- **Positions inside raw literals are tested for `regex` and `format`**, not for `sql` and routes.
- **`\0` and `\uHHHH` have fixes** (`\u{0}`, and the digits in braces).
- **A line that continues after a multi-line raw string is not wrapped** by the formatter, however
  long: it is stable and keeps the value, but can exceed 100 columns.
- **Zed's highlight queries did not need to change**; hover and the language server were not tested
  with multi-line strings.
- **22 existing cases changed**: carriage returns in shown values (10 step and golden cases), recording
  fingerprints (the library's text changed), the listings of `std.text` and of the modules, E0103's
  message, `1e3` (now valid), and `upper` of `wörld`. The frozen v1 corpus gained one entry in its list
  of intended changes.
- **Tests and code were written together**, story by story.

## Not verified

- The grammars in a real editor.
- The language server and hover on programs with raw strings.
- The gate (`make gate`).

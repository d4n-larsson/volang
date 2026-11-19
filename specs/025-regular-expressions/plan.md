# Implementation Plan: Regular Expressions

**Branch**: `025-regular-expressions` (work on `main`, as for specs 001–024) | **Date**: 2026-11-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/025-regular-expressions/spec.md`

## Summary

`std.regex`: a `Regex` record made by `regex("literal")` (checked when the program is checked) or
`parse_regex(s) -> result[Regex]`, and operations `is_match`, `first_match`, `all_matches`,
`replace_all`, `split_on` and `escape`. A new C engine, `src/regex.c`, parses a pattern, compiles it to
a small instruction program and runs it with a Pike VM (a breadth-first NFA simulation that carries
capture positions): time linear in the text for every pattern, leftmost-first submatches as in Perl and
Python. The same parser runs in the checker on literal patterns, so every pattern mistake is a
check-time diagnostic pointing into the literal.

**Where each part lives** (research R1–R6):

| Part | File | What |
|---|---|---|
| Engine | `src/regex.c` (new) | parser (pattern → program, errors with byte ranges and fix kinds), Pike VM, first-byte prefilter, compile cache, the primitives |
| Primitives | `src/check_expr.c`, `src/vo.h`, `src/rt_call.c`, `src/check_effects.c` | `__re_compile(pattern) -> result[str]` (ok: group names; err: the message), `__re(op, pattern, text) -> list[int]` (op 1 test, 2 first match, 3 all matches; flat byte offsets) |
| Library | `std/regex.vo` (new) | `Regex`, `Match`, the eight functions; replacement expansion and split in vo over the offsets |
| Check-time patterns | `src/check_regex.c` (new) | calls of `std.regex.regex` (literal required, pattern checked), `replace_all` with a literal replacement (syntax, `\1` fix, group numbers and names when the `Regex` is traceable) |
| Escape fix | `src/lex.c` | E0103 gains the fix that doubles the backslash |
| Literal decoding | `src/check_format.c` | its decoder (source offsets per decoded byte) becomes shared: `lit_decode` |

**Diagnostics** ([contracts/diagnostics.md](contracts/diagnostics.md)): E0319 `regex_pattern_invalid`,
E0320 `regex_unsupported`, E0321 `regex_not_literal`, E0322 `regex_replacement_invalid`; E0103 gets a
fix. At run time: a forged `Regex` with a bad pattern, or a run-time replacement with a bad reference,
is R0007.

## Technical Context

**Language/Version**: C (C11, freestanding) for the engine and checks; vo for `std/regex.vo`.

**Primary Dependencies**: none (no vendored regex library: research R1).

**Storage**: none (pure; nothing recorded or replayed).

**Testing**:
- golden cases `tests/cases/regex/` (run cases and check cases with JSONL and fixes);
- `tests/apply_fixes.py` gains the `regex` directory;
- `tests/regex_diff.py`: 100,000 random (pattern, text) pairs against Python's `re` (SC-001), with
  `--cross` for the three platforms;
- `tests/regex_speed.py`: pathological patterns, linearity, 10 MB of logs (SC-002, SC-003; `make gate`);
- `tests/doc_conformance.py` runs every `vo doc` example of `std.regex`;
- `tests/catalog.py`, `tests/ref_examples.py`;
- `make quick AREA=regex` between edits; `make test` once per change.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: SC-002 (pathological patterns under 1 s on 100,000 characters, linear growth);
SC-003 (10 MB of logs in under 2 s).

**Constraints**:
- Principle III: integer and byte operations only; no recursion whose depth depends on the input text;
  parser recursion bounded by a nesting limit.
- Security: patterns from data are hostile input: limits on pattern length, program size, repetition
  counts and nesting; the compile cache is bounded.
- Principle V: the reference grows by at most 150 tokens; everything else in `vo doc`.

**Scale/Scope**:
- C: about 1,000 lines (`regex.c` about 800; `check_regex.c` about 180; lexer fix and shared decoder
  about 20);
- vo: `std/regex.vo` about 180 lines with documentation;
- 4 catalog entries; about 30 golden cases; 2 scripts.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate | Status |
|---|---|
| I. Agent-first | PASS. One meaning of "match" (anywhere; `^`/`$` anchor); one named-group form; one replacement syntax (`$`); habits from Python get fixes, not alternative spellings. `Regex` and `str` are different types, so swapped arguments are type errors. No new syntax (no raw strings, no regex literal). |
| II. Machine-readable feedback | PASS. Four new codes, each with a span inside the literal and a fix where one correction is unambiguous; E0103 gains a fix. All pattern errors of a file are reported in one pass. |
| III. Determinism and defined behavior | PASS. Linear-time matching (no backtracking), bounded memory, `--timeout` polled in the match loop. Every failure is defined: `err` for `parse_regex`, R0007 for a forged `Regex` or a bad run-time replacement. |
| IV. Test-first | PASS. Golden cases first; Python's `re` as the oracle for match positions and groups. |
| V. Minimal surface | PASS. A library module, no new builtin name or type keyword; reference +1 row and one sentence. YAGNI: no Unicode classes, no replacement functions, no streaming. |
| Implementation constraints | PASS. C only, no assembly, no vendored code. |

Post-design re-check: unchanged (all PASS).

## Project Structure

### Documentation (this feature)

```text
specs/025-regular-expressions/
├── plan.md              # this file
├── research.md          # R1–R9
├── data-model.md        # Regex, Match, the program, the primitives' encodings, limits
├── quickstart.md        # validation scenarios
├── contracts/
│   ├── library.md       # std.regex: types, functions, docs and examples
│   ├── pattern.md       # pattern grammar and semantics
│   ├── diagnostics.md   # E0319–E0322, the E0103 fix, R0007 messages
│   └── codes.md         # catalog contract (tests/catalog.py)
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/regex.c              # new: engine and primitives
src/check_regex.c        # new: check-time pattern and replacement checks
src/check_format.c       # lit_decode shared
src/check_expr.c         # primitive names and signatures (ids 84, 85)
src/vo.h, src/rt_call.c, src/check_effects.c
src/lex.c                # E0103 fix
std/regex.vo             # new module
docs/reference.md        # std table row, escaping sentence
docs/errors.md           # E0319–E0322; E0103 Fixable: yes
docs/roadmap.md, README.md
site/docs/guide/standard-library.md   # a std.regex section
tests/cases/regex/       # golden cases
tests/regex_diff.py, tests/regex_speed.py
tests/apply_fixes.py, tests/catalog.py, tools/quick.sh, Makefile (gate)
```

**Structure Decision**: two new C files, because the engine (about 800 lines) and its check-time use are
each self-contained; the library follows the existing std pattern (vo wrappers over `__` primitives).

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| An own regex engine (~800 lines of C) | Linear time is a requirement (hostile patterns); the checker must run the same parser for check-time diagnostics with spans | Vendoring RE2 or Rust's regex: C++/Rust, not freestanding C; vendoring a C backtracking library (PCRE2, TRE): no linear-time guarantee or a large surface; writing patterns as scanners by hand: what programs do today |
| The checker knows one std function by name (`std.regex.regex`) | A literal pattern must be a check error, not a run-time one (FR-009) | A builtin `regex`: a new builtin name and type, against "no new builtin names"; checking at run time only: against Principle II |

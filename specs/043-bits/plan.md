# Implementation Plan: Bits (bitwise operations, hex and binary literals)

**Branch**: `043-bits` (work on `main`, as for specs 001–042) | **Date**: 2026-11-21 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/043-bits/spec.md`

## Summary

- **Operations**: a new library module `std/bits.vo` of one-line functions over one new primitive,
  `__bits(op, a, b) -> int`, in `src/rt_str.c`'s neighbour `src/math.c` (R1). Shift and rotation counts
  outside 0 to 63 raise R0007 there. If the wrapper-plus-primitive call is too slow for SC-003, a call
  to `__bits` with a literal operation becomes one instruction; the wrapper call itself stays, so
  traces and coverage are unchanged (R2). Otherwise nothing more is built.
- **Literals**: the lexer reads `0x` and `0b` integers as `T_INT`; one function turns an integer
  token's text into its value for the lexer's range check and the parser (R3). The formatter keeps the
  base and normalizes the case (R4).
- **Text to int**: `parse_hex` and `parse_binary` are written in vo (R5).
- **The reference** is shortened first, by rewording only, to at most 14,740 tokens, so that it ends
  at or below 14,900 with this feature's text (R7).

| Part | File | What |
|---|---|---|
| Library | `std/bits.vo` (new) | the fourteen functions |
| Primitive | `src/math.c`, `src/vo.h`, `src/check_expr.c`, `src/rt_call.c`, `src/doc.c`, `Makefile` | `__bits`; the module in the embedded library |
| Literals | `src/lex.c`, `src/parse.c`, `src/num.c` | `0x`, `0b`; `int_literal_value` |
| Formatter | `src/fmt.c` | base kept, `0xFF` form |
| Hints | `src/lex.c` | `&`, `\|`, `^`, `~`: the message names `std.bits`; `<<` and `>>` are recognised as pairs and reported the same way |
| Editors | `editors/vscode/syntaxes/vo.tmLanguage.json`, `editors/tree-sitter-vo/grammar.js` (regenerated), `editors/vim/syntax/vo.vim`, `site/pygments_vo/pygments_vo.py` | based integer literals |
| Docs | `docs/reference.md` (trimmed, then extended), `docs/errors.md` (E0105), site guide, roadmap, changelog, review | R7 |
| Tests | `tests/bits.py` (new, in `make test`), `tests/cases/{lex,fmt,run,check,doc}/` | R8 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc) and vo.

**Primary Dependencies**: none new.

**Storage**: none.

**Testing**: golden cases for the literal forms, their errors and fixes, the formatter, each
operation's edge values and the runtime error; `tests/bits.py` for random arguments against Python,
four algorithms written in vo against reference implementations (CRC-32, FNV-1a, a varint decoder,
SHA-256 against `std.crypto`), the timing of SC-003 and a literal corpus. `make quick AREA=front` and
`AREA=bits` between edits, `make test` once; the gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: CRC-32 and FNV-1a over 1 MiB in under 0.5 s each (SC-003); measured before any
optimization is written (R2).

**Constraints**: every result defined and identical on three architectures: the C uses unsigned
arithmetic for shifts, rotations and wrapping, never a signed shift or overflow (constitution:
no undefined or implementation-defined behaviour). Reference ≤ 14,900 tokens at the end.

**Scale/Scope**: about 60 lines for the primitive, 80 in the lexer and parser, 30 in the formatter,
120 of vo, 400 of tests; up to 60 more lines if R2's instruction is needed.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. One form per construct | No operator is added. Hex and binary literals are a second notation for ints: see Complexity Tracking. Within a base the formatter leaves one spelling. No digit separators. | Pass, see Complexity Tracking |
| II. Machine-readable feedback | E0105 covers the malformed literals with messages per case and a fix for `0X`/`0B`; R0007 for a bad shift count carries the count. | Pass |
| III. Determinism and defined behaviour | Every operation is total or stops with R0007; no machine-dependent shift. | Pass |
| IV. Test-first | Golden cases per form and operation before the code; reference comparisons in `tests/bits.py`. | Pass |
| V. Minimal surface, reference budget | One module, one primitive; the reference shrinks overall (R7). | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/043-bits/
├── plan.md
├── research.md          # R1–R8
├── data-model.md
├── quickstart.md
├── contracts/
│   └── bits.md          # the functions, the literals, the diagnostics
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
std/bits.vo (new), src/math.c, src/lex.c, src/parse.c, src/num.c, src/fmt.c
src/vo.h, src/check_expr.c, src/rt_call.c, src/doc.c, Makefile
editors/ (three grammars), site/pygments_vo/pygments_vo.py
docs/reference.md, docs/errors.md, tests/bits.py (new), tests/cases/
```

**Structure Decision**: no new C file; the primitive sits with `__math_int`, the literal code with
the other number code.

## Order of work

1. Trim the reference (R7), alone, so its effect is measured by itself.
2. Literals: lexer, value, formatter, golden cases, grammars.
3. The primitive and `std/bits.vo`; golden cases; `tests/bits.py` (random arguments, algorithms).
4. Measure SC-003; build R2's instruction only if needed.
5. Docs, three architectures, `make test`, results.

## Complexity Tracking

The constitution's workflow section requires this table for complexity beyond its principles.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| A second notation for integer literals (constitution I: one syntactic form per construct) | Masks, magic numbers and hash constants are defined and read in hexadecimal; `0x9E3779B9` written as `2654435769` cannot be checked against its source by eye, and a 64-bit constant with its top bit set has no decimal literal at all | Decimal only keeps one form and makes every such constant an unreadable number or a `parse_hex` call at run time. The notation is kept from multiplying spellings: within a base the formatter normalizes the case, and there are no separators, octal or suffixes |

---

description: "Task list for vo-lang spec 043: Bits (bitwise operations, hex and binary literals)"
---

# Tasks: Bits (bitwise operations, hex and binary literals)

**Input**: Design documents from `specs/043-bits/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R8), data-model.md, contracts/bits.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases land in `tests/cases/` first and fail
before its code.

**Organization**:
- Setup: the test script, the area wiring, the results file.
- Foundational: the reference trim, alone (it gates every later reference edit).
- US1 (P1) the bit operations; US2 (P1) wrapping arithmetic and rotations (the same primitive and
  module, so they follow directly); US3 (P2) hex and binary literals; US4 (P2) speed; US5 (P2)
  grammars and documentation.
- Polish: three architectures, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Functions, results, examples, diagnostics**: contracts/bits.md. Literal forms: data-model.md.
- **Golden cases** run through `tests/run.sh` (`--accept` for expectations, reviewed with `git diff
  tests/cases`); a diagnostic with a fix is also applied by `tests/apply_fixes.py`.
- **No existing golden case may change**, except those that print the messages of the operator-habit
  errors (`&`, `|`, `^`, `~`, `<<`, `>>`) and the module listings, which gain `std.bits`.
- **Defined behaviour**: the C for shifts, rotations and wrapping works on `uint64_t` only: no signed
  shift, no signed overflow, no shift by 64 (research R1).
- **Reference budget**: `python3 tests/ref_examples.py` after every reference edit; at most 14,740
  tokens after T003, at most 14,900 at the end.
- **Between edits**: `make quick AREA=front` or `AREA=bits`; `make test` once, through
  `tools/progress.py run test`, reading `build/progress/test.log`; the gate only when asked. Never two
  suites at once.

## Phase 1: Setup

- [X] T001 Create `tests/bits.py` (driver: `--group NAME`, `FAIL` lines, a one-line summary, exit 1 on a failure; `$VO_BIN`) with an empty group list and the directory `tests/bits/`; add the script to the test list in `Makefile`; add area `bits` to `tools/quick.sh` (golden areas `lex fmt run check const doc`, scripts `catalog.py ref_examples.py doc_conformance.py bits.py`, files `std/bits.vo`, `src/math.c`, `tests/bits.py`); create `specs/043-bits/results.md` with headings for SC-001 to SC-005 and a table "Reference sentences shortened" (before, after, tokens saved)
- [X] T002 Add builtin id `B_BITS` (`__bits(op, a, b) -> int`, signature `iii>i`, no effect) in `src/vo.h`, `src/check_expr.c` and `src/rt_call.c`, calling `bits_op(op, a, b, call)` declared in `src/vo.h` and stubbed in `src/math.c` to return 0 (the module file itself is added in T006, so that no empty module is ever listed)

## Phase 2: Foundational

- [X] T003 Shorten `docs/reference.md` by rewording only, to at most 14,740 tokens (research R7): no rule, code, example or table row removed; record each change in the table of `specs/043-bits/results.md` (if rewording cannot reach 14,740, stop where it honestly can and say so there: the hard bound is 14,900 at the end); show the table to the project owner before T018; `python3 tests/ref_examples.py` and the cases that print the reference (`vo reference`) pass, with their expectations re-accepted

## Phase 3: User Story 1 - Work on the bits of an integer (P1) 🎯 MVP

**Goal**: and, or, xor, not and the three shifts.

**Independent Test**: `tests/run.sh run check`; `python3 tests/bits.py --group ops`.

- [X] T004 [P] [US1] Golden cases: `tests/cases/run/bits.vo` (`bit_and`, `bit_or`, `bit_xor`, `bit_not`, `shift_left`, `shift_right`, `shift_right_logical` at 0, 1, -1, 9223372036854775807, -9223372036854775808 and counts 0, 1 and 63, with the examples of contracts/bits.md); `tests/cases/run/bits_bad_shift.vo` and `_json` (R0007 "A shift count must be 0 to 63, got N." for a count of 64, in a function called from `main`, with the count in the report), one more case each for -1 and for `shift_right_logical`; `tests/cases/check/bits_pure.vo` (used in a pure function, a `requires` clause and a `prop`, with no effect declared)
- [X] T005 [P] [US1] In `tests/bits.py`, group `ops`: a file of 10,000 random pairs (the values 0, 1, -1, the two extreme ints and powers of two over-represented; second values also reduced to 0..63), one vo program that reads it with `each_line` and prints each of the seven operations per pair, compared with Python computed with 64-bit masking (SC-001)
- [X] T006 [US1] In `src/math.c`, `bits_op` for operations 1 to 7 of research R1 on `uint64_t` (the arithmetic right shift without a signed shift; R0007 for a count outside 0 to 63); create `std/bits.vo` with the seven functions, doc comments and the examples of contracts/bits.md, and add it to `STD_FILES` in `Makefile` and `"bits"` to `doc_modules` in `src/doc.c`

## Phase 4: User Story 2 - Arithmetic that wraps (P1)

**Goal**: rotations and wrapping add, subtract and multiply.

**Independent Test**: `tests/run.sh run`; `python3 tests/bits.py --group ops` and `--group algorithms`.

- [X] T007 [P] [US2] Extend `tests/cases/run/bits.vo` (`rotate_left`, `rotate_right` by 0, 1 and 63; `wrapping_add`, `wrapping_sub`, `wrapping_mul` at the extreme ints; that `+` on the same values is still R0001, in `tests/cases/run/bits_overflow_unchanged.vo`) and the `ops` group of `tests/bits.py` (the five more operations; R0007 for a rotation by 64)
- [X] T008 [P] [US2] In `tests/bits/`, four vo programs, each reading inputs from a file and printing one result per line: `crc32.vo` (table-driven, the table built at start), `fnv1a.vo` (64-bit), `varint.vo` (LEB128 decode of a byte sequence into ints), `sha256.vo` (the whole algorithm on `bytes`); in `tests/bits.py`, group `algorithms`: 1,000 random inputs each against `zlib.crc32`, a Python FNV-1a, a Python LEB128 decoder and `hashlib.sha256`, and for SHA-256 also equality with `std.crypto.sha256` inside the vo program (SC-002)
- [X] T009 [US2] In `src/math.c`, operations 8 to 12 (a rotation by 0 returns the value without shifting by 64); in `std/bits.vo`, the five functions with doc comments and examples

## Phase 5: User Story 3 - Write constants in the base they are thought in (P2)

**Goal**: `0x` and `0b` literals, `parse_hex`, `parse_binary`.

**Independent Test**: `tests/run.sh lex fmt run const`; `python3 tests/bits.py --group literals`.

- [X] T010 [P] [US3] Golden cases: `tests/cases/run/based_literals.vo` (`0xFF`, `0xff`, `0x0`, `0x7FFFFFFFFFFFFFFF`, `0x8000000000000000`, `0xFFFFFFFFFFFFFFFF`, `0b1010`, a 64-digit binary literal, `-0xFF`, `0x1e3` is 483, each printed; as a `const`, an annotation argument `@range(0x0, 0xFF)` and a `for` bound); `tests/cases/lex/based_bad.vo` and `_json` (`0x`, `0b`, `0xg`, 17 hex digits, 65 binary digits, `0XFF` and `0B1` with their fix applied by `tests/apply_fixes.py`, `0xFFg`, `0b102`, `0x-1`, and `1_000`, which stays the suffix error: there are no separators); `tests/cases/fmt/based_literals.vo` with its `.expected.vo` (`0xff` to `0xFF`, `0x00ff` to `0x00FF`, `0b0011` kept, decimal untouched); `tests/cases/run/parse_hex.vo` (`parse_hex` and `parse_binary`: valid forms in both letter cases, `FFFFFFFFFFFFFFFF` as -1, 16 and 17 digits, empty, a `-`, a prefix and a bad digit as errors)
- [X] T011 [P] [US3] In `tests/bits.py`, group `literals`: 10,000 random hex and binary literals of every length, printed by vo programs and compared with Python's value interpreted as a signed 64-bit pattern (SC-004)
- [X] T012 [US3] In `src/num.c`, `int_literal_value(text, n, &value)` for decimal, `0x` and `0b` text; in `src/lex.c`, the based forms in the number rule with the E0105 cases and the prefix fix of research R3; in `src/parse.c`, the value through `int_literal_value`; in `src/fmt.c`, an `N_INT` whose source starts with `0x` or `0b` written with a lower-case prefix, its digits as written and hex letters in upper case; update E0105's entry in `docs/errors.md`
- [X] T013 [US3] In `std/bits.vo`, `parse_hex` and `parse_binary` per research R5, with doc comments and examples

## Phase 6: User Story 4 - Fast enough to hash real data (P2)

**Independent Test**: `python3 tests/bits.py --group speed`.

- [X] T014 [US4] In `tests/bits.py`, group `speed`: `tests/bits/crc32.vo` and `tests/bits/fnv1a.vo` over one 1 MiB input, timed inside the program with `now_ms`; reported, and each under 0.5 s on x86-64 (SC-003)
- [X] T015 [US4] Only if T014 misses its bound: `OPC_BITS` in `src/vo.h`, `src/compile.c` and `src/vm.c` for a call of `__bits` with a literal operation (research R2); the wrapper call stays, and a trace case over a program using `std.bits` is unchanged by it; if the bound is still missed, report it and stop for a decision; otherwise record in `specs/043-bits/results.md` that it was not needed, with the measured times

## Phase 7: User Story 5 - Documented, highlighted, within budget (P2)

**Independent Test**: `python3 tests/grammars.py`; `python3 tests/ref_examples.py`; `make site-check`.

- [X] T016 [P] [US5] Based integer literals in `editors/vscode/syntaxes/vo.tmLanguage.json`, `editors/tree-sitter-vo/grammar.js` (parser regenerated with `make grammars`), `editors/vim/syntax/vo.vim` and `site/pygments_vo/pygments_vo.py`; `python3 tests/grammars.py` passes over the new case files
- [X] T017 [P] [US5] In `src/lex.c`, `<<` and `>>` recognised as pairs and reported as E0101 over both characters, and the messages for `&`, `|`, `^`, `~`, `<<` and `>>` name `std.bits` (research R6); golden case `tests/cases/lex/bit_operators.vo` (each of the six, one error each; `a < < b` unchanged; `&&` and `||` keep their fixes); re-accept the existing cases that print these messages
- [X] T018 [US5] In `docs/reference.md`: the Integers bullet (`0xFF`, `0b1010`, a 64-bit pattern, still no separators or octal), the "no bitwise operators" clause pointing at `std.bits`, one `std.bits` row in the library table; at most 14,900 tokens
- [X] T019 [P] [US5] Site: a section "std.bits" in `site/docs/guide/standard-library.md` with the FNV-1a example of quickstart.md (with its output block) and a flag-set example; the literals in `site/docs/guide/basics.md` and `types.md`; `CHANGELOG.md`; `docs/roadmap.md` (status row, a "043" section); the "Fixed since the review" table in `docs/review-2026-11/README.md`

## Phase 8: Polish

- [X] T020 Run the golden cases of `lex`, `fmt`, `run`, `check`, `const` and `python3 tests/bits.py` with `VO_BIN` set to `build-aarch64/vo` and `build-riscv64/vo`; record in `specs/043-bits/results.md`
- [X] T021 Run `make test` once through `tools/progress.py run test`, read `build/progress/test.log`, fix what it finds, and complete `specs/043-bits/results.md` (SC-001 to SC-005 with figures, the reference's token count before, after the trim and at the end, which existing cases changed and why, deviations, what was not verified); set the spec's status

## Dependencies

- Setup → T003 (the trim) before any other reference edit (T018). US1 → US2 (same files). US3 is
  independent of US1 and US2 except for `std/bits.vo` (T013 after T009). US4 needs US2's programs.
  US5 needs the functions and literals. Polish last.

## Parallel opportunities

- T004 with T005; T007 with T008; T010 with T011; T016, T017 and T019. The literal work (T012) and
  the primitive (T006, T009) touch different files.

## Implementation strategy

MVP is Setup, the trim, US1 and US2: a program can compute a checksum. Then the literals that make
such programs readable, then the speed measurement (which decides whether T015 exists), then grammars
and documentation.

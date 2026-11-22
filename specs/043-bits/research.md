# Research: Bits

## R1. The operations

**Decision**: one primitive `__bits(op, a, b) -> int` (signature `iii>i`, no effect), implemented in
`src/math.c` on `uint64_t` copies of the arguments, and `std/bits.vo` with one exported one-line
function per operation:

| op | Function | Result |
|---|---|---|
| 1 | `bit_and(a, b)` | `a & b` |
| 2 | `bit_or(a, b)` | `a \| b` |
| 3 | `bit_xor(a, b)` | `a ^ b` |
| 4 | `bit_not(a)` | `~a` |
| 5 | `shift_left(a, n)` | `a << n`, bits leaving at the top |
| 6 | `shift_right(a, n)` | arithmetic: the sign bit repeated |
| 7 | `shift_right_logical(a, n)` | zeros enter at the top |
| 8 | `rotate_left(a, n)` | |
| 9 | `rotate_right(a, n)` | |
| 10 | `wrapping_add(a, b)` | the low 64 bits |
| 11 | `wrapping_sub(a, b)` | |
| 12 | `wrapping_mul(a, b)` | |

`n` outside 0 to 63 raises R0007 ("A shift count must be 0 to 63, got N."). The arithmetic right shift
is computed without a signed shift (`(x >> n) | (x < 0 ? ~(~0 >> n) : 0)` on unsigned values), since a
signed right shift is implementation-defined in C. A rotation by 0 avoids the shift by 64.

**Alternatives considered**: operators (the review and the spec keep the operator table); twelve
builtins without a module (twelve rows in the reference's builtin table, about 150 tokens the
reference does not have).

## R2. Speed

**Decision**: measure first. A wrapper call plus a primitive call per operation is expected to be
around 60 to 100 ns; CRC-32 with a table does four operations per byte, so 1 MiB is 0.25 to 0.4 s,
near the 0.5 s bound. If the measurement misses it:

`OPC_BITS` (`a = op(b, c)`, the operation in the instruction) is emitted for a call of `__bits` whose
first argument is a literal.
Only that. The wrapper call stays: a trace records a call and a return for every vo function, and
coverage counts the library's functions, so compiling the wrappers away at their call sites would
change what `--trace` and `--coverage` show. If the instruction alone does not reach the bound, the
bound is reported as missed and the choice comes back to the project owner.

The instruction is local to `src/compile.c` and `src/vm.c`. If the bound is met without it, it is not
written, and the measurement is recorded.

## R3. Literals

**Decision**: in the lexer's number rule, `0x` followed by a hex digit, or `0b` followed by `0` or `1`,
starts a based integer: all following hex (or binary) digits are taken; `lx_number_suffix` then reports
other letters as today. E0105 cases, each with its message: no digit after the prefix (`0x`, `0b`,
`0xg`); more than 16 hex or 64 binary digits; an upper-case prefix (`0X1F`, `0B1`), with a fix that
lowers it. `int_literal_value(text, n, &value)` in `src/num.c` returns the value for decimal, hex and
binary text (the 64-bit pattern for the last two) and is used by the lexer's range check and by the
parser, replacing their direct calls of `dec_to_int`. `0b102` and `0xFFg` are the suffix error.

A decimal literal starting with `0` followed by `x` or `b` cannot mean anything else today (it is the
suffix error), so no program changes.

## R4. Formatter

**Decision**: for an `N_INT` whose source starts with `0x` or `0b`, the formatter writes the prefix in
lower case and the digits as written, hex letters in upper case; leading zeros are kept (`0x00FF` is a
16-bit mask by intent). Other int literals are written from their value as today.

## R5. Text to int

**Decision**: `parse_hex(s)` and `parse_binary(s)` in vo: 1 to 16 hex digits (either case) or 1 to 64
binary digits, giving the 64-bit pattern, as the literals do; no sign; anything else is `err("invalid hex: TEXT")` or `err("invalid binary: TEXT")`, in `parse_int`'s
style. No prefix is accepted: the caller strips one if its input has it.

## R6. Hints for operator habits

**Decision**: `&`, `|`, `^` and `~` are lexer errors today (E0101, unexpected character; `&&` and `||`
have their own habit messages). `<<` and `>>` are not: they lex as two comparison tokens and fail in
the parser ("Expected an expression, found `<`"). The lexer therefore recognises `<<` and `>>` as a
pair and reports them as it does `&`: one E0101 over both characters, after which lexing goes on (the
parser sees nothing of them). `a < < b` with a space is unchanged. The message of all six gains "for bits, use `std.bits` (`bit_and`,
`shift_left`, ...)". `&&` and `||` keep their fixes to `and` and `or`. No fix is offered for the
single forms (the rewrite needs an import and a call).

## R7. The reference

**Decision**: first a change that only shortens `docs/reference.md`, by at least 250 tokens, to at
most 14,740: rewordings that keep every rule and every example. Candidates, to be confirmed by
measuring each: the library notes that repeat what `vo doc` prints (the `std.web` paragraph, the
`serve_workers` paragraph, the HTTPS paragraph), sentences that list what does not exist where an
error code already says so, and table cells with full sentences. Then this feature adds about 100
tokens: the Integers bullet (`0xFF`, `0b1010`, the 64-bit pattern, still no separators or octal), the
"no bitwise operators" clause pointing at `std.bits`, and one library row. The results list every
sentence that was shortened, before and after.

**Why 250**: the spec aims at 150 tokens of headroom gained and an end at or below 14,900; 14,986 −
250 + 100 is 14,836. If rewording alone cannot reach 250, the trim stops where it honestly can, the
table says so, and the hard requirement is only the end at or below 14,900. The table is shown to the
project owner before the feature's own text is added.

## R8. Tests

- Golden cases: `lex/` (each literal form, each malformed one with its message and the prefix fix,
  `0x1e3`, `-0xFF`, the largest patterns), `fmt/` (case normalized, base and leading zeros kept),
  `run/bits.vo` (every operation at 0, 1, -1, the extreme ints and counts 0 and 63; wrapping at the
  edges), `run/bits_bad_shift.vo` (R0007 for 64 and -1, for each of the five functions),
  `check/bits_pure.vo` (in a contract, a property and a pure function), `const/` (a based literal as
  a constant, an annotation argument and a range bound), the operator-habit hints.
- `tests/bits.py` (in `make test`): group `ops` (10,000 random pairs per operation, with the extreme
  values over-represented, read from a file by one vo program, against Python with 64-bit masking);
  `algorithms` (CRC-32 against `zlib.crc32`, FNV-1a 64, a LEB128 varint decoder, SHA-256 against
  `hashlib` and `std.crypto.sha256`, 1,000 inputs each, the programs kept in `tests/bits/` as vo
  files that the site can show); `literals` (10,000 random hex and binary literals against Python);
  `speed` (SC-003: reported, and checked on x86-64).
- `tests/grammars.py` over the new files; `tests/doc_conformance.py` for the module's examples.

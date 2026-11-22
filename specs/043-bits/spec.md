# Feature Specification: Bits (bitwise operations, hex and binary literals)

**Feature Branch**: `043-bits`

**Created**: 2026-11-21

**Status**: Implemented ([results](results.md))

**Input**: User description: "Bitwise operations and hex literals, the review's language item 5
(docs/review-2026-11/language.md): 'No bitwise operators, and no hex literals. A program cannot compute
a checksum or a hash, read a binary header, decode a varint, or test a flag. `bytes` exists since spec
021, so this is the missing half.' The review's sketch keeps the operator table as it is and makes the
operations functions of a module `std.bits`: and, or, xor, not, shift left, shift right (arithmetic
and logical), plus wrapping add and multiply for hashes (ordinary overflow is R0001), and adds `0xFF`
and `0b1010` literals and `_` digit separators. Must keep: no new operators, one way to write each
thing, defined behaviour for every operation (constitution III), determinism, three architectures,
fixes on diagnostics, the editor grammars held to the toolchain, and the reference under 15,000 tokens
(about 14,986 now: this feature must first make room)."

Background: vo has integers and, since spec 021, bytes, but no way to look inside an integer. A
checksum, a hash, a flag set, a packed header, a variable-length integer, a colour: each is a few
lines in any other language and cannot be written in vo at all, because there is no and, or, xor or
shift, and because integer overflow stops the program where a hash needs it to wrap. The constants such
code is full of (`0xFF`, `0x9E3779B9`) have to be converted to decimal by hand.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Work on the bits of an integer (Priority: P1)

A developer tests and sets flags, masks and shifts fields out of a packed value, decodes a
variable-length integer from bytes, and computes a checksum.

**Why this priority**: it is the capability that is absent; the rest makes it pleasant.

**Independent Test**: a varint decoder, a CRC-32 and a flag set written in vo give the results
Python's gives for the same inputs.

**Acceptance Scenarios**:

1. **Given** two ints, **When** combined with and, or or xor, **Then** the result is the bitwise
   combination of their 64-bit two's complement forms; not gives the complement.
2. **Given** an int and a count from 0 to 63, **When** shifted left, **Then** bits leave at the top
   and zeros enter at the bottom, with no overflow error; shifted right, the sign bit is repeated
   (arithmetic) or zeros enter (logical), by the function chosen.
3. **Given** a shift count below 0 or above 63, **When** shifted, **Then** the program stops with a
   runtime error naming the count: no result depends on the machine.
4. **Given** these functions, **When** checked, **Then** they are pure: usable in any function, in
   contracts and in properties, with no effect.

---

### User Story 2 - Arithmetic that wraps (Priority: P1)

A developer writes a hash (FNV, a multiplicative hash, a checksum with carries discarded) where sums
and products are meant to wrap at 64 bits.

**Why this priority**: without it the bit operations cannot express any real hash: `*` stops the
program on overflow, by design.

**Independent Test**: FNV-1a 64 and a 64-bit multiplicative hash of 1,000 inputs equal Python's
results computed with a 64-bit mask.

**Acceptance Scenarios**:

1. **Given** two ints, **When** added, subtracted or multiplied with the wrapping functions, **Then**
   the result is the low 64 bits of the exact result, as a two's complement int, and never an error.
2. **Given** ordinary `+`, `-` and `*`, **Then** unchanged: overflow is still R0001.
3. **Given** a rotation by 0 to 63, **Then** the bits that leave one end enter the other.

---

### User Story 3 - Write constants in the base they are thought in (Priority: P2)

A developer writes `0xFF`, `0x9E3779B9` and `0b1010_0000` style constants directly: masks, magic
numbers, hash seeds.

**Why this priority**: the operations work on decimal literals, but nobody reads `2654435769`.

**Independent Test**: each hex and binary literal equals its decimal value; a 16-digit hex literal
with its top bit set is the negative int with that bit pattern; malformed forms have errors with fixes
where one correction exists.

**Acceptance Scenarios**:

1. **Given** `0x` followed by 1 to 16 hexadecimal digits, or `0b` followed by 1 to 64 binary digits,
   **Then** an `int` literal with that bit pattern: a value of 2^63 or more is the negative int with
   the same 64 bits (`0xFFFFFFFFFFFFFFFF` is -1).
2. **Given** such a literal wherever an int literal is allowed (expressions, constants, annotation
   arguments, `match` is not affected), **Then** it is accepted.
3. **Given** `0x` or `0b` with no digits, too many digits, a digit not of the base, or an upper-case
   prefix (`0XFF`), **Then** E0105 with a message for that case, and a fix for the prefix.
4. **Given** `vo fmt`, **Then** a hex or binary literal stays in its base, with a lower-case prefix
   and upper-case hex digits, so that each has one spelling; decimal literals are untouched.
5. **Given** an int, **When** formatted with `format`'s existing `x` and `b` options, **Then**
   unchanged; and `std.bits` gives the reverse: text in base 16 or 2 to an int.

---

### User Story 4 - Fast enough to hash real data (Priority: P2)

A hash or checksum over a megabyte of bytes finishes in well under a second, so that programs use the
operations for real work, not only for flags.

**Independent Test**: CRC-32 and FNV-1a over 1 MiB each run in under 0.5 s on the development
machine.

**Acceptance Scenarios**:

1. **Given** a loop that calls two bit operations per byte over 1 MiB, **Then** it finishes in under
   0.5 s.

---

### User Story 5 - Documented, highlighted, within budget (Priority: P2)

`vo doc std.bits` documents every function with an example; the reference describes the literals and
names the module, after text elsewhere in it has been shortened to make room for this and for later
features; the editor grammars colour the new literals.

**Acceptance Scenarios**:

1. **Given** the reference, **Then** it is at most 14,900 tokens with the new text in it. The saving
   elsewhere is by rewording only, without removing a rule; every shortened sentence is listed before
   and after for review. The aim is 150 tokens of headroom gained; if rewording cannot reach that, the
   list says how far it got.
2. **Given** a file with hex and binary literals, **Then** the three grammars and the site's lexer
   give them the number class, as the toolchain does.

### Edge Cases

- `shift_left(1, 63)` is the most negative int; `shift_left(x, 64)` is a runtime error, not 0.
- `0x8000000000000000` is the most negative int, which has no decimal literal today.
- `-0xFF` is unary minus applied to 255. `0x-1` is an error.
- `0xff` and `0xFF`: both accepted; the formatter writes `0xFF`.
- A hex literal followed by a letter that is not a hex digit (`0xFFg`), or `0b102`: the number-suffix
  error, as for `10px`.
- `1e3` stays a float; `0x1e3` is the int 483: the prefix decides.
- Hex and binary literals are ints only: there is no hex float and no hex `bytes` literal
  (`std.bytes.from_hex` exists).
- The wrapping functions on values that do not overflow give what `+`, `-` and `*` give.
- Text to int in base 16 or 2: digits only, read as a 64-bit pattern like the literals; no sign and
  no prefix (a sign on a bit pattern has no clear meaning); errors as `parse_int`'s.
- Constants cannot call functions, so a constant mask is written as a literal: that is what the
  literals are for.

## Requirements *(mandatory)*

### Functional Requirements

**Operations**

- **FR-001**: The standard library MUST provide a module `std.bits` with pure functions on `int`:
  bitwise and, or, xor and not; shift left, arithmetic shift right and logical shift right; rotate
  left and right; wrapping add, subtract and multiply.
- **FR-002**: Every function MUST be defined for every argument: shifts and rotations accept counts
  0 to 63 and stop the program with a runtime error (the existing code for an invalid argument)
  otherwise; nothing else can fail.
- **FR-003**: Results MUST be identical on the three architectures and under record and replay.
- **FR-004**: `std.bits` MUST provide text-to-int for base 16 and base 2, returning a result.

**Literals**

- **FR-005**: Integer literals MUST accept `0x` with 1 to 16 hexadecimal digits and `0b` with 1 to 64
  binary digits, as the 64-bit pattern they write. Malformed forms are E0105 with a message for the
  case, and a fix where one correction exists.
- **FR-006**: `vo fmt` MUST keep a literal's base and write hex digits in upper case with a lower-case
  prefix.
- **FR-007**: Digit separators (`1_000_000`) are NOT added (see Assumptions).

**Fit**

- **FR-008**: No operator is added and no existing operator, literal or program changes meaning.
- **FR-009**: The editor grammars, the site's lexer and the formatter MUST handle the new literals;
  `tests/grammars.py` MUST pass.
- **FR-010**: The reference MUST describe the literals and name the module, and MUST end at or below
  14,900 tokens; what was shortened is listed in the results.
- **FR-011**: `vo doc std.bits` MUST document every function with a runnable example.

### Key Entities

- **Bit operation**: a pure function from ints to an int, on the 64-bit two's complement form.
- **Based literal**: an integer literal in base 16 or 2.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For each operation, 10,000 random argument pairs (including the extreme values) give the
  result a reference implementation with 64-bit masking gives.
- **SC-002**: CRC-32, FNV-1a 64, a varint decoder and SHA-256 written in vo agree with reference
  implementations on 1,000 inputs each (SHA-256 against `std.crypto.sha256`).
- **SC-003**: CRC-32 and FNV-1a over 1 MiB each take under 0.5 s on the development machine (x86-64;
  the other architectures run under emulation and are not timed).
- **SC-004**: Every hex and binary literal in a 10,000-literal corpus has the value the reference gives.
- **SC-005**: The reference is at or below 14,900 tokens; the grammar check passes; the whole suite
  passes on three architectures; no existing golden case changes except those that print the
  reference, the list of modules, or the messages for `&`, `|`, `^`, `~`, `<<` and `>>`.

## Assumptions

- **Functions in a module, not operators**, as the review suggests: the operator table stays as it
  is, and `&`, `|`, `^`, `<<`, `>>` stay errors (their messages can point at `std.bits`).
- **Names**: `and`, `or` and `not` are keywords, so the functions are `bit_and`, `bit_or`, `bit_xor`,
  `bit_not`, `shift_left`, `shift_right` (arithmetic), `shift_right_logical`, `rotate_left`,
  `rotate_right`, `wrapping_add`, `wrapping_sub`, `wrapping_mul`, `parse_hex`, `parse_binary`.
- **No digit separators.** The review proposes `1_000_000`; it would give every number two spellings
  (the constitution's one form per construct), and the reference today says there are none. Left out
  unless asked for.
- **Hex and binary literals are a second notation for ints**, recorded as a justified deviation in
  the plan's Complexity Tracking, as raw strings were: the formatter keeps one spelling within each
  base, and the base is the writer's choice because it carries meaning (a mask is read in hex).
- **A 64-bit pattern, not a magnitude**: hex literals with the top bit set are negative ints. Hash
  constants need all 64 bits and vo has no unsigned type.
- **Out-of-range shift counts stop the program**, as other invalid arguments do, instead of giving 0
  or wrapping the count: each of those choices matches some machine and surprises someone.
- No unsigned type, no fixed-width (8, 16, 32-bit) integer types, no bit counting functions
  (`count_ones`, `leading_zeros`) in this version; they are added when a program needs them.
- The reference is trimmed as part of this feature because three features in a row ended within 60
  tokens of its limit.

# Contract: `std.bits` and based literals

## Operations (on the 64-bit two's complement form of an `int`)

| Call | Result | Examples |
|---|---|---|
| `bit_and(a, b)`, `bit_or(a, b)`, `bit_xor(a, b)` | bit by bit | `bit_and(12, 10)` is 8; `bit_or(12, 10)` is 14; `bit_xor(12, 10)` is 6 |
| `bit_not(a)` | every bit flipped | `bit_not(0)` is -1 |
| `shift_left(a, n)` | zeros enter at the bottom; no overflow error | `shift_left(1, 63)` is -9223372036854775808 |
| `shift_right(a, n)` | the sign bit is repeated | `shift_right(-8, 1)` is -4 |
| `shift_right_logical(a, n)` | zeros enter at the top | `shift_right_logical(-1, 60)` is 15 |
| `rotate_left(a, n)`, `rotate_right(a, n)` | bits leaving one end enter the other | `rotate_left(1, 63)` is -9223372036854775808 |
| `wrapping_add`, `wrapping_sub`, `wrapping_mul` | the low 64 bits of the exact result | `wrapping_add(9223372036854775807, 1)` is -9223372036854775808 |
| `parse_hex(s)`, `parse_binary(s)` | `ok(int)` for 1 to 16 hex (1 to 64 binary) digits, as a 64-bit pattern; no sign, no prefix; `err("invalid hex: …")` otherwise | `parse_hex("ff")` is `ok(255)`; `parse_hex("FFFFFFFFFFFFFFFF")` is `ok(-1)` |

`n` below 0 or above 63: R0007, "A shift count must be 0 to 63, got N." Nothing else fails.

## Literals

| Source | Value |
|---|---|
| `0xFF`, `0xff` | 255 |
| `0x8000000000000000` | -9223372036854775808 |
| `0xFFFFFFFFFFFFFFFF` | -1 |
| `0b1010` | 10 |

| E0105 when | Fix |
|---|---|
| `0x` or `0b` with no digit of its base after it | none |
| more than 16 hex digits or 64 binary digits | none |
| `0X…`, `0B…` | the lower-case prefix |
| letters after the digits (`0xFFg`, `0b102`, `10px`) | none (unchanged) |

`vo fmt`: `0xff` becomes `0xFF`; the base and leading zeros are kept; decimal literals are unchanged.

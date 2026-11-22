# Data Model: Bits

## Integer literals

| Form | Digits | Value |
|---|---|---|
| decimal | `0` or more digits, at most 9223372036854775807 | the number (unchanged) |
| `0x…` | 1 to 16 of `0-9a-fA-F` | the 64-bit pattern: 2^63 and above are negative ints |
| `0b…` | 1 to 64 of `0` `1` | the 64-bit pattern |

No separators, no octal, no suffix, no upper-case prefix. Canonical form: prefix lower case, hex
letters upper case, digits (and leading zeros) as written.

## `std.bits`

```text
bit_and(a: int, b: int) -> int          bit_or(a: int, b: int) -> int
bit_xor(a: int, b: int) -> int          bit_not(a: int) -> int
shift_left(a: int, n: int) -> int       shift_right(a: int, n: int) -> int
shift_right_logical(a: int, n: int) -> int
rotate_left(a: int, n: int) -> int      rotate_right(a: int, n: int) -> int
wrapping_add(a: int, b: int) -> int     wrapping_sub(a: int, b: int) -> int
wrapping_mul(a: int, b: int) -> int
parse_hex(s: str) -> result[int]        parse_binary(s: str) -> result[int]
```

All pure. `n`: 0 to 63, else R0007.

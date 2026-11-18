# Data Model: Math functions

No new runtime types. The entities are the module's functions, the two primitives and the vendored
library record.

## Math function (`std.math`)

| Field | Values |
|---|---|
| name | `sqrt pow exp log sin cos tan asin acos atan atan2 pi floor ceil round` |
| signature | `(x: float) -> float`; `(x: float, y: float) -> float` for `pow` and `atan2`, (`atan2` takes `y` first, as in C); `() -> float` for `pi`; `(x: float) -> int` for `floor`, `ceil` and `round` |
| effects | none |
| op code | 0 `pi`, 1 `sqrt`, 2 `exp`, 3 `log`, 4 `sin`, 5 `cos`, 6 `tan`, 7 `asin`, 8 `acos`, 9 `atan`, 10 `atan2`, 11 `pow`; and in `__math_int`: 1 `floor`, 2 `ceil`, 3 `round` |
| domain | stated per function in [contracts/std-math.md](contracts/std-math.md) |

Validation: a non-NaN argument outside the domain is R0007 (research R5). A NaN argument returns NaN,
except where IEEE gives `1.0` for `pow`. The roundings raise R0007 for NaN, an infinity, or a result
outside `-2^63 .. 2^63-1`.

## Primitive

| Field | `__math` | `__math_int` |
|---|---|---|
| builtin id | 66 | 67 |
| signature code | `iff>f` | `if>i` |
| callable from | `std/` only | `std/` only |
| effect | none | none |

## Vendored library record (`third_party/core-math/VENDOR.md`)

| Field | Value |
|---|---|
| name | CORE-MATH |
| source | https://gitlab.inria.fr/core-math/core-math |
| pin | commit `1851882b0f85ed581af9185cfa020992cec9e7a1` (2026-11-17) |
| hashes | SHA-256 of each kept file |
| license | MIT (`LICENSE`) |
| kept | 15 files (research R1) |
| patches | `0001-no-inline-asm.patch`: the reason, and the 3 places |

## Vector row (`tests/math/vectors.txt`)

`FN X_HEX [Y_HEX] EXPECTED_HEX`: hex float bit patterns, one call per line, `#` comments for each
row's source (random, worst case, special).

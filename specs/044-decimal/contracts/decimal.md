# Contract: std.decimal

All functions are pure. `D` is `Decimal`. Every `D` argument and every `places` argument must have
places 0 to 18 (R0012 otherwise). "Stops" means R0001 unless stated.

```text
export record Decimal { units: int; places: int; }
export variant Rounding { half_even; half_up; down; up; floor; ceiling; }
```

| Function | Result | Notes |
|---|---|---|
| `decimal(units: int, places: int) -> D` | the decimal with those fields | |
| `from_int(n: int) -> D` | `n` at 0 places | |
| `parse_decimal(s: str) -> result[D]` | reads the text form | error `invalid decimal: TEXT` |
| `decimal_text(d: D) -> str` | the text form | `parse_decimal(decimal_text(d)) == ok(d)` |
| `grouped_text(d: D) -> str` | the text form with `,` every three integer digits | |
| `add(a: D, b: D) -> D` | exact; places `max` | stops if it does not fit |
| `sub(a: D, b: D) -> D` | exact; places `max` | stops if it does not fit |
| `mul(a: D, b: D) -> D` | exact; places `a.places + b.places` | stops if units or places (over 18) do not fit |
| `div(a: D, b: D, places: int, mode: Rounding) -> D` | quotient rounded to `places` | R0002 when `b` is zero; stops if it does not fit |
| `round_to(d: D, places: int, mode: Rounding) -> D` | `d` at exactly `places` | stops if adding places does not fit |
| `negate(d: D) -> D` | same places | stops for the most negative units |
| `absolute(d: D) -> D` | same places | stops for the most negative units |
| `compare(a: D, b: D) -> int` | -1, 0, 1 by value | never stops |
| `equal(a: D, b: D) -> bool` | `compare(a, b) == 0` | |
| `is_zero(d: D) -> bool`, `is_negative(d: D) -> bool` | by value | |
| `normalize(d: D) -> D` | fewest places with the same value | zero becomes `0` at 0 places |
| `to_int(d: D) -> result[int]` | the value when whole | error `not a whole number: TEXT` |
| `to_float(d: D) -> float` | nearest float | |
| `total(ds: list[D]) -> D` | exact sum; `0` at 0 places when empty | stops if it does not fit |
| `split_evenly(d: D, n: int) -> result[list[D]]` | `n` parts at `d.places` summing to `d` | error when `n < 1` or `n > 2147483647` |
| `allocate(d: D, ratios: list[int]) -> result[list[D]]` | one part per ratio, summing to `d` | error when empty, a ratio is negative, or the sum is 0 or above 2147483647 |

## Allocation rule

For amount `d` with `u = |d.units|` and ratios `r1..rn` summing to `R`: part `i` gets
`floor(u * ri / R)` units; the `u - sum` left-over units go one each to the first parts whose ratio is not 0; if
`d` is negative every part is negated. `split_evenly(d, n)` is `allocate` with `n` ratios of 1.
The ratios must sum to at most 2147483647 (an error otherwise), so that the share is computed without
overflow as `(u / R) * ri + (u % R) * ri / R`: no amount stops an allocation.

## Unchanged

No syntax, builtin, effect, diagnostic code or `format` placeholder. `==` on `Decimal` is structural.

# Data model: Decimal

## Decimal

| Field | Type | Rule |
|---|---|---|
| `units` | `int` | any 64-bit value |
| `places` | `int` | 0 to 18; outside it every function of the module refuses the value (R0012) |

Value: `units / 10^places`. Zero is `units == 0` at any places; there is no negative zero, infinity or
not-a-number. Several representations have the same value (`15/1`, `150/2`); `normalize` gives the one
with the fewest places.

## Rounding

`half_even`, `half_up`, `down`, `up`, `floor`, `ceiling`: variant cases without values.

| Mode | 0.125 → 2 | -0.125 → 2 | 0.121 → 2 | -0.121 → 2 |
|---|---|---|---|---|
| half_even | 0.12 | -0.12 | 0.12 | -0.12 |
| half_up | 0.13 | -0.13 | 0.12 | -0.12 |
| down | 0.12 | -0.12 | 0.12 | -0.12 |
| up | 0.13 | -0.13 | 0.13 | -0.13 |
| floor | 0.12 | -0.13 | 0.12 | -0.13 |
| ceiling | 0.13 | -0.12 | 0.13 | -0.12 |

## Text form

`-? digits [ . digits{places} ]`: at least one integer digit, exactly `places` fraction digits, no
point when `places` is 0, `-` only when the value is below zero. `parse_decimal` also accepts leading
zeros and `-0`.

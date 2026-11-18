# Contract: `std.math`

Import one item at a time: `import std.math.sqrt;`. Every function is pure (no `uses`). Float results
are correctly rounded (round to nearest, ties to even), so they are bit-identical on every platform.
"R0007" means the program stops with runtime error R0007 at the call. The message is given below, with
values in `str` text.

| Function | Domain (else R0007) | Edges (IEEE) | R0007 message |
|---|---|---|---|
| `pi() -> float` | — | `0x1.921fb54442d18p+1` | — |
| `sqrt(x: float) -> float` | `x >= 0.0` or `-0.0` | `sqrt(-0.0)` is `-0.0`; `sqrt(inf)` is `inf` | `` `sqrt` needs an argument of at least 0.0, got $1. `` |
| `exp(x: float) -> float` | all | overflow `inf`, underflow `0.0`; `exp(-inf)` is `0.0` | — |
| `log(x: float) -> float` | `x > 0.0` | `log(inf)` is `inf` | `` `log` needs an argument greater than 0.0, got $1. `` |
| `sin`, `cos`, `tan(x: float) -> float` | finite `x` | `sin(-0.0)` is `-0.0`; huge arguments reduced exactly | `` `sin` needs a finite argument, got $1. `` |
| `asin`, `acos(x: float) -> float` | `-1.0 <= x <= 1.0` | `asin(-0.0)` is `-0.0` | `` `asin` needs an argument from -1.0 to 1.0, got $1. `` |
| `atan(x: float) -> float` | all | `atan(±inf)` is `±pi()/2` | — |
| `atan2(y: float, x: float) -> float` | all | C Annex F quadrants and signed zeros; `atan2(0.0, 0.0)` is `0.0` | — |
| `pow(x: float, y: float) -> float` | not (`x < 0` and `y` finite non-integer); not (`x` is ±0 and `y < 0`) | `pow(x, 0.0)` and `pow(1.0, y)` are `1.0` even for NaN; overflow `±inf` | `` `pow` needs a non-negative base or an integer exponent, got $1 and $2. `` / `` `pow` of $1 needs an exponent of at least 0.0, got $2. `` |
| `floor`, `ceil`, `round(x: float) -> int` | finite, result within the `int` range | exact; `round` rounds halves away from zero | `` `floor` needs a finite value within the int range, got $1. `` |

Any NaN argument returns NaN, except for the two `pow` cases above. A NaN is never itself out of
domain, but `floor`, `ceil` and `round` of NaN are R0007, as `int(f)` is.

`vo doc std.math.NAME` prints the signature, the domain, the edges and one example. The test suite runs
every example.

The language reference gains one row in the "Standard library and packages" table:

| std.math | `sqrt`, `pow`, `exp`, `log`, trigonometry, `pi`; `floor`, `ceil`, `round` to `int` (correctly rounded) |

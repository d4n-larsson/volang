# Research: Decimal

## R1: is a primitive needed for speed?

**Decision**: no. The module is pure vo.

**Rationale**: a prototype (`Decimal` record, `add`, `mul`, `round_to` with `requires` clauses) runs
the invoice loop of SC-003 (100,000 lines: quantity × price, VAT rounded half-even to 2 places, running
total) in 91 ms on x86-64 against a bound of 1 s.

**Alternatives considered**: a `__decimal` builtin in C: a second implementation to keep right on
three architectures, for a factor nobody has asked for.

## R2: how does an exact operation stop when the result does not fit?

**Decision**: by doing the arithmetic with `int` operators. `a.units * b.units`, the scaling
`units * 10^k` and the sums already stop with R0001; a zero divisor reaches `/` and stops with R0002.
`mul` checks `a.places + b.places <= 18` first and stops with R0001 otherwise, by evaluating an
overflowing expression through one private function (`overflow()`), so the code is the same as for
units.

**Rationale**: the spec asks for exactly the behaviour `int` has; reusing it adds no code and no code
to document.

**Alternatives considered**: results instead of stopping: every `add` would need a `try`; rejected in
the spec ("like `int` arithmetic does").

## R3: division without 128 bits

**Decision**: long division in vo. With `k = places + b.places - a.places`:

- `k >= 0`: `q = a.units / b.units`, `r = a.units % b.units` (truncating, as `int` does), then `k`
  times: `q = q * 10 + (r * 10) / b`, `r = (r * 10) % b`. `q * 10` overflowing is the result not
  fitting: R0001, correctly. `r * 10` can overflow only when `|b.units| > 922337203685477580`; then
  the digit is found without multiplying: `r * 10` is formed by doubling (`2r`, `4r`, `8r`, `8r + 2r`),
  subtracting `b` whenever the running value reaches it and counting, each comparison written as
  `x >= b - x` so that nothing overflows.
- `k < 0`: first `a.units` is divided by `10^-k` (at most 18 digits, fits), keeping that remainder
  as a "sticky" part; then by `b.units`.
- Rounding looks at the last remainder: zero or not, and below, at or above half of the divisor
  (`r` against `b - r`, never `2 * r`), plus the sticky part for `k < 0`. One private function decides
  whether to step away from zero, from the mode, the sign, the parity of `q` and that three-way answer;
  `round_to` uses the same function.
- Signs: the work is done on values of one sign. Magnitudes are taken as non-positive numbers, because
  `-9223372036854775808` has no positive counterpart.

**Rationale**: typical money divisions produce two to six digits, so the loop is short; the slow path
exists only for 19-digit divisors and is exercised by the random extremes of SC-001.

**Alternatives considered**: a `__muldiv(a, b, c)` primitive on `__int128`: smaller vo code, but a
builtin, with ids, signature, dispatch and a C division routine to verify without libc on three
architectures. Kept in reserve; not needed for speed (R1).

## R4: `to_float`

**Decision**: `__text_float(decimal_text(d))`. `__text_float` (`src/stdprim.c`) reads `-? digits
[. digits]` with up to 19 significant digits to the nearest double, which is exactly a decimal's text.

**Rationale**: `float(units) / 10.0^places` rounds twice when `|units| > 2^53`. `parse_float` refuses
more than 17 digits, so the internal reader is used. No `std/*.vo` file calls it yet: T013 tries it first. If a module
cannot call it, `to_float` is `float(units) / float(10^places)` when `|units| < 2^53` (both exact, one
rounding), and the reader is wired for modules only for larger units.

## R5: places outside 0 to 18

**Decision**: every exported function has `requires` clauses on its `Decimal` and `places` arguments
through one private predicate; a violation is R0012 at the caller. The predicate's name
(`places_0_to_18`) is what the message shows, which names the limit (US5.3).

## R6: tests

**Decision**:
- `tests/decimals.py`, groups `arith` (add, sub, mul, compare, equal, normalize), `round` (round_to and
  div in six modes), `text` (parse_decimal, decimal_text, grouped_text, to_int, to_float), `alloc`
  (split_evenly, allocate), `speed`. Each group writes operand lines to a file, runs one vo driver that
  prints a result line per case, and compares with Python's `decimal` at a context of 60 digits. Cases
  whose Python result does not fit are kept out of the main run and run one per process, at most 200
  per group, each expected to stop with R0001.
- Golden cases: `run/decimal` (the acceptance scenarios, printed), `run/decimal_overflow` (R0001),
  `run/decimal_div_zero` (R0002), `run/decimal_bad_places` (R0012), `check/decimal_pure` (used in a
  contract and a property), and the module listing in `doc/all*`.
- The speed bound is skipped when `CI` is set or under emulation, as in `tests/bits.py`.

## R7: the expenses example

**Decision**: `Expense.cents` and `Total.cents` become `amount: Decimal`; reading does
`decimal(cents, 2)` after the existing `int_field`; the report prints `decimal_text`; `sum` uses
`total`; sorting uses `compare`. The JSON input and the printed report do not change (FR-017). The
example's own tests that named `money` are replaced by ones on the report text.

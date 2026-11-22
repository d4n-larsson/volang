# Feature Specification: Decimal (exact decimal numbers)

**Feature Branch**: `044-decimal`

**Created**: 2026-11-22

**Status**: Implemented ([results](results.md))

**Input**: User description: "`std.decimal`: exact decimal numbers as a library type, so that money and
other quantities with a fixed number of decimal places no longer have to be carried as integer cents
and formatted by hand (review docs/review-2026-11/language.md item 6, 'Exact numbers': 'Business
software is the natural home of a language built for reviewable, agent-written code, and it needs a
`decimal` with exact arithmetic, rounding modes and `format` support. A library type over `int` (value
and scale) is enough to start.'). Must keep: no new syntax, no decimal literal, no operator
overloading, nothing silently rounded or overflowed, determinism, three architectures, and the
reference under 15,000 tokens (about 14,848 now: one library row only)."

Background: vo has `int` and `float`. A float cannot hold 0.10 exactly, so money in vo is carried as an
integer number of cents, and every program writes its own `money(cents)` to print it, its own parsing
of `"12.50"`, and its own rounding when a percentage or a division produces half a cent. The expenses
example does exactly this. Each hand-written copy is a place for an off-by-one-cent error that no
reviewer will see.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Carry and print amounts exactly (Priority: P1)

A developer reads `"12.50"` from a file or a request, adds amounts, and prints the total as `"1,290.50"`,
without a float anywhere and without writing the conversion.

**Why this priority**: it replaces the hand-written code every money program has today.

**Independent Test**: the expenses example, rewritten on the module, prints the same report as before
with no `money(cents)` function of its own.

**Acceptance Scenarios**:

1. **Given** the text `"12.50"`, **When** parsed, **Then** the result is the decimal with 1250 units
   and 2 places, and its text is `"12.50"` again: the places written are the places kept.
2. **Given** text that is not a decimal (`""`, `"abc"`, `"1.2.3"`, `"1e3"`, `".5"`, `"5."`, `" 1"`,
   `"+1"`, `"1,000"`, more than 18 places, or too large), **When** parsed, **Then** the result is an
   error that quotes the text; the program does not stop.
3. **Given** two decimals with different places (1.5 and 2.25), **When** added or subtracted, **Then**
   the result is exact and has the larger number of places (3.75).
4. **Given** a decimal, **When** its text with thousands separators is asked for, **Then** the integer
   part is grouped in threes with `,` (`"-1,234,567.80"`).
5. **Given** a list of decimals, **When** summed, **Then** the result is their exact sum; the sum of an
   empty list is 0 with no places.

---

### User Story 2 - Multiply, divide and round with the rule written out (Priority: P1)

A developer computes 25% VAT on 19.99, a unit price from a total, or interest, and states how the
result is rounded: the rounding rule is in the source, where a reviewer reads it.

**Why this priority**: rounding is where money code goes wrong, and where the rule is a business
decision that must be visible.

**Independent Test**: for thousands of random operands, `mul`, `div` and `round_to` in every rounding mode
give what Python's `decimal` module gives.

**Acceptance Scenarios**:

1. **Given** two decimals, **When** multiplied, **Then** the result is exact and its places are the sum
   of theirs (19.99 × 0.25 is 4.9975).
2. **Given** a decimal, a number of places and a rounding mode, **When** rounded, **Then** the result
   has exactly that many places and is rounded by that mode: 4.9975 to 2 places is 5.00 in every mode
   but down and floor (4.99); 0.125 to 2 places is 0.12 half-even and 0.13 half-up; -0.125 is -0.12
   half-even, -0.13 half-up, -0.12 down, -0.13 up, -0.13 floor, -0.12 ceiling.
3. **Given** more places asked for than the decimal has, **When** rounded, **Then** zeros are added and
   the value is unchanged (1.5 to 3 places is 1.500).
4. **Given** two decimals, a number of places and a mode, **When** divided, **Then** the result is the
   exact quotient rounded to those places by that mode (10 / 3 to 2 places half-even is 3.33; 2 / 3 is
   0.67).
5. **Given** a divisor of zero, **When** divided, **Then** the program stops with the division-by-zero
   runtime error (R0002), as `int` division does.
6. **Given** a call to round or divide, **When** written without a mode, **Then** it does not check:
   there is no default rounding.

---

### User Story 3 - Compare, convert and use as data (Priority: P2)

A developer sorts amounts, asks whether a balance is negative, groups by amount, stores amounts in JSON
or a database and reads them back.

**Why this priority**: a number type that cannot be compared, stored or keyed is half a type.

**Independent Test**: a list of random decimals with mixed places sorts into the order Python gives;
each survives a round trip through its text.

**Acceptance Scenarios**:

1. **Given** two decimals, **When** compared, **Then** the answer is by value whatever their places:
   1.5 and 1.50 compare as equal, 1.49 is below both.
2. **Given** 1.5 and 1.50, **When** compared with `==`, **Then** they differ, because `==` compares
   the places too (structural equality, as for every record); the module's equality function says they
   are equal, and a function that removes trailing zeros gives one form per value, for map keys.
3. **Given** a decimal with a whole value (12.00), **When** converted to `int`, **Then** the result is
   12; with a fraction (12.50), the result is an error: no silent truncation.
4. **Given** a decimal, **When** converted to `float`, **Then** the result is the nearest float; the
   function's name says it is approximate.
5. **Given** an `int`, **When** converted, **Then** the result is that value with no places.
6. **Given** a decimal, **When** written to JSON or a database column, **Then** the documented way is
   its text, and reading it back with `parse_decimal` gives the same units and places.

---

### User Story 4 - Split an amount without losing a unit (Priority: P2)

A developer splits a bill of 100.00 among three people, or a payment across invoices by ratio, and the
parts add up to the amount exactly.

**Why this priority**: it is the classic money bug (33.33 × 3 = 99.99) and needs care that a library
should take once.

**Independent Test**: for thousands of random amounts and ratio lists, the parts sum to the amount, and
each part is within one unit of its exact share.

**Acceptance Scenarios**:

1. **Given** 100.00 and 3 parts, **When** split, **Then** the parts are 33.34, 33.33, 33.33: the
   left-over units go to the first parts, one each.
2. **Given** 100.00 and ratios 1, 1, 2, **When** allocated, **Then** the parts are 25.00, 25.00, 50.00;
   given 0.05 and ratios 3, 7, the parts are 0.02 and 0.03 (floor of each share, then the left-over
   units to the first parts, one each).
3. **Given** a negative amount, **When** split, **Then** the parts are the negatives of the split of
   the positive amount.
4. **Given** zero parts, an empty ratio list, a negative ratio or ratios that sum to zero, **When**
   split, **Then** the result is an error.

---

### User Story 5 - Nothing overflows silently (Priority: P1)

A developer multiplies large amounts with many places. When the exact result does not fit, the program
stops, as it does for `int`: a wrong amount is never produced.

**Why this priority**: exactness is the whole promise of the type.

**Independent Test**: random operands whose exact result needs more than 64 bits of units or more than
18 places stop with R0001 in every operation; operands whose result fits never do.

**Acceptance Scenarios**:

1. **Given** operands whose exact sum, difference or product does not fit in a signed 64-bit count of
   units, or whose product would have more than 18 places, **When** computed, **Then** the program
   stops with the integer overflow runtime error (R0001).
2. **Given** a division or rounding whose result fits but whose intermediate value does not fit in 64
   bits (1.000000000000000000 / 3 to 18 places), **When** computed, **Then** the result is right: only
   a result that does not fit stops the program.
3. **Given** a decimal built directly with places outside 0 to 18, **When** passed to any function of
   the module, **Then** the program stops with a contract failure that names the limit.

---

### User Story 6 - Documented, within budget (Priority: P2)

A developer finds the module in the standard-library guide with a worked invoice example; `vo doc
std.decimal` lists every function with examples that are run; the reference gains one library row.

**Acceptance Scenarios**:

1. **Given** the reference, **When** measured, **Then** it is under 15,000 tokens.
2. **Given** the module's doc comments, **When** the documentation checks run, **Then** every example
   is run and passes.
3. **Given** the guide, **When** read, **Then** it shows parsing, VAT with a stated rounding, a total,
   a split, and storing a decimal in JSON as text.

### Edge Cases

- Zero has no sign: `parse_decimal("-0.00")` is 0 units at 2 places and prints `"0.00"`.
- The most negative count of units (-9223372036854775808) is a valid decimal; negating it or taking its
  absolute value stops with R0001, as for `int`.
- Text of a value between -1 and 0 keeps its sign and its leading zero: `"-0.05"`.
- Leading zeros in text are accepted and dropped (`"007.50"` is 7.50); trailing zeros are places and
  are kept.
- Rounding to 0 places gives a decimal with no places, not an `int`.
- A `places` argument below 0 or above 18 is a contract failure.
- Thousands separators are not applied to the fraction, and not to values with fewer than four integer
  digits.
- Comparing values whose common scale would overflow (9000000000000000000 with 0.000000000000000001)
  still answers correctly.
- Splitting an amount into more parts than it has units gives parts of one unit and then zeros.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The standard library MUST provide a module `std.decimal` with a record `Decimal` of two
  `int` fields, `units` and `places` (0 to 18): the value is units / 10^places.
- **FR-002**: `parse_decimal(text)` MUST return a result: the decimal for an optional `-`, one or more digits
  and optionally `.` with one to 18 digits; an error quoting the text for anything else, including a
  value that does not fit. It MUST NOT go through a float.
- **FR-003**: `from_int(n)` MUST give n with no places; `decimal(units, places)` MUST give the decimal
  with those fields.
- **FR-004**: `decimal_text(d)` MUST give the canonical text that `parse_decimal` reads back to the same units and
  places; `grouped_text(d)` MUST give the same with `,` between groups of three integer digits.
- **FR-005**: `add`, `sub` and `mul` MUST be exact: `add` and `sub` give the larger number of places,
  `mul` the sum of the places. `negate` and `absolute` keep the places.
- **FR-006**: The module MUST provide a variant `Rounding` with the cases `half_even`, `half_up`,
  `down`, `up`, `floor` and `ceiling`, with the meanings of Python's `ROUND_HALF_EVEN`, `ROUND_HALF_UP`,
  `ROUND_DOWN`, `ROUND_UP`, `ROUND_FLOOR` and `ROUND_CEILING`.
- **FR-007**: `round_to(d, places, mode)` MUST give the decimal with exactly `places` places, rounded by
  `mode` (exact when places are added). `div(a, b, places, mode)` MUST give the exact quotient rounded
  the same way. Neither has a default mode.
- **FR-008**: `compare(a, b)` MUST give -1, 0 or 1 by value; `equal(a, b)` MUST be true when the
  values are equal whatever the places; `is_zero`, `is_negative` MUST answer by value. `normalize(d)`
  MUST remove trailing zeros of the fraction, giving one representation per value.
- **FR-009**: `to_int(d)` MUST return a result: the integer when the value is whole, an error
  otherwise. `to_float(d)` MUST return the nearest `float`.
- **FR-010**: `total(list)` MUST give the exact sum (0 with no places for an empty list).
- **FR-011**: `split_evenly(d, n)` and `allocate(d, ratios)` MUST return a result holding a list of decimals
  with the places of `d` that sum exactly to `d`: each part is the floor of its exact share of the
  absolute amount in units, the left-over units go one each to the first parts, and a negative amount
  gives the negated parts. An error for n below 1, an empty ratio list, a negative ratio, or ratios
  summing to 0 or to more than 2147483647 (so that no amount can overflow an allocation).
- **FR-012**: An operation whose exact result does not fit in a signed 64-bit count of units, or would
  need more than 18 places, MUST stop the program with R0001; division by a zero value MUST stop with
  R0002. An intermediate value that does not fit MUST NOT cause either when the result fits.
- **FR-013**: Every function MUST refuse (contract failure) a decimal or a `places` argument whose
  places are outside 0 to 18.
- **FR-014**: Every function MUST be pure: no effect, usable in contracts and properties.
- **FR-015**: Results MUST be the same on x86-64, ARM64 and RISC-V.
- **FR-016**: The module's functions MUST have doc comments with run examples; the standard-library
  guide MUST gain a section with an invoice example and the JSON and database advice; the reference
  MUST gain one library row and stay under 15,000 tokens.
- **FR-017**: The expenses example MUST use `std.decimal` for its amounts and lose its own `money`
  function; its input format (integer cents in JSON) and its output MUST stay as they are. The guide pages that
  teach holes and file structure with their own `money` function are not changed.
- **FR-018**: No syntax, operator, literal form, diagnostic code or `format` placeholder is added.
  Existing golden cases MUST be unchanged, except the listing of modules.

### Key Entities

- **Decimal**: a count of units and a number of places (0 to 18); the value is units / 10^places. Two
  decimals with the same value and different places are equal by `equal` and different by `==`.
- **Rounding**: one of six named rules for a result that does not fit the places asked for.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For each of `add`, `sub`, `mul`, `div`, `round_to`, `compare`, `parse_decimal` and `decimal_text`, 10,000
  random cases (mixed places, extreme values, every rounding mode) give the result of Python's
  `decimal` module, or stop with R0001 exactly when Python's exact result does not fit.
- **SC-002**: For 10,000 random amounts with part counts and ratio lists, the parts of `split_evenly` and
  `allocate` sum to the amount and each is within one unit of its exact share.
- **SC-003**: An invoice run of 100,000 lines (quantity × price, VAT rounded to 2 places, running
  total) finishes in under 1 second on x86-64.
- **SC-004**: The expenses example prints the same output as before for its sample data, and its tests
  pass, with at least 5 fewer lines of money handling of its own.
- **SC-005**: The reference is under 15,000 tokens; every doc example of the module passes; the full
  test suite passes on x86-64, and the module's cases pass on ARM64 and RISC-V.

## Assumptions

- **The places are kept, and `==` sees them.** 1.50 stays 1.50 (it prints with two places without
  being told), as in Python and Java. The price is that `==` and map keys, which compare records
  structurally, tell 1.5 from 1.50; `equal`, `compare` and `normalize` are the answer. In money code
  all amounts of a currency share their places, so `==` agrees with `equal` there. The alternative
  (always remove trailing zeros, state the places at every print) makes `==` right and every print
  longer; it was not chosen.
- **The record is open**: vo has no private fields, so `new Decimal { units: 1250, places: 2 }` is
  legal and equivalent to `decimal(1250, 2)`. Invalid places are caught at use (FR-013).
- **64 bits of units**: about 92 quadrillion at 2 places. Arbitrary precision is out of scope.
- **Names avoid what exists**: the builtins `split` and `abs`, `round` in `std.math`, `to_text` in `std.bytes`, and the
  expenses example's own `sum`; hence `split_evenly`, `absolute`, `round_to`, `decimal_text` and `total`.
- **Errors are `str`**, as `parse_int` and `parse_float` give.
- **No `format` placeholder**: `format("{}", decimal_text(d))` and `align_right(grouped_text(d), 12)` are the
  way; a `{}` spec for decimals needs the `format` builtin to know a library type.
- **The thousands separator is `,` and the decimal point `.`**, always. Locales are out of scope.
- **Speed**: the module is written in vo, with no primitive: the plan measured a prototype of SC-003
  at 91 ms.
- **JSON and SQLite are not changed**: a decimal is stored as its text (or as units in an integer
  column when the places are fixed); the guide says so.
- Out of scope: decimal literals, `+` on decimals, currencies and exchange rates, percent helpers,
  square roots and other functions with inexact results, sized integers.

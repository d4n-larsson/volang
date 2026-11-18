# Feature Specification: Math functions

**Feature Branch**: `020-math`

**Created**: 2026-11-18

**Status**: Draft

**Input**: User description: "Math builtins for vo: sqrt, pow, floor, ceil, round, trigonometry (sin, cos,
tan, asin, acos, atan, atan2), log, exp. Pure (no effects). Results must be byte-identical on x86-64,
ARM64 and RISC-V 64 (constitution Principle III): sqrt/floor/ceil/round are exact or correctly rounded in
hardware; the transcendental functions come from the vendored CORE-MATH library (correctly rounded,
MIT), under the constitution 1.4.0 vendoring rules in third_party/. Defined behavior for domain errors
(e.g. sqrt of negative, log of zero), NaN and infinity, and float→int conversions in floor/ceil/round
(overflow is a runtime error). Must fit the 15,000-token language reference budget."

Background: the first gap of the roadmap's "Planned after 017" table. vo has `float` (64-bit IEEE),
`+ - * /`, `abs`, `min`, `max` and `int(f)` (truncates toward zero), and nothing else numeric. A program
that needs a distance, a square root, compound interest, an angle or a rounded price has to write its
own approximation. Agents do write one, and it is usually subtly wrong. Ordinary math libraries do not
give the same last bit on every platform, so vo cannot simply call one: Principle III requires the same
output everywhere.

What this spec must keep:
- **Byte-identical results** on x86-64, ARM64 and RISC-V 64, for every input.
- **Defined behavior**: every input, including NaN, infinities and out-of-domain values, has a stated
  result or a runtime error.
- **The reference budget** (15,000 tokens).
- **No effects**: math is pure; no function in this spec needs a `uses` declaration.

## Clarifications

### Session 2026-11-18

- Q: Builtins or a standard-library module? → A: A module, `std.math`, imported one item at a time
  (`import std.math.sqrt;`) and documented by `vo doc`. It costs nearly nothing in the reference and
  takes no name from existing programs that define their own `log` or `round`.
- Q: What do out-of-domain arguments do? → A: Runtime error R0007 at the call, naming the function and
  the value, as `int(f)` does for NaN. A NaN that silently spreads through later results is what agents
  misread as a correct answer. Overflow and underflow stay IEEE (`inf`, `0.0`), like float arithmetic.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–019: AI coding agents that write vo programs from the reference
and `vo doc`, and the people who review and run those programs.

### User Story 1 - Compute with roots, powers, logarithms and angles (Priority: P1)

An agent writes a program that computes the distance between two points, compound interest, a standard
deviation, or the bearing between two coordinates. It imports `sqrt`, `pow`, `log`, `exp` and the
trigonometric functions from `std.math`, calls them with exactly the signatures `vo doc` shows, and the
program prints the expected values.

**Why this priority**: this is the gap. Without these functions, every numeric program carries a
hand-written approximation.

**Independent Test**: golden cases that print each function's result for known inputs and compare it
with the correctly rounded value.

**Acceptance Scenarios**:

1. **Given** a program printing `sqrt(2.0)`, **When** it runs, **Then** it prints `1.4142135623731`
   (15 significant digits, as `str` prints every float).
2. **Given** `pow(1.05, 10.0)`, `exp(1.0)`, `log(10.0)`, `sin(pi() / 6.0)`, `atan2(1.0, 1.0)`, **When**
   the program runs, **Then** each value is the correctly rounded double of the true result.
3. **Given** a call with an `int` argument, such as `sqrt(2)`, **When** the program is checked, **Then**
   it is the existing type error for a wrong argument type, with the fix `float(2)` where it applies;
   there are no implicit conversions.

---

### User Story 2 - Round floats to integers (Priority: P1)

An agent converts a computed price, a percentage or a grid position to an `int` with `floor`, `ceil` or
`round`, each returning `int`.

**Why this priority**: `int(f)` truncates toward zero, which is wrong for negative values and for
rounding. Agents get this wrong more often than any transcendental function.

**Independent Test**: golden cases over positive, negative, half-way and out-of-range values.

**Acceptance Scenarios**:

1. **Given** `floor(-2.5)`, `ceil(-2.5)`, `round(-2.5)`, `round(2.5)`, and `round` of 0.49999999999999994
   (built by arithmetic, since literals have at most 15 significant digits),
   **When** the program runs, **Then** it prints `-3 -2 -3 3 0` (halves round away from zero).
2. **Given** `round(1.0e300)`-sized values (built from multiplication, since literals have no exponent),
   NaN or an infinity, **When** the program runs, **Then** it stops with runtime error R0007, as
   `int(f)` does, naming the function and the value.

---

### User Story 3 - Same answer on every machine (Priority: P1)

A reviewer runs an agent's program and its golden test on x86-64, an ARM64 laptop and a RISC-V board.
The output is byte-identical on all three.

**Why this priority**: Principle III. A result that differs in the last bit between platforms makes
golden tests and replay unreliable.

**Independent Test**: the determinism check and a cross-platform case that prints each function over a
large fixed set of inputs (including the hard-to-round cases the library's authors publish), run on all
three platforms and compared byte for byte, and compared with independently computed correctly rounded
values.

**Acceptance Scenarios**:

1. **Given** the cross-platform case, **When** it runs on the three platforms, **Then** the outputs are
   byte-identical and match the reference values bit for bit.

---

### User Story 4 - Find the functions without reading the whole reference (Priority: P2)

An agent asks `vo doc` for the math functions and gets each signature, its domain, what it returns for
NaN, infinity and out-of-domain input, and one example.

**Why this priority**: agents learn vo only from in-context documentation (Principle I), and the domain
rules are where they would otherwise guess.

**Independent Test**: a `vo doc` golden case; every example it prints is executed by the test suite.

**Acceptance Scenarios**:

1. **Given** `vo doc` for one function, **When** it runs, **Then** it prints the exact signature, the
   behavior at the edges and an example whose output the test suite checks.

### Edge Cases

- **NaN in, NaN out**: a NaN argument is not out of domain; every float-returning function returns NaN
  for it, except where IEEE 754 defines otherwise (`pow(x, 0.0)` and `pow(1.0, y)` are `1.0` for any `x`,
  `y`, NaN included). A NaN reaches a math function only from arithmetic such as `0.0 / 0.0`.
- **Out of domain is R0007**: `sqrt` of a negative number (including `-inf`; `sqrt(-0.0)` is `-0.0`),
  `log` of zero or a negative number, `asin`/`acos` outside -1..1, `sin`/`cos`/`tan` of an infinity,
  `pow` of a negative base with a non-integer exponent, and `pow(0.0, y)` with negative `y`. The message
  names the function and the argument.
- **Overflow and underflow**: `exp(1000.0)` is `inf`, `exp(-1000.0)` is `0.0`; `pow` overflows to
  `inf` with the right sign. These are IEEE results, not errors (consistent with float arithmetic).
- **Signed zero**: `str` prints `-0.0` and `0.0` the same way it does today; functions return the
  IEEE-correct sign of zero, so a later division shows it (`1.0 / sin(-0.0)` is `-inf`).
- **Huge trigonometric arguments**: `sin(1.0e22)`-sized arguments are reduced exactly, not
  approximately; the result is correctly rounded like any other.
- **`atan2`**: all signed-zero and infinity quadrants follow IEEE 754 / C `atan2`.
- **`round` half-way cases**: halves round away from zero; 0.49999999999999994 rounds to `0` (no
  `floor(x + 0.5)` error).
- **`floor`/`ceil`/`round` range**: a result outside the `int` range, NaN or an infinity is R0007, the
  same rule as `int(f)`.
- **Names already in use**: a program that already defines a function named `log`, `round`, `exp` or
  similar keeps working unchanged; the `std.math` names exist only where imported. Importing
  `std.math.log` into a file that also defines `log` is the existing name-clash error.

## Requirements *(mandatory)*

### Functional Requirements

**Functions**

- **FR-001**: The standard library MUST have a module `std.math` with `sqrt`, `pow`, `exp`, `log`
  (natural logarithm), `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, each taking and returning
  `float`, imported one item at a time like every other module.
- **FR-002**: `std.math` MUST provide `pi() -> float`, the double nearest to π, since vo has no
  constants.
- **FR-003**: `std.math` MUST provide `floor`, `ceil` and `round`, each `(x: float) -> int`; `round`
  rounds halves away from zero.
- **FR-004**: No function in `std.math` may require an effect, and no new builtin or keyword is added.

**Accuracy and determinism**

- **FR-005**: Every float-returning function MUST return the correctly rounded result (round to
  nearest, ties to even) of the exact mathematical value, for every input. Correct rounding makes the
  result unique, so it is the same on every platform by construction.
- **FR-006**: Results MUST be bit-identical on x86-64, ARM64 and RISC-V 64, and MUST NOT depend on
  compiler contraction, extended precision or the floating-point environment.
- **FR-007**: `floor`, `ceil` and `round` MUST be exact.

**Edges**

- **FR-008**: NaN, infinities, signed zeros, overflow and underflow MUST follow IEEE 754 and the C
  standard's Annex F for these functions, except where FR-009 and FR-010 say otherwise.
- **FR-009**: Every out-of-domain argument listed under Edge Cases (domain and pole errors in Annex F
  terms) MUST stop with R0007 at the call, with a message naming the function and the argument; `vo doc`
  MUST state each function's domain.
- **FR-010**: `floor`, `ceil` and `round` of NaN, an infinity, or a value whose result is outside the
  `int` range MUST stop with R0007 and a message naming the function and the value.

**Source of the implementation**

- **FR-011**: The transcendental functions MUST come from an established, correctly rounded library
  vendored under the constitution's rules for third-party code (CORE-MATH, MIT licensed), not from new
  hand-written approximations.
- **FR-012**: The vendored code MUST build freestanding, with no assembly and with the same compiler
  constraints as the rest of the binary, and MUST pass the instrumented build.

**Documentation and tests**

- **FR-013**: Each function MUST have a `vo doc` entry with its exact signature, its edge behavior and
  an example whose output the test suite verifies.
- **FR-014**: The language reference MUST name `std.math` in its standard-library section (one line),
  and the change MUST state the reference's size increase; it MUST stay within 15,000 tokens.
- **FR-015**: Golden cases MUST cover every function at ordinary values, at each edge in this spec, and
  at every R0007 path; a cross-platform case MUST compare a large fixed set of inputs bit for bit against
  independently computed correctly rounded values.

### Key Entities

- **Math function**: name, signature (`float` arguments; `float` or `int` result), domain, edge
  behavior, `vo doc` example.
- **Vendored library**: name, pinned upstream version, checksum, license, the files kept, patches with
  their reasons (`third_party/<name>/VENDOR.md`).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a fixed set of at least 10,000 inputs per function, including the published
  hard-to-round cases, 100% of results equal the correctly rounded value, bit for bit.
- **SC-002**: The same set gives byte-identical output on all three supported platforms.
- **SC-003**: Every edge case listed above has a golden case, and every R0007 path from FR-010 is
  triggered by at least one.
- **SC-004**: The language reference stays within 15,000 tokens, and the increase is stated.
- **SC-005**: A program using all of the functions checks and runs with no warnings and no effects
  declared.
- **SC-006**: A math call is fast enough that a numeric loop of one million calls finishes within the
  existing 1,000 ms performance budget on the native x86-64 build.

## Assumptions

- `float` literals have no exponent and at most 15 significant digits, so tests build huge, tiny and
  long values by arithmetic or `float(int)`.
- `round` rounds halves away from zero, as C `round` does and as most people expect from a price;
  half-to-even is not offered (one way to do each thing, Principle I).
- `log` is the natural logarithm. `log2`, `log10`, hyperbolic functions, `cbrt`, `hypot` and
  `fmod` are out of scope until a program needs them; each would come from the same library.
- `pow` takes two floats only; there is no integer power.
- Integer `sqrt`/`floor` variants and a `trunc` returning `float` are out of scope; `int(f)` already
  truncates.
- `sqrt`, `floor`, `ceil` and `round` need no library: square root is correctly rounded by the hardware
  on all three platforms, and the other three are exact.
- **CORE-MATH has no tagged releases** that we could find; it is published as a repository. The
  constitution requires pinning "a released version whose archive checksum" is recorded. The plan must
  either pin a specific commit's archive (and record its checksum) under a PATCH clarification of the
  constitution, or justify an alternative in Complexity Tracking.
- CORE-MATH may read or set the C floating-point environment and `errno`; the plan shims these without
  modifying vendored files, or records a patch with its reason.

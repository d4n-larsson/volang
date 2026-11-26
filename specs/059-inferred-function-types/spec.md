# Feature Specification: Anonymous Functions Take Their Types From Context

**Feature Branch**: `059-inferred-function-types`

**Created**: 2026-11-25

**Status**: Draft

**Input**: User description: anonymous functions repeat types the checker already knows from the
expected function type (`sort_by(people, fn(p: Person) -> int { return p.age; })`). Take parameter and
return types from the context, and make short functions short.

Background: an anonymous function writes every parameter type and its return type, although it almost
always sits where a function type is expected: an argument of `map`, `filter`, `sort_by`, `fold`,
`concurrent_map`, a std.web route, a returned closure. The reference's own example spends a line per
lambda on `fn(p: Person) -> str {` and three lines on a body that returns one field. Across `examples/`
and `std/` there are 104 anonymous functions with written parameter types. They are the second most
repeated noise in vo code after imports (spec 058), and the README's first example shows them.

What this spec must keep:
- **Principle I, one form:** an anonymous function has one way to be written. Types are not "optional
  where the checker can work them out"; they are never written on an anonymous function, and the place
  it goes gives them, as `[]`, `ok(1)` and `Shape.empty` take theirs (spec 056, FR-018).
- **Effects:** anonymous functions already take their effects from their body (E0507); unchanged.
- **Stability:** every existing anonymous function migrates by a mechanical fix.
- **Principle V:** the reference stays within 15,000 tokens; its examples get shorter.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Lambdas without repeated types (Priority: P1)

An agent sorts, maps and folds:

```text
sort_by(people, fn(p) => p.age);
let names = map(people, fn(p) => p.name);
let total = fold(xs, 0, fn(acc, x) => acc + x);
```

`p` is a `Person` because `people` is a `list[Person]`; `sort_by`'s key type is `int` from the body.

**Why this priority**: it is the most frequent construct in data-handling code and the one that reads
worst today.

**Independent Test**: every anonymous function in `examples/`, `std/` and the reference, migrated, checks
and runs with byte-identical output.

**Acceptance Scenarios**:

1. **Given** `map(people, fn(p) => p.name)` with `people: list[Person]`, **When** checked,
   **Then** `p` is `Person`, the result is `list[str]`, and hover on `p` shows `Person`.
2. **Given** `fold(xs, 0, fn(acc, x) => acc + x)` with `xs: list[int]`, **When** checked,
   **Then** `acc` and `x` are `int`.
3. **Given** `fn make_adder(n: int) -> fn(int) -> int { return fn(x) => x + n; }`, **When**
   checked, **Then** `x` is `int` from the declared return type.
4. **Given** a lambda whose body gives a `str` where `fn(Person) -> int` is expected, **When**
   checked, **Then** E0301 points at that expression and names `int` as expected.

### User Story 2 - Existing code migrates by fixes alone (Priority: P1)

A program written for 0.3 has `fn(p: Person) -> int { ... }` everywhere. `vo check --json` reports each
written type on an anonymous function with a fix that removes it. Where the function has no expected
type (`let f = fn(x: int) -> int { ... };`), the fix moves the types onto the binding:
`let f: fn(int) -> int = fn(x) { ... };`. The same run rewrites single-return bodies as `=> expr`
(FR-009).

**Why this priority**: breaking change; every program with a lambda is affected.

**Independent Test**: every example, std module, the `postgres` package and the migration corpora migrate
by applying fixes alone, then check clean with identical test results and output.

**Acceptance Scenarios**:

1. **Given** `map(xs, fn(x: int) -> int { return x * 2; })`, **When** fixes are applied, **Then** it
   reads `map(xs, fn(x) => x * 2)` and checks.
2. **Given** `let f = fn(x: int) -> int { return x * 2; };`, **When** fixes are applied, **Then** it reads
   `let f: fn(int) -> int = fn(x) => x * 2;` and checks.
3. **Given** a written type that differs from the expected one (today an E0301), **When** checked,
   **Then** the same mismatch is reported (with its existing fix where one applies) before the written
   type is removed, so migration never changes what a program means.

### User Story 3 - Short functions on one line (Priority: P2)

A lambda that only returns a value is written `fn(p) => p.age`, and `vo fmt` keeps it on one line;
today `vo fmt` spreads `fn(p) { return p.age; }` over three. To keep one form, `=> expr` is the only way
to write a body that is a single `return expr;`: the block form with a single `return` is a diagnostic
whose fix rewrites it, and `vo fmt` applies that fix. (Decided with the user on 2026-11-25.)

**Why this priority**: the line count of data-handling code drops by about two thirds per lambda; it is
P2 because it adds a token and a rule for which body shape applies, and stories 1 and 2 stand without it.

**Independent Test**: `sort_by(people, fn(p) => p.age);` checks, runs and stays on one line after
`vo fmt`; `fn(p) { return p.age; }` is a diagnostic whose fix gives the former.

### Edge Cases

- **No expected type**: `let f = fn(x) { ... };`, an anonymous function as a statement, or passed where
  the parameter type is a bare type variable that nothing else fixes. A new error names what is missing
  and says to give the binding a function type; no fix (the types are unknown).
- **Generic calls**: type arguments are first fixed from the arguments that are not anonymous functions
  (function values such as a named `setup` included; amended by plan research R3), then anonymous
  functions are checked left to right, each fixing the type variables in its return type for those after
  it (`map(xs, f)` fixes `T` from `xs`, `U` from `f`'s body). A type variable that is still open when a
  lambda's parameter needs it is the "no expected type" error, naming the type parameter.
- **Return type from the body**: when the expected return type is an open type variable (`U` of `map`,
  `C` of `sort_by`), it is the type of the `=>` expression or of the body's `return` expressions; returns of different types are
  E0301 at the second one; a body with no `return` value gives `void`.
- **Expected effects**: unchanged: the body's effects must fit the expected function type's effects
  (E0502).
- **Effect parameters** of user generics (`[E: effects]`) are fixed as today from the argument's effects.
- **Wrong arity**: a lambda with two parameters where `fn(T) -> U` is expected is E0301 with both
  arities named; no fix.
- **Parameter name `_`**: allowed for a parameter the body does not use, as today.
- **Hover, `vo query type-at`, traces and runtime error reports** show the inferred parameter types,
  since a reader no longer sees them written.
- **Captured variables, closures, contracts**: unchanged; anonymous functions still cannot have
  contracts or type parameters.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: An anonymous function is written `fn(a, b) { ... }` or `fn(a, b) => expr` (FR-009):
  parameter names only, no parameter
  types and no return type. Its parameter types, return type and effects come from the expected function
  type at its position: an argument, a typed `let` or `var`, a `return`, a record field, a list or map
  element, a `set` target.
- **FR-002**: When the expected function type contains open type variables, they are fixed as stated
  under Edge Cases (arguments that are not anonymous functions first, then anonymous functions left to right,
  the return type from the body). The order is part of the reference.
- **FR-003**: An anonymous function with no expected type, or whose parameter type would be an open
  type variable, is a new error with a stable code and a catalog entry saying to write the type on the
  binding.
- **FR-004**: A written parameter or return type on an anonymous function is a new error with a fix.
  The fix removes the types when an expected type exists; otherwise it moves them, as a function type,
  onto the `let` or `var` that binds the function. Where neither applies, there is no fix.
- **FR-005**: A written type that disagrees with the expected type keeps reporting today's mismatch
  first; the removal fix is offered only when the written types equal the expected ones, so applying
  fixes never changes a program's meaning.
- **FR-006**: Named functions are unchanged: they always write every parameter type and their return
  type.
- **FR-007**: Hover, `vo query type-at`, `vo doc` of a program's items, `--trace` records and runtime
  error frames show each anonymous function's parameter types.
- **FR-008**: The std library, the `postgres` package, every example, the reference, the site and the
  catalog are migrated; the README's first example is rewritten to show the result.
- **FR-009**: `fn(p) => expr` is the only form of an anonymous function
  whose body is a single `return expr;`; the block form with a single `return` is a diagnostic with a
  fix, and `vo fmt` applies it. A block body is kept for everything else.
- **FR-010**: The reference states the rule in one paragraph and shows it in its existing examples; its
  size before and after is stated and stays within 15,000 tokens. The changelog lists the change with its
  migration.

### Key Entities

- **Expected function type**: the function type the checker requires at an anonymous function's position;
  its source is one of the positions in FR-001.
- **Open type variable**: a type parameter of a generic call not yet fixed when a lambda's parameter
  types are needed.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All 104 anonymous functions with written types in `examples/` and `std/` migrate by fixes
  alone; afterwards none writes a type, and fewer than 5% needed a typed binding (the "no expected type"
  case).
- **SC-002**: Every migrated program gives byte-identical output and test results on x86-64, ARM64 and
  RISC-V.
- **SC-003**: Characters spent on anonymous function headers in `examples/` and `std/` drop by at least
  50%, and lines spent on single-return lambdas drop by at least 60%.
- **SC-004**: Every new diagnostic has a golden case and a catalog entry with a broken and a fixed
  example; each fix's result checks clean.
- **SC-005**: The reference does not grow (its lambda examples get shorter than the new rule's paragraph
  costs) and stays at or under 15,000 tokens.
- **SC-006**: Check time of the examples and of a generated program with 8,000 items stays within 5% of
  today's.

## Assumptions

- This lands as part of 0.4.0 with spec 058, or alone as the next minor version; it does not depend on
  058.
- Type inference stays local: there is no inference across statements or from later uses of a binding;
  only the expected type at the position and the generic call it is an argument of.
- Function types themselves (`fn(int) -> int uses console`) are unchanged and still written in full
  where they appear as types.
- Reviewers who need a lambda's types use hover or `vo query type-at`; the diff shows parameter names.

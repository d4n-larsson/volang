# Feature Specification: Typed Errors

**Feature Branch**: `032-typed-errors`

**Created**: 2026-11-20

**Status**: Draft

**Input**: User description: "Typed errors: `result[T, E]` where E is the error type, so callers can tell
failures apart with an exhaustive `match` instead of comparing message text. Today every `result[T]`
carries a `str` error; `std.web` maps errors to HTTP statuses by parsing message prefixes
(`err("not_found: detail")`), and match exhaustiveness stops at the error. `result[T]` stays and means
`result[T, str]`, so no existing program changes. `try` passes an error on when the enclosing function's
error type is the same; converting between error types needs one explicit form (candidate: a
`map_err(r, f)` builtin). `main` and tests keep `result[void]` (str errors); a typed error that would
reach them must be converted. Errors reported by the toolchain (test failures, runtime reports, `vo run`
when main fails) print a typed error in vo syntax. Also consider `option[T]` as a library variant
(std.option) for absence, without changing `pop`, `env`, `get_or` in this spec. std.web should accept a
typed error variant mapped to statuses in one checked table, keeping the prefix form working. Source:
docs/review-2026-11/language.md section 1. Must keep: one way to write each thing, no exceptions,
diagnostics with fixes (E0404/E0406 and friends), vo fmt canonical form, vo query/edit, record and replay
(errors in recordings stay text), the reference under 15,000 tokens (about 14,255 now)."

Background: a vo function that can fail returns `result[T]`, `ok(value)` or `err(message)`, and the
message is always a `str`. That is enough to print a failure, but not to act on one. A caller that must
retry on a timeout, answer 404 for a missing note or skip a malformed line can only compare text, and a
reworded message silently breaks it. `match` checks that every case of a variant is handled, which is
vo's best defense against a forgotten branch, but it cannot see inside a string. The standard library
already works around this: `std.web` turns `err("not_found: no note 7")` into a 404 by reading the text
before the colon, a protocol the checker knows nothing about.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A function says how it can fail (Priority: P1)

A developer (or agent) declares the ways a function can fail as a variant and returns
`result[Config, LoadError]`. Callers `match` the error and the checker makes sure every way is handled.

**Why this priority**: it is the feature; the other stories make it usable across a program.

**Independent Test**: a program declares `variant LoadError { missing(str); malformed(str); }`, a
function returning `result[Config, LoadError]`, and a caller that matches the error; it checks and runs,
and removing one case from the caller's `match` is E0401 naming the case.

**Acceptance Scenarios**:

1. **Given** `fn load(path: str) -> result[Config, LoadError]`, **When** it returns
   `err(LoadError.missing(path))`, **Then** it checks, and `err("missing")` (a `str`) is a type error
   naming both types.
2. **Given** a caller with `case err(e)`, **When** checked, **Then** `e` has the type `LoadError`, and a
   `match e` that leaves out a case is E0401 with the fix that inserts it.
3. **Given** `result[T]` written anywhere, **When** checked, **Then** it means `result[T, str]`, and the two
   spellings are the same type; every existing program checks, runs and prints exactly as before.
4. **Given** `result[void, E]`, **When** a function succeeds, **Then** it returns `ok()` as today.
5. **Given** an error type that is a record, a variant, `int` or `str`, **When** used, **Then** it works the
   same way; a function type, `void` or a type containing a function is an error type nobody can print
   or compare, and is refused at the type.
6. **Given** a generic function `fn first_ok[T, E](rs: list[result[T, E]]) -> result[T, E]`, **When**
   called with either kind of result, **Then** `T` and `E` are inferred as other type arguments are.

---

### User Story 2 - Errors pass up, and change form, only explicitly (Priority: P1)

`try` passes an error up when the caller's error type is the same. When it is not, the developer converts
the error with one explicit form, so the place where a low-level failure becomes a high-level one is
visible in the code.

**Why this priority**: without it, typed errors would be unusable across layers, or would leak into every
signature.

**Independent Test**: a three-layer program (file reading with `str` errors, parsing with `ParseError`,
loading with `LoadError`) checks and runs, converting at each layer with the explicit form; writing `try`
across two different error types is an error that names both and shows the form to use.

**Acceptance Scenarios**:

1. **Given** a function returning `result[Config, LoadError]`, **When** it writes `try load_part()` on
   another `result[Part, LoadError]`, **Then** the error passes up unchanged, as today.
2. **Given** the same function, **When** it writes `try` on a `result[str]` (a `str` error), **Then** it is
   a check error naming both error types, saying how to convert, with no automatic conversion.
3. **Given** `map_err(read_file(path), fn(m: str) -> LoadError { return LoadError.missing(m); })`,
   **When** checked, **Then** its type is `result[str, LoadError]`: the value of an `ok` is unchanged, the
   error of an `err` is converted by the function, and the function's effects become the caller's, as
   for `map`.
4. **Given** `main` (or a test) that uses `try` on a typed result, **When** checked, **Then** it is the
   same error as acceptance 2: `main` and tests return `result[void]` with `str` errors, and the program
   converts (for example to a message) where it reaches them.
5. **Given** an `err` value passed through several functions with the same error type, **When** the
   program runs, **Then** the value reaches the caller that matches it unchanged.

---

### User Story 3 - Failures are reported in full (Priority: P2)

Wherever the toolchain shows a value (a failing `expect_eq`, a runtime error's locals, a trace, a
counterexample, `vo query type-at`), a typed error is shown in vo syntax, so a report says which failure
happened and with what.

**Why this priority**: an error value that cannot be seen is a step back from a message.

**Independent Test**: a test with `expect_eq(load("x"), ok(cfg))` that gets
`err(LoadError.missing("x"))` reports exactly that text as the actual value; a property over
`result[int, LoadError]` generates and shrinks typed errors.

**Acceptance Scenarios**:

1. **Given** a failing `expect_eq` between results, **When** reported, **Then** both sides are shown in vo
   syntax (`err(LoadError.missing("x"))`), and the first difference points inside the error.
2. **Given** a runtime error in a function with a typed-error local, **When** reported, **Then** the local
   is shown in vo syntax, as other values are.
3. **Given** a `prop` with a parameter of type `result[T, E]`, **When** run, **Then** inputs include `ok`
   and `err` values of `E`, and a failing one is shrunk as other variants are.
4. **Given** `vo run --trace`, **When** a function returns a typed error, **Then** the `return` record
   shows it in vo syntax.
5. **Given** `vo run --record` and `vo replay`, **When** a program uses typed errors, **Then** recordings
   are unchanged: what a recording holds are the results of the outside world (files, HTTP, programs),
   which keep their `str` errors.

---

### User Story 4 - std.web maps typed errors to statuses in one checked place (Priority: P2)

An API declares its errors as a variant and gives each case its status once. Operations return
`err(NoteError.not_found(id))`; the app answers 404 with a problem body. The checker makes sure every
case has a status. Programs that use the `"kind: detail"` form keep working.

**Why this priority**: `std.web` is the largest user of error kinds today, and the reason for the
prefix protocol; it proves the feature on real code.

**Independent Test**: the notes API example declares `NoteError`, maps each case to a status in one
place, and its tests pass with the same answers as before; leaving a case out of the mapping is a check
error naming it.

**Acceptance Scenarios**:

1. **Given** an app whose operations return `result[Response, NoteError]` and a mapping from each case to
   a status and detail, **When** an operation returns `err(NoteError.not_found(7))`, **Then** the answer is
   the mapped status with an RFC 9457 problem body, as the prefix form gives today.
2. **Given** the mapping, **When** it leaves out a case of the variant, **Then** `vo check` reports it, with
   the fix that inserts the missing case (the mapping is an exhaustive `match`).
3. **Given** an existing app that uses `err("not_found: ...")` and `App.errors`, **When** checked and run,
   **Then** it behaves exactly as before.
4. **Given** an app, **When** it mixes both forms in one app, **Then** each operation's error type decides
   which mapping applies; an app has one error type for its typed operations.

---

### User Story 5 - Absence is a value, not a failure (Priority: P3)

A function that may find nothing returns `Option[T]` from `std.option` (`some(value)` or `none`) instead
of `err("not found")`, so "nothing there" and "something went wrong" are different types.

**Why this priority**: it is a small library type, independent of the rest, and it gives typed errors
their natural partner; the builtins that return a `result` for absence today are left for a later spec.

**Independent Test**: `import std.option.Option;` and a function returning `Option[Note]` check, run and
match; `vo doc std.option` documents it.

**Acceptance Scenarios**:

1. **Given** `fn find(id: int) -> Option[Note]`, **When** it returns `Option.some(n)` or `Option.none`,
   **Then** callers match `some` and `none` exhaustively.
2. **Given** `std.option`, **When** documented, **Then** it has the type and a few helpers (`or_else`,
   `to_result`), each with an example; `pop`, `env`, `get_or`, `first_match` keep their types.

---

### User Story 6 - One canonical form, documented where agents read (Priority: P3)

The reference, the guide, the error catalog and the examples show typed errors and when to use them;
`vo fmt` writes `result[T, E]` one way; `vo query` and `vo edit` work on error variants as on other
variants.

**Why this priority**: how agents learn it.

**Independent Test**: the reference documents typed errors with a runnable example inside its budget;
every new catalog entry has a broken and a fixed example; the examples that compare error text use a
typed error instead.

**Acceptance Scenarios**:

1. **Given** `result[T, str]` written out, **When** formatted, **Then** it stays as written (the two forms
   are the same type, and `result[T]` remains the form for `str` errors); the guide says to write
   `result[T]` for message errors.
2. **Given** the reference, **When** read, **Then** it explains `result[T, E]`, `try` across error types,
   the conversion form and the rule for `main` and tests, within 15,000 tokens.
3. **Given** `vo edit rename` of an error variant's case, **When** run, **Then** every `err(...)` and
   `case` that names it changes.

### Edge Cases

- `err(...)` whose type is not known from its place (`let r = err(LoadError.missing("a"));`): E0307, as for
  `str` errors today; the fix annotates the variable.
- `try` on a `result[T, E]` inside a function returning a different type that is not a result: E0406, as
  today.
- `map_err` on an `ok`: the function is not called (its effects are still declared, as for `map` on an
  empty list).
- `map_err` whose function returns the same type: allowed (it rewrites the error, for example to add
  context).
- `==` between results with typed errors: structural, as for every type.
- `result[T, E]` as a map key, a set element or a `priority_queue` item: the existing rules for `result`
  apply.
- `concurrent_map` returning typed results: allowed when `E` can come back from a task (no functions,
  E0314).
- `to_json`/`from_json` of `result[T, E]`: results are not JSON values today; unchanged.
- An error type that is itself a `result`: allowed but pointless; no special rule.
- A typed error reaching `main` through `try`: the check error of User Story 2, never a runtime surprise.
- An anonymous function whose return type is `result[T, E]`: as for named functions.
- Recursion through `map_err`'s function: allowed, like any function value.

## Requirements *(mandatory)*

### Functional Requirements

**The type**

- **FR-001**: `result[T, E]` MUST be a type for every `T` (including `void`) and every `E` that is not
  `void`, not a function type and does not contain one; `result[T]` MUST be exactly `result[T, str]`.
- **FR-002**: `ok(v)` / `ok()` and `err(e)` MUST build a `result[T, E]` from the expected type, with `e` of
  type `E`; `match` on it MUST bind `case err(e)` with type `E`.
- **FR-003**: Every existing program MUST check, run, format and report byte-identically: no output,
  diagnostic, record or recording changes for programs that do not write `result[T, E]` with `E` other
  than `str`.
- **FR-004**: Generic functions, records and variants MUST take `result[T, E]` with type parameters in
  either position, inferred as other type arguments are.

**Passing errors up and converting them**

- **FR-005**: `try e` MUST pass the error up only when the enclosing function's result has the same error
  type; any other combination MUST be a check error that names both error types and says how to convert.
- **FR-006**: There MUST be exactly one way to convert an error: `map_err(r, f)` with `f: fn(E1) -> E2`,
  giving `result[T, E2]`; its function's effects MUST become the caller's (as for `map`), and it MUST not
  call `f` on an `ok`.
- **FR-007**: `main` and tests MUST keep `result[void]` (a `str` error); a typed error MUST be converted
  before it reaches them (FR-005 reports it).
- **FR-008**: There MUST be no implicit conversion between error types, and no way to turn an error type
  into a `str` other than code the program writes.

**Reports**

- **FR-009**: Every report that shows values (test failures and their first difference, runtime error
  locals, traces, counterexamples, `vo query type-at`, `vo doc` examples) MUST show typed errors in vo
  syntax.
- **FR-010**: Property tests MUST generate and shrink `result[T, E]` values for every `E` they can
  generate.
- **FR-011**: Recordings and replay MUST be unchanged: the outside world's errors stay `str`.

**std.web and std.option**

- **FR-012**: `std.web` MUST let an app's operations return `result[Response, E]` for one error type `E`
  per app, with a single mapping from `E` to a status and detail that the checker verifies covers every
  case; the `"kind: detail"` form and `App.errors` MUST keep working unchanged.
- **FR-013**: `std.option` MUST provide `Option[T]` (`some(T)`, `none`) with documented helpers; no
  existing builtin or library function changes its type in this spec.

**Diagnostics, tools and documentation**

- **FR-014**: Each new mistake MUST have a stable code, a catalog entry with a broken and a fixed example,
  a golden case, and a fix where exactly one correction exists (for example, a `match` on an error missing
  a case: the inserted case).
- **FR-015**: `vo fmt`, `vo query` (refs, type-at), `vo edit rename` and `vo impact` MUST handle
  `result[T, E]` and error variants as they handle other types and variants.
- **FR-016**: The reference MUST document typed errors with a runnable example and stay under 15,000
  tokens; the guide's errors chapter MUST say when to use a typed error (callers act on the kind of
  failure) and when a message is enough (callers only report it).
- **FR-017**: The examples whose callers compare error text, and the notes API's error kinds, MUST use
  typed errors.

### Key Entities

- **Result type**: `result[T, E]`: a value that is `ok(T)` or `err(E)`; `result[T]` is `result[T, str]`.
- **Error type**: the `E` of a result: usually a variant whose cases are the ways a function can fail,
  possibly carrying details; any printable, comparable type.
- **Conversion**: `map_err(r, f)`, the one place where an error of one type becomes another.
- **Error mapping (std.web)**: one function from an app's error type to a status and a detail, checked to
  cover every case.
- **Option**: `std.option.Option[T]`, `some(T)` or `none`: absence that is not a failure.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every program in the test corpus, the examples, the standard library and the benchmark
  reference solutions checks, runs and formats byte-identically (731 of 731 reference runs unchanged).
- **SC-002**: The notes API answers every request of its test suite exactly as before after moving from
  `"kind: detail"` errors to a typed error; the number of places that know an error's status goes from
  one per error site to one per error case.
- **SC-003**: In the examples and the standard library, no caller decides what to do by comparing error
  text after this spec (counted with a search for comparisons against error strings).
- **SC-004**: Every new mistake is reported at check time with its own code; leaving a case of an error
  variant unhandled anywhere a `match` handles errors is reported, never discovered at run time.
- **SC-005**: Checking and running are not measurably slower: a 5,000-line program checks within 5% of
  today, and a loop returning typed errors runs within 5% of the same loop with `str` errors.
- **SC-006**: The reference stays under 15,000 tokens with the new section, and all its examples run.
- **SC-007**: An agent given only `vo reference` uses typed errors correctly: on tasks that ask callers to
  act on the kind of failure, first-try results are no worse than with message errors (benchmark
  protocol, isolated agents; may be scheduled separately).

## Assumptions

- The error type comes second and is optional: `result[T]` remains the everyday form, so the common case
  costs nothing to read or write, and no existing program changes.
- One conversion form (`map_err`) is enough. A `try ... else` statement or automatic conversions would be
  a second way to pass errors up, or hidden control flow; the constitution rules both out.
- `main` and tests keep `str` errors: what reaches the user is a message, and the toolchain's reports of a
  failed `main` stay as they are. Programs choose the message where they convert.
- There is no way to make a `str` out of any value automatically; programs that want a message from an
  error write it (a `describe(e: LoadError) -> str` function with a `match`), which is also where the
  checker can see every case is described.
- `std.web` keeps its current form fully working; the typed form is added beside it and the notes API
  example moves to it.
- `Option[T]` is a library variant, capitalized like other user-visible variants; changing `pop`, `env`,
  `get_or` or `first_match` to it is left for a later spec with its own migration.
- New diagnostics take the next free codes in their ranges; none is reused.
- The benchmark run of SC-007 reuses the isolated agent protocol and may run after the implementation.

# Feature Specification: Typed Holes, Property Tests and Contracts

**Feature Branch**: `005-holes-props-contracts`

**Created**: 2026-11-14

**Status**: Draft

**Input**: User description: "typed holes, property tests and contracts (spec 005, per docs/roadmap.md)". The roadmap entry (docs/roadmap.md, "005: typed holes, property tests and contracts") says:
- **Typed holes.** `todo("parse the header")` type-checks. The checker reports the expected type and the names in scope there. Running stops at the hole with a structured R-code, so the agent's loop becomes "fill one gap at a time" instead of "fix everything first".
- **Property tests.** `prop "sort keeps length" (xs: list[int]) { ... }` generates inputs from the declared types, with shrunk counterexamples as structured records.
- **Contracts.** `requires` and `ensures` on functions, checked at run time.
- **Success:** on benchmark tasks with hidden properties, properties catch more mistakes before the hidden checks do.

Clarified with the user on 2026-11-14:
- **Contracts are checked on every run**, not only in tests.
- **Property tests use their own `prop` item**, as the roadmap shows.
- **Success is gated on a deterministic stand-in** (a fixed set of buggy solutions), not on an agent benchmark run.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–004:
- **AI coding agents**, which write vo programs from the reference and fix them from diagnostics.
- **The people who review and run those programs.**

This spec addresses two of the roadmap's problems with agent-written code:
- **Unfinished code does not compile at all.** Agents work step by step, but today a vo program must be complete before it can be checked cleanly or run.
- **The agent's tests share the agent's misunderstanding.** A handful of hand-picked examples passes. The edge case the agent never thought of is exactly the one the hidden checks try.

### User Story 1 - Check and run unfinished programs (Priority: P1)

An agent sketches a program: `main`, the functions it needs, and their signatures. Where a body or an expression is not written yet, it puts a hole: `todo("parse the header")`.
- **When the agent checks the program**, every hole is reported with what is known there: the type the hole must produce and the names in scope, with their types. Holes do not stop the check from passing.
- **When the agent runs the program**, it runs until it reaches a hole. Then it stops with a structured runtime error that names the hole and where it is.

The agent fills one hole at a time and runs the parts that are done.

**Why this priority**: This changes the agent's loop from "make everything compile" to "fill one gap at a time". It is useful without the other two stories, and it is the cheapest of the three.

**Independent Test**: Check and run programs containing holes in every expression and statement position. Compare the diagnostics, runtime errors and exit statuses with golden expectations.

**Acceptance Scenarios**:

1. **Given** `fn parse(line: str) -> int { todo("parse the header"); }`, **When** the program is checked, **Then**:
   - the check passes (exit 0);
   - one hole warning names the description, the expected type `int`, and `line: str` in scope;
   - no missing-return error is reported.
2. **Given** `let n: int = todo("count");` and `print(todo("message"));`, **When** checked, **Then** the holes are reported with expected types `int` and `str`.
3. **Given** `let x = todo("anything");` with no annotation, **When** checked, **Then** an error says the hole's type cannot be determined and asks for an annotation.
4. **Given** a program whose `main` calls a function whose body is a hole, **When** it runs, **Then**:
   - output before the hole is printed;
   - it stops with exit status 70 and the hole runtime error, carrying the hole's description, its span and the call trace.
5. **Given** a program with holes only in functions that are never called, **When** it runs, **Then** it behaves exactly as if those functions were finished.
6. **Given** a test that reaches a hole, **When** `vo test` runs, **Then** that test is reported as `error` with the hole's code, and the other tests still run.
7. **Given** a function with `uses console` whose body is only a hole, **When** checked, **Then** no "effect not needed" warning is reported for that function.

---

### User Story 2 - Property tests find the edge cases (Priority: P1)

An agent states what must hold for every input: "sorting keeps the length", "decoding an encoding gives back the input", "the merged intervals do not overlap".
- **How it is written:** `prop "sort keeps length" (xs: list[int]) { ... }`.
- **How it runs:** `vo test` generates many inputs from the declared types, deterministically, and runs the body on each.
- **When an input fails**, the report gives the smallest failing input it could find (the counterexample) as structured data, along with the failing check.

**Why this priority**: This is the roadmap's main answer to "the agent's tests share the agent's misunderstanding". Generated inputs reach the empty list, the negative number, the non-ASCII string and the extreme value that hand-written examples skip.

**Independent Test**: Write properties for correct functions (they pass) and for deliberately broken ones (they fail with a small counterexample). Run each twice and compare the reports byte for byte.

**Acceptance Scenarios**:

1. **Given** `prop "reverse twice" (xs: list[int])` over a correct `reverse`, **When** `vo test` runs, **Then** it passes after the default number of generated cases, and the report shows how many cases ran.
2. **Given** a property "every value of the input is in the output" over a `dedupe` that keeps at most 3 distinct values, **When** `vo test` runs, **Then**:
   - the property fails;
   - the counterexample is a list of exactly 4 distinct values, each shrunk toward zero (for example `[0, 1, 2, 3]` or `[0, 1, -1, 2]`);
   - the report shows the failing check's location, expected and actual values.
3. **Given** the same file run twice, or on another machine, **When** `vo test` runs, **Then** the reports are byte-identical. **When** `--seed` is changed, **Then** different inputs may be generated, still deterministically.
4. **Given** a property whose body hits a runtime error for some input (for example overflow, a division by zero or an index out of range), **When** it runs, **Then** it fails with that error's code and a shrunk counterexample.
5. **Given** a property over a record, a variant (including a recursive one such as a tree), a map, a `result` or a generic type with its arguments (`Pair[int, str]`), **When** it runs, **Then** inputs of that type are generated, rendered in vo syntax in the report, and generation always terminates.
6. **Given** a property that discards most inputs with `assume(...)`, **When** too few inputs are accepted, **Then** the property fails with a report saying so, instead of passing on almost no cases.
7. **Given** a property parameter whose type cannot be generated (a function type, or a record that can only contain itself), **When** checked, **Then** a check error names the parameter and the type.
8. **Given** `vo test --json`, **When** a property fails, **Then**:
   - the `test` record carries the counterexample as a list of parameter names and rendered values;
   - it also carries the number of cases tried and the number of shrink steps;
   - the existing `failure` object is unchanged.

---

### User Story 3 - Contracts on functions (Priority: P2)

An agent or a reviewer states what a function needs and what it promises:

```text
fn isqrt(n: int) -> int
    requires n >= 0
    ensures <the returned value> * <the returned value> <= n
```

The conditions are part of the function's declaration, so a reviewer reads them with the signature. They are checked every time the function is called, in `vo run` and in `vo test`.
- **A broken `requires`** stops the program at the call, blaming the caller.
- **A broken `ensures`** stops the program at the return, blaming the function.

Property tests then exercise the contracts with generated inputs.

**Why this priority**: Contracts make a function's assumptions explicit and checked. Combined with property tests, they turn every call made during a property run into a check. They depend less on the other stories, but they are worth less without property tests to exercise them.

**Independent Test**: Run programs that keep and that break each kind of contract. Check the runtime error codes, spans and exit statuses. Run the speed benchmark to show that functions without contracts are not slower.

**Acceptance Scenarios**:

1. **Given** `isqrt` with `requires n >= 0`, **When** it is called with `-1`, **Then** the run stops with exit status 70 and the precondition runtime error:
   - its span is the call;
   - the message quotes the condition `n >= 0`;
   - the trace starts at the caller.
2. **Given** an `isqrt` whose body is wrong for some inputs, **When** it returns a value that breaks its `ensures`, **Then** the run stops with the postcondition runtime error at the `return`, quoting the condition.
3. **Given** a contract condition that calls a function with effects (such as `print`), **When** checked, **Then** it is a check error: contracts must be pure.
4. **Given** a function with several `requires` and `ensures` clauses, **When** it runs, **Then** the clauses are checked in source order, and the first one that fails is reported.
5. **Given** a function with contracts called through a function value (`map(xs, isqrt)`), **When** it runs, **Then** the contracts are checked exactly as for a direct call.
6. **Given** a function returning `result[T]` that leaves through `try`, **When** it returns the propagated `err`, **Then** its `ensures` clauses are checked on that returned value too.
7. **Given** a property test that calls a function with contracts, **When** a generated input breaks a contract, **Then** the property fails with a shrunk counterexample and the contract's runtime error.

---

### User Story 4 - Evidence that properties catch more mistakes (Priority: P2)

The maintainers can show that property tests catch mistakes the example tests miss, on the benchmark's hidden-property tasks. They use a fixed, reproducible set of buggy solutions rather than an agent run.

**Why this priority**: The roadmap gives this as the success measure for the spec. An agent benchmark run is expensive and benchmark v2 has not been run yet, so a deterministic stand-in gates this spec. The agent run is reported later.

**Independent Test**: For each hidden-property task (h01–h10), run the buggy solutions against the visible examples and against the task's properties, and count which of the two catches each bug.

**Acceptance Scenarios**:

1. **Given** at least 5 buggy vo solutions per task, each of which passes every visible example and fails at least one hidden case, **When** each task's properties run against them, **Then** the number caught is recorded per task and in total.
2. **Given** the task's properties, **When** they run against the correct reference solution, **Then** they all pass.

---

### Edge Cases

- **Holes in every position:**
  - as a whole function body, an argument, an operand, a `return` value, a record field initialiser, a list element or a `match` arm body;
  - in a generic function, where the expected type mentions `T`;
  - in an anonymous function;
  - in a test or property body.
- **A hole as a statement in the middle of a block:** the statements after it are unreachable (W0401, as for `return`).
- **A hole whose expected type is a function type or `result`:** reported with the full type; running it still stops with the hole error.
- **Holes and the effects record:** a function whose body is a hole is recorded with the effects it declares.
- **Holes and `vo fmt`:** holes are formatted like any call.
- **Property generation limits:**
  - nested containers (`list[list[map[str, int]]]`) stay small enough that 100 cases finish well inside the default timeout;
  - recursive variants terminate.
- **Values a property must reach:**
  - `int`: 0, negative values, and the extremes;
  - `str`: the empty string and multi-byte characters;
  - lists and maps: empty ones, and ones with repeated elements.
- **Property failures that are not shrunk:**
  - **A timeout** stops the property (R0009) and reports the input that was running.
  - **Running out of memory** (R0006) reports the input that was running.
- **Shrinking budget:** shrinking stops within the test's time limit, keeping the smallest failing input found so far.
- **Property names:** they share the name space and `--filter` with tests. A name used by both a test and a property is an error, as for two tests.
- **Effects in properties:** a property may declare a `uses` clause like a test. Every case runs under that grant and starts from the same state (seed, clock).
- **Contracts on generic functions** use only what the type parameters allow (`T: ordered` for `<`).
- **Contracts that never finish or recurse forever** are stopped by `--timeout` or the stack limit, deterministically.
- **A contract that calls a function that has contracts** checks those too.
- **`ensures` on a function without a return type** may refer only to its parameters, and is checked at every return.
- **Contracts on `main` and on tests:** `main` may have contracts like any function; tests and properties cannot.
- **Contracts on anonymous functions:** not allowed in this spec.

## Requirements *(mandatory)*

### Functional Requirements

**Typed holes**

- **FR-001**: The language MUST provide a hole expression with a description, `todo("...")`, that type-checks as the type its context expects. It MUST be usable wherever an expression of a known type is expected, and as a statement.
- **FR-002**: A hole used as a statement MUST end its path, as `return` does: it satisfies the missing-return check, and later statements are unreachable.
- **FR-003**: When a hole's expected type cannot be determined from its context, the checker MUST report an error that asks for a type annotation. It has no fix, because the missing type is exactly what is unknown.
- **FR-004**: Every hole MUST produce a warning diagnostic (new code) that carries:
  - the description;
  - the expected type;
  - the names in scope at the hole (parameters and local bindings) with their types.

  In JSON, these MUST be added fields of the existing diagnostic record.
- **FR-005**: The check MUST pass when holes are the only problem, so the program can be run. The `check` summary MUST report the number of holes.
- **FR-006**: A function or test that contains a hole MUST NOT get the "effect not needed" or "local never read" warnings.
- **FR-007**: Reaching a hole at run time MUST stop the program with a new runtime error (exit status 70). It carries the description, the hole's span and the call trace. In `vo test`, the test or property is reported as `error` with that code.

**Property tests**

- **FR-008**: The language MUST provide a property item: a name, one or more typed parameters, an optional `uses` clause, and a body of statements. In the body, `expect` and `expect_eq` work as in tests.
- **FR-009**: `vo test` MUST run every property with the same `--filter`, `--timeout` (applying to the whole property), `--json`, `--mem-stats` and exit-status rules as tests. Properties and tests run in source order.
- **FR-010**: Inputs MUST be generated from the parameter types, including:
  - `int`, `float`, `bool` and `str`;
  - lists, maps and `result`;
  - records and variants, including generic ones with their arguments and recursive ones.

  Generation MUST:
  - include small and boundary values (zero, negatives, the `int` extremes, empty strings and collections, multi-byte characters);
  - produce only finite floats;
  - keep sizes bounded so generation always terminates.
- **FR-011**: A parameter whose type cannot be generated (it contains a function type, or no finite value of it exists) MUST be a check error naming the parameter.
- **FR-012**: Generation MUST be deterministic: the same file, seed and version give the same inputs. `vo test` uses seed 1 unless `--seed` is given.
- **FR-013**: A property MUST run a fixed number of accepted cases (default 100), stopping at the first failing case. These count as failures: a failing `expect`/`expect_eq`, and any runtime error other than timeout and out-of-memory.
- **FR-014**: When a case fails, the input MUST be shrunk deterministically to a smaller input that fails with the same code (or the same check). Smaller means:
  - integers closer to zero;
  - shorter strings, lists and maps;
  - simpler variant cases.

  The report MUST give the shrunk counterexample, the failure for that input, the number of cases tried and the number of shrink steps. In `--json`, these MUST be added fields of the `test` record.
- **FR-015**: A builtin `assume(cond)`, allowed only in properties, MUST discard the current input when `cond` is false. If fewer than the required number of cases are accepted within a fixed attempt budget, the property MUST fail with a report that says so.
- **FR-016**: Each case MUST start from the same state, whatever earlier cases did: the seed for `random_int` and the clock. Memory is not reset between cases (the collector reclaims what a case leaves behind), and a property's memory report covers the whole property.
- **FR-017**: A property's text report MUST print the counterexample in vo syntax, one parameter per line, under the existing failure lines. A passing property's line MUST show the number of cases.

**Contracts**

- **FR-018**: A named function MUST be able to declare zero or more `requires` and `ensures` clauses, each a `bool` condition, as part of its declaration. They come after the `uses` clause and before the body, in one canonical order that `vo fmt` produces.
- **FR-019**: A `requires` condition MAY refer to the parameters. An `ensures` condition MAY refer to the parameters and to the returned value, through one fixed name defined by the reference.
- **FR-020**: Contract conditions MUST be pure. Calling something with effects is a check error. Contracts MUST NOT change the function's effects or its line in `vo.effects`.
- **FR-021**: `requires` clauses MUST be checked on every call, in source order, before the body runs, both in `vo run` and in `vo test`. A false one MUST stop the program with a new runtime error (exit status 70):
  - the span is the call expression;
  - the message quotes the condition's source text;
  - the trace starts at the caller.
- **FR-022**: `ensures` clauses MUST be checked in source order on every return, including `try` propagation and falling off the end of a function without a return type. A false one MUST stop the program with a new runtime error (exit status 70):
  - the span is the `return`, the `try` that propagated, or the function's closing brace;
  - the message quotes the condition.
- **FR-023**: Contracts MUST be checked the same way when the function is called through a function value or by a builtin such as `map` or `sort_by`.
- **FR-024**: Functions without contracts MUST run no slower than before this spec, measured with spec 004's speed benchmark.

**Language, reference and catalog**

- **FR-025**: The language reference MUST document holes, properties, `assume` and contracts, with examples that the test suite executes. The increase in its size MUST be stated, and it MUST stay within the 15,000-token budget.
- **FR-026**: Every new code (the hole warning, the cannot-determine-type error if a new code is needed, the property and contract check errors, the hole and contract runtime errors) MUST have:
  - a catalog entry with `vo explain` text;
  - at least one golden case that triggers it.
- **FR-027**: Existing programs MUST behave exactly as before (output, diagnostics, exit statuses). The one exception is programs that use the new keywords or the name `todo` or `assume` as identifiers. These MUST get a clear error, and the migration corpus MUST record them.

**Evaluation**

- **FR-028**: The project MUST include, for each hidden-property task h01–h10:
  - a vo property file written from the task statement and the reference solution's function signatures, never from the hidden cases;
  - at least 5 buggy vo solutions, each passing every visible example and failing at least one hidden case.

  A script MUST run both and report what was caught, per task and in total.

### Key Entities

- **Hole**: a placeholder expression in source. It has:
  - a description;
  - an expected type, and the names in scope with their types, known at check time;
  - a span, which identifies it at run time.
- **Property**: a named item with typed parameters, an optional effect grant and a body. It is run by `vo test` over generated cases.
- **Counterexample**: the smallest failing input found for a property: one rendered value per parameter. It comes with the failure it causes, the number of cases tried and the number of shrink steps.
- **Contract**: an ordered list of `requires` and `ensures` conditions on a named function. They are pure, checked at every call and return, and reported with their source text.
- **Bug set**: per hidden-property task, a fixed collection of buggy solutions used to measure what properties and examples catch.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Over the bug set (at least 50 buggy solutions over h01–h10):
  - properties catch at least 70% of them;
  - on every task, they catch at least one bug that the visible examples miss.

  The examples catch none by construction, so every caught bug is one the examples missed.
- **SC-002**: Every property in the evaluation passes on its task's reference solution. Every property report is byte-identical over 3 runs.
- **SC-003**: For each hole position in the edge cases, a golden case shows:
  - the check passing with the hole warning (expected type and names in scope);
  - running stopping with the hole error at the right span.
- **SC-004**: On deliberately broken functions (at least 10: off-by-one, a missed empty case, a lost duplicate, overflow, an unhandled negative), each failing property reports a counterexample that a reviewer judges minimal. Minimal means no parameter can be made smaller by one shrink step and still fail. Each property, including shrinking, finishes in under 2 seconds in the reference environment.
- **SC-005**: Contract violations are reported with the right code, span, quoted condition and trace in golden cases covering:
  - direct calls;
  - calls through function values and builtins;
  - `try` returns;
  - functions without a return type;
  - generic functions.
- **SC-006**: Speed:
  - each program in spec 004's speed benchmark (no contracts) runs within 5% of the spec 004 binary's time, measured against that binary with alternated runs (median of 5, then of 15 for a program still over the limit);
  - the 703 bench2 reference runs and the whole existing corpus are byte-identical.
- **SC-007**: The language reference stays under 15,000 tokens, grows by at most 2,000 tokens, and every example in it passes.
- **SC-008**: `make test`, the fuzzer, the determinism check and the collector stress pass all pass, with the new golden cases included.

## Assumptions

- **Default case count is fixed at 100.** A per-run option to change it is left out until someone needs it; `--seed` already gives different inputs.
- **Floats are finite.** Generated floats exclude NaN and infinities, because most float properties would fail on `nan != nan` for reasons unrelated to the code under test. A property can still construct them itself.
- **Integer extremes are generated on purpose.** Overflow is a runtime error in vo, so a property such as `a + b == b + a` fails at the extremes. That is a real finding about the code, and `assume` filters it.
- **No `old(...)` in `ensures`.** Postconditions cannot refer to values from before the call. Parameters are immutable, but the contents of a list passed in may change, and comparing against the old contents is out of scope.
- **Contracts are not proved.** They are checked at run time only; static verification is out of scope.
- **The agent benchmark run is a follow-up.** Running agents on the hidden-property tasks with and without properties is reported later. It is not a gate for this spec (clarified with the user).
- **The bug set is written before the properties are run against it**, and the properties are written from the task statement and the reference solution's function signatures only, never from the hidden cases. This keeps the stand-in from flattering the properties.
- **Holes do not make `check` fail.** A strict mode that treats holes as errors (for CI) is left out until someone needs it; the summary's hole count already shows whether a program is finished.
- **Names:** `todo`, `assume`, `prop`, `requires` and `ensures` become reserved or builtin names. Programs that used them as identifiers are expected to be rare. FR-027 makes them fail clearly.

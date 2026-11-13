# Feature Specification: vo-lang v2 Core: Effects, Functions as Values, Generics

**Feature Branch**: `002-effects-functions-generics`

**Created**: 2026-11-13

**Status**: Draft

**Input**: User description: "vo-lang v2 core: checked effects and capabilities, functions as values, and user-defined generics, designed together because each changes the type system and retrofitting effects later breaks every signature. Effects: a function's type declares what it may touch (stdout/stdin, file read, file write, network, clock, randomness, environment); main receives the capabilities it is granted and the runtime enforces them; vo check reports capability changes between versions so a reviewer can approve "this edit adds file writes" instead of reading every line. Functions as values: lambdas/closures, function types, passing functions to builtins (sort with a key, map, filter). User-defined generics for functions and records/variants. Keep vo's agent-first principles (one spelling per construct, first-token statements, machine-readable diagnostics with fixes, determinism, reference within the token budget). Success must include a harder agent benchmark (v2: multi-file projects, refactoring existing code, hidden property checks) because the v1 benchmark could not separate vo from Python."

## User Scenarios & Testing *(mandatory)*

The users are the same as in v1:
- **AI coding agents** that write and change vo code, often without anyone watching.
- **Human reviewers** who approve what agents produce.

v2 is about two problems mainstream languages leave open in AI-assisted development:
1. **Nobody can see what a change lets the program do.** A small edit can quietly add file writes or network access, and nothing in the language shows it.
2. **The language is too limited for real programs.** v1 has no functions as values and no user-defined generics, so common tasks (sort records by a field, a reusable stack) need workarounds.

### User Story 1 - Effects are part of every function's type (Priority: P1)

An agent writes a function that reads a file. The function's signature must say that it reads files. A function that declares nothing is pure: it cannot print, read input, touch files, read the clock, use randomness or read the environment.

If the agent forgets a declaration, `vo check` reports it with a fix that adds exactly the missing effect. Calling an effectful function requires the caller to declare those effects too, so a function's signature always tells the truth about everything it can do, directly or through what it calls.

**Why this priority**: This is the foundation of the review story (US2) and of runtime enforcement. It changes every signature, so it has to land first: adding it later would break every program.

**Independent Test**: A program with a pure helper, a function that prints and a function that reads a file. Remove one effect declaration and confirm `vo check` reports the exact missing effect with a fix. Apply the fix and confirm the program checks and runs.

**Acceptance Scenarios**:

1. **Given** a function with no effect declaration that calls `print`, **When** checked, **Then** an error names the function and the missing effect (console output), with a fix that adds it to the signature.
2. **Given** function A that calls function B, where B writes files, **When** A does not declare file writes, **Then** checking reports A with a fix adding file writes.
3. **Given** a function that declares file reads but never reads files, **When** checked, **Then** a warning names the unused effect, with a fix that removes it.
4. **Given** a pure function (no effects), **When** checked, **Then** no effect diagnostics are reported, and the reference states that such functions always return the same result for the same arguments.
5. **Given** a program whose `main` declares only console output, **When** it runs, **Then** it can never read files, use the network, read the clock or environment, or use randomness. This is guaranteed by checking and enforced again at run time.

---

### User Story 2 - Review what a change allows, not every line (Priority: P1)

The project keeps a reviewable record of the effects of every top-level function next to the code. When an edit changes what a function can do (for example `save_report` gains file writes), `vo check` fails and lists each change as "function: + effect" or "function: − effect". The record can only be updated by an explicit command, and the update shows up in the change set.

A reviewer approves capability changes by reading that short list instead of the whole diff.

**Why this priority**: This is the payoff the effects exist for. It makes changes by unattended agents safe to review quickly.

**Independent Test**: In a recorded multi-file project, an agent-style edit makes a deep helper write a file. `vo check` must fail and list that helper and every caller that gains the effect. Updating the record and checking again passes. The diff of the record shows exactly those lines.

**Acceptance Scenarios**:

1. **Given** a project whose effects record matches the code, **When** an edit adds file writes to one function, **Then** `vo check` fails, listing that function and each function whose effects grew through it.
2. **Given** that failing check, **When** the record is updated with the explicit command, **Then** the record changes by exactly the listed lines and `vo check` passes.
3. **Given** an edit that removes an effect, **When** checked, **Then** the removal is listed too, as a reduction that is never an error.
4. **Given** `--json`, **When** effects changed, **Then** each change is a structured record with the function, the file, the effect, and whether it was added or removed.
5. **Given** a project, **When** asked for its capability summary, **Then** the toolchain lists the effects the whole program and each test can perform.

---

### User Story 3 - Functions as values (Priority: P1)

An agent sorts records by a field, filters a list, transforms each element, or passes a callback. vo provides:
- anonymous functions with one syntax
- function types usable anywhere a type is
- closures that capture surrounding values
- builtins that take functions: sort by key, map, filter, fold, any, all

Effects flow through: a call to map with a printing function has the console effect.

**Why this priority**: The v1 benchmark author had to hand-write insertion sorts and zero-padded sort keys for basic tasks. This is the most common gap agents hit.

**Independent Test**: Sort a list of records by a numeric field (with a tie-break), filter even numbers, sum with fold, and store a function in a record field and call it. Each produces the expected output, and agents writing these from the reference alone succeed.

**Acceptance Scenarios**:

1. **Given** a list of records, **When** sorted by key with a function returning a field, **Then** the result is ordered by that field and stable for equal keys.
2. **Given** a function value stored in a variable, a record field or a list, **When** called, **Then** it runs with the arguments given.
3. **Given** a closure that refers to a surrounding `let` value, **When** called later, **Then** it sees that value.
4. **Given** a closure that refers to a surrounding `var`, **When** checked, **Then** an error explains that closures cannot capture variables that can change, with a fix that copies the current value into a `let` first.
5. **Given** a call to map with a function that prints, **When** the caller does not declare console output, **Then** checking reports the missing effect on the caller.

---

### User Story 4 - User-defined generics (Priority: P2)

An agent writes a generic `Stack[T]` record with push and pop functions, a generic `swap[A, B]` function, and a generic variant (e.g. a `Tree[T]` with leaf and node cases). Type arguments are usually inferred at call sites. When they cannot be, the checker says which type parameter could not be inferred, with a fix that annotates the variable when a later use shows the type. Generic code is checked once, where it is defined, so errors point into the generic body rather than at every use.

**Why this priority**: Reusable containers and helpers are the next most common need after functions as values. They are less urgent, because v1's built-in generic list, map and result already cover many cases.

**Independent Test**: A generic stack used with `int` and with `str`, a generic variant matched exhaustively, and a generic function needing ordering (`max_of`) used with `int` and `str`. All check and give the expected output. Using `max_of` with a record type is rejected with an error naming the ordering requirement.

**Acceptance Scenarios**:

1. **Given** `Stack[T]` with push and pop functions, **When** used with `int` and with `str` in one program, **Then** both work and mixing element types is a type error.
2. **Given** a call whose type argument cannot be inferred (e.g. creating an empty generic stack with no annotation), **When** checked, **Then** the error names the type parameter. When a later use in the same block determines the type (e.g. `push(s, 1)`), a fix adds the annotation (`let s: Stack[int] = …`).
3. **Given** a generic function that compares values with `<`, **When** its type parameter does not require ordering, **Then** the error is reported in the generic function's body with a fix adding the ordering requirement.
4. **Given** a generic variant, **When** matched, **Then** exhaustiveness works as for non-generic variants.

---

### User Story 5 - v1 programs move to v2 in one round of fixes (Priority: P2)

A v1 program has no effect declarations. Checking it under v2 reports each missing declaration with a fix. Applying the first fix of every diagnostic once yields a valid v2 program that behaves identically.

**Why this priority**: Existing code, the test corpus and the benchmark solutions must keep working without manual migration. This also tests that the effect fixes are complete.

**Independent Test**: Run the migration on every v1 golden program and benchmark solution, then compare outputs before and after.

**Acceptance Scenarios**:

1. **Given** any v1 program that checked cleanly under v1, **When** all first fixes are applied once, **Then** it checks cleanly under v2 and produces byte-identical output.
2. **Given** the migrated program, **When** formatted, **Then** effect declarations appear in one canonical order.

---

### User Story 6 - A benchmark that can separate languages (Priority: P1)

The v1 benchmark hit the ceiling: 98% first-attempt success for both vo and Python, so it could not show whether vo helps agents. v2 needs a harder benchmark with four kinds of tasks:
- **Multi-file projects** (at least three files).
- **Changes to an existing codebase:** renaming, changing a signature used in many places, extracting a function.
- **Hidden property checks:** the agent never sees the grading cases, and the checks probe edge cases.
- **Capability tasks:** the agent must do something without exceeding the effects it is allowed, and any excess counts as a failure.

Both languages are run with the same agent, the same limits and the same grading.

**Why this priority**: The goal of the project is to be better for agents. Without a benchmark that can tell the difference, that claim cannot be checked.

**Independent Test**: Run the suite for vo and Python with the same agent. Confirm that at least one language scores below the ceiling, and compare.

**Acceptance Scenarios**:

1. **Given** the v2 suite, **When** run for both languages, **Then** results are recorded per task category with attempts, success, the hidden-check pass rate and capability violations.
2. **Given** a capability task, **When** a solution performs an effect it was not allowed, **Then** the attempt is graded as failed in both languages: in vo by the checker or runtime, in Python by observing the run.

---

### Edge Cases

- **Recursion:** functions that call each other (directly or mutually) must declare the union of their effects. The checker reports the missing ones for every function in the cycle, consistently.
- **Functions passed through generic containers:** a list of functions stored and called later carries the effects of its function type.
- **Effectful lambdas:** an anonymous function that performs an effect has that effect in its type. Assigning it where a pure function type is expected is an error that names the effect.
- **Tests:** a test that needs files gets only what it declares. A test that performs an undeclared effect fails with a structured error, not a crash.
- **Empty effects record:** a new multi-file project has no record yet. `vo check` explains how to create one; that is a warning on every run until the record exists, never an error. Single-file programs get no warning.
- **Effects record drift caused by renames:** a function renamed without any change in effects is shown as a rename, not as one removal plus one addition.
- **Deep generic nesting** (a generic of a generic of a generic) works and keeps type names readable in messages.
- **Clock and randomness:** a program that uses the clock or randomness can still be made reproducible by fixing the time or the random seed for a run or a test.
- **Path escapes:** a file operation inside a scoped grant whose path leaves the scope, by `..`, an absolute path or a symbolic link, is refused at run time with a structured error that names the path and the granted scope.
- **Overlapping scopes:** a function declaring writes to `./out/` that calls one needing `./out/reports/` is covered. One needing `./tmp/` is not, and the fix widens the declaration to both directories.
- **Environment:** a program granted only `HOME` that reads `PATH` is refused at run time, even though the variable name was computed.

## Requirements *(mandatory)*

### Functional Requirements

**Effects**

- **FR-001**: The language MUST have a fixed set of effects: console output, console input, file read, file write, network, clock, randomness and environment.
- **FR-002**: Every function's type MUST state its effects, as a set that may be empty. A function with no declared effects MUST NOT perform any effect, directly or through anything it calls.
- **FR-003**: A function MUST declare every effect of everything it calls, including functions passed to it and functions called through values. A missing effect MUST be a check error with a fix that adds exactly the missing effects.
- **FR-004**: A declared effect the function cannot perform MUST be a warning, with a fix that removes it.
- **FR-005**: Every builtin MUST have fixed, documented effects (for example, `print` has console output and `read_file` has file read). v2 MUST add builtins that read an environment variable and produce random numbers, so that every effect except network can be used. Network operations arrive with the standard library spec.
- **FR-006**: Effects MUST be part of function types. A function value can be used where a function type with the same or more effects is expected, and never where fewer are expected.
- **FR-007**: User-defined functions that take function arguments MUST be able to accept functions with any effects and pass those effects on to their callers, without listing every possible effect.
- **FR-008**: A run MUST be granted exactly the effects (with their scopes) that `main` declares, and a test exactly what the test declares. The runtime MUST refuse anything outside the grant and stop with a structured runtime error with its own code. There are no separate grant flags or manifests: the reviewed signature of `main` (and its line in the effects record) is the grant.
- **FR-009**: Effect declarations MUST be able to carry a scope:
  - file read and file write: limited to one or more directories, relative to the program's working directory
  - environment: limited to named variables
  - network: limited to hosts (once network operations exist)

  An unscoped declaration covers the whole effect. Scope rules:
  - A caller's declaration MUST cover each callee's declaration: the same or a wider scope.
  - A scope that is too narrow is a check error, with a fix widening it to what the callee needs.
  - Because file paths and variable names are runtime values, the runtime MUST also check every file operation and environment read against the granted scope. Paths are normalized first, so `..` and symbolic links cannot leave the scope.
- **FR-009a**: Widening a scope (or removing one) MUST count as effect growth for the effects record (FR-013). Narrowing it counts as a reduction.
- **FR-010**: Tests MUST declare their effects like functions, and `vo test` MUST grant each test only those.
- **FR-011**: The clock and randomness MUST be fixable for a run or a test (a fixed time and a fixed random seed), so programs that use them can reproduce exact output.

**Reviewing capability changes**

- **FR-012**: A project MUST be able to keep an effects record listing the effects of every top-level function and test. It is a plain-text file kept next to the source, in a stable sorted order, so it diffs line by line.
- **FR-013**: `vo check` MUST compare the code with the effects record when the record exists. Any function whose effects grew MUST make the check fail, with one diagnostic per function naming the added effects and the call path that introduced them. Effects that shrank MUST be reported as information, never as errors.
- **FR-014**: The effects record MUST change only through an explicit toolchain command. That command prints the same changes it writes.
- **FR-015**: A rename that keeps a function's effects unchanged MUST be reported as a rename, not as a removal plus an addition.
- **FR-016**: The toolchain MUST be able to print the capability summary of a program (the union of effects reachable from `main`) and of each test, in human-readable and machine-readable form.

**Functions as values**

- **FR-017**: The language MUST have anonymous functions with exactly one syntax. Parameter and return types are written out, as for named functions (a function that returns nothing omits `->`). There is no shorter form.
- **FR-018**: Function types (parameter types, return type and effects) MUST be usable anywhere a type can appear: variables, parameters, return types, record fields, list elements and map values.
- **FR-019**: Anonymous functions MUST be able to use surrounding `let` bindings and parameters, which they capture by value. Using a surrounding `var` MUST be a check error, with a fix that copies the value into a `let` before the anonymous function.
- **FR-020**: Named functions MUST be usable as values by name.
- **FR-021**: The language MUST provide builtins that take functions: sort by key (stable), map, filter, fold, any and all. They are fully type-checked, and effects flow through them.
- **FR-022**: Calling a function value MUST check argument count and types like a direct call, with the same diagnostics.

**Generics**

- **FR-023**: Functions, records and variants MUST be able to declare type parameters.
- **FR-024**: Type arguments MUST be inferred from arguments and the expected type where possible. When they cannot be, the checker MUST name the type parameter that could not be inferred. When a later use determines it, there MUST be a fix that adds a type annotation to the variable. Type arguments are never written at call sites (one spelling).
- **FR-025**: A type parameter MUST be able to require ordering. Comparing with `<`, `<=`, `>` or `>=`, and sorting without a key, MUST work only on ordered types. Equality MUST work for every type, as in v1. User-defined constraints or interfaces are out of scope.
- **FR-026**: Generic code MUST be checked once, at its definition. Errors in a generic body MUST point into that body, not at each place it is used.
- **FR-027**: Messages MUST show generic types readably (e.g. `Stack[int]`, `Tree[list[str]]`).

**Agent-first principles (carried over from v1)**

- **FR-028**: Every new construct MUST have exactly one spelling, every statement MUST still be identifiable by its first token, and indentation MUST still not matter.
- **FR-029**: Every new error, warning and runtime error MUST have a stable code and a catalog entry with an example and a fix. A mechanical fix MUST be offered wherever one correction is unambiguous. This includes, at least: missing effect, unused effect, missing type argument, missing ordering requirement, capturing a `var`, and passing a function with too many effects.
- **FR-030**: The formatter MUST define one canonical form for all new constructs, including a fixed order for effect lists.
- **FR-031**: The complete language reference, including v2, MUST stay within 15,000 tokens. Every new construct needs at least one runnable example in it.
- **FR-032**: Programs that use neither the clock nor randomness MUST stay byte-for-byte deterministic. Programs that use them MUST be deterministic when the clock and seed are fixed.

**Migration**

- **FR-033**: Checking a v1 program under v2 MUST report only migration diagnostics (missing effects, plus the missing-record warning for multi-file programs) whose first fixes, applied once, produce a valid v2 program with identical behavior.

**Benchmark v2**

- **FR-034**: Benchmark v2 MUST contain at least 40 tasks in four categories, at least 8 per category:
  - multi-file projects (at least three source files)
  - changing existing code (rename, signature change across callers, extract function)
  - hidden property checks (grading cases not shown to the agent)
  - capability tasks (allowed effects are stated; any extra effect fails)
- **FR-035**: Benchmark v2 MUST run vo and Python with the same agent, prompts, attempt limit and grading, and record per task and category: success, attempts, the hidden-check pass rate and capability violations. In Python, capability violations are detected by observing what the run did.

### Key Entities

- **Effect**: One of console output, console input, file read, file write, network, clock, randomness, environment. Effects form sets, and sets appear in function types.
- **Function type**: Parameter types, return type and effect set. It is the type of named functions, anonymous functions and function values.
- **Closure**: An anonymous function together with the values it captured from its surroundings.
- **Type parameter**: A named placeholder on a function, record or variant, optionally requiring ordering.
- **Capability grant**: What a run or test may do. It is exactly the effects and scopes declared by `main` or by the test.
- **Scope**: A limit on an effect: directories for file effects, variable names for environment, hosts for network.
- **Effects record**: A plain-text, line-per-function file with the declared effects and scopes of each top-level function and test. Only an explicit command updates it.
- **Capability change**: A difference between the code and the effects record: a function, an effect, added or removed, and the call path that caused it.
- **Benchmark v2 task**: A category, a task statement, visible examples, hidden grading checks, and (for capability tasks) the allowed effects.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Benchmark v2 is not at the ceiling: at least one language scores below 85% first-attempt success overall, and both languages score below 95% in at least two categories.
- **SC-002**: On benchmark v2, agents using vo solve at least as many tasks within three attempts as with Python. They also need at least 20% fewer attempts on average, or they solve at least 10 percentage points more tasks.
- **SC-003**: On capability tasks, 100% of the vo solutions that exceed their allowed effects or scopes are rejected before or during the run with a structured report. Python's rate of undetected violations is recorded for comparison.
- **SC-003a**: In randomized testing of file paths and environment names inside scoped grants, 100% of out-of-scope accesses are refused, including `..`, absolute paths and symbolic links, and no in-scope access is refused.
- **SC-004**: For 20 recorded change sets that alter effects, the effects-record diff lists exactly the functions whose capabilities changed, with no false positives or negatives. A reviewer can name every capability change from that diff alone.
- **SC-005**: 100% of v1 programs (all v1 golden programs and all v1 benchmark solutions) migrate by applying first fixes once, and produce byte-identical output afterwards.
- **SC-006**: Given only the reference, an agent solves at least 90% of a 20-task set that needs sort-by-key, map, filter, fold, closures or generics within three attempts, and at least 75% on the first attempt.
- **SC-007**: The complete reference is at most 15,000 tokens.
- **SC-008**: Checking and running a 1,000-line program that uses effects, closures and generics takes under 1 second in the reference environment.
- **SC-009**: In randomized testing (fuzzing) of the new syntax, there are no crashes, hangs or unstructured outputs, matching v1's guarantee.

## Assumptions

- **Scope.** The user-defined part of v2 adds no traits or interfaces, no effect handlers (code cannot intercept or redefine effects) and no concurrency. The garbage collector, the standard library (including network operations) and packages are separate specs on the roadmap.
- **Network.** The network effect is defined and checked now, but no network operation exists until the standard library spec. Capability tasks in benchmark v2 use file, console, clock, randomness and environment effects.
- **Declarations are written out.** Effects are declared explicitly on named functions; they are not inferred. This follows the "everything explicit" principle. Agents rarely forget them, because a missing declaration always gets an exact fix, and the declaration is what reviewers read. Anonymous functions get their effects from their bodies.
- **`exit`.** `exit` stays effect-free (it ends the program and does not touch the outside world).
- **Stderr.** Console output covers both stdout and stderr.
- **Grants (decided 2026-11-13).** Grants are automatic: whatever `main` or a test declares, with no run-time flags. Safety comes from reviewing those declarations through the effects record, plus scopes on file, environment and network effects. Scope paths are relative to the working directory the program is started in.
- **Measurement.** Benchmark v2 is measured with the same agent setup as v1 (headless agent, three attempts), and in the same reference environment (emulated RV64 on an x86 host).

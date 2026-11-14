# Feature Specification: vo-lang Bytecode Execution

**Feature Branch**: `004-bytecode-compiler`

**Created**: 2026-11-13

**Status**: Draft

**Input**: User description: "bytecode compiler (spec 004)". The roadmap entry (docs/roadmap.md, "004: bytecode compiler") says: vo programs run by walking the checked syntax tree, the slowest kind of interpreter. A 10-million-iteration integer loop takes 5.4 s in vo, 1.0 s in Python 3 and 16 ms in C. Compile checked programs once to compact, typed bytecode and run that instead. Nothing visible changes (output, exit codes, runtime errors and their locations, run-time effect checks, `--timeout`, the collector). Target: at least 4× faster than the tree-walking interpreter on loop-heavy programs, with the ratio to Python reported. The corpus, the stress pass and the benchmark reference runs stay unchanged, and checking plus running a 1,000-line program stays under 1 s. Machine code and a JIT are out of scope.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–003:
- **AI coding agents**, which write vo programs from the reference and fix them from diagnostics.
- **The people who run and review those programs.**

Today every vo operation is executed by re-reading the program's checked syntax tree. Simple computation is about 5× slower than Python and several hundred times slower than C. Programs that do real work in loops, such as simulations, parsers, search and number crunching over large inputs, hit time limits that the same program in another language would not. This spec changes how vo programs are executed, and nothing else: the language, the reference, the diagnostics and every program's output stay exactly as they are.

### User Story 1 - Compute-heavy programs run several times faster (Priority: P1)

An agent writes a program that does real work in loops: it counts, searches, parses text, builds and updates lists and maps, or recurses over a data structure. The program produces the same output as before, in a fraction of the time.

**Why this priority**: This is the reason for the spec. Speed decides whether a correct vo program is usable for a task, and whether it finishes inside the time limits that agents and benchmarks set.

**Independent Test**: Run a fixed set of compute-heavy programs with the previous release and with the new one, compare outputs, and time both.

**Acceptance Scenarios**:

1. **Given** a loop of 10 million integer operations, **When** it runs, **Then** it prints the same result as before and takes at most a quarter of the previous time.
2. **Given** a program dominated by function calls (for example, naive recursive Fibonacci), **When** it runs, **Then** its output is the same and it is at least 4× faster than before.
3. **Given** programs dominated by list, map and string work, closures, or generic functions, **When** they run, **Then** their output is the same and none is slower than before.
4. **Given** the same programs written in Python, **When** both are timed in the reference setup, **Then** the ratio of vo's time to Python's is reported for each.

---

### User Story 2 - Nothing else changes (Priority: P1)

Every existing vo program behaves exactly as before: the same stdout and stderr byte for byte, the same exit status, and the same diagnostics. Runtime errors carry the same codes, messages and source locations. Tests pass and fail the same way, with the same reports. Effect checks at run time still deny the same operations. Agents and reviewers cannot tell from any output which way the program was executed.

**Why this priority**: vo promises byte-identical, deterministic behaviour, and agents fix programs from the exact location of a runtime error. A faster engine that moves an error, changes a message or changes a test result would break both.

**Independent Test**: Run the full existing test corpus, the stress pass, the migration corpus, the reference examples and every benchmark reference solution, and compare each output with the output before this spec.

**Acceptance Scenarios**:

1. **Given** any program that ran before this spec, **When** it runs now, **Then** its stdout, stderr and exit status are byte-identical. The exceptions are the memory-usage report requested with `--mem-stats`, and programs that ran out of stack only because of recursion deeper than 100,000 calls, which may now complete.
2. **Given** a program that stops with a runtime error (overflow, division by zero, index out of range, missing key, invalid argument, capability denied, out of memory), **When** it runs now, **Then** the error has the same code, message, source line and column as before.
3. **Given** a test file with passing and failing tests, **When** `vo test` runs it, **Then** the report (names, order, failure messages, rendered values, exit status) is identical.
4. **Given** a program run with `--timeout`, **When** it exceeds the limit, **Then** it stops with the structured timeout error within the same tolerance as before. This includes a tight loop that makes no calls and allocates nothing.
5. **Given** a 1,000-line program, **When** it is checked and run, **Then** the whole takes under 1 second in the reference environment, as before.

---

### User Story 3 - Limits stay at least as generous (Priority: P2)

A program that recursed deeply or held a lot of data before still works. Running out of stack or memory remains a clear, deterministic error.

**Why this priority**: A faster engine that fails programs the old one ran would be a regression that agents meet as an unexplained new error.

**Independent Test**: Run recursion of increasing depth and the allocation-heavy workloads from spec 003, with the old and the new release.

**Acceptance Scenarios**:

1. **Given** a simple recursive function, **When** it recurses 100,000 calls deep, **Then** it completes, as it does today.
2. **Given** recursion that never ends, **When** it runs, **Then** it stops with the stack-exhausted error at the same source location on every run.
3. **Given** the five allocation-heavy workloads from spec 003, **When** they run, **Then** their peak live memory stays under the bound spec 003 set (64 MiB).
4. **Given** a program whose live data grows without bound, **When** it runs repeatedly, **Then** it stops with the out-of-memory error at the same point every time.

---

### User Story 4 - Confidence that the new engine is equivalent (Priority: P2)

The maintainers can show that the new engine computes the same results as the old one, beyond the hand-written corpus: on generated programs, under collector stress, and under the fault-injection checks spec 003 introduced.

**Why this priority**: The corpus was written against the old engine and covers what its authors thought of. A new engine fails in ways nobody wrote a test for: unusual control flow, deep nesting, rare value kinds, and values the collector cannot see.

**Independent Test**: Generate random valid programs, run each with the release before this spec and with the new one, and compare. Re-run the collector stress pass and the fault matrix on the new engine.

**Acceptance Scenarios**:

1. **Given** a generator of random valid vo programs, **When** thousands of them run on both engines, **Then** every output, exit status and runtime error is identical.
2. **Given** the collector stress mode, **When** the whole corpus runs under it, **Then** the outputs are identical to the normal runs.
3. **Given** a fault-injection build that hides one kind of place where the new engine holds values, **When** the targeted checks run, **Then** they catch it.

---

### Edge Cases

- **`break`, `continue` and `return` in unusual places:** inside nested loops, inside `match` arms, and inside `try` expressions.
- **Runtime errors inside callbacks:** an error inside a closure called by a builtin (`sort_by`, `map`, `filter`, `fold`) reports the closure's location, as today.
- **Closures:** closures created in a loop capture the value of that iteration, and closures can outlive the function that created them.
- **Generics:** generic functions called with many different type arguments, including generic functions passed as values.
- **Effect parameters:** a callback with fewer effects than the parameter allows, and a forwarded callback, are checked at run time as today (R0010).
- **Very large programs:** functions thousands of lines long, deep nesting, thousands of functions, and many large constants all compile and run.
- **Failures in a test file:** a test that fails, runs out of memory or times out does not affect the next test's result or memory report.
- **Integer edges:** `int` overflow at the extremes (R0001), division by zero and the most negative number divided by −1 report the same errors at the same locations.
- **Multi-file programs:** functions and records from imported files behave identically.
- **Programs that never finish:** they are stopped by `--timeout` whether the time goes to vo code, builtins or the collector.

## Requirements *(mandatory)*

### Functional Requirements

**Execution**

- **FR-001**: After a program passes checking, `vo run` and `vo test` MUST translate it once into a compact intermediate form and execute that form. Programs MUST no longer be executed by walking the syntax tree.
- **FR-002**: The translation MUST use the type information the checker has already computed, so that operations whose types are known do not test value types at run time.
- **FR-003**: Translation MUST be deterministic: the same program always produces the same intermediate form.
- **FR-004**: Translation MUST NOT fail on any program that passes checking. Program size limits MUST be at least as generous as the checker's.

**Unchanged behaviour**

- **FR-005**: For every program, input and arguments, stdout, stderr and the exit status MUST be byte-identical to the release before this spec. There are two exceptions: the content of the memory-usage report (FR-011), and the exact recursion depth at which the stack runs out, which may only increase (FR-010).
- **FR-006**: Every runtime error MUST keep its code, message, source span and structured fields.
- **FR-007**: Run-time effect checks (R0010) MUST deny and allow exactly the same operations as before.
- **FR-008**: `--timeout` MUST stop any running program, including a loop that makes no calls and allocates nothing. The stop must come within 1 second of the limit, as specs 001 and 003 require.
- **FR-009**: The test runner MUST produce identical reports. Each test MUST still start from the same state, whatever earlier tests did.
- **FR-010**: A simple recursive function MUST be able to recurse at least 100,000 calls deep. Exhausting the stack MUST raise R0005 deterministically, at the call that exhausted it.
- **FR-011**: Memory reports (`--mem-stats`, R0006's in-use figure) MUST remain deterministic from run to run. Their numbers MAY differ from the previous release, because the new engine allocates differently. The changed golden expectations MUST be recorded as intended changes.
- **FR-012**: The collector MUST find every value the new engine holds. A value still in use MUST never be reclaimed.

**Language, reference and tools**

- **FR-013**: The language, its syntax, its diagnostics and the language reference MUST NOT change. No new command, option or output field is added for users.
- **FR-014**: Every existing tool that only reads programs (`check`, `fmt`, `explain`, `effects`, the migration path) MUST behave identically.

**Verification**

- **FR-015**: The project MUST provide a generator of random valid vo programs. Each generated program MUST be compared between the release before this spec and the new engine, on output, exit status and runtime errors.
- **FR-016**: The collector stress mode and the fault matrix from spec 003 MUST cover the new engine. For every place where the new engine holds values, some fault kind MUST make a targeted check fail when that place is left out of the roots. A new fault kind is needed only for a place the existing kinds do not cover.
- **FR-017**: The project MUST keep a speed benchmark of compute-heavy vo programs with equivalent Python versions. It MUST report vo's time before and after this spec, and the ratio to Python.

### Key Entities

- **Compiled program**: the translated form of one checked program. It holds every function's instructions, its constants, and a map from each instruction back to its source span. It is created once per run, is never written to disk, and lives for the whole run.
- **Source map**: the link from an executed instruction to the source span used in runtime errors and test failure reports. It may be stored inside the instructions rather than as a separate table.
- **Speed benchmark**: a fixed set of compute-heavy vo programs, each with a Python equivalent and an expected output, timed in the reference environment.
- **Program generator**: a deterministic, seedable source of random valid vo programs used to compare the two engines.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On the speed benchmark (at least six programs: an integer loop, recursive calls, list processing, map counting, string processing, and closure- and generic-heavy code), the median time falls by at least 4× (geometric mean over the set). No program in the set is less than 2× faster.
- **SC-002**: The 10-million-iteration integer loop that took 5.4 s on 2026-11-13 runs in at most 1.35 s in the same environment. vo's ratio to Python 3 is reported for every benchmark program.
- **SC-003**: All existing golden cases, reference examples, migration cases and all 703 benchmark reference runs produce byte-identical results. The exceptions are listed as intended changes: the memory figures in `--mem-stats` cases, the in-use figure in R0006 messages, and the case that recurses 200,000 calls deep (FR-010).
- **SC-004**: The whole corpus is byte-identical under collector stress, and every fault kind in the fault matrix, old and new, is caught.
- **SC-005**: At least 10,000 generated programs give identical results on the old and new engines.
- **SC-006**: Checking and running each 1,000-line performance program takes under 1 second. The bench2 reference runs as a whole take at most 10% more CPU time (user plus system) than before. No single run is slower by more than 10% or 5 ms, whichever is larger, measured as the median of 5 runs.
- **SC-007**: A simple recursive function completes at 100,000 calls deep. The five allocation-heavy workloads from spec 003 stay under 64 MiB peak live memory.
- **SC-008**: A tight loop with no calls, run with a 2-second `--timeout`, stops within 1 second of the limit.
- **SC-009**: The language reference is unchanged: the same token count, and every example passes.

## Assumptions

- **Reference environment:** the same as before, `qemu-riscv64` on the development host. The before-and-after numbers come from the last release before this spec, built from the commit before this work started. That binary also serves as the comparison engine for FR-015 and SC-005. The old interpreter does not need to stay in the product.
- **No change in what is measured:** startup and checking time are unchanged, and the 1 s budget for 1,000-line programs already holds. The speedup applies to time spent running vo code. Time inside builtins (sorting, string search, rendering) is already native and is not expected to fall much.
- **No disk cache:** the compiled form is rebuilt on every run. Translation is expected to cost far less than checking, so caching is not needed.
- **Memory figures may move:** the new engine's own data structures and frame layout change how much memory a program uses. Spec 003's bounds (64 MiB peak on the workloads, live data over the limit for R0006) still apply.
- **Out of scope:** machine code, a JIT, and optimisations that change observable behaviour. That excludes reordering or removing operations that can raise runtime errors, and changing evaluation order.
- **Debug output:** a developer-only way to print the compiled form may be added for maintainers. It is not a user-facing feature and is not documented in the reference.

# Feature Specification: vo-lang Core Language and Toolchain

**Feature Branch**: `001-vo-lang-core`

**Created**: 2026-11-13

**Status**: Draft

**Input**: User description: "vo-lang is a new programming language built in assembly. The goal is to make a language optimal for ai agents to work with."

## User Scenarios & Testing *(mandatory)*

The primary user is an **AI coding agent**: a language model that works through file edits and shell commands, reads tool output as text, and has never seen vo-lang in its training data. The secondary user is a **human developer** who reviews what the agent wrote.

Everything below follows from one premise. An agent works by repeating three steps: write code, get feedback, fix the code. The language and its tools should make each step short, predictable, and unambiguous.

### User Story 1 - Write and run a program (Priority: P1)

An agent writes a vo-lang source file, runs it with a single command, and receives the program's output and exit status.

**Why this priority**: Without this, nothing else matters. A language that can run a program that reads input, computes, and writes output is the minimum viable product.

**Independent Test**: Write a program that reads lines from standard input, computes a result (e.g., sums integers, counts words), and prints it. Run it and check that the output and exit status match the expected values.

**Acceptance Scenarios**:

1. **Given** a valid source file that prints a greeting, **When** the agent runs it, **Then** the greeting appears on standard output and the exit status is 0.
2. **Given** a program that reads command-line arguments and standard input, **When** it is run with arguments and piped input, **Then** it can access both and produce the correct output.
3. **Given** a program that explicitly exits with status 3, **When** it is run, **Then** the toolchain exits with status 3.
4. **Given** a program that divides by zero at runtime, **When** it is run, **Then** execution stops and a structured runtime error report names the failure kind, the source location, and the call chain.

---

### User Story 2 - Fix mistakes from machine-readable diagnostics (Priority: P1)

An agent submits code that contains errors. The toolchain reports every error in one pass. Each report has a stable error code, an exact location, a plain statement of what was expected and what was found, and, where possible, a concrete fix. The agent applies the fixes and the code compiles.

**Why this priority**: Agents spend most of their effort in the fix loop. Vague, partial, or human-only diagnostics are the biggest source of wasted agent attempts, which makes this the main thing that sets vo-lang apart from other languages.

**Independent Test**: Take a corpus of programs with deliberate errors (type mismatch, undefined name, missing case in a match, unbalanced delimiter). Run the checker in structured-output mode. Check that every error is reported with code, file, line, column, and span, and that applying each suggested fix removes that error.

**Acceptance Scenarios**:

1. **Given** a file with three independent errors, **When** the agent checks it, **Then** all three are reported in a single run, ordered by location.
2. **Given** a reference to an undefined name that differs by one character from a defined name, **When** checked, **Then** the diagnostic suggests the defined name as an exact text replacement at an exact span.
3. **Given** structured-output mode, **When** any diagnostic is produced, **Then** it parses as a self-contained machine-readable record with a fixed schema.
4. **Given** an error code from any diagnostic, **When** the agent asks the toolchain to explain it, **Then** it receives a description of the error, one example that triggers it, and the corrected version of that example.
5. **Given** an unclosed block, **When** checked, **Then** the diagnostic points to both the opening delimiter and the place where the closing one was expected.

---

### User Story 3 - Learn the language from one compact reference (Priority: P1)

An agent with no prior knowledge of vo-lang reads one reference document that fits in its context window. From it, the agent writes correct programs.

**Why this priority**: vo-lang has no presence in model training data. If the whole language cannot be learned in context, agents will fall back on guesses taken from other languages, and the other goals fail.

**Independent Test**: Give a fresh agent only the reference document and a set of small programming tasks. Measure how many tasks it completes with code that checks and runs correctly.

**Acceptance Scenarios**:

1. **Given** the reference document, **When** its size is measured, **Then** it fits within the token budget defined in the success criteria.
2. **Given** the reference document, **When** it is compared to the toolchain's behavior, **Then** every construct the toolchain accepts is described in it, and every example in it checks and runs with the documented output.
3. **Given** the reference, **When** the agent asks the toolchain for it, **Then** the toolchain can print the reference itself, so the reference never drifts from the installed version.

---

### User Story 4 - Verify behavior with built-in tests (Priority: P2)

An agent writes test cases next to the code they test and runs them with one command. The results come back structured: pass, fail, or error, and for each failure the expected value, the actual value, and the location.

**Why this priority**: Agents need an objective "done" signal. Built-in tests remove the need to set up a test framework and give agents a standard way to check their own work.

**Independent Test**: Write a file containing a function and three tests (two passing, one deliberately failing). Run the test command. Check that the results show 2 passed and 1 failed, with expected and actual values for the failure.

**Acceptance Scenarios**:

1. **Given** a file with test cases, **When** tests are run, **Then** each test's name, status, and duration are reported, and the exit status is nonzero if any test failed.
2. **Given** a failing equality assertion, **When** reported, **Then** the report includes the expected value, the actual value, and the assertion's location.
3. **Given** a test that never terminates, **When** tests are run with a time limit, **Then** that test is reported as timed out and the remaining tests still run.
4. **Given** a name filter, **When** tests are run, **Then** only matching tests run.

---

### User Story 5 - One canonical form for every program (Priority: P2)

An agent (or a human) runs the formatter. Every program has exactly one canonical text form, so formatting choices never show up in diffs and agents never have to choose a style.

**Why this priority**: Canonical form keeps diffs minimal and reviewable, and removes a whole class of inconsistent output from the agent. It has lower priority because programs still work without it.

**Independent Test**: Format a corpus of programs written in varied styles. Check that formatting is idempotent, that semantically identical programs which differ only in whitespace and layout produce identical output, and that program behavior does not change.

**Acceptance Scenarios**:

1. **Given** any valid program, **When** it is formatted twice, **Then** the second pass makes no changes.
2. **Given** a file with syntax errors, **When** formatting is requested, **Then** the file is left unchanged and the syntax errors are reported.
3. **Given** a check-only formatting mode, **When** a file is not in canonical form, **Then** the toolchain exits nonzero and lists the affected files without modifying them.

---

### User Story 6 - Programs across multiple files (Priority: P3)

An agent splits a program into several files. Each file explicitly imports the names it uses from other files, so the meaning of any file can be determined from that file plus the files it names.

**Why this priority**: Real tasks outgrow a single file, but single-file programs already deliver value.

**Independent Test**: Build a three-file program in which one file imports functions and types from the other two. Run it and check the output. Introduce an import cycle and check that it is reported.

**Acceptance Scenarios**:

1. **Given** a file that uses a name from another file without importing it, **When** checked, **Then** an error names the file that defines it and suggests the exact import line.
2. **Given** two files that import each other, **When** checked, **Then** the cycle is reported with the full chain of files.

---

### Edge Cases

- **Empty input**: an empty file, or a file with only comments, checks cleanly. Running it does nothing and exits 0.
- **Invalid encoding**: a source file with invalid UTF-8 produces a diagnostic at the first invalid byte. The toolchain does not crash.
- **Error floods**: a file with hundreds of errors reports up to a fixed cap, then a summary count of the rest. Cascading errors from a single root cause are suppressed.
- **Runaway programs**: infinite loops can be stopped by a caller-specified time limit. Unbounded recursion produces a structured "stack exhausted" error, not a crash.
- **Resource exhaustion**: running out of memory produces a structured runtime error. Silent termination is never acceptable.
- **Arithmetic edge cases**: integer overflow, division by zero, and out-of-bounds indexing are always detected and reported. Results are never undefined or silently wrapped.
- **Overlapping fixes**: when two suggested fixes overlap or touch, even across different diagnostics, only the first (in source order) is offered.
- **Partial edits**: when an agent's edit leaves a delimiter unbalanced, the diagnostic names the opening delimiter's location as well as the point of failure.
- **Large inputs**: source files of at least 10,000 lines, and lines of at least 10,000 characters, are accepted without failure.
- **Missing files**: if the requested source file does not exist or cannot be read, the toolchain reports this as a structured error with a distinct exit status.

## Requirements *(mandatory)*

### Functional Requirements

**Language design: predictability**

- **FR-001**: Each language construct MUST have exactly one syntactic form. There MUST be no optional or alternative spellings, such as optional separators or several loop syntaxes for the same purpose.
- **FR-002**: The kind of every construct MUST be identifiable from its first token, so a reader never needs lookahead or context to tell what a line is.
- **FR-003**: Block structure MUST be marked by explicit delimiters. Indentation MUST NOT affect meaning.
- **FR-004**: The language MUST be statically typed. All type errors MUST be reported before any code runs. Function parameters and return types MUST be declared explicitly. Types of local variables MAY be inferred.
- **FR-005**: The language MUST NOT perform implicit conversions between types.
- **FR-006**: The language MUST NOT permit name shadowing, function overloading, or operator overloading. Each name in scope MUST refer to exactly one definition.
- **FR-007**: The language MUST NOT have hidden control flow. Error handling MUST use values returned from functions. The checker MUST reject code that ignores a returned error.
- **FR-008**: Program behavior MUST be fully defined. Integer overflow, division by zero, and out-of-bounds access MUST stop execution with a structured runtime error.
- **FR-009**: Given the same source, arguments, and input, a program MUST produce the same output every time. Any nondeterministic source, such as time or randomness, MUST be an explicit call.
- **FR-010**: Programs MUST NOT manage memory manually. Memory errors such as use-after-free or double-free MUST be impossible to express.

**Language capabilities (v1)**

- **FR-011**: The language MUST provide signed 64-bit integers, 64-bit floating-point numbers, booleans, and immutable text strings.
- **FR-012**: The language MUST provide growable lists, maps keyed by integer or string, records with named fields, and tagged unions (variants).
- **FR-013**: Matching on a tagged union MUST be exhaustive. A missing case MUST be a check-time error that names the missing cases.
- **FR-014**: The language MUST provide functions, conditionals, loops, early return, and recursion.
- **FR-015**: Programs MUST be able to read command-line arguments, read standard input, write standard output and standard error, read and write files, and set the exit status.
- **FR-016**: Programs MUST be able to span multiple files. Every cross-file name use MUST go through an explicit import that names the imported item. Import cycles MUST be reported as errors.

**Toolchain**

- **FR-017**: The toolchain MUST be a single executable that offers these commands: check (find errors without running), run, test, fmt (format), explain (an error code), and reference (print the language reference).
- **FR-018**: Every command MUST offer a structured machine-readable output mode with a documented, versioned schema, as well as a human-readable mode.
- **FR-019**: Exit statuses MUST be distinct and documented for: success, program/test failure, check errors, runtime error, timeout, and toolchain usage errors.
- **FR-020**: The run and test commands MUST accept a time limit. A program that exceeds it MUST be stopped and reported as timed out.

**Diagnostics**

- **FR-021**: The checker MUST report all independent errors in a single pass, ordered by location, up to a documented cap. Diagnostics that cascade from an already-reported error MUST be suppressed.
- **FR-022**: Every diagnostic MUST include a stable error code, a severity, the file, the start and end line and column, a one-sentence message, and, where applicable, the expected and found items.
- **FR-023**: Where a single unambiguous correction exists, a diagnostic MUST include a fix suggestion given as exact replacement text for an exact span.
- **FR-024**: Error codes MUST be stable across releases. A code MUST NOT be reused for a different meaning.
- **FR-025**: Every runtime error MUST report the failure kind, the source location, and the call chain with locations.

**Testing and formatting**

- **FR-026**: Test cases MUST be declared in source files next to the code under test. They MUST NOT be included when the program is run normally.
- **FR-027**: Test results MUST report each test's name, status (pass/fail/error/timeout), and location. Failed assertions MUST also report expected and actual values.
- **FR-028**: The formatter MUST produce one canonical form per program. It MUST be idempotent, preserve comments, and never change program behavior.

**Documentation**

- **FR-029**: A single language reference MUST describe every construct, every built-in operation, the toolchain commands, the output schemas, and the exit statuses, within the size budget in SC-002.
- **FR-030**: Every code example in the reference MUST be checked and run automatically, and its documented output verified, as part of the toolchain's own test suite.

### Key Entities

- **Source File**: A UTF-8 text file containing vo-lang declarations, optional test cases, and explicit imports. It is the unit of import.
- **Program**: A set of source files reachable through imports from one entry file. It has a single entry point.
- **Diagnostic**: A check-time finding. It has an error code, severity, location span, message, expected/found items, and zero or more fix suggestions.
- **Fix Suggestion**: An exact span plus replacement text that an agent can apply mechanically.
- **Error Code Catalog**: The stable set of all error codes. Each code has a description, an example that triggers it, and a corrected example.
- **Runtime Error Report**: The failure kind, location, and call chain produced when a running program stops abnormally.
- **Test Case**: A named, self-contained check declared in a source file.
- **Test Result**: A test's name, status, duration, location, and, for failures, the expected and actual values.
- **Language Reference**: The single authoritative description of the language, shipped with the toolchain and printable from it.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Given only the language reference, an AI agent with no prior exposure to vo-lang completes at least 70% of a fixed benchmark of 50 small programming tasks on the first attempt, and at least 95% within three attempts using only toolchain feedback.
- **SC-002**: The complete language reference fits within 15,000 tokens, estimated as words × 1.3.
- **SC-003**: 100% of diagnostics produced on the error corpus (every diagnostic-producing golden case) contain a code, a full location span, and a message. At least 80% of diagnostics whose error code the catalog marks as always fixable (`Fixable: yes`, as opposed to `sometimes` for fixes that only exist in some situations) include a fix suggestion, and applying a suggestion removes the targeted error in at least 95% of cases. The overall share of diagnostics with a fix is also reported.
- **SC-004**: For a 1,000-line program, check plus run startup completes in under 1 second on the reference environment, so the agent's feedback loop is never slowed by the toolchain.
- **SC-005**: The formatter is idempotent on 100% of the test corpus, and the formatted programs behave identically to the originals.
- **SC-006**: Running any corpus program 100 times with the same inputs gives byte-identical output and exit status every time.
- **SC-007**: Across the test corpus and randomized malformed-input testing, no toolchain invocation ends without a structured report. There are zero crashes, hangs (when a time limit is given), or silent failures.
- **SC-008**: In a comparison against an established general-purpose language on the same 50-task benchmark, agents need fewer fix attempts per task on average with vo-lang.

## Assumptions

- **Implementation constraint (from the user)**: The toolchain is written in assembly. The target is 64-bit RISC-V (RV64IMAFD, standard lp64d calling convention), chosen 2026-11-13 once the project's SESI checker supported it, running as a Linux user-space program. On non-RISC-V hosts it runs under a user-mode emulator, and the "reference environment" in SC-004 means that setup.
- The toolchain runs programs directly. Producing standalone executables, cross-compiling, and self-hosting (writing vo-lang in vo-lang) are out of scope for v1.
- Out of scope for v1: concurrency, networking, foreign-function interface, package manager or registry, user-defined generics (built-in containers are generic), editor/IDE integration, and a standard library beyond the core I/O, string, list, and map operations listed above.
- Identifiers are ASCII. Strings and source files are UTF-8.
- "AI agent" means an LLM-based coding assistant that works through file edits and shell commands. Results in SC-001 and SC-008 are measured with at least one current frontier model. The benchmark tasks and error corpus are created as part of this feature.
- Human readability matters, but when it conflicts with agent effectiveness, agent effectiveness wins. For example, explicit verbosity is preferred over terse shortcuts.

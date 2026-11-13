# Feature Specification: vo-lang Memory Reclamation and Runtime Robustness

**Feature Branch**: `003-garbage-collector`

**Created**: 2026-11-13

**Status**: Draft

**Input**: User description: "vo-lang spec 003: garbage collector and memory model. Today memory is a bump arena that is never freed, with a 512 MiB cap (R0006): building a string with `+` in a loop runs out after tens of thousands of characters, long-running or allocation-heavy programs cannot exist, and agents are told to work around it in the reference. Add automatic memory reclamation with no change to the language, its syntax or its output: programs that allocate a lot but keep little alive must run in bounded memory. Keep vo's principles: byte-for-byte deterministic output (collection must never change what a program prints or when it fails), structured runtime errors when live memory really exceeds the limit, and the 1 s budget for checking and running a 1,000-line program. Include the runtime robustness follow-ups found in spec 002: `--timeout` must also stop long operations inside the runtime (not only vo loops), and generic operations should not pay a type-substitution walk on every call in hot code. Success must include measurable memory bounds on allocation-heavy workloads (string building, list churn, map churn, closures, generic containers), no regression on the existing test corpus and benchmarks, and stress tests that would catch a collector freeing something still in use."

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001 and 002:
- **AI coding agents**, which write vo programs from the reference and fix them from diagnostics.
- **The people who run and review those programs.**

Today a vo program never gets memory back. Anything that allocates a lot (building text piece by piece, processing a large input line by line, repeatedly creating and discarding lists or maps) eventually stops with an out-of-memory error, even when it only keeps a little data alive. The reference has to warn agents about this and teach a workaround, which is one more thing to get wrong. This spec makes memory reclamation automatic and invisible: programs behave exactly as before, except that they no longer run out of memory when they don't actually need much.

### User Story 1 - Allocation-heavy programs run in bounded memory (Priority: P1)

An agent writes the obvious program: it builds a report by appending to a string in a loop, reads a large input line by line while keeping only running totals, or repeatedly builds a temporary list or map and throws it away. The program runs to completion, using memory proportional to what it keeps alive, not to everything it ever created.

**Why this priority**: This is the reason for the spec. It removes a trap that every allocation-heavy program hits and that the reference currently has to explain.

**Independent Test**: Run a set of allocation-heavy programs that each create far more data over their lifetime than the memory limit, while keeping little alive at any moment. All complete with the expected output and a peak memory far below the limit.

**Acceptance Scenarios**:

1. **Given** a program that builds a long string by appending one piece at a time with `+`, **When** it runs, **Then** it completes and its peak memory stays bounded by a small multiple of the final string's size.
2. **Given** a program that processes a large input line by line and keeps only totals, **When** it runs, **Then** its peak memory does not grow with the input size.
3. **Given** a program that repeatedly creates and discards lists, maps, records, closures and generic containers, **When** it runs, **Then** it completes regardless of how many it creates in total.
4. **Given** the language reference, **When** it is read, **Then** it no longer warns that memory is never reclaimed or prescribes a workaround for it.

---

### User Story 2 - Nothing else changes (Priority: P1)

Every existing vo program behaves exactly as before: the same output byte for byte, the same exit status, the same diagnostics, the same runtime errors in the same places, the same test results. Memory reclamation is invisible to programs, tests and agents.

**Why this priority**: vo promises deterministic, byte-identical output. A reclamation scheme that changes behaviour (or makes it timing-dependent) would break that promise and every golden test.

**Independent Test**: Run the full existing test corpus, the migration corpus, the benchmark reference solutions and the reference examples, and compare every output with the output before this spec.

**Acceptance Scenarios**:

1. **Given** any program that completed before this spec, **When** it runs now, **Then** its stdout, stderr and exit status are byte-identical.
2. **Given** the same program, input and arguments, **When** run repeatedly, **Then** the output is identical every time, including where and how it fails.
3. **Given** a 1,000-line program, **When** checked and run, **Then** the whole takes under 1 second in the reference environment, as before.

---

### User Story 3 - Running out of memory is still a clear, deterministic error (Priority: P1)

A program that genuinely keeps too much alive (for example, it stores every line of an enormous input) still stops with the structured out-of-memory runtime error. The error now means "the data this program holds is too large", not "this program allocated a lot over time". It happens at the same point on every run and reports how much memory was in use and what the limit is. The limit can be raised or lowered for a run or a test.

**Why this priority**: Agents and people must be able to trust the error. It needs to be accurate (only real excess), reproducible, and adjustable.

**Independent Test**: Run a program whose live data grows without bound, repeatedly and with different limits. It always fails at the same step with the structured error, reporting the memory in use and the limit; with a higher limit it fails later, with a lower one earlier.

**Acceptance Scenarios**:

1. **Given** a program whose live data exceeds the limit, **When** it runs, **Then** it stops with the out-of-memory runtime error, which states the memory in use and the limit.
2. **Given** that program run 20 times, **When** the reports are compared, **Then** every run fails at the same place with the same report.
3. **Given** a run or test with an explicit memory limit, **When** the live data exceeds it, **Then** the error uses that limit.

---

### User Story 4 - The time limit stops everything (Priority: P2)

A program that makes the runtime do an enormous amount of work in a single builtin (sorting or joining a huge list, comparing two huge values, a pathological map workload) is stopped by the run's time limit just like a long-running loop. Today the limit is only checked between vo statements, so such work can run far past it.

**Why this priority**: The time limit protects unattended agent runs. A limit with holes cannot be relied on. This is a known gap from spec 002.

**Independent Test**: Run a set of programs that each spend almost all their time inside a single builtin call. All stop with the timeout error shortly after the limit.

**Acceptance Scenarios**:

1. **Given** a run with a time limit and a program that spends longer than that inside one builtin, **When** it runs, **Then** it stops with the timeout runtime error within one second after the limit.
2. **Given** the same program in a test, **When** the test exceeds its time limit, **Then** the test is reported as timed out and the remaining tests still run.

---

### User Story 5 - Generic code is not slower in hot loops (Priority: P2)

An agent writes a generic helper, such as a generic stack, `max_of[T: ordered]` or a generic equality check, and calls it in a tight loop. It runs about as fast as the equivalent code written for one specific type. Today every equality, comparison or rendering of a type-parameter value re-derives its concrete type on each call.

**Why this priority**: Generics should be the natural choice for reusable code. A noticeable penalty would push agents back towards copy-paste. This is a known follow-up from spec 002.

**Independent Test**: Time pairs of programs that differ only in using a generic helper or a type-specific one, with the work in a hot loop.

**Acceptance Scenarios**:

1. **Given** a generic and a type-specific version of the same hot loop, **When** both run, **Then** the generic version is at most 1.5 times slower.

---

### User Story 6 - Confidence that nothing in use is ever reclaimed (Priority: P2)

Reclaiming memory that is still in use would corrupt values silently: a wrong result printed instead of a crash. Maintainers can run the whole test corpus, the fuzzer and the benchmark solutions in a stress mode that reclaims memory as often as possible. Anything still in use that the runtime fails to account for then shows up immediately as different output or a structured internal error.

**Why this priority**: This is the safety net for the riskiest part of the spec. Without it a rare reclamation bug could ship and corrupt agents' results.

**Independent Test**: Run the full corpus in stress mode and compare outputs with normal mode. Seed deliberate faults (for example, forgetting to account for one kind of reference) and confirm stress mode catches each one.

**Acceptance Scenarios**:

1. **Given** the full test corpus, **When** run in stress mode, **Then** every output is byte-identical to normal mode.
2. **Given** a deliberately introduced failure to account for a reference held in any one of the places values can live, **When** the corpus is run in stress mode, **Then** at least one test fails.

---

### Edge Cases

- **Values held only during a builtin call.** A value that exists only while a builtin works, such as the partial result of `map`, `join` or `split` or a sort's scratch space, must survive reclamation in the middle of that call, including while a callback passed to the builtin runs.
- **Captured values.** Values captured by an anonymous function stay alive as long as the function value is reachable, even after the frame that created them returned.
- **Errors and results.** Values inside `err(...)`, a test failure's expected and actual values, and a runtime error's report data must stay valid until reported.
- **Values shared by several places.** A value reachable through several references (the same list stored in a record and in a map) stays one value: a change through one reference is seen through the others, as before.
- **Very deep or very wide structures.** Long lists, deep nesting and long chains of variants are reclaimed and kept correctly without exhausting the runtime's own stack.
- **Memory limit smaller than the program's minimum.** A tiny explicit limit fails early with the out-of-memory error, never with an internal error.
- **Nearly full memory.** A program whose live data stays just under the limit completes, perhaps more slowly, rather than failing.
- **The time limit during reclamation.** The timeout still fires during long reclamation work, and the report is still produced.
- **Tests.** Each test starts from the same memory state, so the order of tests and the memory used by earlier tests do not change any test's behavior or reports.

## Requirements *(mandatory)*

### Functional Requirements

**Reclamation**

- **FR-001**: The runtime MUST automatically reclaim memory used by values that the running program can no longer reach, with no change to the language, its syntax, its builtins or its diagnostics.
- **FR-002**: A value MUST never be reclaimed while it can still be reached from anywhere the program or the runtime can use it. That includes:
  - local variables and parameters of every active call, and values captured by reachable function values;
  - elements, fields and payloads of reachable lists, maps, records, variants and results;
  - values held temporarily by a builtin during its work, including while it calls back into vo code;
  - the type information that generic code needs;
  - test-runner state and the data of a runtime error or test failure being reported.
- **FR-003**: A program whose live data stays under the memory limit MUST NOT fail with an out-of-memory error, however much it allocates in total.
- **FR-004**: Reclamation MUST be deterministic. For the same program, input, arguments and options, reclamation happens at the same points, and memory reports and out-of-memory failures are identical on every run. It MUST NOT depend on wall-clock time.

**Behavior and limits**

- **FR-005**: Every program that completed before this spec MUST produce byte-identical stdout, stderr and exit status, and every test MUST produce the same results and reports.
- **FR-006**: The memory limit applies to live data, with a default of 512 MiB. Each `run` or `test` invocation MUST be able to set a different limit; it applies to the whole run, or to every test in it. When live data exceeds the limit, the program MUST stop with the structured out-of-memory runtime error, which reports the memory in use and the limit.
- **FR-007**: A run or test MUST be able to report its memory use on request: peak live memory, total allocated, and the number of reclamation passes, in human-readable and machine-readable form. Without the request, output MUST be unchanged.

**Time limit**

- **FR-008**: The run and test time limit MUST be enforced during all long-running work, including inside builtins (sorting, joining, splitting, comparing, map and list operations, higher-order builtins) and during reclamation, stopping the program within one second after the limit with the timeout runtime error.

**Generic code**

- **FR-009**: Operations on values of a type parameter (equality, ordering, rendering, sorting) MUST NOT repeat type-resolution work on every operation in a loop. A hot loop through generic code MUST run at most 1.5 times slower than the type-specific equivalent.

**Verification**

- **FR-010**: The toolchain MUST offer a stress mode, for tests and maintainers, that reclaims memory as often as possible without changing any program's output. Running a program in stress mode MUST either produce output identical to normal mode or reveal a reclamation fault.
- **FR-011**: A program that reaches memory it should not, which can only result from a reclamation fault, MUST stop with a structured internal error rather than print corrupted data, wherever such a fault can be detected.

**Documentation**

- **FR-012**: The language reference MUST stop warning that memory is never reclaimed, and MUST document the memory limit, how to change it, and how to request a memory report, within the 15,000-token budget.
- **FR-013**: The error catalog entry for out-of-memory MUST describe the new meaning (live data over the limit) and how to raise the limit or keep less alive.

### Key Entities

- **Live data**: the values the program can still reach. Its size is what the memory limit applies to.
- **Memory limit**: the maximum live data a run or test may hold. It has a default and can be set per run or test.
- **Reclamation pass**: one round of finding and reclaiming unreachable values. When passes happen is determined only by the program's own allocations.
- **Memory report**: peak live memory, total allocated and the number of passes, for one run or test, on request.
- **Stress mode**: a run mode that reclaims as often as possible, used to expose reclamation faults.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Five allocation-heavy workloads each complete, with peak live memory under 64 MiB, while allocating at least 1 GiB in total over their run:
  - building a string of at least 200,000 characters by repeated appending of short pieces (each append copies the whole string, so this alone allocates gigabytes);
  - creating and discarding 1,000,000 lists;
  - inserting and removing 1,000,000 map keys;
  - creating and calling 1,000,000 closures;
  - pushing and popping 1,000,000 items through a generic container.
- **SC-002**: 100% of existing outputs are byte-identical to their outputs before this spec: the golden test cases, the v1 migration corpus, the benchmark reference solutions and the reference examples. The full quality gate passes.
- **SC-003**: In stress mode, 100% of the golden test corpus and the benchmark reference solutions produce output identical to normal mode, and a fuzzing run of at least 3,000 iterations in stress mode has no crashes or unstructured outputs.
- **SC-004**: For each place a value can be held (at least eight kinds; tasks.md T030 maps them to fault kinds: locals, captured values, list elements, map entries, record fields, variant payloads, builtin temporaries, error or failure data), a deliberately introduced failure to account for it is detected by the stress-mode test suite.
- **SC-005**: A program whose live data grows without bound fails with the out-of-memory error at the same step in 20 of 20 runs, and the reported memory in use is within 10% of the limit.
- **SC-006**: Checking and running a 1,000-line program still takes under 1 second, and the existing performance programs run at most 20% slower than before this spec.
- **SC-007**: Each program in a set of at least five that spend their time inside a single builtin call stops with the timeout error within 1 second after a 2-second limit.
- **SC-008**: A hot loop through a generic helper runs at most 1.5 times slower than the type-specific version, for equality, ordering and container operations.
- **SC-009**: The reference stays within 15,000 tokens, contains no workaround for missing memory reclamation, and every example in it passes.

## Assumptions

- **Scope:** reclamation covers memory used by running programs. Checking a program keeps its current memory approach, because checking is short-lived and the 10,000-line stress input already passes.
- **Single-threaded:** vo has no concurrency, so reclamation never runs alongside program code.
- **No new language features:** no finalizers, weak references, manual memory control or memory introspection functions. Memory reports are a toolchain option, not something a program can read.
- **The default limit stays at 512 MiB of live data.** The option names for setting the limit, requesting a report and enabling stress mode are decided in planning and follow the existing CLI conventions.
- **Performance target:** the reference environment is unchanged (emulated RV64 on an x86 host). "At most 20% slower" is measured on the existing performance programs, which allocate little; allocation-heavy programs are expected to get faster, since they no longer grow without bound.
- **Benchmarks:** the agent benchmark does not need a new run for this spec. Its reference solutions must still produce identical output. A completed v2 benchmark run is tracked separately on the roadmap.

# Feature Specification: Agent-Friendly Testing

**Feature Branch**: `018-agent-testing`

**Created**: 2026-11-17

**Status**: Implemented

**Input**: User description: the proposal in `docs/agent_friendly_test_framework.md` ("Designing a
Language-Agnostic, Agent-Friendly Test Framework"), fitted to vo: "specify the development of the test
framework that you think fit this language".

Background: vo already has much of what the proposal asks for. Tests and property tests are part of the
language; `vo test --json` returns one structured record per test; failures carry their location, both
values of `expect_eq` and, for runtime errors, the frames with their local values; runs are
deterministic (fixed seed and clock per test); tests declare their effects; `@spec` links tests to
requirements; `vo impact` finds the tests a diff reaches; `--record`/`vo replay` reproduce a run.

The proposal's design target is the whole loop: discover, understand, create, validate, execute,
diagnose, improve, maintain. What vo still lacks for that loop:

- **Discovery**: no way to list the tests without running them, and the only selection is a substring
  of the name, so "run exactly this test" can run several.
- **Diagnosis**: `expect(a < b)` fails with no values; a failing `expect_eq` on a large list or record
  prints two long values and leaves the agent to find the difference; nothing says whether a failure is
  in the code, in the test, in the environment or unfinished work; a test's `print` output is written
  in between the JSON records.
- **Evidence that tests matter**: no coverage, nothing that warns about a test that checks nothing, and
  no way to tell whether the tests would notice a wrong change. This is the roadmap's problem 2: "the
  agent's tests share the agent's misunderstanding".
- **Reproducible tests that touch the outside**: a test that reads HTTP or runs a program depends on the
  network and the machine. This is a later spec (see below).

What this spec leaves out of the proposal, and why:

- **Language adapters and a language-independent API**: vo is one language. The CLI with `--json` is
  the agent interface (Principle II), so no separate API is built.
- **Retries and flaky-test detection**: runs are deterministic (Principle III), so a test that fails
  once fails every time. Outside inputs, the only source of variation, will be handled by recorded
  tests (a later spec).
- **Skip and expected-failure markers**: a skipped test hides a failure from the next agent. Unfinished
  work already has typed holes (`todo`), which report the test as `error` with the cause "unfinished".
- **Mock objects, spies and dependency injection**: vo has no interfaces or methods. Everything outside
  a program goes through effects, so replaying recorded effects (the later recorded-tests spec)
  replaces mocks of HTTP, files, processes, environment and clock.
- **Create, update, delete, rename and clone operations on tests**: tests are source code. Agents edit
  files, `vo check` validates them, and `vo edit rename` already renames.
- **Setup, teardown and fixtures as framework features**: there are no global variables, so a fixture
  is a function the test calls. Test data generation is `prop`.
- **Tags**: `@spec` IDs already group tests by the requirement they prove, and selection by ID is added
  (User Story 1). A second annotation would break "`@spec` is the only annotation".
- **Suggesting tests**: the agent writes tests. The toolchain gives it the evidence: code that no test
  ran and changes that no test caught.
- **Recorded tests** (the proposal's mocking, reproducibility and environment-variable management):
  moved to the roadmap as the next testing spec (clarified 2026-11-17). The decided mechanism is
  `vo test --record DIR` (one recording per test, named by its identifier) and `vo test --replay DIR`,
  in the existing recording schema, with no language change.
- **Isolating tests that write files**: tests share the real directory and run in source order, as today
  (clarified 2026-11-17).
- **Snapshots, schema and regular-expression assertions, historical results, run IDs and
  cancellation**: no current requirement needs them. `vo test` is one process that ends.

## Clarifications

### Session 2026-11-17

- Q: Is this one spec, or should part move to the roadmap? → A: Discovery, explained failures,
  coverage and test quality (mutation testing included) are this spec. Recorded tests become a later
  spec on the roadmap.
- Q: How will a test be tied to its recorded inputs (for the later spec)? → A: `vo test --record DIR`
  saves one recording per test, named by its identifier, and `vo test --replay DIR` runs on them. The
  language does not change.
- Q: Are tests that write files isolated from each other? → A: No, as today. Tests share the real
  directory and run in source order, so results stay deterministic. One test can see another's files;
  the reference says so.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–017: AI coding agents that write vo programs from the reference,
and the people who review the agents' changes.

### User Story 1 - Find and run exactly the right tests (Priority: P1)

An agent opens an unfamiliar project and asks what tests exist before writing new ones. It gets every
test and property: an identifier it can pass back, what kind it is, where it is, its documentation
comment, its effects and the requirements it names. Nothing runs and nothing outside is touched. The
agent then runs one test by identifier, the tests for one requirement, or the tests a change can
affect.

**Why this priority**: every other workflow starts with "which tests are there, and which ones matter
for this change". Today the agent must read every file and can only filter by a substring of the name.

**Independent Test**: golden cases over a multi-file program: the listing is identical on every run,
running a listed identifier runs exactly that test, and listing a file whose tests write files changes
nothing on disk.

**Acceptance Scenarios**:

1. **Given** a program whose files contain tests and properties, **When** the agent lists its tests,
   **Then** it gets one entry per test and property, in the order they would run. Each entry has a
   stable identifier, its kind (test or property), its file and span, its documentation comment (empty
   when it has none), its declared effects and its `@spec` IDs.
2. **Given** a test that declares `file.write("out/")` and writes a file, **When** the tests are listed,
   **Then** no test body runs and `out/` is unchanged.
3. **Given** a program that does not check, **When** the agent lists its tests, **Then** the result is
   the check diagnostics, exactly as `vo test` would report them, and no listing.
4. **Given** two tests named `parses` and `parses negative`, **When** the agent runs the test with the
   identifier of `parses`, **Then** only `parses` runs.
5. **Given** an identifier that names no test, **When** the agent runs it, **Then** it gets a usage error
   with a stable code, the closest existing identifiers and no test run.
6. **Given** tests annotated `@spec("FR-003")`, **When** the agent runs the tests for `FR-003`, **Then**
   exactly those tests run.
7. **Given** a unified diff that changes one function, **When** the agent runs the tests affected by the
   diff, **Then** exactly the tests and properties that `vo impact` reports for that diff run.

---

### User Story 2 - Understand a failure without reading the whole test (Priority: P1)

A test fails. From the report alone, the agent can tell what was compared and with which values, where
two large values first differ, whether the fault is likely in the code under test, in the test itself,
in the environment or in unfinished work, and what the test printed.

**Why this priority**: the failure report is what the agent acts on. A report that only says "check
failed" sends the agent back to reading code and guessing, which is the slowest part of its loop.

**Independent Test**: one golden case per cause and per kind of comparison, in text and JSON. A JSON
test run whose tests print text still parses line by line.

**Acceptance Scenarios**:

1. **Given** `expect(len(xs) < limit)` that fails with `len(xs)` 7 and `limit` 5, **When** the test is
   run, **Then** the report shows both operands with their source text and values ("`len(xs)` is 7,
   `limit` is 5"), for each of `==`, `!=`, `<`, `<=`, `>` and `>=`.
2. **Given** `expect(a and b)` or `expect(not (x == 3))` that fails, **When** the test is run, **Then**
   the report names the part of the condition that decided it and, when that part is a comparison, its
   operands as in scenario 1. A plain boolean such as `expect(not done)` keeps today's message, which
   names the whole condition (amended in analysis).
3. **Given** `expect_eq` on two lists of 1,000 records that differ in one field of element 412, **When**
   the test is run, **Then** the report gives the path of the first difference (`[412].name`) and the
   two values there, as well as both values shortened as today.
4. **Given** `expect_eq` on two strings longer than 40 characters that differ, or on values that differ
   only in length, **When** the test is run, **Then** the report gives the first differing character
   position or the first missing or extra element. Shorter strings are shown whole, as today (amended in
   analysis).
5. **Given** tests that fail in each of these ways, **When** they are run with `--json`, **Then** each
   non-passing record carries one machine-readable cause from a fixed documented list:
   - a check failed;
   - a runtime error in a function the test called, or in the test's own statements;
   - a `requires` broken (the caller is at fault, and the record says whether that caller is the test);
   - an `ensures` broken (the function is at fault);
   - a hole reached (unfinished);
   - an effect outside the test's grant;
   - a timeout;
   - running out of memory;
   - a property that could not find enough inputs.
6. **Given** a test that prints three lines and then fails, **When** the tests are run with `--json`,
   **Then** the three lines appear in that test's record and not as separate lines in the stream, and
   every line on standard output is a JSON record.
7. **Given** the same run without `--json`, **Then** the human report shows the failing test's printed
   lines under its failure. Passing tests' output is not shown, so a long suite's report stays short.

---

### User Story 3 - See what the tests never ran (Priority: P2)

Before saying a change is done, the agent asks which functions and branches of the program no test
exercised, and gets exact locations it can write tests for.

**Why this priority**: untested code is where an agent's mistakes go unnoticed. Coverage is the
cheapest evidence, and the first step before mutation testing (User Story 4).

**Independent Test**: golden cases over a fixture program with known untested functions, `else`
branches, `match` cases and loop bodies. The report lists exactly those, with spans, and is identical
on every run and every platform.

**Acceptance Scenarios**:

1. **Given** a program with one function that no test calls, **When** the agent runs the tests with
   coverage, **Then** that function is reported as never run, with its span.
2. **Given** an `if` whose `else` branch no test reaches, a `match` case no test takes and a loop whose
   body never runs, **When** coverage is reported, **Then** each is listed with its span and kind.
3. **Given** the same program and tests, **When** coverage is reported twice, on any supported platform,
   **Then** the report is byte-identical.
4. **Given** a program that imports the standard library and a package, **When** coverage is reported,
   **Then** only the program's own files are counted.
5. **Given** coverage with `--json`, **Then** the report ends with totals: functions, branches, and
   how many of each ran.

---

### User Story 4 - Find out whether the tests would catch a mistake (Priority: P2)

An agent has written code and tests that pass. It asks whether the tests would notice if the code were
slightly wrong. The toolchain makes small changes to the code, one at a time, and runs the tests that
reach it. It reports each change that no test notices: where it is, what was changed, and which tests
ran. Simple static warnings flag tests that cannot fail.

**Why this priority**: this is the roadmap's problem 2. Tests written by the same agent as the code
share its misunderstanding. Coverage only shows that code ran; a surviving change shows that its result
was never checked. Spec 005 used such changes only inside the benchmark, never as a tool agents can
run.

**Independent Test**: a fixture where a boundary check (`<` against `<=`) is not tested reports exactly
that change as surviving. After a test for the boundary is added, no change survives. Each static
warning has a case.

**Acceptance Scenarios**:

1. **Given** `fn is_adult(age: int) -> bool { return age >= 18; }` and tests only for ages 10 and 30,
   **When** the agent runs mutation testing, **Then** the change of `>=` to `>` at that span is reported
   as not caught, with the original and changed source text, the function and the tests that ran.
2. **Given** the same code with an added test for age 18, **When** mutation testing runs again, **Then**
   that change is reported as caught and the summary shows zero surviving changes for `is_adult`.
3. **Given** a change that makes a test loop forever or fail at run time, **When** mutation testing
   runs, **Then** the change counts as caught (the timeout or error is the test noticing), and the run
   still ends within its time limit.
4. **Given** the same program, tests and seed, **When** mutation testing runs twice on any platform,
   **Then** the reports are byte-identical: the same changes, in the same order, with the same results.
5. **Given** a diff, **When** mutation testing is restricted to it, **Then** only functions the diff
   touches are changed.
6. **Given** a test with no `expect`, `expect_eq` or call to a function that contains one, **When** the
   program is checked, **Then** a warning says the test checks nothing.
7. **Given** a check whose outcome cannot depend on the code under test (`expect(true)`,
   `expect_eq(3, 3)`, or both sides literals), **When** the program is checked, **Then** a warning says
   the check always has the same outcome.
8. **Given** surviving changes, **Then** the exit status stays 0 when all tests pass. Survivors are
   evidence, not failures, like warnings.

---

### Edge Cases

- A file with no tests: the listing is empty, and coverage reports every function as never run.
- Two files that each define a test named `parses`: their identifiers differ because the file is part
  of the identifier.
- Selecting by `@spec` ID and by diff together: the selection is the intersection. An empty selection
  is a passing run that reports "0 tests selected", not an error.
- `expect` on a comparison whose operands are large values: the operand values are shortened like other
  values (200 bytes). The path to the first difference is `expect_eq`'s report, not `expect`'s (amended
  in analysis).
- `expect_eq` on values with map or ordered-map contents: paths name keys (`["key"]`). Floats that
  differ only in sign of zero, or are NaN, follow the existing equality rules.
- A test that prints more than a limit: the captured output is cut at the limit with a note on how much
  was left out, so one test cannot flood the report.
- Output written by tasks (`concurrent_map`) inside a test: it is captured too, in input order, as `run`
  shows it.
- A property's printed output across 100 cases: only the failing case's output (after shrinking) is kept.
- Coverage of code reached only through a function value or `map`: it counts as run.
- Coverage of a function that has a hole: reported like any other function.
- Mutation testing of a function with contracts: a change that breaks a contract on its own code counts
  as caught.
- A change that does not type-check or cannot be shown as source text is never produced. Only changes
  that keep the program valid are made.
- A change that produces the same behavior as the original (an "equivalent mutant") cannot always be
  told apart. It is reported as surviving, and the report says survivors are candidates, not proof.
- Mutation testing on a program whose tests already fail: it refuses and reports the failing tests,
  since changes cannot be judged against a failing baseline.
- Mutation testing on a large program: the number of changes is capped with a stated limit and the
  report says how many were not tried. Order and selection stay deterministic.
- A test that writes a file another test reads: the second test sees it. Tests run in source order, so
  the result is the same on every run; listing shows that order.

## Requirements *(mandatory)*

### Functional Requirements

**Discovery and selection**

- **FR-001**: `vo test` MUST be able to list every test and property of the given paths without running
  any test body and without performing any effect. Each entry has an identifier, kind, file, span,
  documentation comment, declared effects and `@spec` IDs, in run order, in text and as JSON records
  following the versioned schema.
- **FR-002**: A test's identifier MUST be stable: derived from its file path and its name only, so it
  does not change when other tests are added, moved or edited, and unique within one run.
- **FR-003**: `vo test` MUST be able to run only the tests with given identifiers (one or more), only the
  tests naming a given `@spec` ID, and only the tests and properties that a unified diff affects
  (`vo impact`'s answer). The selections combine as an intersection. The existing `--filter` keeps its
  meaning.
- **FR-004**: An identifier or `@spec` ID that matches nothing MUST be a toolchain error with a stable
  code, listing the closest existing identifiers. A diff that affects no tests is a passing run with
  zero selected.

**Explained failures**

- **FR-005**: A failing `expect` whose condition is a comparison MUST report each operand's source text
  and value. For `and`, `or` and `not`, it reports the part that decided the outcome, applying the same
  rule to that part. For `or`, both parts were false and the report names both, without values
  (amended in planning, research R5).
- **FR-006**: A failing `expect_eq` MUST report the path to the first difference: list index, record
  field, variant case, map key, string character position or length. The values at that path are
  reported as well as the shortened whole values.
- **FR-007**: Every test record that is not `pass` MUST carry a cause from a fixed, documented list
  covering at least: check failed, runtime error in called code, runtime error in the test's own
  statements, `requires` broken (with whether the caller is the test), `ensures` broken, hole reached,
  effect outside the grant, timeout, out of memory, and too few property inputs. A new
  cause MAY be added within the schema version; existing ones are never renamed.
- **FR-008**: Everything a test writes to standard output MUST be captured per test. With `--json` it is
  part of that test's record, and every line `vo test --json` writes to standard output is a JSON
  record. In text mode, the output of a test that does not pass is shown under its report. Captured
  output has a documented size limit per test.

**Coverage**

- **FR-009**: `vo test` MUST be able to report coverage of the program's own files: every function never
  called, and every `if`/`else` branch, `match` case and loop body never entered by the selected tests,
  each with its span and kind, followed by totals. The standard library and packages are excluded.
- **FR-010**: The coverage report MUST be byte-identical across runs and supported platforms for the same
  program, tests and options, and MUST NOT change any test's result.

**Test quality**

- **FR-011**: The checker MUST warn when a test or property contains no check: no `expect` or
  `expect_eq` (functions cannot contain one, E0410; amended in planning, research R9). It MUST also warn when a
  check's outcome is fixed regardless of the program: both sides literals, or a literal condition. The
  warnings get new catalogued codes and never change an exit status.
- **FR-012**: `vo test` MUST offer mutation testing. It makes small, type-preserving changes to the
  functions that the selected tests reach, one change at a time:
  - comparison operators;
  - arithmetic operators;
  - boolean literals and `not`;
  - integer literals off by one;
  - `and`/`or` swapped.
  For each change it runs the selected tests that reach the changed function, and reports every change
  that no test catches, with its span, original text, changed text, function and the tests that ran.
  The JSON record follows the schema; a summary gives the changes made, caught, surviving and not tried.
- **FR-013**: In mutation testing, a test that fails, errors or times out under a change counts as
  catching it. The time limit per changed run MUST be derived from the baseline run, so an infinite loop
  cannot stall the whole run.
- **FR-014**: Mutation testing MUST be deterministic: the same changes in the same order with the same
  results on every run and platform. It MUST be restrictable to the functions a diff touches, and it MUST
  cap the number of changes with a documented default that the user can change.
- **FR-015**: Mutation testing MUST refuse to start, with a stable code, when the selected tests do not
  all pass without changes. Surviving changes MUST NOT change the exit status. Mutation testing MUST
  never write the changed code to disk.

**Isolation**

- **FR-016**: Tests MUST keep sharing the real working directory and running in source order (clarified
  2026-11-17). The reference MUST state that one test can see files another wrote, and that the run
  order is the listing order (FR-001).

**Cross-cutting**

- **FR-017**: Every new report MUST be available as JSON records within schema version 1, adding fields
  only; existing fields and existing test records keep their meaning. The text report MUST show what a
  person needs to act (listing, operands, first difference, output, coverage gaps, surviving changes);
  the cause is JSON only (amended in analysis: the text already shows the code and location it is
  derived from).
- **FR-018**: The language reference MUST grow by at most 700 tokens for this spec. CLI options and
  JSON fields are documented there. New codes go to the error catalog with an example each and a
  golden case each.
- **FR-019**: Every existing golden and step case MUST pass unchanged, except where captured test
  output (FR-008) or the identifier added to every JSON test record (FR-002) changes what a case
  expects. Each such case is listed in the results.
- **FR-020**: All new behavior MUST be byte-identical across x86-64, ARM64 and RISC-V and from run to
  run, including listing order, coverage and mutation reports.

### Key Entities

- **Test entry**: identifier, kind (test, property), file, span, documentation comment, declared effects,
  `@spec` IDs. Produced by listing, and the key for selection, coverage and mutation reports.
- **Test result** (the existing `test` record, extended): status, cause, failure with the comparison
  operands or the difference path, captured output.
- **Coverage report**: never-run functions and never-entered branches, each with span and kind; totals.
- **Mutant**: function, span, original text, changed text, result (caught by which test, or surviving).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For a program with 200 tests and properties in 20 files, listing takes under 1 second.
  Running any listed identifier runs exactly that one test, for every test in the program (checked
  automatically for all 200).
- **SC-002**: Every cause in the documented list has a golden case. 100% of non-passing test records in
  the whole golden suite carry a cause, and every line `vo test --json` writes to standard output across
  the suite parses as JSON.
- **SC-003**: For each comparison operator and each kind of difference (index, field, case, key,
  character, length), a golden case shows the operands or the path. On two 10,000-element lists that
  differ in one element, the reported path is that element's.
- **SC-004**: Coverage on a fixture with known gaps lists exactly those gaps: no missing and no extra
  entries. A suite runs at most 1.5 times slower with coverage than without.
- **SC-005**: On a fixture set of programs, each with a known boundary and tests that miss it, mutation
  testing reports that boundary's change as surviving for every program, and as caught once the missing
  test is added (amended in planning, research R14: the bench2 reference solutions keep their logic in
  `main` and have no vo tests). For each program in `examples/`, the run takes under 60
  seconds with the default cap.
- **SC-006**: The reference grows by at most 700 tokens and stays under 15,000. The full gate passes on
  x86-64, and `make test` passes on ARM64 and RISC-V.

## Assumptions

- The agent interface is the `vo` command line with `--json`. No server, daemon or separate API is
  added.
- "Documentation comment" is the comment right above an item, which `vo fmt` already treats as its
  documentation. Tests gain no new metadata syntax: the name, that comment, the effects and `@spec` IDs
  are the metadata.
- Coverage counts source branches (`if`/`else` arms, `match` cases, loop bodies) and functions. It does
  not count `and`/`or` short-circuits, lines or bytecode.
- Changes in mutation testing are made to the compiled program in memory, not to files. Only the program's
  own files are changed, never the standard library or packages.
- The default cap for mutation testing and the captured-output limit are set during planning and
  documented in the reference.
- Property tests take part in selection, causes, coverage and mutation testing like ordinary tests.
- The text report keeps its current form for passing tests. Only failing tests gain the new detail and
  their captured output.

# Feature Specification: Spec Annotations on Tests

**Feature Branch**: `012-spec-annotations`

**Created**: 2026-11-16

**Status**: Draft

**Input**: User description: "Annotations for spec references: a closed set of annotations written before an item, starting with one, `@spec("FR-003", "US1/AC2")`, on `test` and `prop` items, linking them to requirement IDs of a spec. The checker knows every annotation: an unknown one is an error with a fix to the closest name; arguments are validated by shape (an ID is a word with optional `/` parts). `vo fmt` places annotations on their own line(s) before the item, IDs sorted. `vo test --json` reports each test's spec IDs with its result; a new query lists every referenced ID with its tests and their results (text and JSON), so tools like Spec Kit's converge can find uncovered requirements. The language does not read spec files. The reference, catalog and the IntelliJ plugin cover the syntax."

Background: projects built with vo are specified in Markdown (functional requirements `FR-001`, success
criteria `SC-001`, acceptance scenarios `US1/AC2`, as in this repository's own `specs/`). Today nothing
connects a requirement to the tests that prove it: the link lives in test names, comments or nowhere, so
an agent finishing a feature, or a review, cannot tell which requirements are covered without reading
every test. The user chose annotations (over a header clause) as the form, with a closed set.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–011: AI coding agents that write vo programs from the reference
and fix them from diagnostics, and the people who review and run those programs. A third user here is
tooling (Spec Kit's converge step, CI scripts) that reads machine output.

### User Story 1 - A test says which requirements it proves (Priority: P1)

An agent implements FR-002 of a spec and writes the test for it. It writes `@spec("FR-002", "US1/AC1")`
on the line before `test "..."`. The checker validates the annotation; the test runs as before; its
report now carries the IDs.

**Why this priority**: the annotation itself; everything else reads it.

**Independent Test**: golden cases with annotated tests and properties (checks and runs unchanged),
and with each kind of mistake (unknown annotation, malformed ID, wrong place, no IDs, repeated).

**Acceptance Scenarios**:

1. **Given** `@spec("FR-002")` on the line before a `test`, **When** checked and run, **Then** it checks
   with no diagnostic and the test runs exactly as without the annotation.
2. **Given** `@spec("FR-002", "US1/AC1", "SC-003")` before a `prop`, **When** checked, **Then** it is
   accepted; IDs may be listed in any order.
3. **Given** `@spec("FR 2")`, `@spec("")` or `@spec()`, **When** checked, **Then** each is a check
   error that names the rule (an ID is a word of letters, digits, `-`, `_`, optionally followed by
   `/`-separated parts of the same).
4. **Given** `@spce("FR-002")` (a misspelled annotation), **When** checked, **Then** it is an error with
   a fix replacing it with `@spec`.
5. **Given** `@spec("FR-002")` before a `fn`, `record`, `variant` or `import`, **When** checked, **Then**
   it is an error saying `@spec` goes on a `test` or `prop` (clarified: tests and properties only).

---

### User Story 2 - Test reports carry the requirement IDs (Priority: P1)

CI runs `vo test --json` and gets, for each test, its status and its spec IDs. A script can fail the
build when a test of a given requirement fails, or summarize results per requirement.

**Why this priority**: the first machine use of the annotation; small once US1 exists.

**Independent Test**: golden `vo test --json` outputs of annotated and unannotated tests.

**Acceptance Scenarios**:

1. **Given** an annotated test, **When** `vo test --json` runs, **Then** its `test` record has a `spec`
   field listing its IDs, sorted; a test without the annotation has none (existing outputs unchanged).
2. **Given** the human-readable `vo test` output, **When** an annotated test fails, **Then** the failure
   line names its IDs, so a reader sees which requirement broke.

---

### User Story 3 - Which requirements are covered, and how they stand (Priority: P1)

Before declaring a feature done, an agent (or converge) asks the toolchain which requirement IDs the
program's tests reference, by which tests, and whether those pass. It compares that with the IDs in the
spec and finds FR-007 with no test.

**Why this priority**: the reason for the feature: coverage of requirements by tests becomes a fact the
toolchain reports instead of a guess.

**Independent Test**: golden outputs of the new query on a program with several annotated tests, in
text and JSON, including an ID referenced by two tests; and of `vo test` on it, with one of those tests
failing.

**Acceptance Scenarios**:

1. **Given** a program whose tests reference FR-001 (two tests), FR-002 and US1/AC1, **When** the query
   runs, **Then** it lists each ID once, in sorted order, with its tests (name, file, line).
2. **Given** the same program, **When** `vo test` runs it, **Then** after the usual report each ID gets a
   summary (how many of its tests ran and how many passed), in text and as a JSON record. The query
   itself never runs tests, like every other query: references come from the query, results from
   `vo test`.
3. **Given** `--json`, **When** the query runs, **Then** each ID is one record with its tests, following
   the versioned schema of the other reports.
4. **Given** a program with no annotations, **When** the query runs, **Then** it reports no IDs, exit 0.

---

### User Story 4 - Formatting and editors (Priority: P2)

`vo fmt` gives annotations one canonical form; the IntelliJ plugin highlights them.

**Why this priority**: consistency and readability; nothing depends on it.

**Independent Test**: formatter golden cases; a plugin lexer test.

**Acceptance Scenarios**:

1. **Given** `@spec( "US1/AC1","FR-002" )` written on the same line as `test`, **When** formatted,
   **Then** it becomes `@spec("FR-002", "US1/AC1")` on its own line directly above the test, IDs sorted,
   duplicates kept for the checker to report.
2. **Given** a doc comment and an annotation, **When** formatted, **Then** the comment stays above the
   annotation.
3. **Given** a `.vo` file in IntelliJ, **When** it shows `@spec(...)`, **Then** the annotation name is
   highlighted as an annotation.

---

### Edge Cases

- **Repeated IDs** in one annotation: a warning with a fix removing the repeat.
- **Two `@spec` annotations on one item**: an error with a fix merging them into one.
- **An annotation not followed by an item** (end of file, a blank line then an item is fine; another
  annotation then an item is fine): an error at the annotation.
- **Inside a block**: annotations are top-level only; `@spec` inside a function body is a syntax error.
- **Annotations and test filters**: `vo test --filter` matches test names only, as today.
- **Imported files**: the query covers every file the entry program loads, like the other queries.
- **The standard library and packages** contain no annotated tests; their tests are not part of the
  query.

## Requirements *(mandatory)*

### Functional Requirements

**Annotations**

- **FR-001**: The language MUST support annotations written as `@NAME(ARGS)` on their own line before a
  top-level item, from a closed set known to the checker; this spec defines one: `spec`.
- **FR-002**: `@spec` MUST take one or more string IDs; an ID is letters, digits, `-` and `_`, optionally
  followed by `/`-separated parts of the same; anything else, an empty list, an unknown annotation name
  (with a fix to the closest known name), a repeated annotation on one item (with a merging fix) or an
  annotation in a place it does not belong MUST be a check error naming the rule. Repeated IDs MUST be a
  warning with a fix.
- **FR-003**: `@spec` MUST be allowed on `test` and `prop` items only; on any other item it MUST be a
  check error saying where it belongs.
- **FR-004**: Annotations MUST NOT change how a program checks or runs, except for their own diagnostics.

**Reports and queries**

- **FR-005**: `vo test --json` MUST report an annotated test's IDs (sorted) in its record; the human
  report MUST name them for a test that does not pass; when any test is annotated, `vo test` MUST end with
  a per-ID summary (tests run, tests passed), in text and in JSON. Unannotated programs' reports MUST stay
  unchanged.
- **FR-006**: A new query MUST list every ID referenced by the program's tests and properties, each with
  its tests (name, file, line), in text and in JSON, without running anything.
- **FR-007**: The toolchain MUST NOT read specification documents; matching IDs against a spec is left to
  other tools.

**Formatting, documentation, editors**

- **FR-008**: `vo fmt` MUST print each annotation on its own line directly above its item (below any doc
  comment), as `@spec("A", "B")` with IDs sorted.
- **FR-009**: The reference MUST document annotations and `@spec` in at most 200 tokens; every new code
  MUST have a catalog entry and a golden case; the IntelliJ plugin MUST highlight annotations.
- **FR-010**: Programs without annotations MUST behave byte-identically to the previous release; the
  toolchain MUST keep passing its full quality gate on x86-64, ARM64 and RISC-V.

### Key Entities

- **Annotation**: a name from the closed set and its arguments, attached to the item below it.
- **Spec ID**: a requirement identifier such as `FR-003`, `SC-002` or `US1/AC2`; text the toolchain
  validates by shape only.
- **Coverage entry**: one ID with the tests that reference it and, optionally, their results.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In this repository's examples, every test can be annotated with the requirement it proves
  in under one line each, and the query then lists 100% of those IDs with the right tests.
- **SC-002**: A tool that knows a spec's IDs finds every requirement without a test from one query run
  (demonstrated on a spec with at least 10 requirements, 2 deliberately untested).
- **SC-003**: 100% of the mistake cases (unknown annotation, bad ID, empty list, wrong place, repeats) are
  reported in one check, each with a fix where exactly one correction is unambiguous.
- **SC-004**: An agent given only the reference annotates the tests of a small program with the IDs of a
  given requirement list correctly on its first try (isolated runs, at least 3).
- **SC-005**: The reference grows by at most 200 tokens and stays under 15,000; the full gate passes on
  all three platforms, and unannotated programs give byte-identical results.

## Assumptions

- **Annotations, not a clause** (user's decision): `@NAME(...)` above the item, a closed set; the first
  annotation is `@spec`. Others (for example deprecation) need their own spec.
- **Tests and properties only (clarified 2026-11-16)**: `@spec` marks what proves a requirement;
  marking implementing functions can be added later if it proves useful.
- **Results come from `vo test`, references from the query** (decided in planning, research R3): every
  `vo query` is static; running tests stays the job of `vo test`.
- **IDs are opaque**: the toolchain checks their shape, not that they exist in a spec, and gives them no
  meaning (FR vs SC vs US are all just IDs).
- **The first-token rule** (constitution Principle I) still holds: an item starts with `@`, `fn`,
  `record`, `variant`, `test`, `prop`, `import` or `export`.
- **The previous release is the oracle** for "behaves exactly as before".

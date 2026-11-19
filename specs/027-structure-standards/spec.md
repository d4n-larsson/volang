# Feature Specification: Application Structure Standards

**Feature Branch**: `027-structure-standards`

**Created**: 2026-11-19

**Status**: Implemented (clarified 2026-11-19: accepted warnings are annotated with a reason)

**Input**: User description: "Application structure standards: vo recommends, and checks, how to
structure a program, following DRY and other generally accepted principles, adapted to vo's effects.
Delivered as documentation (a short reference section within the token budget and a site guide
"Structuring a program" with a worked example) plus a few structure rules checked by `vo check` as
warnings, on by default, each with a fix or a concrete instruction where possible. Principles: a pure
core with effects at the edges (functional core, imperative shell: main and other effectful functions
stay thin, decisions and computation live in pure functions that tests can call without effects); DRY
(duplicated code is reported); one concept per module, with small files and functions; tests next to
the code they test; effect-generic code instead of copies per effect set. Motivation: in the voledger
experiment an agent put a whole 1,000+ line program in one main.vo; agents act on diagnostics far more
reliably than on prose. The rules must be measurable and deterministic, have few false positives on the
existing examples, std library and golden cases, and be explainable in one sentence each."

Background: vo tells an agent precisely what is wrong with a program, but nothing about how a program
should be organized. In the first voledger experiment an agent wrote a correct, well-tested ledger tool
as one 739-line `main.vo` file: parsing, money arithmetic, database code, reports and the command line
side by side. It passed every acceptance check, and it is the program a reviewer least wants to read or
change. Agents follow diagnostics in their check-fix loop; they rarely act on style advice they were not
shown at the moment it mattered.

vo is in an unusual position to give that advice: effects are part of every function's signature, so
the most valuable structural rule in the literature, keeping decisions in pure code and effects at the
edges ("functional core, imperative shell"), can be checked rather than only recommended. Pure code is
the code a test can call with no setup, no files and no grant.

The rules are few, measurable, and each fits in one sentence. They are warnings: a program with them
checks, runs and tests as before.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - An effectful function that hides pure logic is pointed out (Priority: P1)

An agent writes `main` (or any function with effects) and puts argument parsing, validation,
calculations and formatting in it next to the reads and prints. `vo check` warns that the function holds
many statements that use no effect, points at the longest stretches of them, and says to move that logic
into pure functions so it can be tested without effects.

**Why this priority**: it is the rule only vo can check, and the one that most improves testability: the
moved code becomes callable from a test that declares nothing.

**Independent Test**: a program whose `main` contains many effect-free statements gets the warning at
`main`; after the logic is moved into pure functions called from `main`, the warning is gone and the new
functions can be tested with no `uses` clause.

**Acceptance Scenarios**:

1. **Given** a function with effects in which at least the threshold number of statements (at any depth)
   perform no effect, **When** the program is checked, **Then** a warning at the function's name says how
   many of its statements use no effect, points at the longest stretches of them as related locations,
   and tells the agent to move that logic into pure functions.
2. **Given** a function with effects with fewer effect-free statements than the threshold (a thin shell
   calling pure functions), **When** checked, **Then** no warning.
3. **Given** a pure function of any length, **When** checked, **Then** this rule never warns about it.
4. **Given** a test or property, **When** checked, **Then** this rule does not apply to it.

---

### User Story 2 - Files and functions that grow too large are pointed out (Priority: P1)

An agent keeps adding to one file or one function. `vo check` warns when a file passes the size limit,
saying to split it by concept into modules, and when a function passes its limit, saying to split it into
smaller functions.

**Why this priority**: it is the direct cause of the voledger result, and the cheapest rule to understand
and act on.

**Independent Test**: a file just over the limit gets one warning at its first line; at the limit, none.
A function just over its statement limit gets one warning at its name; at the limit, none.

**Acceptance Scenarios**:

1. **Given** a file whose code lines (not blank, not only a comment) exceed the file limit, **When**
   checked, **Then** one warning at line 1 gives the count, the limit, and the instruction to split the
   file into modules by concept (naming the largest groups of related functions as candidates, when
   there are any).
2. **Given** a function or test whose statements (counted at every nesting depth, anonymous functions
   included) exceed the function limit, **When** checked, **Then** one warning at its name gives the count
   and the limit.
3. **Given** the voledger program as the agent wrote it, **When** checked, **Then** it gets at least the
   file warning and the effectful-logic warning for its command dispatch.

---

### User Story 3 - Duplicated code is reported, with the right remedy (Priority: P2)

An agent copies a block of code and changes a name or a literal, or writes the same function twice for
two kinds of database. `vo check` reports the second copy, points at the first, and says to make one
function, or, when the copies differ only in their effects, one function with an effect parameter.

**Why this priority**: DRY violations are the most common structural debt in generated code, but
detection must be conservative to avoid noise, so it follows the two simpler rules.

**Independent Test**: two functions whose bodies differ only in local names and literal values produce
one warning on the later one with a related location at the earlier one; two functions that differ only
in effect clauses and effect arguments produce the effect-parameter advice.

**Acceptance Scenarios**:

1. **Given** two runs of statements, each at least as long as the duplicate threshold, that are the same
   except for the names of local variables and parameters and the values of literals, **When** checked,
   **Then** the one that comes later (by file, then position) gets a warning that names the earlier one
   as a related location and says to make one function with the differences as parameters.
2. **Given** two functions identical except for their `uses` clauses and the effect arguments in their
   types, **When** checked, **Then** the warning says to make one function generic over its effects
   (`[E: effects]`).
3. **Given** runs shorter than the threshold, or runs that differ in a call target, an operator or a field
   name, **When** checked, **Then** no warning.
4. **Given** repeated code inside tests and properties, **When** checked, **Then** no warning (tests may
   repeat themselves for readability).
5. **Given** three or more copies, **When** checked, **Then** each copy after the first is reported once,
   all pointing at the first.

---

### User Story 4 - Tests live next to the code they test (Priority: P3)

An agent puts every test in `main.vo` while the code they test lives in other files. `vo check` warns on
a test whose program calls all go to functions of one other file, saying to move the test into that file.

**Why this priority**: useful once programs have several files (which User Story 2 brings about), and
less important than the rules above.

**Independent Test**: a test in `main.vo` that calls only functions of `money.vo` gets the warning;
the same test moved into `money.vo` does not, and `vo test main.vo` still runs it; a test calling
functions of two other files does not.

**Acceptance Scenarios**:

1. **Given** a test or property whose calls to the program's own functions all go to functions declared
   in one other file, **When** checked, **Then** a warning at the test says to move it into that file.
2. **Given** a test that calls functions of its own file, or of two or more other files, or no program
   functions at all, **When** checked, **Then** no warning.
3. **Given** `main.vo` importing `money.vo`, both with tests, **When** `vo test main.vo` runs, **Then** the
   tests of both files run, each reported with its own file in its identifier.

---

### User Story 5 - The standard is written down where agents look (Priority: P1)

An agent (or person) reads the reference or the site guide and finds how vo programs are structured: a
thin effectful shell, a pure core in modules by concept, tests next to the code, one function instead of
copies, and why. A worked example shows a program before and after, and the after version is clean
under every rule.

**Why this priority**: the warnings say what to change; the documentation says what to aim for. Both are
needed for the agent's first draft, not only its second.

**Independent Test**: the reference section and the guide exist, the guide's example runs and checks
with no structure warnings, and the reference stays within its token budget.

**Acceptance Scenarios**:

1. **Given** the reference, **When** an agent reads it, **Then** a short section lists the structure rules,
   one sentence each, with their warning codes.
2. **Given** the site guide "Structuring a program", **When** read, **Then** it explains each principle
   with a before-and-after example, and the after version checks with no warnings and runs.
3. **Given** `vo explain` on any of the new warning codes, **When** run, **Then** it explains the rule and
   shows a minimal example and its fixed version, as for every other code.

---

### Edge Cases

- A command dispatcher (a long `match` whose arms print or write): each arm's effectful statements do not
  count; only the statements that compute do, so a dispatcher that delegates stays under the threshold.
- Creating an anonymous function is effect-free; calling one with effects is not. The statements inside
  an anonymous function are counted like any others.
- Calls to effect-generic functions count as effectful when the effects bound at that call are not empty.
- Holes (`todo(...)`) are effect-free statements.
- Generated or embedded code (the standard library and packages) is never reported: the rules apply to the
  program's own files, as `vo.effects` does.
- A file made mostly of tests: its tests count toward the file size like any code.
- Duplicates across files are reported like duplicates within one file.
- Identical one-line or very short functions (getters, wrappers) are below the duplicate threshold.
- A record or variant declaration repeated with different names is not code duplication (declarations
  are not statements).
- The same program checked on any platform, or twice, gives the same warnings in the same order.
- A program with errors gets no structure warnings until the errors are fixed, so the agent's attention
  stays on what blocks the program.
- An `@allow` with an empty reason, a code that is not a structure code, or a code the declaration does
  not trigger: the first two are errors in the annotation, the last is the unused-annotation warning.
- `vo test main.vo` in a program whose other files have no tests runs exactly the tests it ran before.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `vo check` MUST report each structure rule as a warning with its own stable code, in the
  same format as other diagnostics (text and JSON), with a one-sentence message and a concrete instruction.
  Warnings MUST NOT change whether a program checks, runs or tests, or the exit status of `vo run` and
  `vo test`.
- **FR-002 (effectful logic)**: A function with effects (not a test or property) in which at least R
  statements, counted at every depth, perform no effect MUST be reported at its name, with the longest
  stretches of effect-free statements as related locations and the advice to move that logic into pure
  functions. (Counting rather than requiring consecutive statements: research R3.)
- **FR-003 (file size)**: A program file with more than F code lines MUST be reported once, at line 1,
  with the count, the limit, and the advice to split it into modules by concept.
- **FR-004 (function size)**: A function, test or property with more than S statements (all nesting
  depths, anonymous functions included) MUST be reported once, at its name, with the count and the limit.
- **FR-005 (duplication)**: Two runs of at least D consecutive statements, in non-test code and in different
  blocks, that are equal after renaming local names and replacing literal values MUST be reported on the
  later run with the earlier one as a related location (a block repeating a pattern is not a copy). When the two are whole functions that differ only in effect clauses
  and effect arguments, the advice MUST be an effect parameter; otherwise one function with the
  differences as parameters.
- **FR-006 (test placement)**: A test or property whose calls to the program's own functions all go to one
  other file MUST be reported with the advice to move it into that file.
- **FR-007**: The rules MUST apply only to the program's own files (not the standard library or packages),
  and only when the program has no errors.
- **FR-008**: R, F, S and D MUST be fixed numbers documented in the reference and the catalog, the same for
  every program (no configuration), chosen so that FR-012's corpus has no warnings.
- **FR-009**: Results MUST be deterministic: the same warnings, in the same order, on every run and every
  platform.
- **FR-010**: Each new code MUST have a catalog entry (with example and fixed version) and `vo explain`
  text; the reference MUST gain a structure section of at most 250 tokens; the site MUST gain a guide page
  "Structuring a program" with a runnable before-and-after example.
- **FR-011**: Where the remedy is a mechanical edit that the toolchain can perform safely, the warning MUST
  carry it as a fix; otherwise its message MUST say exactly what to move where.
- **FR-012**: The repository's examples and the site's and reference's runnable examples MUST have no
  structure warnings after this feature, and the standard library none that is not accepted with a
  reasoned `@allow` (changing an example to follow the standard is allowed; raising a limit to hide a
  real problem is not).
- **FR-013**: A program MUST be able to accept a structure warning it deliberately keeps with an annotation
  on the line right above the declaration, `@allow("W07NN", "reason")`, written like `@spec`. The reason
  MUST be a non-empty text. The annotation silences only that code, only for that declaration (a
  function, test or property; for the file-size rule, the first function, test or property of the file). `vo check`
  MUST count the accepted warnings in its summary, and an `@allow` that silences nothing MUST itself be a
  warning, with the fix that removes it. Only structure codes (W07xx) can be allowed.
- **FR-014**: `vo test ENTRY` MUST run the tests and properties of every program file the entry loads
  (the entry and the files it imports, not the standard library or packages), so tests can live next to
  the code they test and still run with the usual command. Test identifiers (`FILE::NAME`) already name
  the file; selection options work as today.

### Key Entities

- **Structure rule**: one measurable property of a program, with a warning code, a threshold, a
  one-sentence statement, an instruction, and (where safe) a fix.
- **Accepted warning**: an `@allow("CODE", "reason")` annotation on a declaration, silencing one structure
  code there, counted in the check summary.
- **Effect-free statement**: a statement of an effectful function whose own expressions require no effect.
- **Duplicate**: a run of statements equal to an earlier run up to local names and literal values.
- **Threshold**: the fixed numbers R (effect-free statements), F (file code lines), S (function statements)
  and D (duplicate statements).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The voledger program as first written (one 739-line file) gets structure warnings, and a
  version restructured by following their instructions has none and still passes its acceptance check.
- **SC-002**: Every example program and every runnable documentation example have zero structure warnings,
  and the standard library none that is not accepted with a reasoned `@allow`; of the repository's golden test programs, at most 5% need their expected
  output updated because of a new warning.
- **SC-003**: On a set of seeded violations (at least 5 per rule), every violation is reported; on a set of
  at least 10 well-structured programs, no rule reports anything.
- **SC-004**: Checking the largest example program and a 5,000-line generated program takes at most 10%
  longer than before.
- **SC-005**: The reference grows by at most 250 tokens and stays within its 15,000-token budget.
- **SC-006**: Every new warning's message is one sentence plus an instruction, and every new code has a
  catalog entry and `vo explain` text that the catalog checks accept.
- **SC-007**: In a rerun of the ledger or sitegen experiment with this feature, the agent's final program
  has no structure warnings and no `@allow`, spread over more than one file, with tests next to the code
  they test.

## Assumptions

- Thresholds, measured in planning (research R1-R5): R = 15 effect-free statements, F = 400 code lines,
  S = 50 statements, D = 6 statements. Limits are deliberately loose: the goal is to catch the clearly
  unstructured program, not to grade style.
- "One concept per module" has no direct measure; the file-size rule and the advice in the guide carry
  it.
- An extract-function edit (`vo edit extract`) that would make the effectful-logic and duplication fixes
  mechanical is a separate, later feature; until then those warnings give instructions (FR-011).
- Naming conventions, comment style and layout are already handled by `vo fmt` and are not part of this
  feature.
- The warnings use a new range of codes (W07xx).
- Both experiment agents kept everything in one file, the second saying the tests had to be in `main.vo`
  because `vo test main.vo` runs only that file's tests (`vo test .` runs all, but neither the docs nor the
  acceptance checkers used it). FR-014 removes that reason.

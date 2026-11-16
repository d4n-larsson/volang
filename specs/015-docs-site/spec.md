# Feature Specification: Documentation Site

**Feature Branch**: `015-docs-site`

**Created**: 2026-11-16

**Status**: Draft

**Input**: User description: "A GitHub Pages site with detailed documentation of the vo language: a static site published from this repository (built by GitHub Pages / GitHub Actions), with a getting-started guide (building vo, the first program, running, testing), a language guide with detailed explanations and runnable examples for every feature (types, records and variants, generics, effects and scopes, results and errors, collections, tests/properties/contracts/holes, modules/exports/packages, the standard library), a toolchain guide (every command: check, run, test, fmt, effects, explain, doc, query, edit, trace, record/replay, package, serve), generated reference pages that stay in sync with the toolchain (the language reference, the error catalog with every code, the standard library from `vo doc --json`), the security model, examples, and the roadmap; every code example on the site is checked by the test suite so the docs cannot drift from the implementation; search, navigation and a design that works for both humans and agents (a plain-text/markdown version of each page for LLM consumption, e.g. llms.txt)."

Background: vo's documentation today is a set of Markdown files in the repository: the language reference
(embedded in the binary, kept under 15,000 tokens so an agent can read all of it), the error catalog,
the security model, the roadmap, per-example READMEs, and what `vo doc` prints for the standard library.
The reference is deliberately terse. There is no place where a person new to vo can learn a feature in
depth, browse the error codes, or find a command's options, and no published address to point anyone to.
The repository is public on GitHub (`d4n-larsson/volang`); GitHub Pages is not enabled yet.

## User Scenarios & Testing *(mandatory)*

Two kinds of readers: **people** (evaluating vo, learning it, reviewing agent-written vo code) and
**agents** (coding agents given vo documentation as context, or fetching it while they work). Both need
the same facts; they read them differently.

### User Story 1 - From nothing to a running, tested program (Priority: P1)

A developer hears about vo, opens the site, and follows "Getting started": builds the toolchain (or uses a
prebuilt binary when one exists), writes the first program, runs it, adds a test and an effect, and sees
an error report with its fix.

**Why this priority**: the first page decides whether anyone continues.

**Independent Test**: a person (or an isolated agent given only that page) follows it on a clean Linux
machine; every command and output shown matches what happens.

**Acceptance Scenarios**:

1. **Given** a clean Linux x86-64 machine with the documented prerequisites, **When** a reader follows
   Getting started step by step, **Then** every command succeeds and prints what the page shows, and the
   reader ends with a program that runs and passes its test.
2. **Given** the page's deliberate mistake (a missing effect), **When** the reader runs `vo check`,
   **Then** the diagnostic matches the page, including the fix.

---

### User Story 2 - Learning a feature in depth (Priority: P1)

A developer (or a reviewer reading agent-written code) wants to understand effects and scopes, or
generics, or property tests: why the feature exists, how it works, its rules and edge cases, the errors it
produces, and complete examples. The language guide has one chapter per feature area.

**Why this priority**: the reference states rules in as few tokens as possible; people need the
explanations, the reasoning and the worked examples.

**Independent Test**: each chapter's examples are extracted and checked; a reviewer confirms each chapter
covers the rules of its reference section (no rule in the reference missing from the guide).

**Acceptance Scenarios**:

1. **Given** the guide, **When** a reader looks for any language feature named in the reference (types,
   records and variants, `match`, functions as values, generics, effects and scopes, results and errors,
   collections, holes, tests, properties, contracts, annotations, modules, `export`, packages, the
   standard library), **Then** a chapter explains it with at least one complete example and the error
   codes it can produce, linked to their catalog pages.
2. **Given** an example in the guide, **When** a reader copies it into a file and runs the command shown
   next to it, **Then** the output is exactly what the guide shows.
3. **Given** a chapter, **When** read top to bottom, **Then** it starts from the problem the feature solves
   and ends with its edge cases and limits, and links to the reference section that states its rules.

---

### User Story 3 - Looking up exact facts (Priority: P1)

A reader wants the exact signature of `std.text.pad_left`, what E0503 means and how to fix it, the
options of `vo test`, or the precedence of operators. Reference pages answer, generated from the same
sources the toolchain uses, so they are never out of date.

**Why this priority**: lookups are most of the visits once someone uses vo, and wrong facts are worse
than no docs.

**Independent Test**: regenerate the site from the repository at a given commit; compare every generated
fact (each error code, each standard-library item, the reference text) against the toolchain's own
output at that commit.

**Acceptance Scenarios**:

1. **Given** the error catalog, **When** the site is built, **Then** every code has its own page (name,
   meaning, whether it is fixable, the example and the fixed version) reachable by its code, and the set
   of pages equals the set of codes the toolchain knows.
2. **Given** the standard library, **When** the site is built, **Then** every module and every item has a
   page or anchor with its exact signature, effects, description and example, identical to what the
   toolchain prints for it.
3. **Given** the language reference, **When** the site is built, **Then** it is published in full, section
   by section, with the same text the toolchain embeds.
4. **Given** a version of the toolchain, **When** a reader opens the site, **Then** the site says which
   version and commit it documents.

---

### User Story 4 - Documentation that cannot drift (Priority: P1)

A maintainer changes the language. If any example on the site no longer checks, runs or prints what the
page says, the quality gate fails before the change can merge; the generated pages change with the
toolchain automatically.

**Why this priority**: the project's principle is a single source of truth; documentation that drifts
teaches agents and people wrong things.

**Independent Test**: introduce a deliberate change that breaks one guide example (and, separately, one
that removes an error code or a library item); the documentation check fails and names the page and the
example.

**Acceptance Scenarios**:

1. **Given** every code block on the site marked as a vo program, **When** the documentation check runs,
   **Then** each one is checked, and each one with a shown command and output is run and compared.
2. **Given** a code block that is deliberately incomplete or wrong (a fragment, an error example), **When**
   the check runs, **Then** it is marked as such on the page and its expected result (a fragment that
   only checks, or a named error code) is verified too.
3. **Given** a link between pages, to an error code, a library item or a reference section, **When** the
   site is built, **Then** every internal link resolves; a broken one fails the build.

---

### User Story 5 - Agents read the documentation (Priority: P2)

A coding agent is given the site's address, or a tool fetches docs into its context. It finds an index
of the documentation in plain text (`llms.txt`), a plain Markdown version of every page, and a single
complete file for loading everything at once.

**Why this priority**: vo is designed for agents; its documentation should be as easy for them to load as
for people to read.

**Independent Test**: fetch `llms.txt`, follow every link it lists, and check that each returns plain
Markdown with no navigation chrome; load the full-text file and check it contains every page.

**Acceptance Scenarios**:

1. **Given** the published site, **When** an agent fetches `/llms.txt`, **Then** it gets a short
   description of vo and a list of every page with a one-line summary and the link to its Markdown
   version.
2. **Given** any page, **When** its Markdown version is fetched, **Then** it has the same content without
   navigation, scripts or styling.
3. **Given** the full-text file, **When** loaded, **Then** it contains the reference, the guide and the
   catalog in reading order, with the version it documents at the top.

---

### User Story 6 - The toolchain guide (Priority: P2)

A reader wants to know everything `vo test` (or `vo query`, `vo edit`, `vo replay`, ...) can do: its
options, what it prints in text and JSON, its exit statuses, and a worked session.

**Why this priority**: the command line is how both agents and people drive vo; today it is described in
one reference section and the specs.

**Independent Test**: for each command, its page lists exactly the options the toolchain accepts (checked
against the toolchain's own usage output), and its worked session is run and compared.

**Acceptance Scenarios**:

1. **Given** every command (`check`, `run`, `test`, `fmt`, `effects`, `explain`, `doc`, `reference`,
   `query`, `impact`, `edit`, `replay`, `package`, `version`), **When** its page is read, **Then** it
   gives the synopsis, every option with its meaning, the output (text and JSON records), the exit
   statuses, and at least one worked session whose output is verified.
2. **Given** the machine-readable output, **When** a reader looks it up, **Then** one page documents each
   JSON record kind and the schema version rules.

---

### User Story 7 - Finding things (Priority: P2)

A reader types "pad left", "E0503", "ordered map" or "timeout" and lands on the right page; every page
has navigation to its neighbors, the sections of the site, and an "edit this page" link.

**Why this priority**: a large documentation site is only as useful as its search and navigation.

**Independent Test**: a fixed list of at least 30 queries (feature names, codes, library items, commands)
each returns the right page among the first three results.

**Acceptance Scenarios**:

1. **Given** the search box, **When** a reader searches for an error code, a library item, a command or a
   feature name, **Then** the page that documents it is among the first three results.
2. **Given** any page, **When** read, **Then** it shows where it is in the site, the previous and next pages
   of its section, a table of contents of its headings, and a link to its source in the repository.
3. **Given** a reader without JavaScript, **When** they browse, **Then** every page's content and
   navigation still work (search may not).

---

### User Story 8 - Publishing and previewing (Priority: P3)

A maintainer merges to the main branch; the site rebuilds and publishes by itself. Before merging, they
build and preview the site locally with one command.

**Why this priority**: the site must stay current without manual steps.

**Independent Test**: a commit to the main branch that changes a guide page appears on the published site
within the documented time, with the new version shown.

**Acceptance Scenarios**:

1. **Given** a push to the main branch, **When** the publishing workflow runs, **Then** it builds the
   toolchain, regenerates the reference pages, runs the documentation check and publishes only if it
   passes.
2. **Given** a maintainer's machine, **When** they run the documented build command, **Then** the complete
   site is produced locally and can be previewed in a browser.

---

### Edge Cases

- **Examples that are not complete programs** (fragments, signatures, examples of mistakes): marked as
  such on the page and checked in the way the mark says, never silently skipped.
- **Examples needing files, network or input**: carry their files and input with them (or say they need
  a service); the check provides them; examples that need the network use a local service, never the
  internet.
- **Output that varies** (time, random): examples fix their seed or clock as the toolchain allows, so
  outputs are deterministic.
- **Renamed or removed features**: generated pages disappear with them; links to them fail the build until
  the guide is updated.
- **Long pages** (the error catalog, the standard library): split into one page per code or per module,
  with an index.
- **Old links**: a page that moves keeps its old address as a redirect.
- **The 15,000-token budget**: applies to the embedded reference, not to the site; the site's guide is
  free to be long, and never contradicts the reference.
- **Dark and light mode, narrow screens, keyboard only**: the site works in all of them.

## Requirements *(mandatory)*

### Functional Requirements

**Content**

- **FR-001**: The site MUST have a Getting started guide covering prerequisites, building the toolchain
  for each supported platform, the first program, running, testing, a first effect, a first error and its
  fix.
- **FR-002**: The site MUST have a language guide with a chapter for each feature area of the reference,
  each explaining the purpose, the rules, worked examples, edge cases, limits and the error codes involved,
  and linking to the reference section and catalog pages.
- **FR-003**: The site MUST have a toolchain guide with a page per command (synopsis, every option,
  text and JSON output, exit statuses, a worked session) and a page on machine-readable output.
- **FR-004**: The site MUST publish, generated from the toolchain's own sources at the documented commit:
  the full language reference; one page per error code (and an index); one page per standard-library
  module with every item (signature, effects, description, example).
- **FR-005**: The site MUST include the security model and security reviews, the examples (each with its
  source and README), the roadmap, and the specification index (one line per spec with its status).
- **FR-006**: Every page MUST state the toolchain version and commit the site was built from.

**Correctness**

- **FR-007**: Every vo code block on the site MUST be verified by the documentation check: complete
  programs checked and, where a command and output are shown, run and compared exactly; fragments and
  error examples verified according to their marking (checks as a fragment, or produces the named code).
- **FR-008**: The documentation check MUST run as part of the project's quality gate, and a failure MUST
  name the page and the example.
- **FR-009**: Every internal link MUST resolve when the site is built; a broken internal link MUST fail
  the build. Moved pages MUST keep their old address as a redirect.
- **FR-010**: Generated pages MUST be regenerated on every build, never edited by hand; the set of
  error-code pages MUST equal the toolchain's catalog and the set of library pages its modules.

**Agents**

- **FR-011**: The site MUST publish `llms.txt` (a description of vo and a list of every page with a
  one-line summary and a link to its Markdown version), a Markdown version of every page, and one
  full-text file with the reference, guide and catalog.

**Finding and reading**

- **FR-012**: The site MUST have full-text search over all pages, working without a server.
- **FR-013**: Every page MUST show its place in the site, the previous and next pages of its section, a
  table of contents, and a link to its source in the repository.
- **FR-014**: Pages MUST be readable without JavaScript, on narrow screens, with keyboard only, in light
  and dark mode, with vo syntax highlighted in code blocks.
- **FR-015**: The site MUST NOT load third-party scripts, fonts or trackers, and MUST NOT collect
  analytics.

**Publishing**

- **FR-016**: The site MUST be built and published automatically on every push to the main branch, only
  when the build and the documentation check pass.
- **FR-017**: A maintainer MUST be able to build and preview the full site locally with one documented
  command.
- **FR-018**: Adding the site MUST NOT change the toolchain's behavior or the embedded reference beyond
  fixes the guide reveals (recorded as such).

### Key Entities

- **Guide page**: hand-written explanation with examples; belongs to a section (Getting started, Language
  guide, Toolchain guide); has a source file in the repository.
- **Generated page**: produced from a toolchain source (reference, error catalog, standard library,
  examples, specs index) at build time.
- **Example**: a code block on a page, with its kind (program, fragment, error example), optional
  files, input, command and expected output.
- **Agent artifact**: `llms.txt`, the per-page Markdown versions, the full-text file.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of vo code blocks on the site are verified by the documentation check (none skipped
  without a marking that is itself verified).
- **SC-002**: 100% of error codes and 100% of standard-library items have their page or anchor, and the
  generated content matches the toolchain's own output exactly.
- **SC-003**: 0 broken internal links on the published site.
- **SC-004**: A newcomer completes Getting started (a running, tested program with an effect) in under 15
  minutes on a prepared Linux machine; demonstrated with at least 3 isolated agent runs given only the
  Getting started page, which reach a passing test on their first attempt.
- **SC-005**: For a fixed list of at least 30 lookups (features, codes, library items, commands), the right
  page is among the first three search results for at least 90% of them.
- **SC-006**: A change to the main branch appears on the published site within 15 minutes; the full site
  builds locally in under 2 minutes.
- **SC-007**: Every page meets WCAG 2.1 AA contrast in both color modes and is fully usable by keyboard.

## Assumptions

- **Public site**: the repository is public, so the published documentation is public too; enabling
  GitHub Pages in the repository settings is a one-time maintainer step that this feature documents
  rather than performs.
- **Latest only**: the site documents the main branch at its current commit (version shown on every
  page); documentation per release waits until there are releases to document.
- **Address**: the default GitHub Pages address (`d4n-larsson.github.io/volang`); a custom domain can be
  added later without changing the content.
- **Sources stay in the repository**: the reference, the catalog and the library documentation remain the
  single sources (constitution Principle V); the site renders them and adds explanation around them. The
  guide never restates a rule differently from the reference; where they disagree, the reference wins and
  the guide is fixed.
- **Existing checks are reused**: the reference examples, catalog examples and library examples are
  already checked by the test suite; the documentation check extends the same approach to guide pages.
- **English only.**
- **No build steps that need the internet beyond fetching dependencies at publish time**; the local build
  works offline once dependencies are present.

# Feature Specification: Small Language and Toolchain Fixes

**Feature Branch**: `009-small-fixes`

**Created**: 2026-11-15

**Status**: Draft

**Input**: User description: "Small language and toolchain fixes found while writing the example applications (the cheap ones): (1) a builtin get_or(m, k, default) for maps, to replace the has/set counting pattern; (2) pad_left and pad_right with a default fill of a space, so the fill argument is optional or a two-argument form exists; (3) `case ok()` accepted as the pattern for a result[void] value, matching the ok() constructor (today only `case ok(_)` parses); (4) the R0010 message names the function whose grant was checked (the caller whose grant a library function inherited, e.g. main), not the library function (`exchange`); (5) replay compares an HTTP request's method and path (the request line), not only host and port, so a changed program that asks for another path stops with R0014 instead of silently getting the recorded response."

Background: these came up while writing `examples/expenses`, `examples/status-board` and the spec 007
task programs. Each one cost a workaround or produced a misleading result; none needs a new language
concept.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–008: AI coding agents that write vo programs from the reference
and fix them from diagnostics, and the people who review and run those programs.

### User Story 1 - Reading a map with a fallback (Priority: P1)

An agent counts words, sums amounts per category or looks up a setting that may be missing. Today every
such lookup is a four-line `if has(m, k) { ... } else { ... }`, written again in every program (both
examples and three of the eleven spec 007 task programs did it; the test generator defines its own
helper). With `get_or`, the lookup is one expression: `set counts[w] = get_or(counts, w, 0) + 1;`.

**Why this priority**: it is the most repeated workaround, it is the kind of code agents get subtly
wrong (a misspelled key in one branch), and it is the cheapest to add.

**Independent Test**: golden programs that read present and missing keys of maps with different key
and value types, and a check that a wrong default type is rejected.

**Acceptance Scenarios**:

1. **Given** a map that holds a key, **When** `get_or(m, key, default)` is evaluated, **Then** it is the
   stored value, and the map is unchanged.
2. **Given** a map that does not hold the key, **When** `get_or` is evaluated, **Then** it is the
   default, and the map is unchanged (no entry is added).
3. **Given** a default whose type differs from the map's value type, or a key of the wrong type, **When**
   checked, **Then** a type error names the expected and the found type.
4. **Given** a program that defines its own function named `get_or`, **When** checked, **Then** it is
   the existing "builtin cannot be redefined" error, with its usual message.

---

### User Story 2 - Replay refuses a request it did not record (Priority: P1)

A developer records a run of a program that talks to a service, changes the program, and replays the
recording to debug it. Today replay matches each HTTP exchange by host and port only: if the changed
program asks for `/v2/status` instead of `/status`, it silently receives the recorded response for the
old path, and the developer debugs against data the program never asked for. Replay must instead stop
and say which request differs.

**Why this priority**: replay is a debugging guarantee ("the program runs on exactly what it received");
a silent mismatch breaks it and can mislead a fix. It is a correctness fix, not a convenience.

**Independent Test**: record a run against the HTTP test server, change the requested path (and,
separately, the method), replay, and check the stop and its message; replay unchanged programs and old
recordings and check they still run as before.

**Acceptance Scenarios**:

1. **Given** a recording of `GET /status` on `localhost:8000`, **When** a changed program replays it
   and asks for `GET /v2/status` on the same host and port, **Then** replay stops with R0014 naming the
   request asked for and the request recorded.
2. **Given** the same recording, **When** the program asks for `POST /status` instead of `GET /status`,
   **Then** replay stops with R0014 in the same way.
3. **Given** a program that makes exactly the recorded requests, **When** replayed, **Then** it gets
   the recorded responses and its output is byte-identical to the recorded run's.
4. **Given** a recording made before this change (an HTTP event without the request line), **When**
   replayed, **Then** it is matched by host and port as before, so existing recordings keep working.

---

### User Story 3 - Permission errors name the right function (Priority: P2)

A program whose `main` declares `uses net("example.com")` calls `std.http.get("http://localhost/")`.
The run stops with R0010, but the message says "`exchange` declares console, net(\"example.com\")":
`exchange` is an internal library function the program never mentions, and it declares nothing of its
own (library functions and anonymous functions run with their caller's grant). The message must name the
function whose declaration was actually checked, here `main`, so the reader knows which `uses` clause to
change.

**Why this priority**: a misleading error costs an agent a wrong fix, but the run already stops at the
right place, so it is less urgent than a silent replay mismatch.

**Independent Test**: golden cases where a denied effect happens inside a library function and inside
an anonymous function, each checking the function named in the message and in the JSON report.

**Acceptance Scenarios**:

1. **Given** `main` declares `net("example.com")` and calls `std.http.get` for another host, **When**
   the run stops with R0010, **Then** the message names `main` and its declaration.
2. **Given** a function `load` that declares `file.read("data/")` and passes an anonymous function that
   reads another directory, **When** the anonymous function's read is denied, **Then** the message names
   `load`, whose grant the anonymous function used.
3. **Given** a denied effect in a function that declares its own effects, **When** reported, **Then**
   the message is exactly as today (that function is named).
4. **Given** any of these, **When** reported with `--json`, **Then** the report's function field names
   the same function as the message.

---

### User Story 4 - Matching a successful `result[void]` the way it is built (Priority: P2)

A `result[void]` is built with `ok()`, and an agent naturally matches it with `case ok() { ... }`.
Today that is a parse error ("Expected a name, found `)`") and only `case ok(_)` works; this cost a
round in writing the spec 007 task programs.

**Why this priority**: an agent hits it on the first `result[void]` it matches, and the fix it needs
is not obvious from the message; but it has a workaround.

**Independent Test**: golden cases matching `result[void]` values, matching `result[int]` with
`case ok()`, and `case ok(_)` on a `result[void]` (the new warning and its fix).

**Acceptance Scenarios**:

1. **Given** a `result[void]` value, **When** matched with `case ok() { ... }` and `case err(e) { ... }`,
   **Then** it checks and runs, taking the `ok()` arm on success.
2. **Given** a `result[int]` value, **When** matched with `case ok() { ... }`, **Then** a check error
   says the case needs a binding (`case ok(v)` or `case ok(_)`), with a fix.
3. **Given** a `result[void]` matched with `case ok(_)`, **When** checked, **Then** it still checks
   and runs, with a new warning whose fix rewrites it to `case ok()` (so `case ok()` is the one form for
   `result[void]`, and existing programs keep working: warnings never change an exit status).

---

### User Story 5 - Padding with spaces without spelling the fill (Priority: P3)

Almost every call to `pad_left` and `pad_right` in the examples and task programs passes `" "` as the
fill. vo has no default arguments and no overloading (Principle I), so the fill cannot become optional.
Instead `std.text` gains `align_left(s, width)` and `align_right(s, width)`, named for where the text
goes: `align_left` puts the text on the left and spaces on the right (as `pad_right(s, width, " ")`),
`align_right` puts spaces on the left (as `pad_left(s, width, " ")`). `pad_left` and `pad_right` keep
their explicit fill for every other character.

**Why this priority**: it saves a few characters per call and prevents nothing; the value is small.

**Independent Test**: golden cases padding plain and multi-byte text to widths shorter and longer than
the text.

**Acceptance Scenarios**:

1. **Given** text shorter than the width, **When** passed to `align_left` or `align_right`, **Then** the
   result has exactly that many characters (counting characters, not bytes), with spaces on the right or
   the left respectively.
2. **Given** text at least as long as the width, **When** padded, **Then** it is returned unchanged.
3. **Given** existing calls of `pad_left(s, width, fill)` and `pad_right(s, width, fill)`, **When** run,
   **Then** they behave exactly as before.

---

### Edge Cases

- **`get_or` arguments**: all three arguments are evaluated before the call, like every vo call (the
  default is evaluated even when the key is present); there is no lazy default. The map may be empty.
  Nested maps work: `get_or(outer, k, new map[str, int])` returns the stored inner map or the new one.
- **`get_or` and effects**: it is pure; it needs no `uses`.
- **Replay and requests**: the comparison covers the request method and the request target (path and
  query) as the program sent them; header and body differences are not compared. A host, port, method or
  target difference is R0014; so is a request when the recording has no exchange left.
- **Replay and `https://` or bad URLs**: these never reach the network or the recording (they are `err`
  before anything is sent), so they replay the same way as before.
- **R0010 when nobody declares the effect**: if no function between the library or anonymous function
  and `main` declares effects of its own, the message names `main` (which, being pure or lacking the
  effect, "declares nothing" or its actual list).
- **R0010 for file and environment scopes**: the same rule applies to `file.read`, `file.write` and
  `env` denials inside anonymous functions; library functions do not use those effects today.
- **`case ok()` in a `match` on a non-result value**: unchanged errors (a pattern for another type).

## Requirements *(mandatory)*

### Functional Requirements

**Maps**

- **FR-001**: The language MUST provide a builtin `get_or(m, key, default)` for a `map[K, T]`, a key of
  type `K` and a default of type `T`, returning the value stored for the key, or the default when there is
  none. It MUST NOT change the map, and it MUST be pure.
- **FR-002**: A wrong key or default type MUST be a check error naming the expected and found types. A
  user function named `get_or` MUST get the existing builtin-redefinition error.

**Replay**

- **FR-003**: Recording an HTTP exchange MUST also save the request's method and target.
- **FR-004**: Replay MUST match each HTTP request to the next recorded exchange by host, port, method and
  target, and MUST stop with R0014 when any of them differs, naming the request asked for and the request
  recorded. An exchange recorded without method and target MUST be matched by host and port only.

**Effect errors**

- **FR-005**: An R0010 report MUST name the function whose effect declaration was checked: the running
  function when it declares its own effects, otherwise the nearest caller (through library functions and
  anonymous functions, which run with their caller's grant) that does, or `main`. The message text and the
  JSON report MUST name the same function.

**Patterns**

- **FR-006**: `case ok()` MUST be accepted as the success pattern of a `result[void]`, and MUST be a check
  error with a fix (to `case ok(v)` / `case ok(_)`) for a result of any other type.
- **FR-007**: `case ok(_)` on a `result[void]` MUST keep checking and running, with a new warning whose
  mechanical fix rewrites it to `case ok()`; on results of other types `case ok(_)` is unchanged.

**Text**

- **FR-008**: `std.text` MUST provide `align_left(s: str, width: int) -> str` (the text, then spaces up to
  `width` characters) and `align_right(s: str, width: int) -> str` (spaces, then the text), counting
  characters, not bytes, and returning text at least `width` characters long unchanged. `pad_left` and
  `pad_right` with an explicit fill MUST keep their behavior. Both new items MUST be documented in
  `vo doc` with an example.

**Documentation, catalog and compatibility**

- **FR-009**: The reference MUST document `get_or` (in the builtins table), the `case ok()` pattern, and
  how replay matches HTTP requests; `vo doc` MUST show any new or changed library item. Every new or
  changed diagnostic MUST have a catalog entry and a golden case.
- **FR-010**: Existing programs MUST behave exactly as before, with these exceptions only, each recorded in
  results: a program that defines its own `get_or` (the builtin name is now reserved), a replay of a
  changed program whose requests differ in method or target (now R0014), the function named in R0010
  messages for denials inside library and anonymous functions, and the new warning on `case ok(_)` for a
  `result[void]` (which changes no exit status).
- **FR-011**: The toolchain MUST keep passing its full quality gate on x86-64, ARM64 and RISC-V. Programs
  that use none of these features MUST give byte-identical results to the previous release.

### Key Entities

- **HTTP exchange (in a recording)**: host, port, and now the request method and target, plus the
  result and the response (or error text), as recorded by `vo run --record`.
- **Effect grant**: the set of effects a running function may use; for library and anonymous functions,
  the grant of the caller that declared it. R0010 reports name the declaring function.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every map lookup with a fallback in the two examples and the spec 007 task solutions can be
  written as one `get_or` expression; after rewriting the examples' counting code with it, their tests
  and replays still pass.
- **SC-002**: In 100% of replay tests where a changed program asks for a different method or path on the
  same host and port, replay stops with R0014 naming both requests; 100% of the existing replay and HTTP
  cases, and the committed example recording, replay exactly as before.
- **SC-003**: In 100% of the R0010 golden cases for denials inside library and anonymous functions, the
  message and the JSON report name the declaring function; every existing R0010 case is unchanged.
- **SC-004**: `case ok()` checks and runs on every `result[void]` golden case, and is rejected with a fix
  on every non-void result case; `case ok(_)` on a `result[void]` gets the warning, and applying its fix
  leaves a program that checks with no warning and gives the same output.
- **SC-007**: In the two examples and the spec 007 task solutions, every `pad_left`/`pad_right` call with
  a space fill can be replaced by `align_right`/`align_left` with byte-identical output.
- **SC-005**: The reference grows by at most 300 tokens and stays under 15,000; every reference example
  and catalog entry passes.
- **SC-006**: The full gate passes on all three platforms, and 10,000 generated programs give 0
  disagreements with the previous release.

## Assumptions

- **Scope**: only the five fixes listed; default arms in `match`, `try` inside tests, several entry
  programs per effects record, multi-key sorting and string formatting are left for a later decision, since
  they touch Principle I.
- **The previous release is the oracle**: after spec 008 the C toolchain at the last commit (with spec
  007) is the reference for "behaves exactly as before"; the assembly release is no longer needed for this
  spec's comparisons, except where the gate already uses it.
- **Reserving `get_or`** is accepted as the one name reservation, like `std` in spec 007; in this
  repository only the test program generator defines its own `get_or`, and it is renamed.
- **Request bodies and headers are not compared** on replay (the user asked for the request line); a
  program that sends a different POST body to the same path still gets the recorded response. This is
  stated in the reference.
- **Recordings stay schema 1**: the request method and target are added fields of the existing `http`
  event; older recordings remain valid.

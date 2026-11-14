# Feature Specification: Standard Library and Content-Pinned Packages

**Feature Branch**: `007-stdlib-packages`

**Created**: 2026-11-14

**Status**: Draft

**Input**: User description: "standard library and content-pinned packages (spec 007, per docs/roadmap.md)". The roadmap entry (docs/roadmap.md, "007: standard library and packages") says:
- JSON, time, random, text, and HTTP behind the `net` effect (002 already defines and checks it; no operation uses it yet).
- `vo doc --json`: exact signatures for the installed version.
- Packages pinned by content hash and resolved at check time, never at run time; a package's effects appear in `vo.effects`, so a new dependency that wants the network is visible in review.

Clarified with the user on 2026-11-14:
- **Plain HTTP only.** `http://` requests over the network; `https://` is reported as unsupported (a clear error, never a silent plain-text request). TLS is a later spec.
- **Packages come from local directories.** A manifest names each package's directory and its content hash; nothing is downloaded in this spec.
- **Success is gated on a deterministic stand-in**: fixed task programs that use the library, written against `vo doc` output, plus a check that every documented signature is exactly what the checker accepts. An agent benchmark run is a follow-up.

Planning note (2026-11-14): `vo doc` takes one optional NAME and finds a program's packages through the nearest `vo.packages` from the working directory (plan, research R7), so there is no ENTRY argument.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–006:
- **AI coding agents**, which write vo programs from the reference and fix them from diagnostics.
- **The people who review and run those programs.**

This spec addresses the roadmap's fourth problem with agent-written code: **agents invent APIs that do not exist**. Their memory of a library drifts from the installed version, so they call functions with the wrong names, arguments or behavior. vo answers with a small standard library whose exact signatures the toolchain prints, and with packages whose content cannot change without the change being visible.

It also gives vo programs the everyday tools they lack: reading and writing JSON, working with dates and times, richer text and randomness helpers, and talking to HTTP services.

### User Story 1 - Exact signatures on demand (Priority: P1)

An agent about to use a library function asks the toolchain for it instead of guessing: `vo doc` lists the library's modules, `vo doc std.json` lists every item of a module with its exact signature, effects and a one-line description, and `vo doc --json` gives the same as records. The answer always matches the installed toolchain and the program's packages, because it comes from the same declarations the checker uses.

**Why this priority**: This is the roadmap's direct answer to invented APIs, and every other story relies on agents being able to look things up. It is useful as soon as one module exists.

**Independent Test**: Run `vo doc` on every module and builtin, and check each printed signature by compiling a call that uses it exactly as printed (it must check clean) and a call that departs from it (it must be rejected).

**Acceptance Scenarios**:

1. **Given** the installed toolchain, **When** `vo doc` runs, **Then** it lists the builtins and every standard module with a one-line description each, and exits 0.
2. **Given** `vo doc std.json`, **When** it runs, **Then** it lists every item of the module (functions, records, variants) with its full signature including type parameters and effects, and a one-line description.
3. **Given** `vo doc std.json.parse_json`, **When** it runs, **Then** it prints that one item, its description and a short example; `--json` gives the same as one record with the signature as structured data.
4. **Given** a directory whose manifest (`vo.packages`) lists `money`, **When** `vo doc money.format` runs there, **Then** the package module's items are listed the same way.
5. **Given** a name that does not exist (`vo doc std.jsn`), **When** it runs, **Then** a structured error names the closest existing module or item.
6. **Given** a program that calls a library function with the wrong arguments, **When** checked, **Then** the diagnostic quotes the exact signature, as for any other function.

---

### User Story 2 - JSON (Priority: P1)

An agent's program reads a JSON configuration file, changes a value and writes it back; another builds a JSON report from records. The library parses any valid JSON text into a vo value, turns a vo value back into JSON text, and offers helpers to read fields without long `match` chains.

**Why this priority**: JSON is the format agents meet most often, in inputs, configuration and service responses, and vo has no way to handle it today.

**Independent Test**: Parse a corpus of valid and invalid JSON documents; compare the results and errors with expectations; check that writing and parsing again gives the same value.

**Acceptance Scenarios**:

1. **Given** a valid JSON document (objects, arrays, strings with escapes and non-ASCII text, numbers, `true`, `false`, `null`, nesting), **When** parsed, **Then** the result is `ok` with a value that represents exactly that document, object keys in their original order.
2. **Given** invalid JSON (a trailing comma, an unterminated string, a bad escape, leading zeros, text after the value), **When** parsed, **Then** the result is `err` with a message giving the problem and its position (line and column).
3. **Given** a parsed value, **When** written back as JSON, **Then** parsing the output gives an equal value; compact and indented output are both available, and output is deterministic (same value, same bytes).
4. **Given** a parsed object, **When** a field is read with a helper (as text, as an integer, as a nested object), **Then** a missing field or a value of the wrong kind is an `err` naming the field, not a runtime crash.
5. **Given** a number too large for an integer, or with a fraction, **When** read as an integer, **Then** the helper returns `err`; read as a float it returns the nearest float.
6. **Given** a document nested deeper than the documented limit, **When** parsed, **Then** the result is `err` (never a crash or a hang).

---

### User Story 3 - Content-pinned packages (Priority: P1)

A team keeps shared vo code (a money formatter, a parser for their log format) in its own directory and uses it from several programs. Each program lists the package in a manifest next to its code, with the package's content hash. The checker loads the package from that directory, verifies the hash, and refuses to check the program if the package's content changed. A package's functions appear in `vo.effects` like the program's own, so a package update that starts using the network shows up in review as a capability change.

**Why this priority**: Code reuse across programs is the other half of the roadmap item, and the content pin is what makes reuse safe to review. It needs no network and no download.

**Independent Test**: Build programs that import package items; check, run and test them; then change one byte of a package file, rename a file, or add a file, and check that every such change is reported before anything runs.

**Acceptance Scenarios**:

1. **Given** a manifest naming package `money` with its directory and hash, **When** a program imports `money.format.cents` and is checked, run or tested, **Then** the package is loaded from that directory and the program works as if the code were local.
2. **Given** one byte of a package file changed (or a file added, removed or renamed), **When** the program is checked, **Then** a check error (new code) names the package, the expected hash and the actual hash, and the program does not run.
3. **Given** a command to add or update a package entry, **When** it runs on a directory, **Then** it computes the hash, writes the manifest entry, and prints exactly what changed (added, updated, with the old and new hash).
4. **Given** a package function that uses `net`, **When** the program starts calling it, **Then** the effects record shows the package function's effects next to the caller's, and the check reports the caller's new capability (E0504), as for local code.
5. **Given** an import of a package that is not in the manifest, or a manifest entry whose directory is missing, **When** checked, **Then** a check error with a fix or a clear message is reported.
6. **Given** a package that imports a standard module or another package listed in the same manifest, **When** checked, **Then** it works; a package listed nowhere is an error.
7. **Given** a program with packages, **When** it runs, **Then** its packages are resolved and verified during the check that precedes every run, and the running program reads no package file.

---

### User Story 4 - Time, text and random helpers (Priority: P2)

An agent formats a timestamp as an ISO 8601 date, computes the weekday of a date, pads columns in a report, replaces substrings, splits lines, picks a random element or shuffles a list. These are small, everyday functions that agents otherwise rewrite by hand, with bugs.

**Why this priority**: They remove a steady source of hand-written mistakes, but each is small and none blocks the others.

**Independent Test**: Golden programs calling each helper on boundary inputs (leap years, month ends, the epoch, negative times, empty strings, multi-byte characters, empty lists), run twice to show determinism.

**Acceptance Scenarios**:

1. **Given** a time in milliseconds since the epoch, **When** converted, **Then** the library gives the UTC calendar date and time (year, month, day, hour, minute, second, millisecond, weekday), and back, correctly across leap years and for times before 1970.
2. **Given** a date, **When** formatted, **Then** ISO 8601 text is produced (`2026-11-14T09:30:00Z`); **When** that text is parsed, **Then** the same time comes back; malformed text is an `err` naming the problem.
3. **Given** the current time is asked for, **When** the program runs, **Then** it needs the `clock` effect, and `--clock-ms` fixes it as for `now_ms`.
4. **Given** text helpers (replace, starts/ends with, contains, repeat, pad to a width, split into lines and into words), **When** applied to empty strings and multi-byte text, **Then** the results are correct and widths count characters, not bytes.
5. **Given** random helpers (pick an element, shuffle, a float in [0, 1)), **When** run with the same `--seed`, **Then** the results are identical; they need the `random` effect; picking from an empty list is an `err` or documented runtime error.

---

### User Story 5 - Plain HTTP behind `net` (Priority: P2)

An agent's program fetches JSON from a local service and posts a result back. `std.http` sends `GET` and `POST` requests to `http://` URLs and returns the status, headers and body. Calls need the `net` effect, scoped to hosts (`net("localhost")`), and run-time checks refuse hosts outside the grant. Responses are part of what `vo run --record` saves, so a run that talked to a service replays without it.

**Why this priority**: `net` has existed since spec 002 without any operation. Plain HTTP covers local services, test servers and many internal tools; TLS is out of scope (clarified).

**Independent Test**: Run programs against a local test server (started by the test harness) that serves fixed responses, including redirects, chunked bodies, errors and slow replies; record and replay them; and check the effect and scope rules.

**Acceptance Scenarios**:

1. **Given** a local server, **When** a program with `uses net("localhost")` calls `get("http://localhost:PORT/data")`, **Then** it receives `ok` with the status, headers and body.
2. **Given** an `https://` URL, **When** requested, **Then** the result is `err` saying HTTPS is not supported (nothing is sent).
3. **Given** a host outside the function's `net` scope, **When** requested, **Then** the run stops with R0010, as for files outside a `file.read` scope.
4. **Given** a function that calls `get` without declaring `net`, **When** checked, **Then** E0501 with a fix adds it.
5. **Given** a connection refused, an unknown host or a malformed response, **When** requested, **Then** the result is `err` with a message; a server that stops answering is stopped by the run's `--timeout` (R0009), as any long operation is.
6. **Given** a run recorded with `--record` that made requests, **When** replayed with the server stopped, **Then** the program gets the recorded responses and produces the same output.
7. **Given** a response with a chunked body or a `Content-Length`, **When** received, **Then** the body is assembled correctly; redirects are not followed automatically (the status and `Location` header are returned).

---

### User Story 6 - Evidence that the library is usable from its docs (Priority: P2)

The maintainers can show that programs written only from `vo doc` output work, and that the documented signatures are exactly what the checker accepts. They use a fixed set of task programs and a signature conformance check, not an agent run.

**Why this priority**: This is the success measure for the spec (clarified with the user). The agent run with and without `vo doc` is a follow-up.

**Independent Test**: Run the task programs' tests and the signature conformance check.

**Acceptance Scenarios**:

1. **Given** at least 10 task programs (JSON round trips, a config file update, date arithmetic, a text report, an HTTP client against the test server, and a program using a package), each written from the task statement and `vo doc` output only, **When** their tests run, **Then** all pass.
2. **Given** every item `vo doc` lists, **When** the conformance check builds a call exactly as documented, **Then** it checks clean; **When** it builds calls with one wrong argument type or count, **Then** each is rejected.

---

### Edge Cases

- **Name clashes**: a program's own function named like a library item it does not import is fine; importing a library item that clashes with a local name is the existing duplicate-definition error. Library modules are only reached through `import std....`; nothing new becomes a builtin.
- **JSON numbers**: integers within the vo `int` range stay exact; others become floats; `-0`, exponents and very long digit strings are handled; NaN and infinities cannot be written (an `err`).
- **JSON strings**: `\uXXXX` escapes, including surrogate pairs, decode to UTF-8; unpaired surrogates and invalid UTF-8 input are `err`; output escapes control characters.
- **Duplicate JSON object keys**: the last one wins, and the order is the position of the first.
- **Time**: years 1–9999 are supported; dates and texts outside that range are `err` from `to_ms`, `date` and `parse`, and `from_ms` is defined for every time (it never fails); time zones other than UTC are out of scope, and parsing accepts `Z` and numeric offsets, converting to UTC.
- **Text widths**: padding counts characters (code points); combining characters and double-width characters are not special.
- **Packages**:
  - the hash covers every file under the package directory that ends in `.vo`, with its relative path, in a fixed order;
  - a package directory inside the program's own directory is allowed;
  - two packages with the same name are an error;
  - a package import cycle is the existing cycle error;
  - a package's own `vo.effects` is not used: the program's record lists everything.
- **HTTP**:
  - host names resolve through the system's hosts file, then a DNS server from the system's resolver configuration;
  - responses larger than a documented limit are `err`;
  - headers keep their order; names compare case-insensitively;
  - only `GET` and `POST` (with a body and a content type);
  - IPv6 literal hosts are out of scope unless the plan finds them cheap.
- **Determinism**: JSON, time formatting and text helpers are pure; random and clock helpers are fixed by `--seed` and `--clock-ms`; HTTP is the one new source of outside input, and it is recorded and replayed like the others (spec 006).
- **Tests**: `vo test` gives HTTP the same treatment as files: a test needs `uses net(...)` to make requests.

## Requirements *(mandatory)*

### Functional Requirements

**Documentation of signatures**

- **FR-001**: The toolchain MUST provide `vo doc` with three levels: all modules (and the builtins), one module, one item. Each item shows its exact signature (type parameters, parameters with names and types, return type, effects), a one-line description and, at the item level, an example.
- **FR-002**: `vo doc` MUST derive signatures from the same declarations the checker uses, so they cannot differ from what the checker accepts. `--json` MUST give one record per item with the signature as structured data. Builtin signatures come from the reference's builtin table, which is the checker's documented contract for builtins; the conformance check (SC-002) holds both to the checker.
- **FR-003**: `vo doc NAME` MUST document a program's package modules the same way (`vo doc money.format`), finding packages through the nearest manifest from the working directory.
- **FR-004**: An unknown module or item MUST be a structured error with a new code and the closest existing name.

**Standard library**

- **FR-005**: The library MUST be organized in modules reached with `import std.MODULE.ITEM;` (`std.json`, `std.time`, `std.text`, `std.random`, `std.http`), using the existing import rules. The language reference MUST list the modules in one short section and point to `vo doc` for their items; the items' documentation lives in `vo doc`, not in the reference.
- **FR-006**: `std.json` MUST provide: a value type representing any JSON value with object keys in order; parsing text into it (an `err` with message, line and column for invalid input); writing it as compact or indented text; and helpers to read a field or element as a given kind, returning `err` naming the field. Parsing MUST accept exactly RFC 8259 JSON and limit nesting depth.
- **FR-007**: `std.time` MUST provide: conversion between milliseconds since the epoch and UTC calendar fields (including weekday); ISO 8601 formatting and parsing (`Z` and numeric offsets); adding and subtracting durations; and the current time, which needs `clock` and is fixed by `--clock-ms`.
- **FR-008**: `std.text` MUST provide at least: replace (all occurrences), starts with, ends with, contains, repeat, pad left and right to a width in characters, split into lines, and split into words on whitespace.
- **FR-009**: `std.random` MUST provide: a random element of a list, a shuffled copy of a list, and a float in [0, 1); each needs `random` and is fixed by `--seed`.
- **FR-010**: `std.http` MUST provide `GET` and `POST` to `http://` URLs, returning a result with the status, headers (in order) and body as text. It MUST need the `net` effect; the request's host MUST be checked at run time against the running function's `net` scope (R0010 outside it). `https://` URLs and anything else that cannot be sent MUST return `err` without sending. Network failures, malformed responses and responses over the size limit MUST return `err`.
- **FR-011**: HTTP responses (and failures) MUST be part of a `vo run --record` recording and MUST be replayed by `vo replay` without the network.
- **FR-012**: Every library function MUST have defined behavior on every input: `err` results for expected failures, and documented runtime errors only where the function's description says so.

**Packages**

- **FR-013**: A program MAY have a package manifest next to its entry file (found like `vo.effects`, the nearest one in its directory or above) listing packages by name, directory and content hash. Imports whose first segment is a package name MUST resolve into that package's directory.
- **FR-014**: The checker MUST compute each used package's content hash (a cryptographic hash over its `.vo` files' relative paths and contents, in a fixed order) and MUST refuse to check the program when it differs from the manifest (new code, naming the package, the expected and the actual hash). Nothing about packages MUST be resolved at run time.
- **FR-015**: A command MUST add or update a manifest entry from a directory, computing the hash, and print the change. Only that command changes the manifest.
- **FR-016**: Package functions MUST be part of the program for effects: their effects are checked, they appear in `vo.effects` with their package-qualified path, and a capability a package function gains reaches its callers' records as for local code.
- **FR-017**: Errors MUST be reported for: an import of an unknown package, a missing package directory, two packages with one name, and a malformed manifest (each with a code; with a fix where one correction is unambiguous).

**Language, reference, catalog and compatibility**

- **FR-018**: The reference MUST document the module system for `std`, the manifest and the new commands, with executed examples; the increase MUST be stated and stay within the 15,000-token budget.
- **FR-019**: Every new code MUST have a catalog entry and at least one golden case.
- **FR-020**: Existing programs MUST behave exactly as before; the names `std` and the package names in a manifest become reserved as the first segment of an import path only.
- **FR-021**: Programs that use no library module and no package MUST run no slower than before this spec (measured as in spec 006).

**Evaluation**

- **FR-022**: The project MUST include at least 10 task programs that use the library (with their task statements and tests), written from the statements and `vo doc` output only, and a conformance check that compiles every documented item as documented and with one deliberate mistake per parameter.

### Key Entities

- **Module**: a named group of library items under `std`, imported item by item.
- **Item**: a function, record or variant with an exact signature, effects, a one-line description and an example.
- **JSON value**: a vo value that represents any JSON document, with object keys in order.
- **Calendar time**: UTC year, month, day, hour, minute, second, millisecond and weekday, convertible to and from epoch milliseconds.
- **HTTP response**: status, headers in order, and body.
- **Package**: a named directory of vo files used by a program.
- **Manifest** (`vo.packages`): the list of a program's packages, each with its directory and content hash. The spec and plan call it the manifest throughout.
- **Content hash**: a cryptographic hash over a package's `.vo` files' relative paths and contents.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: At least 10 task programs written only from task statements and `vo doc` output (covering JSON, time, text, random, HTTP and a package) pass all their tests.
- **SC-002**: For every item `vo doc` lists (100%), a call written exactly as documented checks clean, and every call with one wrong argument type or a wrong argument count is rejected.
- **SC-003**: A JSON corpus of at least 100 valid and 100 invalid documents: every valid one parses to the expected value and survives a write-and-parse round trip unchanged; every invalid one returns `err` with a position; no input crashes or hangs (also under the malformed-input fuzzer).
- **SC-004**: Every one-byte change, added file, removed file and renamed file in a package (at least 20 cases) is detected at check time, before anything runs.
- **SC-005**: Time conversions agree with an independent reference implementation on at least 10,000 generated times across years 1–9999, including leap days and times before 1970.
- **SC-006**: HTTP programs against the local test server get the expected status, headers and body in every test scenario (plain, chunked, error status, redirect, refused, slow), and every recorded HTTP run replays with byte-identical output with the server stopped.
- **SC-007**: Programs that use no library or package run within 5% of the spec 006 binary on spec 004's speed benchmark; the existing corpus and the 703 bench2 reference runs are byte-identical.
- **SC-008**: The language reference stays under 15,000 tokens and grows by at most 2,000 tokens; every example in it passes.
- **SC-009**: `make test`, the fuzzer, the determinism check and the collector stress pass all pass, with the new cases included.

## Assumptions

- **The agent benchmark run is a follow-up**, as for specs 005 and 006 (clarified with the user).
- **The task programs for SC-001 are written by a separate agent** from the task statements and `vo doc` output only, without reading the library's source, so they test the documentation rather than the implementation.
- **No downloading**: packages come from local directories (clarified). A fetch command, a registry and version ranges are out of scope; the manifest pins exact content.
- **One manifest per program**: transitive manifests are not read; a package may import other packages only if they are in the program's manifest.
- **UTC only**: the time module works in UTC; local time zones and daylight saving time are out of scope.
- **HTTP/1.1 without TLS, keep-alive or proxies** (clarified); one request per connection; redirects are returned, not followed.
- **JSON numbers** are represented as integers when they are integers in range and as floats otherwise.
- **Library code may be written in vo** where that is natural (for example JSON and time), with primitives provided by the runtime where vo cannot express them (sockets, the real-time clock); the plan decides.
- **The library's documentation does not count against the reference budget**: the reference lists the modules and points to `vo doc`, which reads its descriptions from the library itself.

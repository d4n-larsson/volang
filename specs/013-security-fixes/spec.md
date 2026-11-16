# Feature Specification: Security Fixes from Review 1

**Feature Branch**: `013-security-fixes`

**Created**: 2026-11-16

**Status**: Draft

**Input**: User description: "Security fixes from docs/security-review-1.md: make effects hold as a security boundary and fix every finding of review 1 (C1-C5, H1-H11, M1-M5, L1-L11), grouped by its root causes R1-R5: (R1) every effect is also checked at run time against the root grant (main's or the test's clause), never a requirement set used as a grant, and generic inference checks nested function types and variant payloads; (R2) paths with NUL are refused, a failed getcwd fails closed, and scoped files are opened through directory descriptors from one start directory without following links after the check; (R3) std.http's request and response parsing is total (every input gives a value or err, invalid UTF-8 included), request parsing runs under the handler guard and deadline, with one framing parser and a total request size cap; (R4) symbolic links in imports, packages and temporary files are handled safely; (R5) a server restores all runtime state after a failed request and the collector is never interrupted; plus C4 (sort_by snapshot), C5 (replay program path), H11/L10 (stack checks in equality and rendering), M5 (header injection) and the remaining Lows. Each finding's reproduction becomes a failing golden case first."

Background: [docs/security.md](../../docs/security.md) promises that a vo program can do nothing outside
its grant, whatever the rest of its code does. [Review 1](../../docs/security-review-1.md) showed that the
promise does not hold: 5 Critical, 11 High, 5 Medium and 11 Low findings, each with a reproduction. This
spec makes the promise true for every finding. Finding IDs (C1, H7, ...) refer to the review.

## User Scenarios & Testing *(mandatory)*

The users are the people who run vo programs written by others (agents, colleagues, package authors),
the people who run vo servers, and the agents and people who use the toolchain on code they did not write.

### User Story 1 - A program cannot act outside the grant its runner reviewed (Priority: P1)

A person reads `main`'s `uses` clause, sees `console, file.read("data/")`, and runs an agent's program.
Whatever the program contains (generic functions, function values in lists or variants, anonymous
functions, callees with their own narrower scopes, symlinks under `data/`, odd path strings), it can
only print and read under `data/`. Today five different ways around that exist (C1, C2, C3, H1, H2).

**Why this priority**: it is the security promise itself; without it every other fix is moot.

**Independent Test**: the review's reproductions for C1, C2, C3, H1 and H2 as golden cases: each must
check with an error or stop with R0010 instead of reaching outside the grant.

**Acceptance Scenarios**:

1. **Given** a program that passes a function needing `file.read` inside a `list`, `map`, `result`,
   generic record or generic variant to a context that does not declare it (C2), **When** checked,
   **Then** it is a check error naming the missing effect; no such program runs.
2. **Given** `main` declares `file.read("data/")` and passes an anonymous function reading `/etc/hostname`
   through a generic `each[T, E: effects]` (C3), **When** run, **Then** the read stops with R0010.
3. **Given** a function declaring `file.read("data/link/")` called from a `main` declaring
   `file.read("data/")`, with `data/link` a symlink to `/` (H1), **When** it reads a file outside `data/`,
   **Then** R0010.
4. **Given** a path containing a NUL byte (C1), **When** a program reads, writes or checks it, **Then**
   R0010 (or an `err`), never an access to a different file.
5. **Given** the program starts in a directory whose path cannot be read (H2), **When** it uses any
   scoped file effect, **Then** every scoped access is refused; nothing is allowed by default.
6. **Given** any program, **When** it performs an effect, **Then** the effect is allowed only if both the
   running function's grant and the root grant (`main`'s clause for `vo run`, the test's clause for
   `vo test`) allow it.

---

### User Story 2 - Scoped file access is not redirected by another process (Priority: P1)

A program with `file.read("data/")` runs on a machine where another process (another user, a script)
can rename directories or swap symlinks. The access the program makes is to the file that was checked,
or it fails; it never lands outside `data/` (H3, H4).

**Why this priority**: the review demonstrated it deterministically (H3) and with a 34% race win (H4).

**Independent Test**: the review's rename and race demonstrations, repeated many times: zero accesses
outside the scope.

**Acceptance Scenarios**:

1. **Given** the working directory is renamed and replaced while a program runs (H3), **When** it reads a
   relative path in its scope, **Then** it reads the file in the directory it started in, or fails;
   never a file outside the scope.
2. **Given** a symlink inside the scope is swapped between the check and the access (H4), **When** the
   program reads or writes it, **Then** the access goes to a file inside the scope or fails; over 1,000
   attempts with an aggressive swapper, zero accesses outside.
3. **Given** a symlink inside the scope that points to another file inside the scope, **When** read,
   **Then** it works as today (links are not forbidden, only escapes).

---

### User Story 3 - The runtime cannot be corrupted or crashed by vo code (Priority: P1)

Hostile vo code cannot corrupt the toolchain's memory (C4) or crash it (H11, L10); every such case is a
runtime error with a code.

**Why this priority**: memory corruption under the toolchain is code execution, the end of every boundary.

**Independent Test**: the review's `sort_by` shrink case and cyclic/deep equality and rendering cases,
under the sanitizer build: no report, a clean runtime error.

**Acceptance Scenarios**:

1. **Given** a `sort_by` key function that changes the list being sorted (C4), **When** run, **Then** a
   runtime error says the list changed, and no memory outside the list is read or written.
2. **Given** `==` on two cyclic values, or on two values nested millions of levels deep (H11), **When**
   run, **Then** it gives an answer or R0005, never a crash.
3. **Given** `expect_eq` on a value nested a million levels deep (L10), **When** the test fails and the
   value is rendered, **Then** R0005 (or a capped rendering), not an internal error.

---

### User Story 4 - A vo server survives any request (Priority: P1)

A server built with `std.http.serve` keeps answering whatever clients send: invalid bytes, odd chunked
bodies, endless chunk extensions, huge or slow requests (H7, H8, H9). After a request whose handler runs
out of memory or time, the server is exactly as healthy as before (H10, L11).

**Why this priority**: today one request of under 100 bytes stops any vo server.

**Independent Test**: the review's attack scripts against a running server: after each, the server
answers the next request correctly; its memory stays bounded.

**Acceptance Scenarios**:

1. **Given** any request bytes (invalid UTF-8 in the request line, headers or body; malformed chunked
   framing), **When** sent, **Then** the server answers 400 (or the handler's response) and keeps serving.
2. **Given** a chunked body of many small chunks within the size limit (H8), **When** sent, **Then** it is
   handled within the per-request time limit (or answered with an error at the limit) and the server
   keeps serving.
3. **Given** a request whose framing grows without end (chunk extensions, trailers) (H9), **When** sent,
   **Then** it is cut off at the total request size cap or the read time limit with an error answer, and
   memory stays bounded; a stop signal is honored meanwhile.
4. **Given** a handler that fails with an out-of-memory or time-limit error (H10), **When** the next
   1,000 requests arrive, **Then** they are answered with memory bounded by `--max-memory`, as before the
   failure; later error reports carry no stale fields (L11).
5. **Given** a handler that echoes request data into a response header (M5), **When** the data contains
   CR, LF or NUL, **Then** that header is not sent as written (the response is refused or the header
   dropped), so no header can be injected.

---

### User Story 5 - Hostile servers, recordings and packages cannot reach outside (Priority: P2)

A vo client talking to a hostile server gets `err` values, never a crash (M2). `vo replay` of a recording
someone sent loads only the program it is meant to replay, and checks every file event against the
grant without touching the file system (C5, M1, L2, L6). A pinned package runs only code its pin covers
(H5, H6, L3).

**Why this priority**: each is a real attack on a documented feature, but needs the user to act (replay a
file, use a package, call a hostile server).

**Independent Test**: the review's reproductions for C5, M1, M2, H5, H6, L2, L3, L6.

**Acceptance Scenarios**:

1. **Given** any response bytes from a server (M2), **When** a program calls `get` or `post`, **Then** it
   receives an `ok` response or an `err`, and keeps running.
2. **Given** a recording whose program field names an absolute path, a path with `..`, a non-`.vo` file,
   a device or a FIFO (C5, L4), **When** replayed, **Then** it is refused as an invalid recording and no
   such file is opened.
3. **Given** a recording with a forged `ok` for a file outside the grant (M1), **When** replayed, **Then**
   R0010, as in the recorded run; and replay resolves nothing on the real file system (L2).
4. **Given** a package directory with a symlinked or unreadable subdirectory (H5), or a package file
   replaced between pinning check and loading (H6), **When** the program is checked or run, **Then** only
   the bytes the pin covers are loaded, or the package is refused.
5. **Given** malformed recordings or manifests (L3, L6), **When** read, **Then** a clean T0008/E0703 with a
   bounded amount of memory.

---

### User Story 6 - Tools are safe on a hostile source tree (Priority: P2)

Running `vo check`, `fmt`, `doc`, `query`, `edit`, `effects --update` or `package` on a tree someone else
wrote cannot overwrite files outside it (M3), print files outside it (M4), control the terminal (L7) or
hang (L4).

**Why this priority**: agents and editors run these commands on every repository they touch.

**Independent Test**: the review's reproductions for M3, M4, L4 and L7.

**Acceptance Scenarios**:

1. **Given** a symlink planted where a command writes its temporary file (M3), **When** the command runs,
   **Then** it refuses (or writes elsewhere) and no file outside the tree changes.
2. **Given** an import whose file is a symlink leading outside the program's tree (M4), **When** checked,
   **Then** it is an error naming the link (clarified: symlinks are contained, allowed only when they
   resolve inside the program's tree, the entry file's directory, or the package's own directory), and no
   content of a file outside the tree is printed. A link that stays inside works as today.
3. **Given** control bytes in source (L7), **When** a diagnostic shows the line, **Then** they appear
   escaped.
4. **Given** a FIFO or device named `*.vo` (L4), **When** loaded, **Then** it is refused, without blocking.

---

### Edge Cases

- **Existing programs with honest grants** keep working: the root-grant check only refuses what was
  already outside the grant. A program whose callee declares a scope wider than `main`'s is already a
  check error; nothing new is refused for programs that check today and stay within their grant.
- **Library functions and anonymous functions** keep running with their caller's grant; the root-grant
  check applies to them like to everything else.
- **Tests** use the test's clause as the root grant; properties too.
- **Replay** checks file events lexically against the grant (no file system access); recordings made
  before this spec still replay if their program path is a plain relative `.vo` path.
- **Symlinks inside a file scope that point inside it** keep working (User Story 2, scenario 3).
- **The server's request size cap** is at least the current body limit (16 MiB) plus the head limit, so
  requests accepted today stay accepted.

## Requirements *(mandatory)*

### Functional Requirements

**R1: the grant (C2, C3, H1, L1)**

- **FR-001**: Every effectful operation MUST be allowed by both the running function's grant and the root
  grant (`main`'s clause in `vo run` and `vo replay`, the test's or property's clause in `vo test`);
  otherwise R0010.
- **FR-002**: A requirement set (effects some code needs, in any scope) MUST never act as a grant.
- **FR-003**: The checker MUST check function types inside any type (lists, maps, results, records,
  variants, generic arguments, variant payloads) for effects, so no function value carries an effect into
  a context that does not declare it (C2).
- **FR-004**: A literal path, host, variable name or address outside the caller's scopes SHOULD be a check
  error (L1); run-time enforcement remains the guarantee.

**R2: paths (C1, H2, H3, H4)**

- **FR-005**: A path containing a NUL byte MUST be refused by every file effect (R0010) (C1).
- **FR-006**: If the start directory cannot be determined, every scoped file access MUST be refused (H2).
- **FR-007**: A scoped file access MUST reach the file that the scope check accepted, relative to the
  directory the program started in, or fail; renaming directories or swapping links while the program
  runs MUST NOT redirect it outside the scope (H3, H4).

**R3: HTTP parsing (H7, H8, H9, M2, M5, L9)**

- **FR-008**: Parsing a request MUST give a request value or a 400 answer for every input, and MUST run
  under the per-request time limit and the handler's recovery (H7, H8).
- **FR-009**: Parsing a response in `get`/`post` MUST give a response value or an `err` for every input
  (M2).
- **FR-010**: Request framing MUST be decided once (the same rules for where a body ends and what it
  contains), and every request MUST be bounded by a total size cap and the read time limit, checked
  continuously; a stop signal MUST be honored while reading (H9, L9).
- **FR-011**: Response header names and values containing CR, LF or NUL MUST NOT be sent as written (M5).

**R4: symbolic links in the toolchain (H5, H6, M3, M4, L3, L4)**

- **FR-012**: Commands that write files (fmt, edit, effects --update, package) MUST NOT write through a
  symlink they did not create, and MUST NOT overwrite a file they did not intend to (M3).
- **FR-013**: Imports and package loading MUST NOT read files outside the program's tree or the package's
  directory through symlinks (M4, H5): a symlink is allowed when it resolves inside the program's tree
  (the entry file's directory) or the package's own directory, and is an error naming the link otherwise.
- **FR-014**: A package MUST run only the bytes its pin was computed from; an unreadable directory in a
  package MUST refuse the package (H5, H6).
- **FR-015**: Only regular files MUST be read as source, recordings or package files; a FIFO or device is
  refused without blocking (L4). Manifest tokens with control bytes MUST be refused (L3).

**R5: server state (H10, L11)**

- **FR-016**: After a handler fails in any way, the runtime state (memory management, error report
  fields, limits) MUST be as before the request (H10, L11).
- **FR-017**: A time-limit error MUST never interrupt memory management mid-way (H10).

**Memory safety and limits (C4, H11, L10, L6)**

- **FR-018**: `sort_by` MUST NOT read or write outside its list when the key function changes the list;
  that change MUST be a runtime error (C4).
- **FR-019**: Equality and rendering of values MUST end in a result or R0005 for any depth, and terminate
  for cyclic values (H11, L10).
- **FR-020**: Reading a recording MUST be bounded by `--max-memory` and reject malformed literals and
  overflowing integers with T0008 (L6).

**Replay and packages (C5, M1, L2, L5)**

- **FR-021**: `vo replay` MUST load only a program named by a relative path ending in `.vo` without `..`,
  and only a regular file (C5).
- **FR-022**: Replay MUST check every file event against the grant as the recorded run did, without
  resolving paths on the real file system (M1, L2).
- **FR-023**: E0701's suggested command MUST name the directory that is pinned (L5).

**Hardening (L7, L8)**

- **FR-024**: Human-readable diagnostics MUST show control bytes of the source escaped (L7).
- **FR-025**: DNS queries MUST use an unpredictable transaction ID and accept only answers to the question
  asked (L8).

**Process**

- **FR-026**: Every finding of review 1 MUST have a golden or step case built from its reproduction that
  fails before its fix and passes after; `docs/security-review-1.md` MUST record each finding as fixed
  with the case that proves it.
- **FR-027**: Programs that check and stay within their grant today MUST behave byte-identically; the
  full gate MUST pass on x86-64, ARM64 and RISC-V; the reference MUST state the root-grant rule (at most
  150 tokens added).

### Key Entities

- **Root grant**: the effect set a person reviewed: `main`'s `uses` clause for `vo run`/`vo replay`, the
  test's or property's clause for `vo test`.
- **Running grant**: the effect set of the function executing (its declaration, or its caller's for
  library and anonymous functions).
- **Finding**: an entry of review 1 with its reproduction; fixed when its case passes.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of review 1's findings (32) have a case from their reproduction, failing before and
  passing after the fix.
- **SC-002**: A second review pass over the review's attack scripts and programs (re-run unchanged)
  reproduces 0 of the 32 findings.
- **SC-003**: The symlink race (H4) and directory-rename (H3) demonstrations, each repeated 1,000 times,
  reach a file outside the scope 0 times.
- **SC-004**: A vo server subjected to every request attack of the review in sequence answers a normal
  request correctly after each, with memory under its `--max-memory` limit throughout.
- **SC-005**: Every existing program, example and golden case that stays within its grant gives
  byte-identical results; the full gate passes on all three platforms; 10,000 generated programs give 0
  disagreements with the previous release.

## Assumptions

- **Notes of the review stay design questions**, not part of this spec: ports in `net` scopes, checking
  DNS answers against private addresses (rebinding), invalid UTF-8 in strings read from input in
  general, and polymorphic-recursion cost. They are recorded as follow-ups.
- **Symlinks are contained, not banned (clarified 2026-11-16)**: links that stay inside the program's
  tree or the package's directory keep working; package pins cover everything a package can load.
- **The previous release is the oracle** for "behaves exactly as before".
- **New system calls may be added** to the platform layer where the fixes need them (for example
  descriptor-relative opening, a random source for DNS), each listed in `docs/security.md`'s count.
- **Grant semantics do not change for honest programs**: the root-grant check refuses only what already
  violated the documented promise.

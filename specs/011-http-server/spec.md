# Feature Specification: HTTP Server Support in `std.http`

**Feature Branch**: `011-http-server`

**Created**: 2026-11-15

**Status**: Draft

**Input**: User description: "create a spec for vo server support in std.http"

Background: `std.http` (spec 007) is a client: `get` and `post` under the `net` effect, scoped by host,
recorded and replayed (spec 009 added request matching). A vo program cannot answer HTTP requests, so
small local services, mock servers for tests and dev tools still need another language; the examples
use Python's `http.server` as their "service".

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–010: AI coding agents that write vo programs from the reference
and fix them from diagnostics, and the people who review and run those programs.

### User Story 1 - A program answers HTTP requests (Priority: P1)

An agent writes a small JSON service: a handler function receives each request (method, path, query,
headers, body) and returns a response (status, headers, body); `serve` runs it on a port. A browser,
`curl` or a vo client gets the answers. The handler is an ordinary function, so routing is a `match` or
`if` on the path, and JSON comes from `std.json`.

**Why this priority**: it is the feature; everything else makes it safe, testable and reproducible.

**Independent Test**: step cases that start a vo server, send requests from the test harness (several
paths, methods, a body, headers), check every response byte for byte, and stop it.

**Acceptance Scenarios**:

1. **Given** a program whose handler returns `200` with a body for `GET /hello`, **When** it is served
   and a client sends `GET /hello`, **Then** the client receives that status, the handler's headers plus
   `Content-Length`, and the body.
2. **Given** a `POST` with a body and a `Content-Type`, **When** the handler reads the request, **Then**
   it sees the method, the path, the query string, the headers in their order and the whole body.
3. **Given** a handler that returns a status other than 200 (404, 500, a redirect), **When** served,
   **Then** the client receives exactly that status and the handler's headers.
4. **Given** a request that is not valid HTTP/1.x (a garbage line, a body longer than announced, headers
   over the size limit), **When** it arrives, **Then** the server answers `400` itself without calling the
   handler, and keeps serving.
5. **Given** a handler that stops with a runtime error (clarified: answer and keep serving), **When** a
   request triggers it, **Then** that client receives `500`, the runtime error report (code, message,
   trace) is written to stderr (and to the recording), and the server keeps answering other requests.

---

### User Story 2 - Serving is a declared, scoped effect (Priority: P1)

A reviewer reads `main`'s `uses` clause and `vo.effects` and knows the program opens a server, on which
port, and that the handler cannot do more than its own declaration allows. A program that does not declare
serving cannot open a port.

**Why this priority**: effects are vo's safety story (Principle III, spec 002); a server is the most
outward-facing thing a program can do, so it must be visible and bounded like the rest.

**Independent Test**: golden cases checking the effect is required (E0501 with the fix adding it),
scoped (a port outside the scope is R0010 at run time), and recorded in `vo.effects`.

**Acceptance Scenarios**:

1. **Given** a function that calls `serve` without declaring the serving effect, **When** checked,
   **Then** E0501 names the missing effect with a fix that adds it.
2. **Given** `main` declares serving on port 8000, **When** the program asks to serve on 9000, **Then**
   it stops with R0010 before opening anything.
3. **Given** a handler that reads files, **When** checked, **Then** the function calling `serve` must
   declare the handler's effects as well (the existing effect-parameter rule), and `vo.effects` shows them.
4. **Given** a serving scope that names only a port, **When** the server starts, **Then** it listens on
   the loopback interface (`127.0.0.1`) only, reachable from this machine (clarified).
5. **Given** a scope that explicitly names all interfaces for a port, **When** the server starts,
   **Then** it is reachable from the network, and that scope is visible in the `uses` clause and in
   `vo.effects` like any other.

---

### User Story 3 - Handlers are tested without a network (Priority: P1)

An agent tests its service with `vo test`: a test builds a request value, calls the handler, and checks
the response, with no port and no server. Property tests do the same with generated paths and bodies.

**Why this priority**: it is how agents get fast, deterministic feedback (Principle IV); a server that
can only be tested over sockets would be tested much less.

**Independent Test**: golden test files whose tests call handlers directly, pass, and fail with the usual
reports when a handler is wrong.

**Acceptance Scenarios**:

1. **Given** a handler and a test that builds `new Request { method: "GET", path: "/x", ... }`, **When**
   `vo test` runs, **Then** the handler runs as an ordinary function and the test needs no serving effect.
2. **Given** a pure handler, **When** it is used in a property test, **Then** it behaves like any pure
   function (same request, same response).

---

### User Story 4 - A server run can be recorded and replayed (Priority: P2)

A developer runs the service with `--record`, reproduces a bug with real traffic, then replays the
recording: the program receives exactly the same requests in the same order, with no port opened, and its
responses can be compared with the recorded ones. A changed program that answers differently is visible.

**Why this priority**: record/replay is vo's debugging guarantee (spec 006) and must not stop at the first
server; it follows the serving itself.

**Independent Test**: a step case that serves, sends requests, stops, then replays the recording and
compares the responses; a changed handler gives a reported difference.

**Acceptance Scenarios**:

1. **Given** a recorded server run, **When** replayed, **Then** the handler receives every recorded
   request in order, no port is opened, and the program's output is byte-identical to the recorded run's.
2. **Given** a changed handler, **When** replayed, **Then** every response that differs from the recorded
   one is reported (which request, recorded and new status and body).
3. **Given** a recording made while the handler itself made HTTP requests or read files, **When**
   replayed, **Then** those are replayed too, as in any other recording.

---

### User Story 5 - Stopping and timeouts (Priority: P2)

A server is a program that does not finish on its own; tools and tests need it to stop in a defined way.

**Why this priority**: a server that `vo run --timeout` kills after 10 seconds, or that hangs a test
harness, is unusable; but the rule is small once serving exists.

**Independent Test**: step cases for each way the server stops and for a slow handler.

**Acceptance Scenarios**:

1. **Given** `vo run` of a serving program with the default `--timeout`, **When** it serves, **Then**
   the time limit applies to each request's handling, not to the server's lifetime, so a server can run
   for hours.
2. **Given** a handler that loops forever, **When** a request triggers it, **Then** that request is
   stopped at the time limit (R0009) as a runtime error of the handler (see User Story 1, scenario 5).
3. **Given** the server is told to stop (Ctrl-C or a termination signal), **When** it receives it,
   **Then** it finishes the request in progress, closes the port and exits with status 0.

---

### Edge Cases

- **Port in use or not allowed** (another process, a port below 1024 without privileges): `serve` returns
  `err` with the reason; nothing is served.
- **One request at a time**: vo has no concurrency; requests are handled in arrival order and others wait
  in the system's queue. Throughput is not a goal.
- **Keep-alive**: each response closes its connection (`Connection: close`), as the client does; HTTP/1.1
  clients handle that.
- **Large bodies**: request bodies and headers have documented size limits (as the client caps responses
  at 16 MiB); a request over them is `413`/`431` answered by the server without calling the handler.
- **Chunked request bodies** are decoded before the handler sees them; responses are sent with
  `Content-Length`, never chunked.
- **HTTPS** is out of scope, as for the client.
- **`HEAD`**: the server answers with the handler's headers for the same `GET` and no body.
- **The handler's own effects**: a handler that calls `std.http.get` needs `net` with that host, declared
  by the function that calls `serve`; the grant rules of spec 007 apply unchanged.

## Requirements *(mandatory)*

### Functional Requirements

**Serving**

- **FR-001**: `std.http` MUST provide a way to serve HTTP/1.x on a port with a handler function that
  takes a request and returns a response. Requests carry the method, the path, the query string, the
  headers in their order and the body; responses carry a status, headers and a body.
- **FR-002**: The server MUST answer malformed requests (`400`), requests over the size limits (`413`,
  `431`) and `HEAD` requests without calling the handler where the contract says so, and MUST keep
  serving after them.
- **FR-003**: A handler that stops with a runtime error (including R0009 at the time limit) MUST make the
  server answer that request with `500`, write the error report to stderr and the recording, and keep
  serving; the exit status of the run is not changed by it.

**Effects**

- **FR-004**: Serving MUST require a declared effect scoped by port, checked statically (E0501/E0503,
  with fixes) and at run time (R0010), listed in `vo.effects` and the reference's effect table.
- **FR-005**: The function that calls `serve` MUST declare the handler's effects (existing effect
  parameters); handlers run with that grant.
- **FR-006**: A serving scope that names only a port MUST listen on the loopback interface; listening on
  all interfaces MUST need a scope that says so explicitly.

**Testing, record and replay**

- **FR-007**: Request and response MUST be ordinary records that tests and properties can build and
  compare, so handlers are testable without any effect.
- **FR-008**: `vo run --record` MUST save every request the server received and every response it sent;
  `vo replay` MUST feed the recorded requests to the handler in order without opening a port and MUST
  report every response that differs from the recorded one.

**Lifetime**

- **FR-009**: `--timeout` MUST bound each request's handling, not the server's lifetime; a termination
  signal MUST stop the server cleanly with exit status 0.

**Documentation and compatibility**

- **FR-010**: The reference MUST document serving (the effect, the functions, the limits) within the
  token budget; `vo doc std.http` MUST document every new item with an example; every new code MUST have a
  catalog entry and a golden or step case.
- **FR-011**: Existing programs, recordings and the client part of `std.http` MUST behave exactly as
  before. The toolchain MUST keep passing its full quality gate on x86-64, ARM64 and RISC-V.

### Key Entities

- **Request**: method, path, query string, headers (in order), body.
- **Response**: status, headers, body.
- **Handler**: a function from a request to a response, with the effects its caller declares.
- **Server run (in a recording)**: the sequence of received requests and sent responses.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: An agent given only the reference writes a working JSON service with two routes and a 404
  for the rest, and tests its handler with `vo test`, in at most one fix round (isolated agent runs, at
  least 3).
- **SC-002**: 100% of the step cases (routes, methods, bodies, headers, malformed and oversized requests,
  `HEAD`) get byte-exact responses from a running vo server.
- **SC-003**: A server keeps answering for at least 10 minutes and 10,000 sequential requests under
  `vo run` with the default `--timeout`, and stops with exit status 0 on a termination signal.
- **SC-004**: A recorded server run replays with byte-identical output, and a changed handler's
  differences are reported for 100% of the changed responses.
- **SC-005**: The `status-board` example's service can be a vo program instead of Python, and the example
  still records and replays.
- **SC-006**: The reference grows by at most 500 tokens and stays under 15,000; the full gate passes on
  all three platforms, and existing programs give byte-identical results.

## Assumptions

- **Plain HTTP/1.x only**, one request at a time, `Connection: close`; no HTTPS, no HTTP/2, no websockets,
  no concurrency (vo has none).
- **No routing framework**: routing is ordinary vo code on the request's path and method; `std.http` may
  offer small helpers (a header lookup on requests, a JSON response) if they save every handler the same
  lines.
- **Static files** (a `python3 -m http.server` equivalent) are not part of this spec; a handler can serve
  files with `read_file` under its `file.read` scope.
- **The previous release is the oracle** for "behaves exactly as before".
- **Clarified 2026-11-15**: a failing handler answers `500` and the server keeps serving (not a stop of
  the program); servers listen on loopback unless the scope explicitly names all interfaces.
- **Ports**: numbers 1–65535; privileged ports follow the operating system's rules.

# Feature Specification: Web Package for Building APIs

**Feature Branch**: `029-web-package`

**Created**: 2026-11-20

**Status**: Draft (clarified 2026-11-20: routes as a route table value; record/JSON conversion derived by the toolchain with limits declared on fields; shipped as `std.web`)

**Input**: User description: "A web package for building APIs in vo: helpers and tools on top of
std.http.serve / serve_workers, taking inspiration from how the Java Spring framework (Spring Web MVC /
Spring Boot) works. Today a vo API is one handle(Request) -> Response function where the program splits the
path by hand, parses query strings, matches methods, decodes and validates JSON bodies, builds JSON and
error responses, and repeats the same 404/405/400 logic in every service (see examples/notes-api/api.vo
route()). Spring's ideas to adapt to vo (no reflection, no runtime annotations beyond @spec/@allow, no shared
mutable state, effects checked): declarative routing, request binding, validation, response helpers,
centralized error handling (RFC 9457 problem+json), filters/interceptors, dependency injection (the
serve_workers setup result as the application context), configuration, actuator (/health, /info), and
testing support (MockMvc: calling the router in tests, no network). Keep vo's guarantees: handlers stay
ordinary functions with checked effects and scopes, routing is pure and testable, everything deterministic
and replayable, one obvious way to do each thing (agent-first), errors as values. Measure: the notes-api
example rewritten with the package is markedly shorter with the same behavior and tests; 404/405/400
handling and JSON binding come for free; an agent builds a small CRUD API correctly on the first try more
often than with plain std.http."

Background: `std.http` gives a server one function from a request to a response. Everything an API needs
between the two is written by hand in every program: the notes-api example spends most of its 190-line
`api.vo` splitting the path, parsing the id, choosing 404 or 405, reading JSON fields one by one, building
error bodies and JSON responses. Agents writing APIs repeat this, and each repetition is a chance to answer
405 where 404 was due, forget a `Content-Type`, accept a body with a missing field, or leak an internal
error. Spring solves the same problem in Java with declared routes, typed binding of request parts,
validation, central error handling and test support; vo needs the same help without Spring's runtime
reflection, hidden injection or shared state.

## Clarifications

### Session 2026-11-20

- Q: How are routes declared: annotations on functions, a route table value, or both? → A: A route table
  value built in code (`get("/notes/{id}", show)` ...), one per application: the whole API is visible in one
  place and is ordinary, testable code; no route annotations.
- Q: Is the conversion between records and JSON derived by the toolchain or written per record? → A:
  Derived by the toolchain from the record's declaration, with limits (length, range, pattern, allowed
  values, list size, optional) declared on the fields; available to every program, not only web ones.
- Q: Standard library or a separate package? → A: Standard library: `std.web`, shipped with the toolchain,
  which may give it toolchain support.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Routes are declared, not parsed by hand (Priority: P1)

A developer (or agent) lists the API's operations: a method, a path pattern with named variables
(`/notes/{id}`) and the function that answers it. Matching, path variables, 404 for an unknown path and
405 (with the allowed methods) for a known path with another method come with it.

**Why this priority**: routing is the code every API repeats and gets wrong first; everything else hangs
on it.

**Independent Test**: an API of five operations on two resources; requests for each operation reach the
right function with the right path variables; an unknown path is 404, a wrong method is 405 listing the
allowed ones; no routing code in the program.

**Acceptance Scenarios**:

1. **Given** operations `GET /notes`, `POST /notes`, `GET /notes/{id}`, **When** `GET /notes/7` arrives,
   **Then** the `GET /notes/{id}` function is called with `id` = 7.
2. **Given** the same operations, **When** `PUT /notes/7` arrives, **Then** the answer is 405 with an
   `Allow` header listing `GET`, and the problem body says which methods are allowed.
3. **Given** the same operations, **When** `GET /users` arrives, **Then** the answer is 404 with a problem
   body.
4. **Given** two operations whose patterns can match the same request with the same method (`/notes/{id}`
   and `/notes/{name}`), **When** the program is checked (a route list written in one place) or the
   application starts, **Then** it is an error naming both, before
   anything runs.
5. **Given** a literal segment and a variable at the same place (`/notes/latest` and `/notes/{id}`),
   **When** `/notes/latest` arrives, **Then** the literal one answers.
6. **Given** a path variable declared as a number, **When** `/notes/abc` arrives, **Then** the answer is
   400 naming the variable, not 404 and not a call with a wrong value.

---

### User Story 2 - Request input arrives typed and checked (Priority: P1)

A function answering an operation receives its inputs as values: path variables, query parameters,
headers and the JSON body as a record of the program's own type. Input that does not fit (a missing
required field, a number given as text, an unknown enum value, a value outside its declared limits) never
reaches the function: the client gets 400 naming every field that is wrong and why.

**Why this priority**: binding and validating input by hand is most of today's handler code and the most
common source of 500s and silent bad data.

**Independent Test**: `POST /notes` with a body record `{text: str (1 to 200 characters), tags: list of
str (at most 5)}`; a good body reaches the function as that record; a missing `text`, an empty `text`, six
tags, a number for `text`, and a body that is not JSON each answer 400 with a problem naming the field(s).

**Acceptance Scenarios**:

1. **Given** an operation taking a body record, **When** the body is valid JSON matching it, **Then** the
   function receives the record with every field set.
2. **Given** the same operation, **When** the body lacks a required field and has another of the wrong
   type, **Then** the answer is 400 listing both fields with a reason each, and the function is not called.
3. **Given** a field declared optional, **When** it is missing, **Then** the function receives the
   declared "absent" value, never a made-up default.
4. **Given** declared limits on a field (length, range, pattern, allowed values, list size), **When** a
   value breaks them, **Then** the answer is 400 (or 422 for a well-formed body breaking a rule) naming the
   field and the limit.
5. **Given** query parameters `?limit=10&done=true` declared as a number and a yes/no value with defaults,
   **When** they are given, missing or malformed, **Then** the function gets the given values, the
   defaults, or the client gets 400 naming the parameter.
6. **Given** a body larger than the operation allows or with another content type than JSON, **When** it
   arrives, **Then** the answer is 413 or 415.

---

### User Story 3 - Answers and errors in one consistent form (Priority: P1)

Functions answer with values: a record becomes a JSON body with the right content type; helpers give the
common answers (200 with a body, 201 with a `Location`, 204, 404 for a missing thing). An error returned by
a function (`err(...)` or a declared application error such as "note not found" or "conflict") becomes an
RFC 9457 problem response, mapped to a status in one place in the program, not in every function. A runtime
error answers 500 with a problem body that shows no internals.

**Why this priority**: consistent error answers are what clients and agents rely on; mapping them in one
place removes most `match` boilerplate from handlers.

**Independent Test**: a function returning a record answers 200 JSON; one returning "created" answers 201
with `Location`; one returning the application error "not found" answers 404 problem+json; one that
divides by zero answers 500 problem+json with no stack trace in the body (the report still goes to stderr).

**Acceptance Scenarios**:

1. **Given** a function returning a record, **When** called, **Then** the response is 200 with
   `Content-Type: application/json` and the record's fields as JSON.
2. **Given** the program declares that its error "not_found" maps to 404 and "conflict" to 409, **When** a
   function returns them, **Then** the responses are 404 and 409 problems with the error's detail.
3. **Given** an error the program did not map, **When** returned, **Then** the response is 500 problem with
   a generic title, and the detail is not sent to the client unless the program says so.
4. **Given** any response the package produces for a failure (400, 404, 405, 413, 415, 422, 500), **When**
   inspected, **Then** it is `application/problem+json` with `type`, `title`, `status` and, where
   relevant, `detail` and the invalid fields.

---

### User Story 4 - The whole API is testable without a network (Priority: P1)

A test builds a request (method, path, headers, JSON body), sends it to the application as a value and
checks the response: status, headers, JSON fields. No port, no process, no effects beyond those the
operation itself uses; routing, binding, validation, middleware and error mapping all run as in
production.

**Why this priority**: vo's promise is tests that run anywhere and deterministically; an API package that
can only be tested through a socket would break it.

**Independent Test**: the notes API's tests drive every operation through the application value with an
in-memory database: each passes with `vo test`, with no `serve` effect declared.

**Acceptance Scenarios**:

1. **Given** an application value, **When** a test sends `GET /notes/1`, **Then** it gets exactly the
   response a client would get, without opening a port.
2. **Given** a test, **When** it checks a JSON response, **Then** it can read fields of the body as values
   and compare them (`status is 201`, `body field "id" is 3`) in one line each.
3. **Given** the same request twice in a test with the same context, **When** compared, **Then** the
   responses are identical.

---

### User Story 5 - Cross-cutting behavior in one place (Priority: P2)

Behavior that applies to many operations is written once and listed in order around the routes: a request
id on every response, a log line per request, CORS headers, an authentication check that answers 401
before the operation runs, timing. Each such step sees the request, may answer early, and may change the
response. Which steps apply to which routes is visible where the application is assembled.

**Why this priority**: needed by every production API, but an API works without it.

**Independent Test**: an application with three steps (request id, a bearer-token check on `/admin/...`
routes, a log line); a request without a token to `/admin/stats` gets 401 and the operation is not called;
every response carries `X-Request-Id`; each request logs one line.

**Acceptance Scenarios**:

1. **Given** steps A, B listed in that order, **When** a request arrives, **Then** A runs before B on the
   way in and after B on the way out.
2. **Given** a step that answers early (401), **When** it does, **Then** no later step and no operation
   runs, and the earlier steps still see the response.
3. **Given** a step that logs, **When** it runs, **Then** its effect (console, file) is declared and
   checked like any other; a step that is pure needs nothing.
4. **Given** CORS enabled for listed origins, **When** a preflight `OPTIONS` arrives from one of them,
   **Then** it is answered with the allowed methods and headers; from another origin, without them.

---

### User Story 6 - The application's context and configuration (Priority: P2)

What operations need (a database connection, settings, clients for other services) is built once per
worker and handed to every operation as one typed value: the program's own record, built by its setup
(as `serve_workers` does today). Settings come typed from environment variables and an optional settings
file, with defaults; a missing or malformed required setting stops the program at start with a message
naming it, before it serves.

**Why this priority**: Spring's injection and configuration make large apps manageable; vo needs the
explicit, checked equivalent, but small APIs work without it.

**Independent Test**: an application whose context holds a database and settings `port` (default 8080),
`max_notes` (required); without `MAX_NOTES` it stops at start naming it; with it, operations read both;
changing the settings file changes behavior without code changes.

**Acceptance Scenarios**:

1. **Given** a context record built by a setup, **When** any operation runs, **Then** it receives that
   worker's context; operations never reach anything not in it or in their own arguments.
2. **Given** a setting declared with a type and a default, **When** the environment and the settings file
   both give it, **Then** the environment wins; when neither does, the default is used.
3. **Given** a required setting missing or malformed, **When** the program starts, **Then** it stops with
   exit status 1 and one message naming every missing or malformed setting.
4. **Given** settings read at start, **When** the program is recorded and replayed, **Then** replay uses
   the recorded values.

---

### User Story 7 - Health and information endpoints (Priority: P3)

An application can expose `GET /health` (up, or down with the failing check, as JSON) and `GET /info`
(name and version) by listing them, as Spring Boot's actuator does; health checks are the
program's own functions (the database answers, a dependency is reachable).

**Why this priority**: expected by deployments and load balancers, small once the rest exists.

**Independent Test**: an application listing health with a database check: 200 `{"status":"up"}`; with
the check failing, 503 `{"status":"down","checks":{"database":"..."}}`.

**Acceptance Scenarios**:

1. **Given** health enabled with checks, **When** all pass, **Then** 200 with status up; when one fails,
   503 naming it.
2. **Given** info enabled, **When** requested, **Then** 200 with the declared name and version.

---

### User Story 8 - The example and the guide use it (Priority: P3)

The notes-api example is rewritten with the package (same requests, same answers, same requirements and
tests), and the guide shows how to build an API with it, so agents learn one way.

**Why this priority**: the measure of the package and how agents find it.

**Independent Test**: the rewritten notes-api passes its tests and replays its recording with the same
responses (problem bodies excepted, where the change is listed), in markedly fewer lines.

**Acceptance Scenarios**:

1. **Given** the rewritten example, **When** compared with today's, **Then** it answers every documented
   request the same way, except error bodies, which become problem+json.
2. **Given** an agent with the guide, **When** asked to build a CRUD API, **Then** it uses the package.

### Edge Cases

- A path with a trailing slash (`/notes/`), double slashes, or percent-encoded characters in a variable
  (`/files/a%2Fb`): one documented rule, the same everywhere; a variable never matches across `/`.
- `HEAD` for a `GET` route: answered like `GET` without a body; `OPTIONS`: the allowed methods.
- A JSON body with unknown fields: refused with 400 naming them (strict by default), so typos do not pass
  silently.
- Duplicate query parameters (`?tag=a&tag=b`) for a single value: 400; for a list: both values.
- Very deep or large JSON bodies: refused within the existing body limit, never a crash.
- An operation function that itself returns a raw response: passed through unchanged (an escape hatch for
  files, redirects, other content types).
- A middleware step that fails with a runtime error: 500 problem, like an operation.
- Two settings files or settings given twice: one documented precedence, no merging surprises.
- Routes declared but never reachable (shadowed by an earlier, wider one): reported when checked.

## Requirements *(mandatory)*

### Functional Requirements

**Routing**

- **FR-001**: A program MUST be able to declare operations as method + path pattern + answering function,
  with named path variables, as one route table value per application built in ordinary code (no route
  annotations).
- **FR-002**: Matching MUST prefer literal segments over variables, MUST answer 404 for an unknown path and
  405 with an `Allow` header for a known path with an unsupported method, and MUST answer `HEAD` and
  `OPTIONS` automatically.
- **FR-003**: Ambiguous or unreachable routes MUST be reported, naming the routes involved: by `vo check`
  for routes written in one list literal, otherwise when the application starts (before it serves).
- **FR-004**: Routing MUST be pure and deterministic: the same request and routes always reach the same
  operation.

**Input**

- **FR-005**: An operation MUST be able to receive path variables, query parameters and headers as typed
  values (text, whole number, decimal, yes/no, and lists of these), with optional ones and defaults.
- **FR-006**: An operation MUST be able to receive a JSON body as a value of a record type of the program,
  including nested records, lists, optional fields and enum-like variants. The conversion between a
  record and JSON (both ways) MUST be derived by the toolchain from the record's declaration, with no
  per-record code, and MUST be available to every program (not only through `std.web`); a record that
  cannot be converted (it holds a function, say) MUST be reported when the program is checked.
- **FR-007**: Record fields and parameters MUST be able to declare limits (optional, length, numeric range,
  pattern, allowed values, list size) where they are declared; the derived conversion MUST check them and
  report every violation at once; a limit that cannot hold (a range on a text, a malformed pattern) MUST be
  reported when the program is checked.
- **FR-008**: Input that cannot be bound or breaks a limit MUST NOT reach the operation; the answer MUST be
  400 (or 422 for a rule broken by a well-formed body) naming every bad field or parameter and why; bodies
  of another content type MUST get 415, too large ones 413.

**Output and errors**

- **FR-009**: Operations MUST be able to answer with a record (200 JSON), or with helpers for 201 with
  `Location`, 204, other statuses, extra headers, and a raw response as an escape hatch.
- **FR-010**: Errors returned by operations MUST be mapped to statuses in one place per application;
  every failure answer the package produces MUST be an RFC 9457 `application/problem+json` body.
- **FR-011**: A runtime error in an operation or step MUST answer 500 problem without internal details;
  the report goes to stderr as today.

**Steps (middleware)**

- **FR-012**: A program MUST be able to list steps that run in order around operations (for all routes or
  routes under a path prefix); a step MAY answer early or change the response.
- **FR-013**: The package MUST provide ready steps for request ids, one log line per request, CORS and
  bearer-token / API-key checks with a program-supplied check function.

**Context, configuration, health**

- **FR-014**: The application's context MUST be one value of the program's own type, built per worker by a
  setup (serve_workers' model), and passed to every operation and step that asks for it.
- **FR-015**: Settings MUST be declarable with types, defaults and required ones, read from environment
  variables and an optional settings file (environment wins), all checked at start with one message for
  every problem; reading them is an effect like any other and is recorded.
- **FR-016**: An application MUST be able to list `GET /health` (with program-supplied checks; 200 up,
  503 down) and `GET /info`.

**Guarantees**

- **FR-017**: Operations, steps and checks MUST remain ordinary functions whose effects are declared and
  checked; the package MUST add no effect of its own beyond `serve` and what the program's functions use.
- **FR-018**: An application MUST be callable with a request value in tests, giving the same response as
  over the network, without the `serve` effect.
- **FR-019**: Recording and replay of a program using the package MUST work as for any server.
- **FR-020**: There MUST be one documented way to do each of: declaring a route, reading each kind of input,
  answering, mapping an error, adding a step, declaring a setting.
- **FR-021**: The package MUST be documented in the reference (within its token budget), the guide, and
  `vo doc`, and the notes-api example MUST use it.
- **FR-022**: The package MUST be part of the standard library as `std.web`, shipped and versioned with the
  toolchain.

### Key Entities

- **Application**: the routes, steps, error mapping, health/info and context setup of one API; a value
  that can serve or be called in tests.
- **Route / Operation**: method, path pattern (literal segments and typed variables), the answering
  function, and its input declarations.
- **Request input**: path variables, query parameters, headers, body; each typed, optional or required,
  with limits.
- **Answer**: a status, headers and a body (a record as JSON, nothing, or raw).
- **Problem**: the RFC 9457 failure body: type, title, status, detail, and invalid fields.
- **Step**: a function around operations: sees the request, may answer early, may change the response.
- **Context**: the per-worker value of the program's type given to operations.
- **Setting**: a named, typed configuration value with a source (environment, file, default).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The notes-api example rewritten with the package has at least 40% fewer lines in its API
  code, keeps all its requirements and tests, and answers every request of its recorded session with the
  same status and body (error bodies excepted, which become problem+json).
- **SC-002**: A program using the package contains no code for path splitting, method matching, 404/405
  answers, query parsing or JSON field-by-field reading.
- **SC-003**: For a generated corpus of 1,000 malformed requests against a 10-operation API (bad JSON,
  wrong types, missing fields, broken limits, wrong methods, unknown paths), every answer is the documented
  4xx problem naming the cause, no operation is called with bad input, and no request answers 500.
- **SC-004**: In the agent benchmark (the CRUD task of the experiment kits), agents using the package pass
  the acceptance tests on the first run at least 25 percentage points more often than agents using plain
  `std.http`, with no increase in time to finish.
- **SC-005**: Every operation of an example application can be tested with `vo test` without a port,
  and the tests run in under one second.
- **SC-006**: Routing and binding cost at most 10% of throughput compared with the same API written by
  hand on `std.http`, for a 20-route application.
- **SC-007**: Existing programs using `std.http` directly behave exactly as before.

## Assumptions

- The package builds on `std.http.serve_workers` (spec 028) and `std.sqlite` (spec 026); serving a single
  worker is the same application with one worker.
- Optional request fields are scalars, enum-like values and lists; an optional nested object is modelled
  as a list of at most one item.
- JSON is the only body format the package binds and produces (other content types go through the raw
  escape hatch); no XML, form encoding or multipart uploads in this spec.
- No sessions, cookies-based login, OAuth flows, templating/HTML views, WebSockets, server-sent events,
  HTTPS serving, OpenAPI generation, or rate limiting in this spec; authentication is a step with a
  program-supplied check.
- "Inspired by Spring" means its concepts (declared routes, binding, validation, advice, filters,
  injection as an explicit context, externalized configuration, actuator, mock requests), not its
  mechanisms (reflection, classpath scanning, proxies, runtime annotations, mutable singletons).
- Error mapping uses vo's errors as values: operations return results; no exceptions are introduced.
- The agent benchmark of SC-004 uses the existing experiment-kit method (isolated agents, an acceptance
  suite, the same task with and without the package).

# Feature Specification: Concurrent Request Handling

**Feature Branch**: `028-concurrent-serve`

**Created**: 2026-11-19

**Status**: Implemented (clarified 2026-11-19: the program declares workers, `--workers` overrides; a setup function per worker; see [results](results.md))

**Input**: User description: "Concurrent request handling for std.http.serve: today the server reads many
connections at once but calls the handler for one request at a time, so one slow request (a large query,
an outbound HTTP call, a slow client) stalls every other client; this is the main blocker for running a
production API in vo. Serve requests concurrently while keeping vo's guarantees: handlers stay ordinary
functions with the caller's effects and scopes, no shared mutable memory between requests (state lives in
files or databases, as today), deterministic tests and record/replay still work, and a handler that
crashes or times out affects only its own request. Fit vo's process-based concurrency (tasks are
processes that share nothing): e.g. a fixed number of worker processes sharing the listening port, chosen
by the program or a command-line option, with SQLite databases opened per worker. Keep the current
per-request limits (head and body size, --timeout per handler call, 500 on a runtime error), graceful
shutdown on SIGTERM (finish in-flight requests), and the security model (the serve address and every
effect still checked). Measure: a slow endpoint no longer delays fast ones; throughput scales with
workers on a multi-core machine; replay of a recorded multi-request session stays deterministic."

Background: a vo server (`std.http.serve`) accepts up to 64 connections at once, but calls its handler
for one request at a time, in the order requests arrive complete. A handler that takes two seconds (a
large report, a call to another service, a slow database query) makes every other client wait two
seconds; under load, latency grows with the queue. This is the first gap between vo and a production API.

vo's concurrency model is processes that share nothing: a task works on its own copy of everything and
only its result comes back. That model suits a server well (one request cannot corrupt another's
memory), but it also means that a database or any other resource opened before the server starts
belongs to that one process (using it elsewhere is R0007 today), and that two requests running at the
same time can both read a file, change it and write it back, losing one change: a mistake today's
sequential server cannot make.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A slow request no longer delays the others (Priority: P1)

A service has a fast endpoint (a health check, a lookup) and a slow one (a report that takes seconds).
While several slow requests are being answered, fast requests are still answered at once.

**Why this priority**: it is the blocker: without it, one slow client degrades the whole service.

**Independent Test**: a server with a fast and a 2-second endpoint, concurrency enabled with 4 workers;
start 3 slow requests, then 50 fast ones: every fast request is answered within its usual time.

**Acceptance Scenarios**:

1. **Given** a server running with N workers and N−1 slow requests in progress, **When** a fast request
   arrives, **Then** it is answered without waiting for any slow one.
2. **Given** more requests in progress than workers, **When** another request arrives, **Then** it waits
   for the first free worker and is answered in arrival order among the waiting ones.
3. **Given** a server started the way servers start today (no worker count declared), **When** it runs,
   **Then** it handles one request at a time, exactly as today; `--workers N` on its command line does not
   change that, and a note on stderr says the program declares no workers.
4. **Given** a server whose program declares a worker count, **When** it runs with `--workers N`, **Then**
   it runs N workers instead of the declared count (N from 1 to 64).

---

### User Story 2 - A failing request affects only itself (Priority: P1)

A handler crashes on one request (a runtime error), runs out of time, or exhausts its memory. That request
gets an error response; the other requests in progress and the ones after it are answered normally, and
the server keeps its full capacity.

**Why this priority**: concurrency must not turn one bad request into a degraded or dead server.

**Independent Test**: while 20 requests are in flight, one request triggers a runtime error, another
exceeds `--timeout`, a third exceeds `--max-memory`: those three get 500 (as a failing handler does today),
the other 17 get their normal answers, and the next 100 requests are all answered.

**Acceptance Scenarios**:

1. **Given** a handler that stops with a runtime error, **When** its request is handled, **Then** it is
   answered 500, the report goes to stderr as today, and no other request is affected.
2. **Given** a handler that exceeds `--timeout` or `--max-memory`, **When** the limit is reached, **Then**
   only that request fails, and the worker that ran it is ready (or replaced) for the next request.
3. **Given** a worker that disappears unexpectedly (killed from outside), **When** it was handling a
   request, **Then** that request is answered 500 if it can still be, a new worker takes its place, and
   the event is reported on stderr.

---

### User Story 3 - Shutting down finishes the work in progress (Priority: P1)

The service is stopped (Ctrl-C, SIGTERM from a deployment). It stops accepting new connections, lets the
requests in progress finish, and exits.

**Why this priority**: deployments stop servers constantly; dropping in-flight requests loses user work.

**Independent Test**: start 5 slow requests, send SIGTERM: all 5 complete with their normal answers, new
connections are refused, and the program exits with status 0 once they are done.

**Acceptance Scenarios**:

1. **Given** requests in progress, **When** the server receives SIGTERM or Ctrl-C, **Then** it accepts no
   new connections and `serve` returns `ok()` after the requests in progress are answered.
2. **Given** a request that does not finish within its time limit after shutdown began, **When** the limit
   passes, **Then** it is stopped as any timed-out request is, and shutdown completes.
3. **Given** a second Ctrl-C during shutdown, **When** it arrives, **Then** the server stops at once.

---

### User Story 4 - Each worker has its own resources (Priority: P2)

A service keeps its data in a SQLite database. With several workers, each needs its own connection to the
database file (connections belong to the process that opened them), opened once rather than on every
request, and requests in different workers see each other's committed changes.

**Why this priority**: almost every API has a database; without a clear pattern, agents will open it in
`main` (R0007 in a worker) or once per request (slow).

**Independent Test**: a service that counts requests in a SQLite file database, served by 4 workers;
1,000 requests: the final count is exactly 1,000, and the database was opened once per worker.

**Acceptance Scenarios**:

1. **Given** a setup function the program gives when it starts the concurrent server, **When** a worker
   starts, **Then** the setup runs once in that worker (opening its database, reading its configuration),
   and every request that worker handles receives the setup's result alongside the request.
2. **Given** a setup that fails (returns an error or stops with a runtime error), **When** a worker starts,
   **Then** the server does not start (or, for a replacement worker, stops), reporting the error: a
   server whose workers cannot open their resources must not answer requests.
3. **Given** two requests in different workers changing the same rows inside transactions, **When** both
   commit, **Then** both changes are kept (the database's locking serializes them; a busy database waits,
   within the request's time limit).

---

### User Story 5 - Recordings and tests stay deterministic (Priority: P2)

A server run is recorded with `vo run --record`. Replaying it gives every request the same response as in
the recorded run, whatever order the requests' handlers ran in, and whatever the number of workers when
replaying. Handler tests (`vo test`) are unaffected: they call the handler directly.

**Why this priority**: record/replay is how a vo service's production incidents are reproduced; losing it
would trade a debugging strength for concurrency.

**Independent Test**: record a session of 200 requests to a 4-worker server that reads and writes a
database; replay it 20 times: the same responses each time, with no network and no files touched.

**Acceptance Scenarios**:

1. **Given** a recording of a concurrent server session, **When** it is replayed, **Then** each request's
   handler receives the inputs that request received when recorded (its own file reads, database results,
   HTTP responses, clock readings) and each response is compared with the recorded one, as today.
2. **Given** a program changed since recording, **When** a request's handler asks for different inputs,
   **Then** replay stops with R0014 naming that request.

---

### User Story 6 - The concurrency model is documented where agents look (Priority: P3)

An agent writing a server learns from the reference and the guide: handlers run at the same time in
separate processes; nothing in memory is shared between requests; state belongs in a database with
transactions; reading a file, changing it and writing it back is unsafe when requests overlap; how many
workers to run.

**Why this priority**: the change makes one class of mistake possible (lost updates on files); the
documentation and examples must steer agents away from it.

**Independent Test**: the reference and site guide state the model; the `notes-api` example, which saves
its notes to a JSON file on each request, is changed to keep them in SQLite with transactions, and a
concurrent load test against it loses no update.

**Acceptance Scenarios**:

1. **Given** the reference, **When** an agent reads the `serve` section, **Then** it finds the concurrency
   model, the worker choice and the state guidance in a few sentences.
2. **Given** the example programs, **When** they are run with several workers under a concurrent load test,
   **Then** no update is lost.

---

### Edge Cases

- One worker: behaves exactly like today's server (one request at a time).
- More workers than cores: allowed; useful when handlers wait on databases or other services.
- A client that sends its request slowly: it holds a connection, not a worker; workers are only taken by
  complete requests (as today's server reads requests before calling the handler).
- A request that arrives while all workers are busy and the waiting queue is full (64 connections): handled
  as today when the connection limit is reached.
- A handler that writes to stdout or stderr: each request's output appears whole, never interleaved
  line-by-line with another request's output.
- The listening address is checked against the `serve` grant once, before any worker starts; every worker
  runs with exactly the grant `main` has.
- A handler that itself uses `concurrent_map`: its tasks run inside that worker, as tasks inside tasks do
  today.
- `--seed`: each worker gets its own random sequence derived from the seed, so a recorded run is
  reproducible.
- A server started inside a test (`vo test`): not supported today, unchanged.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: A server MUST be able to handle up to W requests at the same time, W the number of workers,
  each in its own process that shares no memory with the others.
- **FR-002**: Concurrency MUST be declared by the program: it starts the server with a worker count (1 to
  64) and a setup function (FR-008). The command-line option `--workers N` (1 to 64) MUST override the
  declared count; on a program that declares no workers it MUST change nothing and print a note on stderr.
  A server started as today (no declaration) MUST behave exactly as today: one request at a time.
- **FR-003**: Requests MUST be given to free workers in the order they arrive complete; a request MUST wait
  only for a free worker, never for another particular request.
- **FR-004**: Each handler call MUST keep today's limits: request head and body size, `--timeout` per
  handler call, `--max-memory` per worker; reaching a limit or a runtime error MUST fail only that request
  (500, as today) and leave the worker ready or replaced.
- **FR-005**: A worker that ends unexpectedly MUST be replaced, its request answered 500 when the
  connection still allows it, and the event reported on stderr.
- **FR-006**: On SIGTERM or Ctrl-C the server MUST stop accepting connections, finish the requests in
  progress (each within its time limit), and then `serve` MUST return `ok()`; a second Ctrl-C MUST stop
  it at once.
- **FR-007**: Every worker MUST run with exactly the grant of the program's `main` (or test); the serve
  address MUST be checked once before any worker starts; every effect a handler performs MUST be checked
  at run time as today.
- **FR-008**: The concurrent server MUST take a setup function, run once in each worker before it handles
  requests, whose result each handler call receives with its request; the setup runs with the program's
  grant, and its effects count as the caller's. A setup that fails MUST stop the server before it answers
  any request (or, for a replacement worker, stop it with the error reported).
- **FR-009**: A handler's output to stdout and stderr MUST appear whole per request, never interleaved
  with another request's output.
- **FR-010**: Recording MUST keep each request's inputs (file reads, database results, HTTP responses,
  clock readings, random values) with that request; replay MUST give each request its own recorded inputs,
  whatever order handlers run in and whatever the number of workers, and MUST compare each response with
  the recorded one.
- **FR-011**: The reference, the site guide and the `serve` documentation MUST state the concurrency model
  (separate processes, no shared memory), recommend a database with transactions for state, and warn that
  reading, changing and writing back a file is unsafe when requests overlap.
- **FR-012**: The example programs that keep state MUST stay correct under concurrent requests (the
  `notes-api` example moves its notes from a JSON file to SQLite with transactions).

### Key Entities

- **Worker**: a process that handles one request at a time with the program's grant and its own copy of
  the program's state; replaced when it ends unexpectedly.
- **Waiting request**: a complete request that no worker has taken yet, in arrival order.
- **Per-worker resource**: something a handler needs that belongs to one process, such as an open database;
  produced by the setup function in each worker.
- **Declared concurrency**: the worker count and setup function the program gives when it starts the server;
  `--workers` overrides the count.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With 4 workers and 3 requests to a 2-second endpoint in progress, 50 requests to a fast
  endpoint are each answered within 50 ms (they waited for no slow request).
- **SC-002**: For a handler that only computes (no waiting), throughput with 4 workers on a machine with at
  least 4 cores is at least 3 times the throughput with 1 worker.
- **SC-003**: With one crashing, one timing-out and one memory-exhausting request among 20 in flight, the
  other 17 and the next 100 requests are all answered normally.
- **SC-004**: On SIGTERM with 5 requests in progress, all 5 receive their normal answers and the program
  exits with status 0.
- **SC-005**: 1,000 requests incrementing a counter in a database through 4 workers leave the counter at
  exactly 1,000; the `notes-api` example loses no note under a concurrent load test of 1,000 requests.
- **SC-006**: A recorded 200-request session against 4 workers replays to identical responses 20 times in
  a row, with no network or files touched.
- **SC-007**: Programs that do not ask for concurrency (or ask for one worker) behave as before: every
  existing server golden case passes unchanged.

## Assumptions

- Workers are processes, as vo's tasks are; threads with shared memory are out of scope (they would break
  the no-shared-memory guarantee).
- The worker count is fixed for a run (no automatic scaling).
- The 64-connection limit and the per-connection timeouts stay as they are; how many requests wait for a
  worker is bounded by them.
- HTTPS serving is out of scope (a proxy in front, as today).
- Keeping state in memory across requests (caches) is out of scope: state belongs in a database or files;
  a future feature may add a shared cache.
- The time limit for a request in progress during shutdown is its usual `--timeout`.

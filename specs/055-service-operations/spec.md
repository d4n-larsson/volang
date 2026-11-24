# Feature Specification: Operating a Service

**Feature Branch**: `055-service-operations`

**Created**: 2026-11-24

**Status**: Implemented ([results](results.md))

**Input**: User description: "Specify the operating a service feature." Second in the roadmap's open gaps
under constitution Principle VI (backend network services: easier to write, review or operate); the
"operate" part has had the least so far.

Background: a vo service today has `health_route` (GET /health: 200 when every check passes, else 503
with the failing checks), `info_route` (name and version), `request_ids` (an `X-Request-Id` on every
request and response) and `log_requests` (one free-text line per request, written by a function the
service passes in, such as `to_stderr`). `serve_workers` and `serve_app` already stop gracefully: on
the first Ctrl-C or SIGTERM they stop accepting and answer the requests already read; a second signal
stops at once. What an operator does not have:

- **Logs a machine can read.** Lines are free text: a log collector cannot parse them, an operator
  cannot filter by level, request or field, and nothing ties a line written inside an operation to the
  request it served.
- **Metrics.** No request counts, error counts, latency distribution or requests in flight, and no way
  for a service to count its own events (notes created, sign-ins refused).
- **Liveness apart from readiness.** An orchestrator cannot tell "restart me" (the process is stuck)
  from "send me no traffic for now" (the database is unreachable, startup is not finished).
- **A bound on stopping.** A request that never finishes keeps a stopping server alive until it is
  killed.

From an agent's point of view:
- **Operable by default, in a few lines.** The service it writes logs, counts and reports its health
  the way platforms expect, without the agent designing a logging or metrics layer.
- **Reviewable.** A reviewer sees in the effects which code writes logs, and a secret written to a log
  is caught by the toolchain where it can be, not by an incident.
- **Testable.** Log records and metrics are checked in ordinary tests through `handle_request`, with
  the same output every run.

What this spec must keep:
- **Principle III**: output identical on x86-64, ARM64 and RISC-V, over runs and in replay.
- **Principle II**: machine-readable records; any new diagnostic has a stable code.
- **Principle VI**: answered above.
- **Effects stay the boundary**: a service cannot log or expose more than it declares.
- No global variables; no new syntax unless unavoidable; `vo test --record`/`--replay` (spec 054)
  unaffected; the reference stays within 15,000 tokens (about 71 left), with the cost stated and what is
  moved out to make room; the docs site loads nothing from other sites.

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents writing vo services, the people who review their changes, and the
operators (and their platforms: container orchestrators, log collectors, metrics scrapers) who run them.

### User Story 1 - Structured logs tied to the request (Priority: P1)

An agent's notes service writes one JSON record per line: the time, a level, a message, the request id
and named fields. The request log step writes one record per request with method, route, status and
duration. A line the operation writes ("note saved", with the note's id) carries the same request id, so
an operator finds everything one request did. Started with the level set to `warn`, the service writes
only warnings and errors.

**Why this priority**: logs are the first thing an operator reads; free text is what they cannot use.

**Independent Test**: a test calls `handle_request` on an app with the log step and an operation that
logs; it checks the records written: one per line, valid JSON, the fields named, the same request id.

**Acceptance Scenarios**:

1. **Given** a service with the request log step, **When** it answers a request, **Then** it writes
   exactly one record for it, holding time, level, message, request id, method, route pattern, status
   and duration in milliseconds.
2. **Given** an operation that writes a log record with named fields, **When** it runs for a request
   with an id, **Then** its record carries that request id.
3. **Given** a level chosen at start (from an environment variable or a setting), **When** records
   below that level are written, **Then** they are left out.
4. **Given** a field value with quotes, newlines or non-ASCII text, **When** written, **Then** the record
   is still one line of valid JSON.
5. **Given** `vo test` (seed 1, clock 0) or a replay, **When** the same requests are handled, **Then** the
   records are byte-identical on every run and architecture.
6. **Given** a service that uses today's `log_requests` with a text function, **When** it runs, **Then**
   it behaves as before.

---

### User Story 2 - No secrets in logs (Priority: P1)

The service logs requests, including their headers on errors. The values of `Authorization`, `Cookie`,
`Set-Cookie` and `Proxy-Authorization` appear as `[redacted]`. When the agent writes a log record with a
field that holds a password, a session token or a key, the toolchain points it out before the service
runs. vo has no secret type, so secrets are known by name (Q2): fields and headers named like secrets,
and names the service lists, are written as `[redacted]`, and `vo check` warns when a log field's value
comes straight from such a name.

**Why this priority**: logs are shipped to more systems and kept longer than anything else a service
writes; a credential in a log is a breach.

**Independent Test**: a test logs a request carrying each credential header and a record with
secret-named fields; it searches the written records for the secret values.

**Acceptance Scenarios**:

1. **Given** a request with credential headers, **When** any record holding its headers is written,
   **Then** those headers' values are `[redacted]`.
2. **Given** a record with a field named like a secret (`password`, `passwd`, `secret`, `token`,
   `api_key`, `key`, `authorization`, `cookie`, `session`, compared without case and as a word in a
   longer name such as `reset_token`) or named in the service's own list, **When** written, **Then** its
   value is `[redacted]`.
3. **Given** code that logs a field whose value is read straight from a secret-named variable, field or
   parameter (`log_info("signed in", [field("user", u.name), field("pw", form.password)])`), **When**
   checked, **Then** `vo check` reports a warning with a stable code naming the value, and its fix
   removes the field.
4. **Given** a secret-like name that is not a secret (`key` of a map entry), **When** checked, **Then**
   the warning can be silenced the way other warnings are (`@allow`).

---

### User Story 3 - Metrics an operator can scrape (Priority: P1)

The operator's metrics scraper reads `GET /metrics` from the service: requests counted by method, route
pattern and status class, a request duration histogram, requests in flight, and the service's own
counters ("notes_created_total"). Counts are totals for the service, whatever number of worker
processes answers requests.

**Why this priority**: metrics are how an operator sees errors and slowness before users report them.

**Independent Test**: a test sends requests through `handle_request` on an app with the metrics step,
then reads the metrics route and compares the text with a golden file.

**Acceptance Scenarios**:

1. **Given** the metrics step and route, **When** requests are answered, **Then** the route's answer, in
   the text exposition format scrapers read, counts them by method, route pattern (not the raw path)
   and status class, with a duration histogram over fixed buckets and the number in flight.
2. **Given** a service counter or gauge the agent declares, **When** an operation adds to it, **Then**
   the metrics route shows it with its help text and labels.
3. **Given** requests for many different paths (`/notes/1` ... `/notes/10000`), **When** counted,
   **Then** they fall under one route pattern (`/notes/{id}`); unmatched paths fall under one label; the
   number of series stays bounded.
4. **Given** a service served by several worker processes, **When** the metrics route is read, **Then**
   it shows totals over all of them.
5. **Given** a label value or metric name that is not allowed by the format, **When** declared or
   used, **Then** it is refused (by the checker where it can, else with a clear error) rather than
   breaking the scrape.

---

### User Story 4 - Liveness and readiness (Priority: P2)

The orchestrator asks two questions: is the process alive (restart it if not), and should it get traffic
(take it out of rotation if not). The service answers liveness while it can answer at all, and readiness
only while its checks pass (its database answers).

**Why this priority**: without the split, a short database outage makes the orchestrator restart every
instance, which turns an outage into a longer one.

**Acceptance Scenarios**:

1. **Given** the health routes, **When** the database check fails, **Then** readiness answers 503 with
   the failing check and liveness still answers 200.
2. **Given** a service using today's `health_route`, **When** it runs, **Then** `/health` behaves as
   before.

---

### User Story 5 - Stopping within a bound (Priority: P2)

On SIGTERM the service stops accepting, finishes the requests in flight and exits; a request still
running when the drain limit passes is cut off, and the service exits with a record saying so.

**Why this priority**: platforms kill a process that does not stop in time; a bounded, logged stop is
what makes a deploy predictable.

**Acceptance Scenarios**:

1. **Given** a request in flight and SIGTERM, **When** the request finishes within the drain limit,
   **Then** it is answered and the service exits successfully.
2. **Given** a request still running when the drain limit passes, **When** the limit passes, **Then**
   the service exits, and the stop is reported in its log.
3. **Given** no drain limit set, **When** the service stops, **Then** a default limit applies.

---

### User Story 6 - The example service is operable (Priority: P2)

An example service (notes-api or secure-notes) gains structured logs, metrics, readiness on its
database and a drain limit, with tests of each, so an agent copies a working pattern.

**Acceptance Scenarios**:

1. **Given** the example, **When** `make test` runs, **Then** its tests of log records, metrics and
   readiness pass.

### Edge Cases

- **Logging inside a worker or a background task**: records go to the same output, one whole record per
  line; lines from different processes never interleave mid-record.
- **A very large field value**: written whole or cut at a stated length with a marker; never a broken
  line.
- **Time in records under `vo test` and replay**: a record's `time` is an output stamp, not a clock
  read the program receives: the test clock under `vo test`, the last replayed clock value under
  `vo replay` (so a replay's stderr can differ from the original run in `time` only). Durations the
  request steps measure are clock reads, recorded and replayed like any other.
- **A route that panics or an unmapped error**: the request record has status 500 and the error text,
  as `log_requests` writes today.
- **The metrics and health routes themselves**: counted like other routes (or left out, as the plan
  decides), never failing the scrape.
- **A metric declared twice with different kinds or labels**: refused.
- **Counter overflow**: counters are integers well beyond realistic lifetimes; no wrap is visible.
- **Metrics in `vo test`**: each test starts with fresh metrics.
- **A second SIGTERM during the drain**: stops at once, as today.
- **Plain `serve` (one process, no app)**: answers one request at a time in the program's own process,
  so the drain limit is checked between requests: queued requests past the limit get 503, but a handler
  already running is not cut. Logs and metrics are for any program; the steps are for `std.web` apps.

## Requirements *(mandatory)*

### Functional Requirements

**Logs**

- **FR-001**: A service MUST be able to write log records as single lines of JSON with at least time,
  level, message, and named fields; levels are debug, info, warn and error.
- **FR-002**: Records written while answering a request MUST carry that request's id when the app
  gives requests ids.
- **FR-003**: A step MUST write one record per request with method, route pattern, status, duration in
  milliseconds and the request id, and the error text for an unmapped error.
- **FR-004**: The lowest level written MUST be chosen at start without changing code (from an
  environment variable or a setting); records below it are not written.
- **FR-005**: Writing a log record MUST need a new effect, `log`, separate from `console`, so
  `vo.effects` and review tell logging apart from printing, and code can be allowed to log without being
  allowed to print (Q1).
- **FR-006**: The values of the headers `Authorization`, `Proxy-Authorization`, `Cookie` and
  `Set-Cookie` MUST never be written in a log record; they are written as `[redacted]` (a case of
  FR-007's rule, stated on its own as the guarantee).
- **FR-007**: Field and header values whose names look like secrets (the list in US2, scenario 2) or
  are in a list the service gives MUST be written as `[redacted]`; `vo check` MUST warn with a stable
  code when a log field's value is read straight from such a name (Q2). Secrets under other names are
  not detected; the guide says so.
- **FR-008**: Today's `log_requests`, `to_stderr` and `clock_ms` MUST keep working unchanged.

**Metrics**

- **FR-009**: A service MUST be able to answer a metrics route in the Prometheus text exposition format
  (version 0.0.4), readable by standard scrapers.
- **FR-010**: A step MUST count requests by method, route pattern and status class (2xx, 3xx, 4xx, 5xx),
  record request durations in a histogram with fixed buckets, and track requests in flight.
- **FR-011**: A service MUST be able to declare its own counters, gauges and histograms with names,
  help text and labels, and change them from its operations.
- **FR-012**: Label values from requests MUST be bounded: route patterns, not raw paths; methods outside
  the standard set and unmatched paths fall under one value each.
- **FR-013**: With several worker processes, the metrics route MUST show totals over all of them.
- **FR-014**: Metric names and labels MUST follow the format's rules; invalid ones are refused, by the
  checker where it can with a stable code and, for characters the format does not allow, a fix
  replacing each with `_`.

**Health**

- **FR-015**: A service MUST be able to answer liveness and readiness separately: liveness 200 while
  the process answers; readiness 200 only while its checks pass, else 503 with the failing checks.
- **FR-016**: Today's `health_route` MUST keep its behavior.

**Stopping**

- **FR-017**: On SIGTERM or SIGINT a serving program MUST stop accepting, answer the requests it has
  read, and exit within a drain limit; with worker processes (`serve_workers`, `serve_app`) requests
  still running at the limit are cut off; plain `serve` checks the limit between requests (Edge Cases).
- **FR-018**: The drain limit MUST have a default and be settable by the service or the command line.
- **FR-019**: A stop MUST be reported: how many requests were drained and whether any were cut off.

**Fit**

- **FR-020**: Logs and metrics MUST be testable through `handle_request` without a server, and give
  byte-identical results under `vo test` and in replay on the three architectures.
- **FR-021**: No global variables and no new syntax; `vo test --record`/`--replay` and `vo run --record`
  MUST behave as before.
- **FR-022**: The reference MUST describe the feature in the fewest words and stay within 15,000
  tokens; the cost is stated, and any text moved out to make room goes to the docs site.
- **FR-023**: An example service MUST use structured logs, metrics, readiness and a drain limit, with
  tests of each, run by `make test`.

### Key Entities

- **Log record**: one line of JSON: time, level, message, request id when there is one, named fields.
- **Log level**: debug, info, warn, error; the lowest written is chosen at start.
- **Metric**: a name, help text, kind (counter, gauge, histogram), label names; one series per
  combination of label values.
- **Metrics registry**: the metrics a service declares and their current values, totalled over
  worker processes.
- **Health check**: a named check (exists today); readiness runs them, liveness does not.
- **Drain limit**: how long a stopping server waits for requests in flight.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every acceptance scenario is a golden or scripted case; log records and metrics text are
  byte-identical over 20 runs and on x86-64, ARM64 and RISC-V.
- **SC-002**: Every log record written by the example under a scripted load of 1,000 requests parses as
  JSON, and every record written while answering a request carries its id.
- **SC-003**: A standard metrics parser reads the metrics route of the example without errors, and its
  request totals match the number of requests sent, with 4 worker processes.
- **SC-004**: Requests for 10,000 distinct paths of one route add no more series than one request does.
- **SC-005**: No credential header value and no value of a secret-named field, sent in a scripted run,
  appears in any log record (searched for each value); every case of logging a secret-named value
  straight in a set of 10 written to test it gets the warning.
- **SC-006**: With a request that runs past a 2-second drain limit, the service exits within 3 seconds
  of SIGTERM and its log says a request was cut off.
- **SC-007**: Adding structured logs, request metrics and readiness to an existing example service takes
  at most 15 lines of change.
- **SC-008**: The reference stays at or under 15,000 tokens.

## Assumptions

- **stdout and stderr are the interface**: records go to the process's output and the platform collects
  them; no log files, rotation or shipping over the network.
- **The metrics format is Prometheus text** (the one most scrapers read); the metrics route is served by
  the service itself on its own address.
- **JSON lines are the one log format**; a person reads them with any JSON tool. A plain text form for
  development is not added.
- **Time in records** is UTC with milliseconds, stamped by the runtime from the program's clock without
  recording a read (the program never receives it outside `vo test`), so adding a log line does not
  change a recording. It is fixed in tests; in replay it is the last replayed clock value.
- **Graceful stopping already exists** (spec 028's `serve_workers`); this spec adds the drain limit, its
  default (30 seconds unless the plan finds a reason otherwise) and the report.
- **Histogram buckets** for request durations are fixed by the standard library (a common set from 5 ms
  to 10 s); a service declaring its own histogram gives its buckets.
- Out of scope: distributed tracing (spans, exporters), pushing logs or metrics to a collector, log files
  and rotation, dashboards and alerting, profiling.

## Clarifications

### Session 2026-11-24

- Q: Which effect does writing a log record need? → A: A new `log` effect, separate from `console`, so
  review tells logging apart from printing and code can log without being allowed to print.
- Q: How are secrets kept out of log records beyond the credential headers, with no secret type in vo?
  → A: By name: fields and headers named like secrets, and names the service lists, are written as
  `[redacted]`; `vo check` warns when a log field's value comes straight from such a name. No new type.

# Research: Operating a Service

From reading `src/effects.c` (the effect names), `src/packages.c` (their order), `src/serve.c` and
`src/serve_workers.c` (serving, stopping, worker processes), `src/build.c` (`--vo-` options of a built
executable), `src/testrun.c` (captured test output), `src/check_web.c` (E0330 on route patterns),
`std/web.vo`, `std/http.vo`, `std/time.vo`, and `examples/notes-api`.

## What exists

- **Effects** are a fixed list of names, one bit each (`es_names` in `src/effects.c`, `effect_order` in
  `src/packages.c`, `EF_*` in `src/vo.h`); spec 035 added `entropy` this way.
- **`serve_workers`** (spec 028): the parent keeps the connections and hands each complete request to a
  free worker process over a pipe. A worker's stdout and stderr go to the parent through pipes and are
  written whole when its response is sent (R7), so output of different workers never interleaves
  mid-line. The first SIGINT/SIGTERM stops accepting and answers what was read; a second stops at once.
  There is no time limit on the drain.
- **`vo test`** captures each test's output and shows it for a failing test; the clock is fixed (clock
  0, `now_ms` returns 0, 1, 2, ...) and the seed is 1.
- **A built executable** takes toolchain options as `--vo-NAME` (`app_start`), so a `vo run` option is
  available to built services too.
- **`std.web`** marks an unmapped error with an internal `Vo-Error` response header that `serve_app`
  removes before sending (`without_vo_error`). Steps run before routing, so they do not know the route
  pattern a request matched.
- **The checker** checks literal route patterns at their call (`src/check_web.c`, E0330).
- Free codes: W0506 (W05xx: effects), E0341, R0016.

## R1. The `log` effect covers logs and metrics (Q1)

**Decision**: a new effect `log` (bit after `entropy`, last in the canonical order). Writing a log record
and changing or reading metrics need it. It is an output to the operator: nothing the program receives.

**Rationale**: Q1 chose an effect separate from `console`. Metrics are the same kind of thing (what the
service tells its operator, which never changes its own answers), and one effect keeps the reference
and `vo.effects` small. Changing a counter must not be pure: a pure function that counts would make
property tests and contract conditions depend on state.

**Alternatives considered**: a second effect `metrics` (one more name for reviewers to learn, for no
review question that `log` does not answer); metrics with no effect (breaks purity).

## R2. Record stamps are outputs; durations are clock reads

**Decision**:
- **A log record's `time`** is stamped by the runtime from the program's clock **without** a clock read
  being recorded: the real time under `vo run`, the test clock under `vo test` (the stamp does not move
  it: `1970-01-01T00:00:00.000Z` until the test reads the clock), the last replayed clock value under
  `vo replay`. Writing a record adds no event to a recording.
- **Durations** in `request_logs` and `request_metrics` are measured with `now_ms`, a recorded clock
  read, as `log_requests` does today. The steps use `log, clock`. Under `vo test` the test clock gives
  each request a duration of 1 ms (one read before, one after).

**Rationale**: the stamp is an output the program never receives (outside `vo test`, where it is
fixed), so it cannot make the program's behavior differ from its recording. Recording it would shift
every later clock event when a log line is added, and spec 054's recorded tests would fail with R0014
after a logging change. A duration, though, reaches the program as a value (the metrics text is a
`str` a program can read), so it must be an input like any other: Principle III, "nondeterminism enters
only through explicitly named builtins", with replay giving it back.

**Consequences**: a replay's stderr can differ from the original run in `time` only (stated in the
guide). Adding the request steps to an app adds clock reads to its recordings, as any code that reads the
clock does; its recorded tests are re-recorded once.

## R3. Where records go, and what tests see

**Decision**: one record per line on **stderr**, written with one write. Under `vo test` they go to the
test's captured output (shown when it fails, as `print` output is) and are kept for the test: `logged()`
returns the records written so far in the current test, as JSON text, so a test checks them (parse with
`parse_json` or compare). Outside `vo test`, `logged()` returns an empty list. Worker processes need
nothing new: their stderr already reaches the parent whole (R7 of spec 028).

A field value longer than 4,096 bytes is cut there and ends with `…[cut]`, so a record stays a
reasonable line.

**Alternatives considered**: a sink function passed in (as `log_requests` takes) — it gives the test a
hook but makes every caller thread a function through, and is what Q1 replaced with an effect.

## R4. The record

**Decision**: `{"time":"…Z","level":"info","message":"…","request_id":"…", FIELDS}`: keys in that order,
`request_id` only while a request with an id is being answered, then the fields in the order given.
`time` is UTC with milliseconds (RFC 3339). Field values are any `json` value (`field[T: json](name,
value)`), so numbers stay numbers. A field named like a fixed key is written with `field_` before its
name. Levels: `debug`, `info`, `warn`, `error`.

**Level chosen at start**: `vo run --log-level LEVEL` (a built service: `--vo-log-level LEVEL`); default
`info`. `vo test` keeps every level. An option, not a variable the runtime reads behind the program's
`env` effect: the level is the operator's setting of the process, like `--workers`.

## R5. The request id in records written by operations

**Decision**: the request log step tells the runtime the id of the request it is answering (from its
`X-Request-Id`, which `request_ids` sets) and clears it after; every record written in between carries
it. A worker answers one request at a time, so per-process state is exact.

**Rationale**: the alternative, binding `@header("X-Request-Id")` in every operation input and passing it
to every log call, is what agents forget. The state lives in the runtime like the clock; the language
has no global variable.

## R6. Secrets by name (Q2)

**Decision**: a name is **secret-like** when one of its words is `password`, `passwd`, `secret`,
`token`, `key`, `authorization`, `cookie` or `session` (words split at `_`, `-`, `.` and lower-to-upper
case changes, compared without case; `api_key`, `reset_token`, `sessionId`, `Set-Cookie` and
`Proxy-Authorization` all match). Before a record is written:
- a field whose name is secret-like is written `"[redacted]"`;
- inside a field value, at any depth, an object member whose name is secret-like is `"[redacted]"`,
  and so is the `value` of an object `{"name": N, "value": V}` (a header) whose `N` is secret-like;
- `redact_logs(names)` (uses `log`) adds words for the process, for names the list misses (`ssn`).

The four credential headers of FR-006 are covered by the word rule.

**Check (W0506 `secret_logged`)**: in a call to `log_debug`/`log_info`/`log_warn`/`log_error`, a
`field(NAME, VALUE)` whose VALUE is a variable, parameter or field access with a secret-like last name
(`form.password`, `token`) is a warning at VALUE: "`form.password` looks like a secret: it is written as
[redacted] at best; leave it out of the log record". Its fix removes the field. `@allow("W0506", ...)`
silences it. Values computed from a secret (`hash(pw)`) are not followed: the rule is syntactic.

**Rationale**: the runtime rule guarantees redaction for every name it knows, and the check makes the
agent drop the field instead of relying on it.

## R7. Metrics live in a shared runtime registry

**Decision**: one registry per program, in the runtime (not a global variable of the language): a
fixed-size table of series in memory mapped shared (`MAP_SHARED | MAP_ANONYMOUS`). `serve_workers` maps
it before forking its workers, so every worker updates the same counters and any worker answering
`/metrics` shows the totals (FR-013). Updates take a lock word in the same memory (the owner's pid; a
waiter takes over a lock whose owner died, since a dead worker is replaced). At most 2,048 series;
further series are dropped and counted in `vo_metrics_dropped_series_total`, never an error that breaks
a request. Under `vo test` the registry is private and emptied before each test. If the system refuses
the mapping, the first metric call (or `serve_workers` mapping it before forking) stops with R0006
`out_of_memory`, as any other exhausted resource (Principle III).

**Rationale**: workers share no memory today, and the parent does not run vo code; a shared table is the
only way a single answer shows totals without a round trip through the parent. Integers and floats are
stored as 64-bit values; all three architectures have the needed atomics.

**Alternatives considered**: metrics in the app's context C (per worker: a scrape would see one
worker's counts); workers sending updates to the parent with each response and the parent rewriting the
`/metrics` answer (a second protocol and a special response).

## R8. The metrics API

**Decision** (`std.metrics`):
- `counter(name, help, labels) -> Metric`, `gauge(...)`, `histogram(name, help, labels, buckets)`: plain
  values describing a metric (pure).
- `add(m, values, n)` (counter: n ≥ 0; gauge: any), `set_to(m, values, n)` (gauge), `observe(m, values,
  x)` (histogram): `uses log`. `values` are the label values in the order of `labels`.
- `metrics_text(metrics) -> str` (`uses log`): the registry in the text exposition format 0.0.4: `# HELP`
  and `# TYPE` lines, series sorted by name then label values, histograms as cumulative `_bucket{le=…}`,
  `_sum`, `_count`. A metric in `metrics` with no labels and no updates is shown with 0.
- Counter and gauge values are `int`; histogram observations and sums are `float`.

**Checks**: a literal name or label name that does not match the format's rules (`[a-zA-Z_:][a-zA-Z0-9_:]*`;
labels without `:` and not starting `__`) is **E0341 `bad_metric_name`** at the literal. Its fix
replaces each character the rule does not allow with `_` (one correction: the format's own convention);
a label starting `__` or a repeated label has no fix (the right name is the author's choice), which the
catalog entry says. At run time
(**R0016 `metric_conflict`**): one name used with another kind, help or label list; a number of values
that does not match the labels; a counter given a negative amount.

**Label values**: escaped as the format requires (`\\`, `\"`, `\n`).

## R9. Request steps and routes in `std.web`

- **The route pattern on the response**: `handle_request` adds an internal `Vo-Route` header with the
  pattern of the route that answered (none for 404/405); `serve_app` removes it with `Vo-Error`. Steps
  see it on the response `next` returns.
- **Signatures**: the steps carry their own effects in the step type, as spec 052's `Way[Store, uses
  clock, entropy]` does: `request_logs[C, E: effects]() -> Step[C, uses log, clock, E]`, the same for
  `request_metrics`, and `metrics_route[C, E: effects](metrics) -> Route[C, uses log, E]`. Put in an
  `App[C, uses …]` whose effects include them, E is what is left. Spec 052 left inferring E from such a
  mixed argument unimplemented (E0310), so this spec adds it in `src/check_generic.c`: for an expected
  effect argument that is a set S and a written argument `uses W, E`, E is S without W; when S lacks a
  written effect the mismatch is reported as today. Without it every app would write
  `request_logs[Store, uses file.read("data/")]()`.
- **`request_logs() -> Step`** (uses log, clock): one record per request, message `request`, fields
  `method`, `route` (the pattern, or `unmatched`), `status`, `ms` (from `now_ms`, R2), and `error` for an
  unmapped error; level `info`
  below 500, `error` from 500; `debug` for `/health/…` and `/metrics` answered 200 (probes every few
  seconds would drown the log). Sets the request id for records written inside (R5).
- **`request_metrics() -> Step`** (uses log, clock): `http_requests_total{method,route,status}` (status `2xx`
  ... `5xx`; methods outside GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS are `other`; no route is
  `unmatched`), `http_request_duration_seconds{method,route}` with buckets 0.005, 0.01, 0.025, 0.05, 0.1,
  0.25, 0.5, 1, 2.5, 5, 10 (seconds, from `now_ms`), and `http_requests_in_flight`. The metrics and
  health routes are counted like any other, so a scrape counts itself: a scrape of an otherwise idle
  service shows `http_requests_in_flight 1`.
- **`metrics_route(metrics) -> Route`**: GET `/metrics`, `text/plain; version=0.0.4`, the service's own
  metrics listed so they show before their first update.
- **`health_routes(checks) -> list[Route]`**: GET `/health/live` (200 `{"status":"up"}`, runs no check)
  and GET `/health/ready` (as `health_route`). `health_route` stays.

`log_requests`, `to_stderr` and `clock_ms` are unchanged (FR-008).

## R10. Logs API (`std.log`)

`log_debug`, `log_info`, `log_warn`, `log_error` (`message: str, fields: list[Field]`), `field[T:
json](name, value) -> Field`, `logged() -> list[str]`, `redact_logs(names)`. All but `field` use `log`.
Programs that are not services use it as well (a command-line tool, a worker).

## R11. Stopping within a limit

**Decision**: `vo run --drain SECONDS` (built: `--vo-drain`), default 30, 0 to 3,600. In
`serve_workers`, the first signal starts the drain: no new connections, requests already read are
answered; when the limit passes, busy workers are killed, their connections get `503` and the server
returns `ok()`. Plain `serve` answers one request at a time in the program's own process, so it checks
the limit between requests (a handler that never returns is not cut; the guide says to use
`serve_workers` or `serve_app` for that). At the end the runtime writes one record:
`{"time":…,"level":"info","message":"stopped","drain_s":S,"drained":N,"cut_off":M}` (`warn` when M > 0;
`drain_s` is the limit in force, so the default is visible and testable). A second signal still stops
at once.

**Rationale**: 30 s is the usual platform grace period (Kubernetes' default
`terminationGracePeriodSeconds`), so the service finishes inside it. An option rather than a parameter
keeps `serve_app` and `serve_workers` signatures unchanged.

## R12. Reference cost

New: the `log` row in the effects table, rows for `std.log` and `std.metrics` in the library table, two
step names and two route names on the `std.web` row, `--log-level`, `--drain` in the CLI synopsis, and
W0506 needs no reference text (the catalog has it). Estimated at about 110 tokens against 71 left, a
margin too thin to trust: the additions are written first and measured with the existing token check,
then as much as needed (at least the overshoot plus 20 tokens of headroom) is moved to the docs site's
guide, starting with detail of `std.web` and `serve_workers` the guide already explains; if that is not
enough, the new rows are shortened before anything else is cut. The change states the numbers.

## R13. The example and tests

- **notes-api** gains `request_ids`, `request_logs`, `request_metrics`, `metrics_route` with a
  `notes_created_total` counter, `health_routes` with its database check, and a `log_info` when a note is
  created; tests through `handle_request` check records (`logged()`), the metrics text and readiness.
  Target: at most 15 changed lines outside tests (SC-007).
- **Golden cases**: `tests/cases/log/` (records, levels, escaping, redaction, request id, cut values),
  `tests/cases/metrics/` (each kind, text format, conflicts R0016, cardinality), `tests/cases/check/`
  (E0341, W0506 with fix), `tests/cases/web/` (steps, routes, `Vo-Route` not sent).
- **`tests/operations.py`** (new, in `make test`): the example under `vo run --workers 4` with 1,000
  requests: every stderr line parses as JSON and carries a request id (SC-002); `/metrics` read by a
  strict parser of format 0.0.4 written in the script (no scraper is installed in CI), totals equal the
  requests sent (SC-003); 10,000 distinct paths add no series (SC-004); secrets searched in the output
  (SC-005); a slow handler with `--drain 2` exits within 3 s of SIGTERM with a `cut_off` record (SC-006).

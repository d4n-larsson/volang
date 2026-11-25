# Operating a Service

A service that runs somewhere else has to tell its operator what it is doing: log records a collector
can parse, metrics a scraper can read, health answers an orchestrator can act on, and a stop that ends
inside the platform's grace period. `std.log`, `std.metrics` and a few steps and routes of
[`std.web`](apis.md) cover all four. Writing a record or changing a metric needs the `log` effect, so
`vo.effects` shows which functions do.

## Log records

A record is one line of JSON on stderr: the time (UTC), the level, the message, the id of the request
being answered, then the fields in the order given. Field values keep their JSON form, so numbers stay
numbers:

```vo-test
import std.log;

test "a record" uses log {
    log.log_info("started", [log.field("port", 8080), log.field("ready", true)]);
    expect_eq(log.logged(), [`{"time":"1970-01-01T00:00:00.000Z","level":"info","message":"started","port":8080,"ready":true}`]);
}
```

```output
PASS a record (0ms)
1 passed, 0 failed
```

- The levels are `log_debug`, `log_info`, `log_warn` and `log_error`. `vo run --log-level warn` writes
  only the last two (default `info`; a built service takes `--vo-log-level`).
- Under `vo test` every record is kept: `logged()` gives the current test's records, and their time is
  the test clock. A failing test shows them with its output.
- A string longer than 4,096 bytes is cut and ends with `…[cut]`. A field named `time`, `level`,
  `message` or `request_id` is written `field_NAME`.

## The request id

`request_logs()` writes one record per request: `method`, `route` (the pattern, `/notes/{id}`, never the
raw path; `unmatched` when no route answered), `status`, `ms`, and `error` for an unmapped error. Put
`request_ids(random_id)` before it: every record written while the request is answered, also by the
operation, carries its `request_id`, so one request's records can be found together. Probes of
`/health/...` and `/metrics` answered 200 are written at `debug`, 5xx at `error`.

## Secrets

A field, a member of an object at any depth, or the value of a `{name, value}` pair (a header) whose name
is secret-like is written `"[redacted]"`. A name is secret-like when one of its words (split at `_`,
`-`, `.` and case changes) is `password`, `passwd`, `secret`, `token`, `key`, `authorization`, `cookie`
or `session`: `api_key`, `reset_token`, `Set-Cookie` and `Proxy-Authorization` are; `author` and
`keyboard` are not. `redact_logs(["ssn"])` adds words for the process.

The checker adds W0506 when a field's value is a variable or field access with a secret-like name
(`field("pw", form.password)`); its fix removes the field. Secrets under other names are not detected:
keep them out of fields. `redact_logs` hides values in log records written by the program; `vo test
--redact NAME` hides values in test recordings, which are files.

## Metrics

`counter`, `gauge` and `histogram` describe a metric (a name, a help line, label names); `add_to`, `set_to`
and `observe` change its series, one per tuple of label values. A name has one kind, help and label list
for the whole program (R0016 otherwise), and literal names are checked against the format's rules
(E0341). The registry is shared by `serve_workers`' worker processes, so any worker's answer shows the
totals; it holds 2,048 series.

`request_metrics()` counts `http_requests_total{method,route,status}` (status `2xx` to `5xx`, methods
outside the usual seven as `other`), times `http_request_duration_seconds{method,route}` and counts
`http_requests_in_flight` (a scrape counts itself). Labels come from route patterns, never from paths,
so 10,000 different note ids make no new series. `metrics_route([notes_created])` answers `GET /metrics`
in the text format 0.0.4 that Prometheus and compatible scrapers read; the listed metrics show before
their first update.

## Liveness and readiness

`health_routes(checks)` answers `GET /health/live` (200 while the process answers; no check runs) and
`GET /health/ready` (503 naming each check that fails). An orchestrator restarts a service whose
liveness fails and sends no traffic while readiness fails: a database that is down makes the service
unready, not dead. `health_route`'s `/health` stays as it was.

## Stopping

On SIGTERM or SIGINT a serving program stops accepting, answers the requests it has read, and ends with a
record:

```text
{"time":"2026-11-24T06:14:19.181Z","level":"info","message":"stopped","drain_s":30,"drained":12,"cut_off":0}
```

`vo run --drain SECONDS` (default 30, `--vo-drain` for a built service) bounds that: with
`serve_workers` or `serve_app`, requests still running at the limit are answered 503 and counted in
`cut_off` (the record is then `warn`). Keep the limit under the platform's grace period (Kubernetes
waits 30 s by default before it kills); `drain_s` shows the limit in force. Plain `serve` answers one
request at a time and checks the limit between requests, so a handler that never returns is not cut
off. A second signal stops at once.

## Replay

A record's time is an output, not a value the program reads: it is not recorded, and a replay's stderr
can differ from the original run's in `time` only. The request steps read the clock (`now_ms`) for their
durations, as any code that reads the clock does, so adding them to an app changes its recordings:
re-record its recorded tests once.

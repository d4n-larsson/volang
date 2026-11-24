# Contract: Operating a Service

## The `log` effect

`log` is the last name in the canonical effect order (after `entropy`). It allows writing log records
and changing or reading metrics. It never grants reading anything from outside.

## `std.log`

```vo
export record Field { name: str; value: Json; }

export fn field[T: json](name: str, value: T) -> Field
export fn log_debug(message: str, fields: list[Field]) uses log
export fn log_info(message: str, fields: list[Field]) uses log
export fn log_warn(message: str, fields: list[Field]) uses log
export fn log_error(message: str, fields: list[Field]) uses log
# The records written so far in this test (JSON text, one per record); outside vo test, an empty list.
export fn logged() -> list[str] uses log
# More words that make a name secret-like (R6), for this process.
export fn redact_logs(words: list[str]) uses log
```

Example record (under `vo test`):

```json
{"time":"1970-01-01T00:00:00.000Z","level":"info","message":"note created","request_id":"id-1","id":7,"password":"[redacted]"}
```

## `std.metrics`

```vo
export record Metric { name: str; help: str; kind: str; labels: list[str]; buckets: list[float]; }

export fn counter(name: str, help: str, labels: list[str]) -> Metric
export fn gauge(name: str, help: str, labels: list[str]) -> Metric
export fn histogram(name: str, help: str, labels: list[str], buckets: list[float]) -> Metric
export fn add(m: Metric, values: list[str], n: int) uses log        # counter (n >= 0) or gauge
export fn set_to(m: Metric, values: list[str], n: int) uses log     # gauge
export fn observe(m: Metric, values: list[str], x: float) uses log  # histogram
# The registry in the text exposition format 0.0.4; metrics listed here show before their first update.
export fn metrics_text(metrics: list[Metric]) -> str uses log
```

Text (abridged):

```text
# HELP http_requests_total Requests answered.
# TYPE http_requests_total counter
http_requests_total{method="GET",route="/notes/{id}",status="2xx"} 3
# HELP notes_created_total Notes created.
# TYPE notes_created_total counter
notes_created_total 1
```

## `std.web` additions

```vo
export fn request_logs[C, E: effects]() -> Step[C, uses log, clock, E]
export fn request_metrics[C, E: effects]() -> Step[C, uses log, clock, E]
export fn metrics_route[C, E: effects](metrics: list[Metric]) -> Route[C, uses log, E]   # GET /metrics
export fn health_routes[C, E: effects](checks: list[HealthCheck[C, E]]) -> list[Route[C, E]]  # GET /health/live, /health/ready
```

- In an `App[C, uses log, clock, file.read("data/")]`, `request_logs()` infers `E` as the app's effects
  without `log, clock` (new inference, research R9); an app whose effects lack `log` or `clock` gets the
  existing type mismatch error naming the missing effect.
- `request_logs` and `request_metrics` measure with `now_ms` (recorded); under `vo test` each request
  takes 1 ms. A scrape counts itself in `http_requests_in_flight`.
- `handle_request` adds `Vo-Route: PATTERN` to the response of a matched route; `serve_app` never sends it.
- Unchanged: `health_route`, `info_route`, `request_ids`, `log_requests`, `to_stderr`, `clock_ms`.

## Command line

| Option | Commands | Meaning |
|---|---|---|
| `--log-level LEVEL` | `run` (built: `--vo-log-level`) | lowest level written: `debug`, `info` (default), `warn`, `error`; other values T0001 |
| `--drain SECONDS` | `run` (built: `--vo-drain`) | the drain limit after SIGINT/SIGTERM, 0 to 3,600, default 30; other values T0001 |

## Diagnostics

| Code | Name | When |
|---|---|---|
| W0506 | `secret_logged` | a log field's value is a variable, parameter or field access with a secret-like last name; fix removes the field |
| E0341 | `bad_metric_name` | a literal metric or label name breaks the format's rules, or a label is repeated; fix: each disallowed character becomes `_` (no fix for a `__` label prefix or a repeated label) |
| R0016 | `metric_conflict` | a metric name used with another kind, help or labels; a wrong number of label values; a counter given a negative amount |
| R0006 | `out_of_memory` (existing) | the system refuses the shared metrics mapping |

## Stop record

```json
{"time":"…","level":"warn","message":"stopped","drain_s":30,"drained":12,"cut_off":1}
```

`level` is `info` when `cut_off` is 0; `drain_s` is the limit in force.

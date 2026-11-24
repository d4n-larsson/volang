# Data Model: Operating a Service

## Log record (one line on stderr)

| Key | Value | When |
|---|---|---|
| `time` | UTC, RFC 3339 with milliseconds (`2026-11-24T06:14:19.181Z`) | always (R2: an output stamp from the program's clock, not a recorded read; in replay the last replayed value) |
| `level` | `debug`, `info`, `warn`, `error` | always |
| `message` | the message text | always |
| `request_id` | the id of the request being answered | while a request log step answers a request with an id (R5) |
| field names | any `json` value | the fields given, in order; a field named `time`, `level`, `message` or `request_id` is written `field_NAME` |

Rules: one write per record; no newline inside (JSON escapes); a string value over 4,096 bytes is cut and
ends with `…[cut]`; secret-like names redacted (below).

## Log level

`debug` < `info` < `warn` < `error`. The lowest written is set by `--log-level` (default `info`);
`vo test` writes and keeps all.

## Field

`{name: str, value: Json}`, made by `field[T: json](name, value)`.

## Secret-like name (R6)

A name whose words (split at `_`, `-`, `.`, and lower-to-upper case changes; compared lower-case)
include one of: `password`, `passwd`, `secret`, `token`, `key`, `authorization`, `cookie`, `session`, or a
word given to `redact_logs`. Applied to: field names; object member names inside field values at any
depth; the `name` of a `{name, value}` object (its `value` is redacted). The value written is
`"[redacted]"`.

## Metric

| Field | Type | Rule |
|---|---|---|
| `name` | str | `[a-zA-Z_:][a-zA-Z0-9_:]*` (E0341 when a literal breaks it) |
| `help` | str | any text; escaped in `# HELP` |
| `kind` | `counter`, `gauge`, `histogram` | set by the function that makes it |
| `labels` | list[str] | each `[a-zA-Z_][a-zA-Z0-9_]*`, not starting `__`, no duplicates (E0341) |
| `buckets` | list[float] | histograms: increasing; `+Inf` added |

## Series (in the registry)

A metric's name and one tuple of label values: an `int` value (counter, gauge) or, for a histogram,
bucket counts, sum (`float`) and count. Created on first update. Identity rules (R0016): a name has one
kind, help and label list for the whole program; values must match the label count; a counter never goes
down.

## Registry (R7)

Per program, shared by its worker processes; at most 2,048 series (more are dropped and counted in
`vo_metrics_dropped_series_total`); emptied before each test under `vo test`. Rendered sorted by metric
name, then label values (byte order).

## Request metrics (R9)

| Metric | Kind | Labels |
|---|---|---|
| `http_requests_total` | counter | `method`, `route`, `status` (`2xx`…`5xx`) |
| `http_request_duration_seconds` | histogram (0.005 … 10) | `method`, `route` |
| `http_requests_in_flight` | gauge | none |

`method` outside GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS is `other`; no matching route is
`route="unmatched"`. Durations are `now_ms` reads (recorded), in seconds; 1 ms per request under the test
clock. In-flight counts the request being answered, so a scrape shows at least 1.

## Shared registry failure

The system refusing the shared mapping is R0006 `out_of_memory` at the first metric call (or when
`serve_workers` maps it before forking).

## Drain (R11)

States of a serving program: serving → (first SIGINT/SIGTERM) draining → (all answered, or the limit
passed) stopped; a second signal goes from draining to stopped at once. On stopping: one `stopped` record
with `drain_s` (the limit in force), `drained` and `cut_off`. With worker processes, requests running at
the limit are cut off (503); plain `serve` checks the limit between requests.

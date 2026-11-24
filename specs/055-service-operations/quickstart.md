# Quickstart: Operating a Service

Validation scenarios for spec 055. Interfaces: [contracts/api.md](contracts/api.md); records and
metrics: [data-model.md](data-model.md).

## Prerequisites

`make` (builds `build/vo`); Python 3 for `tests/operations.py`.

## 1. Logs in a test (US1, US2)

In `examples/notes-api`, `vo test` runs the tests that create a note through `handle_request` and read
`logged()`. Expected: one `request` record per request with `request_id`, `method`, `route`
(`/notes/{id}`, not the raw path), `status`, `ms` 1 (the test clock) and `time` `1970-01-01T00:00:00.000Z`; the operation's
`note created` record carries the same `request_id`; a `password` field reads `[redacted]`. Twenty runs
give identical output.

## 2. The secret check (US2)

`vo check` on a file that logs `field("pw", form.password)` reports W0506 at `form.password`;
`vo check --fix` removes the field.

## 3. Metrics (US3)

`vo test` in the example: after three requests, the `/metrics` answer equals the golden text, with
`http_requests_total{method="GET",route="/notes/{id}",status="2xx"} 3` and `notes_created_total 1`.

```sh
python3 tests/operations.py
```

Expected: with `--workers 4` and 1,000 requests, the totals on `/metrics` equal 1,000, the text passes
the strict format parser, 10,000 distinct note paths add no series, and every stderr line is a JSON
record with a request id.

## 4. Readiness (US4)

With the example's database made unreadable, `GET /health/ready` answers 503 naming `database`;
`GET /health/live` answers 200; `GET /health` behaves as before.

## 5. Stopping (US5)

`tests/operations.py`'s drain group: a handler that takes 10 s, `--drain 2`, SIGTERM after the request is
read. Expected: exit within 3 s; the last stderr line is a `stopped` record with `"drain_s":2` and
`"cut_off":1` at level `warn`. Without `--drain`, the stop record says `"drain_s":30`.

## 6. Unchanged

`make test` passes: `log_requests`, `health_route`, spec 054's recorded tests (log records add no
events to recordings; only the request steps' clock reads do), `vo run --record`/`vo replay`; the
reference token check stays at or under 15,000.

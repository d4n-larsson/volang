# Results: Operating a Service

## SC-001

Every acceptance scenario is a golden or scripted case:
- US1: `tests/cases/log/` (records, levels, escape, cut, request_logs, log_requests_unchanged).
- US2: `log/redact_*` and `check/secret_logged*`.
- US3: `tests/cases/metrics/`, `web/request_metrics*` and `check/bad_metric_name*`.
- US4: `web/health_routes`.
- US5: `tests/operations.py` group `drain`.
- US6: the example's tests and the `workers` and `logs` groups.

`tests/run.sh --repeat 20 tests/cases/log` passes with identical output each run. Three architectures:
the 41 new golden cases (log, metrics, web, the W0506/E0341 check cases, the inference cases, `effects/log_effect`) pass with the x86-64 expected output on the ARM64, RISC-V and instrumented builds, and `operations.py --quick` passes on ARM64 and RISC-V (under qemu).

## SC-002

`operations.py` group `logs`, on the example under `--workers 4`:
- 1,000 requests plus 10,000 distinct paths and the scrapes give 11,011 records.
- Every stderr line parses as JSON.
- Every `request` and `note created` record carries a `request_id`.
- The last line is the `stopped` record.

## SC-003

`operations.py` group `workers`: the example under `vo run --workers 4` takes 1,000 requests from 16
client threads (`--quick`: 200). The `/metrics` answer passes a strict parser of the text format 0.0.4
written in the script, which checks:
- comment, name and label syntax, escapes and values;
- histogram buckets increasing to `+Inf`, cumulative counts, and `_count` equal to the `+Inf` bucket.

The parser is itself checked to refuse five broken texts. The totals of `http_requests_total` equal the
requests sent. No third-party scraper is installed in CI, so "a standard metrics parser" is this one.

## SC-004

The same group, after the baseline: 10,000 requests to distinct `/notes/N` paths (`--quick`: 1,000)
add no series. Labels come from route patterns, not paths.

## SC-005

`operations.py` group `secrets`: a service logs each request's headers and JSON body. A request with
`Authorization`, `Cookie`, `Proxy-Authorization` and a `password` body field is sent, and a search of
all stdout and stderr finds none of the four values; `[redacted]` and the non-secret `user` are there.
`check/secret_logged` has the 10 positive cases (all W0506) and 5 negatives (none).

## SC-006

`operations.py` group `drain`, case "cut off at the limit":
- A worker handler running 10 s, `--drain 2`, SIGTERM.
- The client gets 503, and the process exits with code 0 after 2.0 s.
- The last line is `{"level":"warn","message":"stopped","drain_s":2,"drained":0,"cut_off":1}` (plus
  `time`).

Also checked:
- A request finishing inside the limit is answered (`drained` 1).
- The default limit shows `drain_s` 30.
- A second signal stops at once.
- Plain `serve` cuts off a request still queued after the limit, while one taken before it is answered.

## SC-007

Not met. The example needed 24 changed code lines outside tests, plus 14 imports, against the target of
15:
- the app's effects;
- typed lists for the routes and the health check;
- the steps;
- the `notes_created` counter;
- two lines in `create`;
- `main`'s clause.

The typed `let`s are there because inference does not follow the effect parameter `D` through two
generic calls (the example's comment, from spec 052). Without them it would be about 15 lines. The tests
and the helper `call` also gained `uses clock, random, log` (7 lines, not counted).

## SC-008

The reference is 14,970 tokens (words × 1.3), under 15,000 by 30.

## Reference cost

| Step | Tokens |
|---|---|
| before spec 055 | 14,929 |
| `log` effect row; `entropy, log` added to the canonical order line (which lacked `entropy`) | 14,944 |
| `std.log`, `std.metrics` rows; four names on the `std.web` row; `--log-level L`, `--drain SEC` | 14,994 |
| moved to the guide: the browser sign-in redirect (in `apis.md`), "A dead worker is replaced." (in `standard-library.md`) | 14,970 |

## Overhead

1,000 requests to the example (8 client threads, 4 workers), with and without `request_logs()` and
`request_metrics()`, median of 5 runs each, alternating: 194 ms with them, 188 ms without, so 3.1%
(goal: 10%).

## Deviations

- **Two builtins, not eight.** `__log(op, a, b)` and `__metric(op, key, x)` replace the eight planned
  builtins. The record's JSON (escaping, cutting, redaction of nested members and headers) is built in
  `std/log.vo` on the `Json` tree. The C side keeps the per-process state, the stamp, the secret-word rule
  (shared with the checker's W0506) and the one write per record.
- **`Vo-Route` is not visible to `handle_request`'s caller.** Steps see it on the response `next`
  returns. `handle_request` removes it before returning, so no existing test that prints or indexes
  headers changed, and `serve_app` never sends it. `tests/cases/web/route_header` shows both.
- **Checker fix found on the way (spec 052's effect arguments).** A function type written with effects
  (`fn(Request) -> Response uses console`) did not match the same type coming from a type's effect argument
  (`Step[C, uses console]`'s `next`), though both printed alike. A step value with effects could not be
  written by hand. `ty_func_fits` now compares nested function types treating the two forms as equal when
  neither has scopes.
- **The effect inference** (research R9) is a rule in `ty_unify`: a pattern `uses W, E` against a set S
  that has W binds E to S without W. Spec 052's known gap is closed for this shape. The cases are
  `generic/effects_rest_infer` and `effects_rest_missing`.
- **`@allow` accepts W0506**, beside the structure warnings. E0121's message now says so
  (`structure/allow_args` re-accepted).
- **Plain `serve` drains too.** Before, it stopped at the first signal without answering requests already
  read. It now answers them until `--drain` passes. It notices a signal that came during a handler before
  taking the next request, so the limit is checked between requests.
- **Every serving program ends with the `stopped` record** (FR-019). The serve, serve_concurrent and
  serve_workers step cases gained that line. `tests/steps.py` normalizes record times to `{time}`.
- **The example's readiness test** checks that `/health/live` and `/health/ready` answer apart. A
  failing database cannot be shown there, because a closed SQLite handle is R0007 and `select 1` never
  touches the file. The 503 path is `web/health_routes`.
- **`ms` in the example's request record is 3 under the test clock**, not 1. `request_metrics` runs inside
  `request_logs` and reads the clock too.
- **`recordings/demo.jsonl`** of the example was re-recorded (same requests, plus `GET /metrics`). The
  steps add clock and random reads, so the old recording stopped with R0014, as research R2 foresaw.
- **ARM64 builds with `-mno-outline-atomics`.** The registry's lock uses `__atomic_compare_exchange_n`.
  GCC's default outline atomics call libgcc helpers that need `getauxval`, and the freestanding build has
  no libc.
- **No R0006 case.** The shared mapping cannot be made to fail portably, so `metrics/no_memory` does not
  exist; `rt_metrics.c` raises R0006 when `mmap` refuses.
- **W0506's fix on a call's only field** makes the list `new list[Field]`. When `Field` is not imported,
  the fix is one edit from the imports to the list that adds `import std.log.Field;`, because a
  diagnostic's fixes are alternatives (`check/secret_logged_import`). Such edits overlap, so a single
  check shows the fix only on the first of them in a file; the next check shows the rest. Fix coverage
  over the corpus is 94% (`apply_fixes.py`, gate 80%).

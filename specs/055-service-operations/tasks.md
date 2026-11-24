---

description: "Task list for vo-lang spec 055: Operating a Service"
---

# Tasks: Operating a Service

**Input**: Design documents from `specs/055-service-operations/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R13), data-model.md, contracts/api.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases land first and fail before its code.

**Organization**:
- Setup: results file, quick area, the `log` effect name, the two options parsed.
- Foundational: the record writer and its builtins (R2–R4), the route pattern on responses, and effect
  inference for steps that carry their own effects (R9).
- US1 (P1) structured logs; US2 (P1) secrets; US3 (P1) metrics; US4 (P2) liveness and readiness;
  US5 (P2) the drain limit; US6 (P2) the example.
- Polish: reference and its cost, guide page, catalog, docs, three architectures, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Behavior**: contracts/api.md is the source of every name, option, record key, metric name and code;
  data-model.md of every rule (word list, cut length, series limit, buckets).
- **Golden cases**: `tests/cases/AREA/NAME.vo` with `.out`/`.err`/`.code` (and `.cmd` when not
  `run {}`), as the existing areas. Write the case, run it with `tests/run.sh tests/cases/AREA` (step cases: `python3 tests/steps.py`), see it fail, then write the code.
- **std**: every exported item has a doc comment with an example line (`#     EXPR is VALUE`), checked by
  the doc tests; one module per file, files under 400 lines (W0702).
- **C**: zero warnings; no static buffer over 64 KiB; records and updates allocate nothing on the vo
  heap beyond the record text; `ponytail:` comments on known ceilings (2,048 series, the lock).
- **Unchanged**: `log_requests`, `to_stderr`, `clock_ms`, `health_route`, spec 054 recordings (cases under
  `tests/cases/recorded_tests_steps` and `tests/cases/replay` must not change).
- **Between edits**: `make quick AREA=ops`; `make test` once at the end; never two suites at once.

## Phase 1: Setup

- [X] T001 Create `specs/055-service-operations/results.md` with headings SC-001 to SC-008, "Reference cost" and "Deviations"; add area `ops` to `tools/quick.sh` (case dirs `log metrics web check effects effect_sets replay serve_workers_steps doc`, script `operations.py --quick`, files `src/rt_log.c src/rt_metrics.c std/log.vo std/metrics.vo std/web.vo tests/operations.py`)
- [X] T002 Add the effect `log` after `entropy` (R1): `es_names` in `src/effects.c`, `effect_order` in `src/packages.c`, `EF_LOG` (4096) in `src/vo.h`, the effect name list in `src/replay.c`, the canonical order line and effects table in `docs/reference.md` (one row: `| log | log records and metrics (std.log, std.metrics) |`), `editors/vim/syntax/vo.vim`, `site/pygments_vo/pygments_vo.py`, `tests/doc_conformance.py`; a case `tests/cases/effects/log_effect.vo` where a function calling a `uses log` function without declaring it gets E0501 with the fix adding `log`, and `vo fmt` puts `log` last
- [X] T003 In `src/main.c` and `src/build.c`: `vo run --log-level LEVEL` (`debug`, `info`, `warn`, `error`; anything else T0001 "`--log-level` takes debug, info, warn or error.") into `opt_log_level` (default info = 1), and `--drain SECONDS` (whole number 0 to 3,600; else T0001) into `opt_drain` (default 30); both only with `run` (`only_with`); built executables take `--vo-log-level` and `--vo-drain`; add them to the options list text; re-accept `tests/cases/cli/bad_flag` if its listed options change; a case `tests/cases/cli/log_level_bad` (`.cmd` `run --log-level loud {}`) shows T0001

## Phase 2: Foundational

- [X] T004 Create `src/rt_log.c` (R2–R5, data-model "Log record"): `rt_log_write(level, message, fields_json)` builds `{"time":…,"level":…,"message":…,["request_id":…,]FIELDS}` with keys in that order, a field named `time`/`level`/`message`/`request_id` written `field_NAME`, string values over 4,096 bytes cut and ending `…[cut]`, JSON escaping of control characters and quotes, and one `write` to fd 2 (or, under `vo test`, appended to the test's captured output and to the test's kept list); records below `opt_log_level` are skipped (none skipped under `vo test`); `time` is UTC `YYYY-MM-DDTHH:MM:SS.mmmZ` from the runtime's current clock value **without** recording a clock event (real time under `vo run`, the test clock under `vo test`, the last replayed value under `vo replay`); per-process state: the current request id (`rt_log_request(Str*)`, 0 to clear), the kept list (`rt_logged()`), `rt_log_reset()` (called by the test runner before each test with the metrics reset of T027)
- [X] T005 Builtins for T004 in `src/vo.h` (`B_LOG_WRITE`, `B_LOG_REQUEST`, `B_LOGGED`, `B_LOG_REDACT` after the last builtin number), `src/check_expr.c` (names `__log_write`, `__log_request`, `__logged`, `__log_redact`, each needing `log`), `src/rt_call.c` (dispatch); `src/testrun.c` calls `rt_log_reset()` before each test
- [X] T006 In `std/web.vo` (R9): `handle_request` adds the header `Vo-Route: PATTERN` to the response of the route that answered (none for 404/405); `without_vo_error` (used by `serve_app`) also removes `Vo-Route`; a case `tests/cases/web/route_header.vo` shows the header from `handle_request` and `tests/cases/web_steps` (or `tests/serve_workers.py`) shows it is never sent; existing web cases unchanged (re-accept only where a test prints all headers, and say so in results "Deviations")
- [X] T007 Effect inference for steps (research R9): in `src/check_generic.c`, when a generic call's result type has an effect argument `uses W, E` and the expected type's effect argument is a set S (`steps: list[Step[C, uses log, clock, file.read("data/")]]` in an `App`), infer E as S without W; when S lacks one of W, report the existing mismatch naming the missing effect. Cases first: `tests/cases/generic/effects_rest_infer.vo` (a local `fn tag[C, E: effects]() -> Step[C, uses console, E]` placed in an `App[int, uses console, clock]` checks and runs), `effects_rest_missing.vo` (the same in an `App[int, uses clock]`: the error names `console`); spec 052's known gap closes (note in results)

**Checkpoint**: `__log_write` writes a record line; steps can read the route pattern.

## Phase 3: User Story 1 - Structured logs tied to the request (P1) 🎯 MVP

**Goal**: JSON records with level, time, message, fields and the request id; one per request from a step.

**Independent Test**: `tests/run.sh tests/cases/log` passes `records`, `levels`, `escape`,
`request_id`, `cut`, `request_logs`, `log_requests_unchanged`.

- [X] T008 [P] [US1] Case `tests/cases/log/records.vo`: a test writing `log_info("started", [field("port", 8080), field("name", "notes"), field("ok", true)])` and one of each level, then printing `logged()`; expected: records with `"time":"1970-01-01T00:00:00.000Z"`, numbers and booleans unquoted, keys in order
- [X] T009 [P] [US1] Case `tests/cases/log/levels.cmd` (`run --log-level warn {}`): `main` writes one record per level; stderr holds only the `warn` and `error` records
- [X] T010 [P] [US1] Case `tests/cases/log/escape.vo`: a field value with `"`, `\`, a newline, a tab, a control byte and non-ASCII text gives one line of valid JSON (checked by `parse_json` in the test) 
- [X] T011 [P] [US1] Case `tests/cases/log/cut.vo`: a field value of 5,000 bytes is written as its first 4,096 bytes followed by `…[cut]`; a field named `message` is written `field_message`
- [X] T012 [P] [US1] Case `tests/cases/log/request_logs.vo`: an app with steps `[request_ids(fn() -> str { return "id-1"; }), request_logs()]` and a route `GET /notes/{id}` whose operation calls `log_info("note read", [field("id", 7)])`; `handle_request` for `/notes/7`, `/missing` and an operation returning `err("boom")`; `logged()` holds: the operation's record with `"request_id":"id-1"`, then `request` records with `method`, `route` (`/notes/{id}`, then `unmatched`), `status`, `ms` 1 (the test clock: one `now_ms` read before, one after), level `info`, and for the 500 level `error` with `error`; a plain `on_get("/health/ready", …)` route of the case answered 200 is logged at `debug` (no dependency on US4's `health_routes`); a record written after `handle_request` returns has no `request_id`
- [X] T013 [P] [US1] Case `tests/cases/log/log_requests_unchanged.vo`: today's `log_requests(fn() -> int { return 0; }, fn(line: str) { ... })` text line, byte-identical to before
- [X] T014 [US1] Create `std/log.vo` (contracts/api.md): `Field`, `field[T: json]` (value through `to_json`), `log_debug`/`log_info`/`log_warn`/`log_error` (fields as a JSON object text passed to `__log_write`), `logged()`, `redact_logs(words)`; module doc line; each with a doc example
- [X] T015 [US1] In `std/web.vo`: `request_logs[C, E: effects]() -> Step[C, uses log, clock, E]` (contracts/api.md, R9): reads `X-Request-Id` from the request, calls `__log_request(id)` before `next` and clears it after, writes the `request` record from the response's status, `Vo-Route` (or `unmatched`) and `Vo-Error`, with `ms` from two `now_ms` reads (recorded clock reads, R2: no unrecorded duration builtin); level rule: `debug` for paths starting `/health/` or equal to `/metrics` answered 200, `error` from 500, else `info`
- [X] T016 [US1] Make T008–T013 pass (T012 needs T007); run each case 20 times and on the instrumented build (identical output); `make quick AREA=ops`

**Checkpoint**: structured logs with request ids work in tests and under `vo run`.

## Phase 4: User Story 2 - No secrets in logs (P1)

**Goal**: secret-like names redacted at any depth; W0506 on logging a secret-named value.

**Independent Test**: `tests/cases/log/redact_*` and `tests/cases/check/secret_logged*` pass.

- [X] T017 [P] [US2] Case `tests/cases/log/redact_fields.vo`: fields named `password`, `api_key`, `reset_token`, `sessionId`, `Set-Cookie`, `author` (not secret: `author` is not `authorization`) and `keyboard` (not secret: the word is `keyboard`); the secret ones read `"[redacted]"`, the others their values
- [X] T018 [P] [US2] Case `tests/cases/log/redact_nested.vo`: a field holding a request's headers (list of `{name, value}`) with `Authorization`, `Cookie`, `Proxy-Authorization`, `Accept`, and an object with a nested `token` member: credential values and the token are `[redacted]`, `Accept` kept
- [X] T019 [P] [US2] Case `tests/cases/log/redact_words.vo`: `redact_logs(["ssn"])` then a field `user_ssn` is redacted
- [X] T020 [P] [US2] Cases `tests/cases/check/secret_logged.vo` with the 10 positive cases of SC-005, each its own `log_*` call, all getting W0506 at the value: a field access `form.password`, a parameter `token`, a local `let secret = …`, `u.api_key`, a nested access `req.auth.session_id`, `cfg.dbPasswd` (case change), `x.authorization`, `h.cookie`, `opts.private_key`, and one at `log_debug`/`log_warn`/`log_error` each covered among them; and negatives with no warning: `u.name`, `hash(pw)` (computed), `u.author`, `keyboard`, a string literal value; `secret_logged_fix` (`vo check --fix` removes the field; the result checks clean), `secret_logged_allow` (`@allow("W0506", "the map entry's key, not a secret")` silences it)
- [X] T021 [US2] In `src/rt_log.c`: the secret-like rule of data-model.md ("words split at `_`, `-`, `.`, and lower-to-upper case changes; compared lower-case"; words `password`, `passwd`, `secret`, `token`, `key`, `authorization`, `cookie`, `session` plus `redact_logs` words) applied to field names, object member names at any depth, and `{name, value}` objects (redact `value`); `__log_redact(words)` adds words for the process
- [X] T022 [US2] W0506 `secret_logged` (R6): in a new `src/check_ops.c` (also home of E0341 in T031), for calls resolving to `std.log`'s `log_debug`/`log_info`/`log_warn`/`log_error`, each `field(NAME, VALUE)` in a list literal argument whose VALUE is an identifier or field access with a secret-like last name gets the warning at VALUE with the message of research R6 and a fix deleting that list element (and its comma); register it where other W05xx warnings are reported; honor `@allow`
- [X] T023 [US2] `docs/errors.md`: entry `## W0506 secret_logged` with description, `Fixable: yes`, Example and Fixed (as catalog.py requires) and its golden case; make T017–T020 pass; `python3 tests/catalog.py`

**Checkpoint**: no credential or secret-named value reaches a record; the checker points at the code.

## Phase 5: User Story 3 - Metrics an operator can scrape (P1)

**Goal**: counters, gauges, histograms in a shared registry; request metrics; `/metrics` in format 0.0.4.

**Independent Test**: `tests/cases/metrics/*` pass and `python3 tests/operations.py --group workers`
shows totals over 4 workers.

- [X] T024 [P] [US3] Cases `tests/cases/metrics/counter.vo`, `gauge.vo`, `histogram.vo` (buckets `[0.1, 1.0]`, observations 0.05, 0.5, 5.0: cumulative `le="0.1"` 1, `le="1"` 2, `le="+Inf"` 3, `_sum` 5.55, `_count` 3), `text.vo` (two metrics, labels with `"`, `\` and a newline escaped, series sorted by name then label values, `# HELP`/`# TYPE` lines, a listed metric with no updates shown as 0), `fresh.vo` (two tests each adding 1 see 1)
- [X] T025 [P] [US3] Cases `tests/cases/metrics/conflict.vo` (R0016: same name as counter then gauge; two label values for one label; `add` of -1 to a counter) and `tests/cases/check/bad_metric_name.vo` (E0341 at `counter("http-requests", …)` with its fix, at label `"__x"`, at a repeated label; none for valid names or non-literal names)
- [X] T026 [P] [US3] Cases `tests/cases/web/request_metrics.vo` (an app with `request_metrics()` and `metrics_route([notes_created])`: after `GET /notes/1`, `GET /notes/2`, `GET /missing`, `POST /notes` (creating a note: `add(notes_created, [], 1)`), the `/metrics` answer has `http_requests_total{method="GET",route="/notes/{id}",status="2xx"} 2`, `{…route="unmatched",status="4xx"} 1`, the duration histogram with every observation 0.001 s (1 ms under the test clock) in the first bucket, `http_requests_in_flight 1` (the scrape counts itself, data-model "Request metrics"), `notes_created_total 1`, content type `text/plain; version=0.0.4`) and `web/request_metrics_methods.vo` (`PROPFIND` counted as `method="other"`)
- [X] T027 [US3] Create `src/rt_metrics.c` (R7, data-model "Series", "Registry"): a region mapped `MAP_SHARED|MAP_ANONYMOUS` on first use or by `rt_metrics_share()`; a lock word holding the owner's pid (a waiter takes over when `kill(pid, 0)` says the owner is gone); a table of at most 2,048 series keyed by metric name and label values, storing kind, help, label names once per metric; `int` values for counters and gauges, bucket counts, `float` sum and count for histograms; further series dropped and counted in `vo_metrics_dropped_series_total`; a refused mapping is R0006 `out_of_memory` (a case `tests/cases/metrics/no_memory` only if the mapping can be made to fail portably, else a note in results); R0016 for the identity rules ("a name has one kind, help and label list for the whole program; values must match the label count; a counter never goes down"); `rt_metrics_text(listed)` rendering format 0.0.4 sorted by name then label values (byte order), floats in vo's float text, `+Inf`; `rt_metrics_reset()` called with `rt_log_reset()` before each test; builtins `__metric_add`, `__metric_set`, `__metric_observe`, `__metrics_text` (each needing `log`) in `src/vo.h`, `src/check_expr.c`, `src/rt_call.c`
- [X] T028 [US3] In `src/serve_workers.c`: call `rt_metrics_share()` before the first worker is forked, so workers and replacements share the registry
- [X] T029 [US3] Create `std/metrics.vo` (contracts/api.md): `Metric`, `counter`, `gauge`, `histogram` (pure, building the record; `buckets` "increasing; `+Inf` added"), `add`, `set_to`, `observe`, `metrics_text`; doc examples
- [X] T030 [US3] In `std/web.vo`: `request_metrics[C, E: effects]() -> Step[C, uses log, clock, E]` and `metrics_route[C, E: effects](metrics) -> Route[C, uses log, E]` (contracts/api.md; R9: `http_requests_total{method,route,status}`, `http_request_duration_seconds{method,route}` with buckets 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10, `http_requests_in_flight`; method outside GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS is `other`; no `Vo-Route` is `unmatched`; duration from two `now_ms` reads, in seconds; in-flight raised before `next` and lowered after, so a scrape counts itself) and the route (GET `/metrics`, `Content-Type: text/plain; version=0.0.4`)
- [X] T031 [US3] E0341 `bad_metric_name` in `src/check_ops.c`: literal first arguments of `counter`/`gauge`/`histogram` against `[a-zA-Z_:][a-zA-Z0-9_:]*`, literal label names against `[a-zA-Z_][a-zA-Z0-9_]*` not starting `__`, repeated labels; position at the bad character as E0330 does; a fix replacing each disallowed character with `_` (none for a `__` prefix or a repeated label; the catalog entry says why); a case `tests/cases/check/bad_metric_name_fix` (`vo check --fix` turns `http-requests` into `http_requests`); `docs/errors.md` entries `## E0341 bad_metric_name` and `## R0016 metric_conflict` with Example and Fixed and golden cases
- [X] T032 [US3] Create `tests/operations.py` with group `workers`: build `examples/notes-api` (after T040, or a small app under `tests/cases/ops_app/` until then) and run it with `vo run --workers 4` on a free port; send 1,000 requests across routes; read `/metrics`; parse it with a strict parser of format 0.0.4 written in the script (comment lines, metric names, label syntax and escapes, values, histogram bucket order and `+Inf`, `_sum`/`_count`); totals of `http_requests_total` equal the requests sent (SC-003); then 10,000 requests to distinct `/notes/N` paths add no series (SC-004); with `--quick`, 200 requests and 1,000 paths
- [X] T033 [US3] Make T024–T026 and `operations.py --group workers` pass; `make quick AREA=ops`

**Checkpoint**: one scrape shows totals over all workers in a format scrapers read.

## Phase 6: User Story 4 - Liveness and readiness (P2)

**Goal**: `/health/live` and `/health/ready` apart; `/health` unchanged.

**Independent Test**: `tests/cases/web/health_routes.vo` passes.

- [X] T034 [P] [US4] Case `tests/cases/web/health_routes.vo`: with a check that fails, `/health/ready` answers 503 `{"status":"down","checks":{"database":"…"}}`, `/health/live` 200 `{"status":"up"}` (its check is not run: the check counts its calls); with the check passing both 200; an app with `health_route` answers `/health` as before
- [X] T035 [US4] In `std/web.vo`: `health_routes(checks) -> list[Route[C, E]]` (GET `/health/live`, GET `/health/ready` sharing `health_route`'s body through a private function); make T034 pass

## Phase 7: User Story 5 - Stopping within a bound (P2)

**Goal**: the drain ends at `--drain` seconds with a `stopped` record.

**Independent Test**: `python3 tests/operations.py --group drain` passes.

- [X] T036 [P] [US5] In `tests/operations.py` group `drain`: a program serving with `serve_workers` (2 workers) whose `/slow` handler sleeps with a busy loop of about 10 s (not `sleep`, which moves the clock in tests only) and `/fast` answers at once; (a) `--drain 2`, a `/slow` request in flight, SIGTERM: the client gets 503, the process exits within 3 s with code 0, the last stderr line is `{"time":…,"level":"warn","message":"stopped","drain_s":2,"drained":0,"cut_off":1}` (SC-006); (b) `/fast` in flight and SIGTERM: answered 200, `"cut_off":0` at `info`; (c) no `--drain`: a `/fast` request and SIGTERM give a stop record with `"drain_s":30` (the default in force, without waiting 30 s); (d) a second SIGTERM stops at once, as today; (e) plain `serve` with `--drain 1` and two queued requests of 2 s: the limit is checked between requests
- [X] T037 [US5] In `src/serve_workers.c`: on the first signal record the deadline `now + opt_drain` (monotonic clock); in the loop, when it passes with workers busy, kill them (as `sw_stop_all(1)`), answer their connections `503 Service Unavailable` (`Connection: close`) and stop; count requests answered during the drain and those cut off; write the stop record through `rt_log_write` with `drain_s` (the limit in force), `drained` and `cut_off` (level `warn` when cut off > 0, else `info`) before returning `ok()`
- [X] T038 [US5] In `src/serve.c` (plain `serve`): after the first signal, before each queued request check the deadline; past it, answer the rest 503 and stop; write the same stop record
- [X] T039 [US5] Make T036 pass; existing serve and serve_workers cases unchanged (`make quick AREA=serve_workers`)

## Phase 8: User Story 6 - The example service is operable (P2)

**Goal**: notes-api shows the whole pattern, with tests.

**Independent Test**: `cd examples/notes-api && ../../build/vo test` passes the new tests.

- [X] T040 [US6] `examples/notes-api`: steps `request_ids(random_id)`, `request_logs()`, `request_metrics()`; routes `metrics_route([notes_created])` and `health_routes([database check])` beside the existing `health_route` if other code uses it; a `notes_created` counter (`counter("notes_created_total", "Notes created.", [])`) added to on create; `log_info("note created", [field("id", id)])`; the app's effects gain `log, clock` (and `random` for `random_id`); `uses log` where needed; re-record the example's recorded tests if any (none today); at most 15 changed lines outside tests (SC-007: count them in results)
- [X] T041 [US6] Tests in `examples/notes-api` (next to the code, W0705): the create request writes a `note created` record with the request's id and a `request` record; `/metrics` shows `notes_created_total 1` after one create; `/health/ready` is 503 when the database check fails (an unreadable path), `/health/live` 200; update `examples/notes-api/README.md` (logs, metrics, health, `--drain`, `--log-level`) and `examples/notes-api/vo.effects` (`vo check` regenerates)
- [X] T042 [US6] Point `tests/operations.py` (T032) at the example; group `logs`: under the 1,000 requests of T032 every stderr line parses as JSON and every `request` record and every `note created` record carries a `request_id` (SC-002); group `secrets`: requests with `Authorization`, `Cookie` and a form field `password` (sent to a route that logs its headers in the test app) and a search of all output for each value finds none (SC-005)

## Phase 9: Polish & Cross-Cutting Concerns

- [X] T043 `docs/reference.md` (R12): rows for `std.log` and `std.metrics` in the library table; `request_logs`, `request_metrics`, `metrics_route`, `health_routes` on the `std.web` row; `--log-level LEVEL`, `--drain S` in the CLI synopsis; write the additions first and run the token check; then move as much `std.web`/`serve_workers` detail already covered in the guide to `site/docs/guide/` as leaves at least 20 tokens of headroom under 15,000; if that is not enough, shorten the new rows before cutting anything else; record before/after numbers in results "Reference cost"
- [X] T044 [P] `site/docs/guide/operating.md` (new, in the site nav after the web guide): logs and levels, the request id, secrets (the word list, `redact_logs`, W0506, "secrets under other names are not detected: keep them out of fields"), metrics and their names, `/metrics`, liveness and readiness for an orchestrator, `--drain` and the platform grace period (the `drain_s` in the stop record), the replay note of R2 (a replay's stderr can differ from the original run in record `time` only; adding the request steps adds clock reads, so recorded tests are re-recorded once); one sentence telling `redact_logs` (log records, in code) apart from `vo test --redact` (test recordings, spec 054); no external resources (site rule)
- [X] T045 [P] `docs/security.md` (logs and secrets), `docs/roadmap.md` (055 section and table row; "Operating a service" removed from the open gaps), `CHANGELOG.md`, `examples/README.md` (notes-api line), `site/docs/guide/effects.md` and `site/docs/guide/standard-library.md` (the `log` effect, `std.log`, `std.metrics`)
- [X] T046 Add `"operations.py --quick"` to the `make test` list in `Makefile`; `tests/operations.py` without `--quick` in the gate list if the gate has a slower list
- [X] T047 Three architectures: the `log`, `metrics`, `web` and `check` cases and `operations.py --quick` on the ARM64 and RISC-V builds and the instrumented build (as earlier specs: `make` targets `build-aarch64`, `build-riscv64`, `build-instrumented`); identical outputs (SC-001)
- [X] T048 Measure the overhead (plan Performance Goals): 1,000 requests to the example with and without the two steps, median of 5 runs; record in results; investigate if over 10%
- [X] T049 `make test` once; fill `specs/055-service-operations/results.md` (each SC with its measurement, "Reference cost", "Deviations"); mark the spec's Status implemented; all tasks `[X]`

## Dependencies & Execution Order

- **Setup (T001–T003)** → **Foundational (T004–T007)** → stories.
- **T007** (effect inference) before T015, T030 and the example: every step and route of this spec
  relies on it.
- **US1 (T008–T016)** first: US2's redaction lives in the same writer, and US3's request metrics follow
  T015's pattern (signature, `now_ms` durations).
- **US2 (T017–T023)** after US1; **US3 (T024–T033)** after Foundational and T015; US2 and US3 can
  proceed in parallel (different files except `src/check_ops.c`: T022 creates it, T031 adds to it).
- **US4 (T034–T035)** only needs Foundational; **US5 (T036–T039)** needs T004 (the stop record) and
  T003 (`--drain`).
- **US6 (T040–T042)** after US1–US4 (uses every piece); T032's workers group moves to the example in T042.
- **Polish** last; T043 after every std name exists.

## Parallel Opportunities

- Within each story, the case tasks marked [P] are separate files and can be written together.
- US4 (std/web.vo only) and US5 (C serving files) can run beside US2/US3 once their prerequisites hold;
  `std/web.vo` edits (T015, T030, T035) are sequential since they share the file.
- T044 and T045 are separate documents.

Example (US3): T024, T025, T026 together; then T027 → T028, T029 → T030, T031; then T032 → T033.

## Implementation Strategy

- **MVP**: Setup, Foundational, US1: structured logs with request ids, usable on their own.
- Then US2 (secrets) before anything ships, since logs without redaction are a risk.
- Then US3 (metrics), US4 (readiness), US5 (drain), US6 (example), Polish.
- `make quick AREA=ops` after each task; `make test` once at T049.

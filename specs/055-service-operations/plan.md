# Implementation Plan: Operating a Service

**Branch**: `055-service-operations` (work on `main`, as for earlier specs) | **Date**: 2026-11-24 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/055-service-operations/spec.md`

## Summary

A new effect `log` covers log records and metrics, both outputs to the operator (R1). A record's time
is an output stamp, not a recorded clock read, so adding a log line does not change a recording or
spec 054's recorded tests. Durations, which reach the program through the metrics text, are recorded
`now_ms` reads, so the request steps use `log, clock` (R2).
- **`std.log`:** writes JSON records to stderr, one line each. Under `vo test` they are kept for
  `logged()` (R3, R4). A request log step tells the runtime the request id, so records written by an
  operation carry it (R5). Secret-like names are redacted by a word rule, and W0506 warns when a log
  field's value is a secret-named variable (R6).
- **`std.metrics`:** keeps metrics in a runtime registry in shared memory. `serve_workers` maps it before
  forking, so any worker answers `/metrics` with the totals (R7, R8). E0341 checks literal names, and
  R0016 reports conflicts at run time.
- **`std.web`:** gains `request_logs`, `request_metrics`, `metrics_route` and `health_routes`.
  `handle_request` passes the matched route pattern to steps in an internal header (R9). The steps'
  types carry their effects (`Step[C, uses log, clock, E]`), and the checker learns to infer `E` from
  an app's effects, which spec 052 left undone (R9).
- **Stopping:** `--drain SECONDS` (default 30) bounds the existing graceful stop, which ends with a
  `stopped` record (R11).
- **Example:** notes-api uses all of it (R13).

| Part | File | What |
|---|---|---|
| Effect | `src/effects.c`, `src/packages.c`, `src/vo.h`, `src/replay.c` (effect names) | `log` after `entropy` |
| Records | `src/rt_log.c` (new) | record writing, escaping, cutting, redaction words, level, request id, test capture; builtins |
| Metrics | `src/rt_metrics.c` (new) | shared registry, lock, series table, text rendering, R0016 |
| Builtins | `src/check_expr.c`, `src/rt_call.c`, `src/vo.h` | `__log_write`, `__log_request`, `__logged`, `__log_redact`, `__metric_add`, `__metric_set`, `__metric_observe`, `__metrics_text` |
| Checks | `src/check_ops.c` (new) | E0341 on literal names, with its fix; W0506 with its fix |
| Inference | `src/check_generic.c` | E of `Step[C, uses W, E]` from an expected effect set (R9) |
| Serving | `src/serve_workers.c`, `src/serve.c`, `src/main.c`, `src/build.c` | registry mapped before fork; drain limit; stop record; `--log-level`, `--drain` (+ `--vo-` forms) |
| Tests runner | `src/testrun.c` | records into the captured output and `logged()`; registry and request id reset per test |
| Library | `std/log.vo`, `std/metrics.vo` (new), `std/web.vo` | the API of contracts/api.md; `Vo-Route` |
| Docs | `docs/reference.md`, `docs/errors.md` (W0506, E0341, R0016), `site/docs/guide/operating.md` (new), roadmap, changelog, `docs/security.md` (logs and secrets) | R12 |
| Example | `examples/notes-api/` | R13 |
| Tests | `tests/cases/log/`, `tests/cases/metrics/`, `tests/cases/check/`, `tests/cases/web/`, `tests/operations.py` (new), `tools/quick.sh` (area `ops`) | R13 |

## Technical Context

**Language/Version**: C11 freestanding (toolchain, runtime); vo (std, example); Python 3 (test driver).

**Primary Dependencies**: none new.

**Storage**: metrics in shared anonymous memory for the process tree; no files.

**Testing**:
- golden cases per area;
- `tests/operations.py` (workers, load, scrape, drain);
- `make quick AREA=ops` between edits, `make test` once at the end;
- the instrumented, ARM64 and RISC-V builds.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain and standard library (one new effect; no syntax).

**Performance Goals**:
- A log record or metric update costs no system call beyond the record's one write.
- 1,000 requests with both steps stay within 10% of the time without them, measured in
  `tests/operations.py`.

**Constraints**:
- No language global variables; no static buffer over 64 KiB (the registry is mapped, at most 2,048
  series).
- Spec 054 recordings unchanged.
- Reference ≤ 15,000 tokens: about 110 tokens added against 71 left; measured after writing, then as
  much moved to the guide as keeps 20 tokens of headroom (R12).

**Scale/Scope**:
- C: about 400 lines in `rt_log.c` and `rt_metrics.c`, 150 in checks, 100 in serving and options.
- vo: 250 lines across the three std files.
- Tests: 30 golden cases and 300 lines of Python.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One way per job: `log_*` with fields, two steps, two route helpers; secrets handled by the runtime and pointed out by the checker, not left to the agent. | Pass |
| II. Machine-readable feedback | Records are JSON lines; metrics a standard text format; W0506, E0341, R0016 are stable codes with catalog entries; W0506 and E0341 (for disallowed characters) have fixes. | Pass |
| III. Determinism | Every value the program receives is recorded: durations are `now_ms` reads; record stamps are outputs the program never reads outside `vo test` (R2). Registry and request id reset per test; output sorted; a refused mapping is R0006. | Pass |
| IV. Test-first | Golden cases of each story before the code. | Pass |
| V. Minimal surface | One effect for logs and metrics; no syntax; existing functions unchanged; reference cost stated and paid for by moving text (R12). | Pass |
| VI. Backend network services | Operate: logs a collector parses, metrics a scraper reads, readiness apart from liveness, a bounded stop. Review: `log` in `vo.effects`, secrets flagged. Write: a few lines in an app. | Pass |

Re-checked after Phase 1: unchanged. Note on "no global variables": the registry, the level, the redaction
words and the current request id are runtime state, like the clock and the seed; the language gains none.

## Project Structure

### Documentation (this feature)

```text
specs/055-service-operations/
├── plan.md
├── research.md          # R1–R13
├── data-model.md        # record, level, secret-like names, metric, series, registry, drain
├── quickstart.md
├── contracts/
│   └── api.md           # effect, std.log, std.metrics, std.web additions, options, codes
└── tasks.md             # /speckit-tasks
```

**Structure Decision**: two new runtime files (`rt_log.c`, `rt_metrics.c`) and two new std modules
(`std/log.vo`, `std/metrics.vo`), since logs and metrics are used outside web apps too. The web pieces
stay in `std/web.vo`.

## Complexity Tracking

| Item | Why needed | Simpler alternative rejected because |
|---|---|---|
| Shared-memory registry with a lock | FR-013: totals over worker processes from any worker's answer | Per-worker metrics show one worker's counts; parent aggregation needs a second protocol |

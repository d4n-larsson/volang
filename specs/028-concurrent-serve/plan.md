# Implementation Plan: Concurrent Request Handling

**Branch**: `028-concurrent-serve` (work on `main`, as for specs 001–027) | **Date**: 2026-11-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/028-concurrent-serve/spec.md`

## Summary

`std.http.serve_workers(address, workers, setup, handle)` (and `serve_bytes_workers`) answer up to
`workers` requests at once. The parent process keeps today's connection handling unchanged (64
connections, each request read whole within its limits, slow clients, stop signals); W worker processes,
forked when the server starts, each run `setup` once and then handle one request at a time, receiving
complete requests from the parent over a pipe and sending back the response, the request's output and its
recording events (research R1). A failing handler answers 500 and its worker goes on; a worker that dies
is replaced; SIGTERM finishes the requests already read and returns `ok()`. `vo run --workers N`
overrides the declared count. Recordings keep each request's inputs as one block in arrival order, so
replay runs the requests one at a time in one process with the same responses (R9). `serve` and
`serve_bytes` are unchanged.

| Part | File | What |
|---|---|---|
| Library | `std/http.vo` | `serve_workers`, `serve_bytes_workers` (vo), over the new primitives |
| Workers | `src/serve_workers.c` (new) | forking and replacing workers, dispatch, the request/response protocol on pipes, per-request output and recording events, shutdown |
| Connections | `src/serve.c` | the parent's connection loop reused: next complete request, reply by connection, stop |
| Primitives | `src/rt_call.c`, `src/check_expr.c`, `src/vo.h` | `__serve_workers_start`, `__serve_workers_next`, `__serve_workers_reply` and the worker loop entry |
| Recording | `src/replay.c` | `setup` blocks; per-request blocks written in arrival order; replay of a worker recording in one process |
| CLI | `src/main.c` | `--workers N` (run only; T0001 elsewhere and out of 1–64) |
| Randomness | `src/rt_effects.c` (or where the seed lives) | per-request random sequence in workers (R8) |
| Example | `examples/notes-api/` | notes in SQLite with transactions, served by workers |
| Docs | `docs/reference.md`, `site/docs/guide/`, `docs/security.md` | the model, the API, the state guidance |

## Technical Context

**Language/Version**: C (C11, freestanding) for the runtime; vo for `std/http.vo` and the example.

**Primary Dependencies**: none new (fork, pipes, poll, signalfd as spec 011/016 use).

**Storage**: none; recordings as today, with `setup` blocks.

**Testing**: golden step cases `tests/cases/serve_workers_steps/` (start, request, request-many, signal,
no-children); `tests/serve_workers.py` (SC-001, SC-003, SC-004, SC-005, notes-api load, SC-006 replay);
`tests/serve_workers_speed.py` (SC-002, `make gate`); every existing server case unchanged (SC-007);
`make quick` between edits, `make test` once per change.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64 (qemu for the step cases, as spec 016).

**Project Type**: compiler toolchain and runtime (CLI).

**Performance Goals**: fast requests within 50 ms while slow ones run (SC-001); ≥ 3× throughput with 4
workers on ≥ 4 cores (SC-002).

**Constraints**: no shared memory between requests; every worker with `main`'s grant; deterministic
replay; existing servers unchanged.

**Scale/Scope**: about 700 lines of C, 60 of vo, docs and one example rewritten.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | Two new functions with their own names; `serve` keeps its meaning; one way to declare workers (the argument), one to override (`--workers`). No hidden concurrency. | Pass |
| II. Machine-readable feedback | No new diagnostic codes; `--workers` misuse is T0001; worker events are notes on stderr (with `--json`, records as other notes). Recording schema 1 gains kinds (`setup`, `setup_end`), none removed. | Pass |
| III. Determinism | Requests' outputs are concurrent by request of the program; each request's random sequence is derived from the seed and its number; replay is sequential and gives identical responses; every failure ends in a 500 or a report, never a hang (shutdown bounded by `--timeout`). | Pass |
| IV. Test-first through the CLI | Step cases and the scripts land before the code; existing server cases unchanged. | Pass |
| V. Single source of truth, minimal surface | Reference, catalog (no new codes), contracts updated together; reference growth ≤ 150 tokens; YAGNI: no autoscaling, no shared cache, no HTTPS. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/028-concurrent-serve/
├── plan.md
├── research.md           # R1–R12
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── library.md        # serve_workers, serve_bytes_workers
│   ├── cli.md            # --workers, notes
│   └── recording.md      # setup blocks, request blocks with workers
└── tasks.md
```

### Source Code (repository root)

```text
src/serve_workers.c       # new
src/serve.c, src/rt_call.c, src/check_expr.c, src/vo.h, src/replay.c, src/main.c, src/rt_effects.c
std/http.vo
examples/notes-api/
tests/cases/serve_workers_steps/, tests/serve_workers.py, tests/serve_workers_speed.py
docs/reference.md, docs/security.md, docs/roadmap.md, site/docs/guide/
```

**Structure Decision**: the worker pool in its own file beside `serve.c` (connections) and `task.c`
(tasks), reusing the task machinery's patterns (fork, pipes, message framing) rather than its code paths,
since a worker serves many requests while a task returns one result.

## Complexity Tracking

No constitution violations.

# Implementation Plan: Workers (processes that run beside the program and exchange messages)

**Branch**: `047-workers` (work on `main`, as for specs 001–046) | **Date**: 2026-11-22 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/047-workers/spec.md`

## Summary

A worker is a fork of the program, as a `concurrent_map` task is, that does not have to be waited
for: the program keeps a table of up to 64 of them and exchanges framed, typed messages with each
over pipes. One new runtime file, one internal builtin, and a library module in vo.

- **Process and pipes** as tasks have them, with the kernel asked to end a worker when the program
  dies (R1, R6). Measured: a start costs 3 ms with 100 MiB held; two bare pipes make a round trip in
  12 microseconds, so the spec's bound for one is 50, not 20.
- **Surface**: `std/workers.vo` over `__worker(OP, ...)`; a wait is `Wait.forever` or
  `Wait.at_most(ms)` (R2).
- **Messages** in `task.c`'s typed transfer form; types that cannot cross are refused when the worker
  is started (R0015), not at check time: they are type arguments of a library function (R3).
- **The program never waits to send while the worker waits to be read**: it takes in the worker's
  messages meanwhile, up to 64 MiB (R4).
- **A worker's failure is a value**; one nobody asked about is reported at the program's end with
  status 70 (R5).
- **Workers are started only by the program's own process**, not inside a task, a worker or a
  `serve_workers` worker (R7). This rules out `with_deadline` in a request handler.
- **Output** of a worker travels up its pipe in order with its messages and is written by the
  program when it receives past it, or at the worker's stop or the program's end (R10).
- **A replay does not run workers**: the recording holds what the program received from them and
  their output; the replayed program prints and ends as the recorded run did (R11).

| Part | File | What |
|---|---|---|
| Runtime | `src/workers.c` (new), `src/vo.h` | the table, start, tell, receive, receive_first, stop, the worker's side, shutdown, recording |
| Hooks | `src/interp.c` (a worker's runtime error), `src/sys.c` (`vo_exit`: a worker's exit; the program's shutdown), `src/task.c` and `src/serve_workers.c` (children close worker pipes), `src/testrun.c` (workers ended with each test), `src/rt_call.c`, `src/check_expr.c` (the builtin), `src/replay.c` (two event kinds) | |
| Library | `std/workers.vo` (new), `Makefile`, `src/doc.c`, `tools/quick.sh` | contracts/workers.md |
| Example | `examples/job-queue/` (new), `examples/README.md` | R14 |
| Docs | `docs/reference.md` (one row, the `std.bits` row shortened), `docs/errors.md` (the R0015 entry covers a message type that cannot cross), `site/docs/guide/` (a page on workers, linked from the concurrency text), `CHANGELOG.md`, `docs/roadmap.md` (row, section, the "Events" row), `docs/review-2026-11/README.md` | |
| Tests | `tests/workers.py` (new), `tests/cases/workers/` (new), `tests/cases/doc/all*.out` | R13 |

## Technical Context

**Language/Version**: C11 (freestanding, no libc) for the runtime; vo for the library.

**Primary Dependencies**: none new. System calls already wrapped in `src/sys.c`: fork, pipe, poll,
kill, wait, the parent-death signal.

**Storage**: none.

**Testing**: golden cases for deterministic behaviour; `tests/workers.py` for volume, leaks, time
limits, speed and replay; a `vo test` case; the example's tests. `make quick AREA=workers` between
edits, `make test` once; the gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: a small round trip under 50 microseconds; a start under 10 ms with 100 MiB
held; four workers at least 2.5 times one process on four cores (SC-004).

**Constraints**: no worker outlives the program; deterministic results when every receive names its
worker and has no limit; reference under 15,000 tokens (14,979 now); `concurrent_map` and
`serve_workers` unchanged.

**Scale/Scope**: about 600 lines of C, 150 of vo, 450 of test script, 14 golden cases, an example of
about 150 lines, a guide page.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. One form per construct | No syntax. One way to wait (`Wait`), one way to start. `concurrent_map` stays the form for "these N things, then go on"; the guide says when to use which. | Pass |
| II. Machine-readable feedback | No new code: R0007 and R0015 for misuse; everything a running program can meet is a `WorkerError` value. | Pass |
| III. Determinism and defined behaviour | Output and results are the same on every run when receives name their worker and have no limit: a worker's output is written by the program in order with its messages, and `tell` answers from what has been received. Timing enters only through named operations (`Wait.at_most`, `receive_first`, `stop_worker`, the program's end while workers run); a recording fixes those. No shared memory. | Pass |
| IV. Test-first | Golden cases and `tests/workers.py` before the code they cover. | Pass |
| V. Minimal surface, reference budget | One builtin, internal; one library row, paid for by shortening another; two `ponytail:` limits named (R7, R11). | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/047-workers/
├── plan.md
├── research.md          # R1–R14
├── data-model.md
├── quickstart.md
├── contracts/
│   └── workers.md
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/workers.c (new), src/vo.h, src/interp.c, src/sys.c, src/task.c, src/serve_workers.c, src/testrun.c,
src/rt_call.c, src/check_expr.c, src/replay.c
std/workers.vo (new), Makefile, src/doc.c, tools/quick.sh
examples/job-queue/ (new)
docs/reference.md, site/docs/guide/, CHANGELOG.md, docs/roadmap.md
tests/workers.py (new), tests/cases/workers/ (new)
```

**Structure Decision**: one new C file for the runtime, hooks of a few lines in the files that own
the paths a worker crosses.

## Order of work

1. Golden cases and `tests/workers.py` written and failing; the module registered.
2. The builtin, the table, start and the worker's end; `tell`, `receive`, the worker's side.
3. Failure as a value; stop; shutdown at the program's end; the hooks in tasks, serve workers and
   tests; the leak test.
4. `receive_first`; `with_deadline`; the bounds.
5. Workers under `vo test`; recording and replay; trace.
6. The example; docs; three architectures; `make test`; results.

## Complexity Tracking

Nothing beyond the constitution's principles.

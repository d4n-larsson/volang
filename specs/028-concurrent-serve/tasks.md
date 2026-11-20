---

description: "Task list for vo-lang spec 028: Concurrent Request Handling"
---

# Tasks: Concurrent Request Handling

**Input**: Design documents from `specs/028-concurrent-serve/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R12), data-model.md, contracts/library.md,
contracts/cli.md, contracts/recording.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's step cases or script checks land first and fail before
its code.

**Organization**:
- Setup: test wiring and the results file.
- Foundational: the parent's connection loop made reusable, the worker pool with dispatch, the
  `serve_workers` library functions and their primitive, `--workers`: the smallest working concurrent
  server, which every story builds on.
- US1 (P1) slow vs fast requests; US2 (P1) isolation and per-request output; US3 (P1) shutdown; US4 (P2)
  per-worker setup and state; US5 (P2) recording and replay; US6 (P3) documentation and the notes-api
  example.
- Polish: throughput, verification on three platforms.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Step cases** go in `tests/cases/serve_workers_steps/<name>/steps` (as `tests/cases/serve_concurrent/`):
  `start vo run main.vo`, `request`, `request-many`, `request-open`/`read-open`, `signal TERM|INT`,
  `no-children`. Timing-sensitive checks (latency, load, replay ×20) go in `tests/serve_workers.py`, which
  starts servers itself.
- **Never change `serve`/`serve_bytes` behavior**: every case under `tests/cases/serve*` and
  `tests/cases/https*` must pass unchanged after each task (SC-007).
- **Processes**: every fork is reaped; `no-children` after each step case's server stops must print 0.
- **GC safety**: values held across allocations are rooted; run the stress build over the new step cases
  before marking a C task done.
- **Between edits**: `make quick AREA=serve_workers`; `make test` once per change; never two suites at
  once.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: area `serve_workers` (cases `serve`, `serve_concurrent`, `serve_workers_steps`,
  `replay`; script `serve_workers.py --quick`) and the file mapping `src/serve.c`, `src/serve_workers.c`,
  `std/http.vo`, `tests/serve_workers*.py` → `serve_workers`.
- [X] T002 [P] `specs/028-concurrent-serve/results.md`: SC-001–SC-007 (pending), Verification, Deviations.
- [X] T003 [P] `tests/serve_workers.py` skeleton: starts a vo server on a free port in a temporary directory
  from an inline program, sends requests (concurrently with threads), measures each response's latency,
  sends signals, stops it and checks no process is left; `--quick` runs the short checks only.

---

## Phase 2: Foundational (blocking)

**Purpose**: a `serve_workers` server that answers requests with W workers.

- [X] T004 Step case `tests/cases/serve_workers_steps/basic/` (fails first): a program calling
  `serve_workers("{vport}", 2, setup, handle)` with a setup returning `ok("ready")` and a handler echoing
  the setup's value and the path; `start vo run main.vo`, three `request GET /a`..`/c` (each answered with
  `ready /a`...), `signal TERM` (exit 0, `serve_workers` returned `ok()`), `no-children` (0).
- [X] T005 `src/serve.c`: split `rt_serve_next` into a reusable event loop: the poll set of the listener,
  the signalfd and the connections (`sv_poll_fds`), handling their events (`sv_handle_events`: accept,
  read, refuse, write), taking the oldest complete request with its connection index (`sv_take_request`)
  and replying to a given connection (`sv_reply_to`); `rt_serve_next`/`rt_serve_reply` rebuilt on them with
  identical behavior (every `tests/cases/serve*` case unchanged).
- [X] T006 `src/serve_workers.c` (new), `src/vo.h`: the pool of data-model.md ("Pool"): `workers` "1 to 64",
  slots with pid and two pipes, a waiting queue in arrival order, `next_number`; forking W workers after the
  listener exists (each: setup once, then a loop reading `request` messages and answering each with the
  `answer` closure under the same recovery as `rt_serve_call`, sending `response`); the parent's loop
  polling the connections (T005), the signalfd and the workers' response pipes, giving the oldest waiting
  request to the first `ready` worker, writing responses to their connections. Message framing:
  length-prefixed, as `src/task.c`'s.
- [X] T007 `src/rt_call.c`, `src/check_expr.c`, `src/vo.h`: the primitive `__serve_workers(listener: int,
  workers: int, setup: fn() -> result[S], answer: fn(S, str) -> str) -> result[void]` (std only, as the
  other `__serve_*`), dispatching to T006.
- [X] T008 `std/http.vo`: `serve_workers` and `serve_bytes_workers` exactly as contracts/library.md
  (signatures, doc comments with examples, `err("workers must be 1 to 64, got N")`), building the `answer`
  closure as `serve` builds its own (parse the request, call `handle(state, request)`, render; 400 on a bad
  request); `tests/doc_conformance.py` passes.
- [X] T009 `src/main.c`: `--workers N` for `vo run` ("N from 1 to 64 (else T0001)"; T0001 on other
  commands), replacing the `workers` argument at the primitive (T007); on a program whose server is
  `serve`/`serve_bytes`, the note of contracts/cli.md on stderr at the first `serve` call. Make T004 pass.

**Checkpoint**: T004 passes; every existing server case unchanged.

---

## Phase 3: User Story 1 — A slow request no longer delays the others (Priority: P1) 🎯 MVP

**Goal**: fast requests answered at once while slow ones run.

**Independent Test**: with 4 workers and 3 two-second requests in progress, 50 fast requests are each
answered within 50 ms.

- [X] T010 [P] [US1] `tests/serve_workers.py` check `slow_vs_fast` (SC-001): `/slow` sleeps 2 s, `/fast`
  answers at once; 4 workers; 3 slow requests, then 50 fast ones: each fast response within 50 ms; the
  same program with `serve`: the fast ones wait (documents today's behavior).
- [X] T011 [P] [US1] Step cases `tests/cases/serve_workers_steps/queue/` (2 workers, 3 slow requests opened
  with `request-open`, then a fast one: answered after the first slow one finishes, in arrival order among
  the waiting) and `workers_option/` (`--workers 1` turns the same program sequential; `--workers 0` and
  `--workers 65` are T0001; `--workers 2` on a `serve` program prints the note and serves sequentially).
- [X] T012 [US1] Make T010 and T011 pass (dispatch order, the waiting queue bounded by the 64 connections).

---

## Phase 4: User Story 2 — A failing request affects only itself (Priority: P1)

**Goal**: runtime errors, timeouts and memory limits fail one request; a dead worker is replaced; output
appears whole per request.

**Independent Test**: among 20 requests in flight, a crashing, a timing-out and a memory-exhausting one
get 500; the other 17 and the next 100 are answered.

- [X] T013 [P] [US2] `tests/serve_workers.py` check `isolation` (SC-003) as the Independent Test, plus a
  worker killed with SIGKILL from outside while handling a request: that request gets 500 (or its
  connection closes), the note `worker N ended unexpectedly; it was replaced` appears on stderr, and 100
  further requests are answered.
- [X] T014 [P] [US2] Step case `tests/cases/serve_workers_steps/output/`: two workers, two requests whose
  handlers each print three lines with a short sleep between them: each request's three lines appear
  together on stdout (FR-009); a handler's runtime error report appears whole on stderr.
- [X] T015 [US2] `src/serve_workers.c`: per-request output (research R7: the worker's stdout and stderr
  flushed into `output` messages at the end of each call; the parent writes them whole when the response
  is sent); a dead worker (its response pipe closed without a response): the request answered 500 if its
  connection is still open, the note, a replacement forked from the parent (running the setup); timeouts
  and `--max-memory` per worker through the existing recovery (500). Make T013 and T014 pass.

---

## Phase 5: User Story 3 — Shutting down finishes the work in progress (Priority: P1)

**Goal**: SIGTERM answers every request already read, then `serve_workers` returns `ok()`.

**Independent Test**: 5 slow requests in progress, SIGTERM: all 5 answered, new connections refused, exit 0.

- [X] T016 [P] [US3] Step case `tests/cases/serve_workers_steps/shutdown/` (5 slow requests open, `signal
  TERM`, each answered, exit 0, `no-children` 0) and `tests/serve_workers.py` check `shutdown` (SC-004,
  plus: a second SIGINT during shutdown stops at once; a request still running at its `--timeout` is
  stopped with 500 and shutdown completes).
- [X] T017 [US3] `src/serve_workers.c`: shutdown per research R6 (close the listener, answer the requests
  already read, close the request pipes, reap; a second signal kills the workers and closes every
  connection). Make T016 pass.

---

## Phase 6: User Story 4 — Each worker has its own resources (Priority: P2)

**Goal**: the setup runs once per worker; its result reaches every request of that worker; failures stop
the server.

**Independent Test**: 1,000 requests increment a SQLite counter through 4 workers: exactly 1,000; the
database opened once per worker.

- [X] T018 [P] [US4] `tests/serve_workers.py` check `counter` (SC-005): setup opens `data/count.db`
  (counts its opens in a second table), the handler increments the counter in a `transaction`; 1,000
  concurrent requests: counter 1,000, opens 4. Step cases `tests/cases/serve_workers_steps/setup_err/`
  (setup returns `err("no database")`: `serve_workers` returns that err before any request, exit 1,
  `no-children` 0) and `setup_effects/` (the setup's effects are checked: a setup reading outside the
  grant is R0010 at start).
- [X] T019 [US4] `src/serve_workers.c`: the setup's `err` or runtime error at start stops the other workers
  and returns the `err` (or the runtime error); in a replacement worker, shuts down as on SIGTERM and
  returns it (research R5); the setup under `--timeout`. Make T018 pass.

---

## Phase 7: User Story 5 — Recordings and tests stay deterministic (Priority: P2)

**Goal**: a recorded worker session replays to the same responses in one process.

**Independent Test**: a recorded 200-request session against 4 workers reading and writing a database
replays to identical responses 20 times, with no network or files touched.

- [X] T020 [P] [US5] `tests/serve_workers.py` check `replay` (SC-006) as the Independent Test; step case
  `tests/cases/serve_workers_steps/replay/` (record 20 requests to 2 workers, delete the database, `vo
  replay` twice: identical output; a changed handler: R0014 naming the request; `vo replay --workers 2`:
  T0001).
- [X] T021 [US5] `src/replay.c`, `src/serve_workers.c`: recording per contracts/recording.md (each worker's
  events go to the parent inside its messages; `setup` blocks written when a worker is ready; request blocks
  written contiguously in arrival order, finished blocks held until the ones before them are written);
  replay of `__serve_workers` in one process (the setup once on the first `setup` block, the other blocks
  skipped, each request on its own block, responses compared as today).
- [X] T022 [US5] Randomness per request (research R8): in a worker, each handler call's random sequence is
  derived from the run's seed and the request's number; replay derives it the same way. Make T020 pass.

---

## Phase 8: User Story 6 — The model, documented, and the example made safe (Priority: P3)

**Goal**: agents learn the model where they look; the notes-api example keeps state safely.

**Independent Test**: the reference and guide state the model; notes-api under 1,000 concurrent requests
loses no note.

- [X] T023 [P] [US6] `docs/reference.md`: the `serve_workers` bullet in the standard-library notes (the
  model: separate processes, no shared memory, setup per worker, `--workers`, state in a database with
  transactions, never read-change-write a file across requests), ≤ 150 tokens with the std table row
  (`tests/ref_examples.py`); the `vo run` options list gains `--workers`.
- [X] T024 [P] [US6] `site/docs/guide/standard-library.md` (std.http section) and `site/docs/tools/run.md`:
  `serve_workers` with a runnable example (as far as `tests/site_examples.py` runs servers) and the state
  guidance; `docs/security.md` S3 (processes for workers), S5 (limits per worker), S9 (robustness with
  workers); `docs/roadmap.md` (spec 028 done, with "Not done": automatic scaling, a shared cache, HTTPS
  serving).
- [X] T025 [US6] `examples/notes-api/`: notes in SQLite (`data/notes.db`, opened in a setup, changes in
  `transaction`s), served by `serve_workers("8080", 4, ...)`; tests still next to the code (spec 027);
  README, `vo.effects` and `recordings/demo.jsonl` updated; `tests/serve_workers.py` check `notes_load`:
  1,000 concurrent POSTs, then GET lists exactly 1,000 notes (SC-005).

---

## Phase 9: Polish and verification

- [X] T026 [P] `tests/serve_workers_speed.py` (SC-002, `make gate`, skipped with a message on machines with
  fewer than 4 cores): a computing handler, 2,000 requests from 8 clients, 1 vs 4 workers: ≥ 3×.
- [X] T027 `make test` gains `"serve_workers.py --quick"`; SC-007: every existing `tests/cases/serve*`,
  `https*` and `replay` case unchanged.
- [X] T028 `make lint`; `make instrumented` (golden suite and `tests/serve_workers.py --quick` with
  `VO_BIN=build-instrumented/vo`); the stress build over `tests/cases/serve_workers_steps`.
- [X] T029 `make test` on x86-64, then `make ARCH=aarch64 test` and `make ARCH=riscv64 test`, one at a time.
- [X] T030 Fill in `results.md`; spec status Implemented; every task [X].

---

## Dependencies & Execution Order

- Phase 1 → Phase 2 (T004 first; T005 → T006 → T007 → T008 → T009).
- US1, US2, US3 after Phase 2; all touch `src/serve_workers.c`, so implement them one at a time (their
  checks can be written in parallel).
- US4 after US2 (replacement workers run the setup, T015).
- US5 after US1–US4 (recording covers setups and replacements).
- US6 after US4 (the example uses the setup); T023/T024 can start any time after Phase 2.
- Phase 9 last.

## Parallel Opportunities

- T002, T003 together.
- The checks and step cases of each story (T010, T011, T013, T014, T016, T018, T020) once Phase 2 is done.
- T023 and T024; T026 with the stories.

## Implementation Strategy

- **MVP**: Phase 1, Phase 2, US1: concurrent handling with the waiting queue, which removes the blocker.
- Then US2 and US3 (production safety), US4 (databases), US5 (replay), US6 (docs and the example), Polish.
  Every step keeps `serve` unchanged.

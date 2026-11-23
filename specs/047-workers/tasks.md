---

description: "Task list for vo-lang spec 047: Workers"
---

# Tasks: Workers (processes that run beside the program and exchange messages)

**Input**: Design documents from `specs/047-workers/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R14), data-model.md, contracts/workers.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases and its group in `tests/workers.py`
land first and fail before its code.

**Organization**:
- Setup: the module registered, the builtin's shell, the test script.
- Foundational: the table, starting a worker, the worker's end. Every story needs them.
- US1 (P1) exchange; US3 (P1) failure and lifetime, before US2 (P1) deadline and stop, because
  `with_deadline` is built from start, receive and stop and its failure case is US3's;
  US4 (P2) several workers; US5 (P2) computing; US6 (P2) tests, recordings, traces; US7 (P2) example
  and documentation.
- Polish: three architectures, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Types, signatures, stops**: contracts/workers.md and data-model.md (the error cases, the life of
  a worker, the limits: "workers running at a time: 64", "one message, encoded: 16 MiB", "in flight
  in each direction before the sender waits: 1 MiB", "taken in by the program while it waits to
  send: 64 MiB per worker"). Decisions: research.md R1 to R14.
- **Runtime code** goes in `src/workers.c`; other files get hooks of a few lines that call into it.
  Model: `src/task.c` (fork, pipes, framing, `enc_value`/`dec_value`, `group_stop`) and
  `src/serve_workers.c` (long-lived children, buffers kept as collector roots).
- **Golden cases** live in `tests/cases/workers/`; only deterministic programs (every receive names
  its worker; time limits only where the outcome cannot depend on timing: a worker that never emits,
  or one that has already ended). A case needs a `.code` file; run one directory per call of
  `tests/run.sh`; accept with `--accept` and review the diff.
- **No existing golden case may change**, except `tests/cases/doc/all.out` and `all_json.out` (the
  module listing). `concurrent_map` and `serve_workers` cases must pass unchanged after every phase:
  `make quick AREA=tasks` and `AREA=serve_workers`.
- **Every wait is bounded in tests**: scripts run programs with `--timeout` and a subprocess
  timeout, so a hang is a failure, not a stuck suite.
- **Doc comments**: every exported item has one with examples in the form `#     EXPR is VALUE`
  (or with setup statements before the expression); private functions have none. No spec numbers on
  the site.
- `make quick AREA=workers` between edits; `make test` once, in T024; the gate is not run.

## Phase 1: Setup

- [X] T001 Create `std/workers.vo` with its header comment and the types of data-model.md (`Worker`,
  `Link`, `Wait`, `WorkerError`, `Arrival`), each with a doc comment and a comment per field or
  case; add it to `STD_FILES` in `Makefile` and `"workers"` to `doc_modules` in `src/doc.c`; add
  `workers.py` to the test list in `Makefile`; add area `workers` to `tools/quick.sh` (golden areas
  `workers tasks doc`; scripts `doc_conformance.py workers.py`; file mapping for `src/workers.c`,
  `std/workers.vo`, `tests/workers.py`); `make` builds.
- [X] T002 The internal builtin `__worker(OP, ...)`: its name in `builtin_names` and id in
  `src/check_expr.c` (typed loosely, as `B_COLL`: the first argument an int literal, the result the
  expected type), its `BU_` entry and static assert in `src/rt_call.c` dispatching to
  `worker_op(call, a0, a1, a2)` in a new `src/workers.c` that stops with T0003 "not implemented" for
  every operation; `src/workers.c` in the build; prototypes in `src/vo.h`.
- [X] T003 Create `tests/workers.py` on the pattern of `tests/iteration.py` (`--group`, `case()`,
  temp directory under `build/tmp`, emulation and `CI` detection, every run with `--timeout` and a
  subprocess timeout) with groups `exchange`, `leaks`, `deadline`, `speed`, `replay` as in
  research.md R13. `leaks` finds processes by an environment variable the script sets for the run
  (read from `/proc/*/environ`), so it sees only its own. All groups fail now.

## Phase 2: Foundational

- [X] T004 `src/workers.c`: the table of 64 slots (pid, pipe ends, generation, the concrete `In` and
  `Out` types, the bytes arrived and not read, the end state and its text, whether the end was
  delivered), kept as collector roots; operation 0, start: R0007 outside the program's own process
  (R7), R0015 for a message type that cannot cross, by walking the concrete types (R3), `too_many`
  when no slot is free; `docs/errors.md`: the R0015 entry also covers a message type that cannot
  cross (run `python3 tests/catalog.py`); pipes (1 MiB each), flush of pending output, trace and
  recording, fork; the
  child asks to die with its parent, closes every other worker's pipe ends, takes its own random
  sequence (`rt_task_seed`), turns recording off, marks every slot ended in its own copy of the table, sends its output up the
  control pipe as frames from then on (R10), stops with R0007 if it reads standard input, and calls
  the function; the handle is slot and generation.
- [X] T005 The worker's end (R5): a last framed message for "function returned", for a runtime error
  reaching the worker's base recovery point (a hook beside the task hook in `src/interp.c`; the text
  is the error as the program would print it) and for `exit(n)` (a hook in `vo_exit`, `src/sys.c`),
  then `exit_group`. In the program: a control pipe that closes without a last message is `failed`
  with the wait status's signal. `std/workers.vo`: `start_worker`.

## Phase 3: US1 - Work that runs beside the program (P1)

**Goal**: start, tell, receive, and the worker's side. **Independent test**:
`tests/cases/workers/exchange` and `tests/workers.py --group exchange`.

- [X] T006 [US1] Golden cases, failing: `tests/cases/workers/exchange.vo` (story 1 scenarios 1 to 4:
  a doubler sent 1,000 numbers, the answers in order; the worker's copy of a list it captured
  changes without the program's changing, and the reverse; a worker that emits three messages and
  returns: three messages, then `ended`, then `ended` again; `Wait.at_most(0)` and
  `Wait.at_most(50)` on a worker that never emits: `timeout`; `tell` to a worker whose end has been
  received: `ended`, and before that `ok`; a worker that prints between its messages: each line
  comes out just before the message after it;
  records, lists, maps, variants and `result` values as messages), `tests/cases/workers/bad_type`
  (R0015 for a function in `Out`, and for an `ordered_map` in `In`), `tests/cases/workers/bad_wait`
  (R0007 for `Wait.at_most(-1)`), `tests/cases/workers/effects.vo` in the check area style (a worker
  function that prints needs `uses console` on the function that starts it: the existing error
  without it), `tests/cases/workers/grant` (a worker reading a file outside the granted directory:
  `failed` naming R0010), `tests/cases/workers/seed` (two workers emit `random_int` values under
  `--seed`: the same on every run, different from each other), `tests/cases/workers/stdin` (a worker
  that reads standard input: `failed` naming R0007).
- [X] T007 [US1] `src/workers.c`: tell (encode with the frame's concrete `In`, compare with the
  slot's types (R0007 on a mismatch or a stale or forged handle), `too_large` over 16 MiB, write
  without blocking; the waiting-to-send loop comes in T015), receive (what has arrived first; else
  poll the control pipe within the limit and `--timeout`'s deadline; decode with `Out`), and the
  worker's side: next message (poll the inbox) and emit (blocking write), each flushing the
  worker's output first. `std/workers.vo`: `tell`, `receive`, `next_message`, `emit`.
- [X] T008 [US1] `tests/workers.py --group exchange` passes: 64 workers, 10,000 messages each way
  each (640 and 1,000 under emulation), none lost, repeated or out of order (SC-001).

## Phase 4: US3 - Failures are values, and nothing is left behind (P1)

**Goal**: isolation and no leaks. **Independent test**: `tests/workers.py --group leaks` and the
cases below.

- [X] T009 [US3] Golden cases, failing: `tests/cases/workers/failed.vo` (a worker that divides by
  zero after two messages: two messages, then `failed` whose text holds the code and the message,
  then the program goes on and prints; `tell` to it: `ok` before the failure was received, `failed`
  after; a worker that calls `exit(0)`:
  `ended`; `exit(3)`: `failed` naming the status), `tests/cases/workers/unasked` (a failure nobody
  asked about: the runtime error on standard error at the program's end, exit status 70),
  `tests/cases/workers/too_many` (the 65th start is `too_many`; after one is stopped, a start
  succeeds), `tests/cases/workers/nested` (R0007 for `start_worker` inside a worker and inside a
  `concurrent_map` task; `concurrent_map` inside a worker works).
- [X] T010 [US3] `src/workers.c`: delivery of `failed` and `ended` after the messages that arrived
  before them, writing the worker's output frames passed on the way, marking the end delivered; the
  shutdown called from `vo_exit` in `src/sys.c` before the exit hooks: every running worker killed
  and reaped, the output that had arrived written in slot order, each
  undelivered failure written to standard error, status 0 turned into 70. `src/task.c` and
  `src/serve_workers.c`: their children close the inherited worker pipes and mark every slot ended
  (R6).
- [X] T011 [US3] `tests/workers.py --group leaks` passes: programs with workers computing, sleeping
  and waiting on a TCP connection to a local server that never answers, ended by return, `exit`, a
  runtime error, `--timeout`, SIGTERM and SIGKILL: after 200 runs of each (20 under emulation) no
  process of the run exists one second later (SC-002).

## Phase 5: US2 - A time limit for one call, and stopping work (P1)

**Goal**: `stop_worker`, `with_deadline`. **Independent test**: `tests/workers.py --group deadline`.

- [X] T012 [US2] Golden case, failing: `tests/cases/workers/deadline.vo` (story 2: `with_deadline`
  of a function that returns at once: its value; of one that loops forever, 100 ms: `timeout`; of
  one that fails: `failed` with its text; `stop_worker` on a worker that loops, then `receive` and
  `tell`: `ended`; `stop_worker` twice; a worker stopped while it sleeps; the program's own values
  unchanged by the function).
- [X] T013 [US2] `src/workers.c`: stop (SIGKILL, wait, the slot's end `ended`, delivered).
  `std/workers.vo`: `stop_worker`, and `with_deadline` in vo as research.md R9.
  `tests/workers.py --group deadline` passes: 100 of 100 runs return `timeout` within 300 ms of a
  100 ms limit, and the function's process is gone (SC-003); a stopped worker's process is gone
  within 200 ms whether it computes, sleeps or waits on the network (FR-007).

## Phase 6: US4 - Several sources, whichever speaks first (P2)

- [X] T014 [US4] Golden case `tests/cases/workers/first.vo`, failing (story 4 scenarios 1 to 3 in
  deterministic form: workers that have all finished, so the earliest in the list is chosen; the
  index of each arrival; one ended and one failed worker each reported once, then skipped;
  `err(ended)` when all are reported; `timeout` on workers that never emit); then operation
  receive-first in `src/workers.c` (R4) and `receive_first` in `std/workers.vo`. In
  `tests/workers.py --group exchange`: three workers emitting after 50, 150 and 250 ms arrive in
  that order, and a worker forwarding lines from a local TCP server delivers each line before the
  connection closes (scenario 4).
- [X] T015 [US4] The bounds (FR-011), with a case in `tests/workers.py --group exchange`: the
  program tells a worker 100,000 messages without receiving while the worker emits one for each;
  nothing is lost and nobody waits forever (the waiting-to-send loop of R4 in `src/workers.c`); a
  worker that emits without end while the program only tells gives `full`; a message over 16 MiB is
  `too_large` in both directions.

## Phase 7: US5 - Computing on several cores (P2)

- [X] T016 [US5] `tests/workers.py --group speed` passes on x86-64 outside CI: a round trip of a
  record of two ints under 50 microseconds; a start from a program holding 100 MiB under 10 ms;
  four workers summing ranges at least 2.5 times as fast as one process when the machine has four
  or more cores (SC-004). The figures are printed everywhere.

## Phase 8: US6 - Tests, recordings and traces (P2)

- [X] T017 [US6] Workers under `vo test`, cases in `tests/cases/test/`: a test that starts two workers and leaves them running
  passes and leaves no process; a test with an undelivered failure fails, naming it
  (`src/testrun.c` calls the shutdown after each test).
- [X] T018 [US6] Recording and replay (R11): event kind `worker` in
  `src/replay.c`; in a recording run each operation of the program writes its outcome and the
  workers' output it wrote; in a replay no worker is started and each operation reads its event.
  A step case `tests/cases/workers/record_replay` (record a program whose two workers race with
  `receive_first`, replay it: same output and status). `tests/workers.py --group replay` passes:
  100 recordings each replay to their own output and exit status; a program that names its workers
  prints the same in 1,000 of 1,000 runs (100 and 100 under emulation) (SC-005).
- [X] T019 [US6] `--trace`: a worker's records go up its control pipe and are written under its own
  path, as tasks' are; a golden or step case in the trace area with one worker. `--max-memory` in a
  worker: a case where the worker exceeds it and the program receives `failed` with R0006.

## Phase 9: US7 - The example and documentation (P2)

- [X] T020 [US7] Doc comments with run examples on every exported item of `std/workers.vo`;
  `python3 tests/doc_conformance.py` passes (if its sample values cannot make a `Link` or a
  `Worker`, teach it, as for `Option[User]`); accept the module listing in `tests/cases/doc/all.out`
  and `all_json.out` after reading the diff.
- [X] T021 [P] [US7] `examples/job-queue/` as research.md R14 (`main.vo`, a module with the job
  functions and their tests, `README.md`, `vo.effects`): three workers, each job under a deadline,
  results handled as they arrive; `vo test` runs without the network; a row in `examples/README.md`;
  `python3 tests/build_examples.py` passes.
- [X] T022 [P] [US7] `docs/reference.md`: one `std.workers` row and the `std.bits` row shortened;
  `python3 tests/ref_examples.py` under 15,000. A guide page `site/docs/guide/workers.md` in the
  site's navigation: a background worker, a deadline for one call, waiting on several, a pool for
  computing, when `concurrent_map` is the better tool, what a worker's copy means (changes do not
  come back; a stopped worker leaves files as they were at that moment, SQLite rolls back), when a
  worker's output appears (when the program receives from it, at its stop or at the end; a worker
  that must log as it goes appends to a file), what replay does with workers, the limits, and that workers are started by the program's
  own process. `python3 tests/site_examples.py` and `make site-check` pass. `CHANGELOG.md`,
  `docs/roadmap.md` (row, section "047: workers", the "Events" row), `docs/review-2026-11/README.md`
  (row "Concurrency (language 8)").

## Phase 10: Polish

- [X] T023 Three architectures: `make ARCH=aarch64` and `make ARCH=riscv64`; with `VO_BIN` set, run
  `tests/run.sh tests/cases/workers`, `tests/run.sh tests/cases/tasks` and
  `python3 tests/workers.py` on each.
- [X] T024 `python3 tools/progress.py run test` once; read `build/progress/test.log`; fix and rerun
  only on failure.
- [X] T025 Write `specs/047-workers/results.md` (SC-001 to SC-006 with figures, deviations, the two
  `ponytail:` limits, what was not verified); set the spec's status to Implemented.

## Dependencies

- T001, T002 → T003, T004 → T005 → everything.
- US1 (T006–T008) → US3 (T009–T011) → US2 (T012, T013). US4 (T014, T015) needs US3. US5 (T016)
  needs US1. US6 (T017–T019) needs US2 and US4 (every operation exists before it is recorded).
  US7 needs all.
- T021 and T022 touch different files and can run in parallel after T020.

## Implementation strategy

MVP is the three P1 stories: exchange, failure as a value with nothing left behind, and a deadline.
The leak test (T011) is the gate for going on: a facility that leaves processes behind is worse than
none. Stop after any phase with `make quick AREA=workers` green and the task and serve cases
unchanged.

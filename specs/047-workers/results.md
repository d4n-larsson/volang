# Results: Workers (spec 047)

Status: implemented. `make test` passed (one run). The gate was not run.

One new runtime file (`src/workers.c`, about 700 lines), one internal builtin (`__worker`), one
recording event kind (`worker`), one trace record kind (`worker`), and `std/workers.vo` in vo. No
syntax and no diagnostic code was added.

## After the analysis

The ten findings of `/speckit-analyze` were applied before the code was written. The one that changed
the design: a worker's output does not go straight to the terminal (which made the order of output
depend on timing, against constitution III). It travels up the worker's pipe in order with its
messages, and the program writes it just before the next message it receives from that worker, or
when the worker is stopped or the program ends. `tell` answers from what the program has received,
not from whether the worker happens to be gone.

## SC-001: exchange

`python3 tests/workers.py --group exchange`: 64 workers, 10,000 messages each way each (640,000 round
trips), none lost, repeated or out of order. Under qemu on ARM64 and RISC-V: 64 workers, 200 messages
each. Also: three workers with delays of 50, 150 and 250 ms arrive in that order through
`receive_first`; a worker forwards three lines from a connection that the server never closes;
100,000 messages told before any is received all come back; a worker that emits without end while
the program only tells gives `full`; a message over 16 MiB is `too_large` in both directions.

## SC-002: nothing left behind

`--group leaks`: a program with four workers (computing, sleeping, waiting on a connection that never
answers, and one running `concurrent_map` tasks), ended by return, `exit`, a runtime error,
`--timeout`, SIGTERM and SIGKILL, 200 runs each (20 each under qemu): the exit status is the expected
one every time, and one second later no process of any run exists.

A deliberate fault was put in to see that the check notices: without the request to die with the
parent, 120 processes were left after 20 runs ended by SIGTERM, and the same for SIGKILL. Removed.

## SC-003: deadlines

`--group deadline`: 100 of 100 calls that never return, given 100 ms, gave `timeout`; the slowest
took 102 ms (bound 300). A stopped worker was gone in 0 to 1 ms whether computing, sleeping or
waiting on the network (bound 200). While the program then sleeps, no other process of it exists.

## SC-004: cost

x86-64, in the `make test` run: a round trip of a record of two ints 9.4 microseconds (bound 50); a
start from a program holding 100 MiB 2 ms (bound 10); a fixed computation 962 ms in one process and
312 ms with four workers, 3.08 times (bound 2.5; 12 cores). Emulated, not bounded: round trips of 22
(ARM64) and 44 (RISC-V) microseconds.

## SC-005: determinism and replay

- 100 recordings of a program whose five workers race (time limits of 2 ms, `receive_first`, a stop,
  a failure nobody receives) each replay to the stdout, stderr and exit status (70) of their own run.
  The runs printed 99 or 100 different outputs, so the race is real. A replay starts no process.
- A program whose receives each name their worker, with workers that print, sleep random times and
  answer: the same stdout, stderr and status in 1,000 of 1,000 runs.
- Golden: `workers/record_replay` (the data file is deleted before the replay), `trace/worker`.

## SC-006: the example, the budget, the suite

- `examples/job-queue`: three workers, seven jobs with 300 ms each, answers as they arrive, the job
  that is too large given up with `stop_worker`; 3 tests of pure functions; a recording that replays
  from the built executable too.
- Reference: 15,000 tokens of 15,000. To fit, the `std.bits` row lost five names.
- `tests/doc_conformance.py` (319 examples), `tests/catalog.py`, `tests/site_examples.py` and
  `make site-check` pass.
- ARM64 and RISC-V under qemu: `tests/cases/workers` (16), `tests/cases/tasks` (34), the two step
  cases and `tests/workers.py` (26 checks each) pass.

## Golden cases

`tests/cases/workers/`: `exchange`, `failed`, `unasked` (status 70), `too_many`, `nested`,
`nested_task`, `deadline`, `first`, `bad_type` and `bad_order` (R0015), `bad_wait` (R0007), `effects`
(E0501), `grant` (R0010 inside a worker), `seed`, `stdin`, `memory` (R0006 inside a worker),
`record_replay`; `tests/cases/test/workers_left`; `tests/cases/trace/worker`.

## Existing files changed outside the plan's list

- `tests/cases/tasks/explain_r0015.out`: the R0015 entry of `docs/errors.md` now also covers a
  worker's messages.
- `src/rt_effects.c`: `sleep` writes out pending output before it waits. Before, a program that printed
  and then slept showed nothing until it ended, which a program with a periodic worker would hit at
  once. No golden output changed.
- `src/check_expr.c`: `__worker(1, body)` counts as a call of `body`, so the function that starts a
  worker must declare the worker's effects (and `start_worker` gets no W0501).
- `tests/doc_conformance.py`: type parameters that are words (`In`, `Out`), and generic records whose
  parameters no field mentions.
- `src/vm.c`: the `worker` trace record.

## Deviations

- **A failure is given once.** After a `receive` gave `failed` or `ended`, the worker's slot is free
  and every later operation on the handle gives `ended`. This is what makes the count of running
  workers, and so `too_many`, independent of timing.
- **`full` stops the worker**: part of the message may already be in its pipe.
- **A worker does not enforce `--timeout` itself**: the program does, and ends its workers. Otherwise
  each waiting worker reported its own R0009 beside the program's.
- **A collection with its own order is refused where it is sent** (`tell`, `emit`), not at
  `start_worker`: it is a property of the value, not of the type. A function in a message type is
  refused at `start_worker`.
- **An undelivered failure under `vo test`** is raised as the worker's own runtime error (its code
  and position). A worker that ended with `exit(3)` has no code: the test fails with R0007 and the
  message "A worker ended with exit status 3, and the test did not receive its failure."
- **At the program's end a failure without a runtime error** (an exit status) is one plain line on
  standard error, also under `--json`.
- **A runtime error in a `concurrent_map` task inside a worker** is printed to the worker's standard
  error, and the program gets `failed("exit status 70")`, not the error's text.
- **`with_deadline` gives `err(ended)`** when the function's result is over 16 MiB.
- **Doc examples of `tell`, `receive`, `receive_first` and `stop_worker`** use a handle of a worker
  that has ended (`new Worker { id: 0 }`), as `std.tcp`'s use a closed connection: an example is one
  expression, and getting a handle out of `start_worker`'s result takes a `match`.
- **Properties**: workers a property's cases leave behind are ended after the property, not after
  each case, and their undelivered failures are not reported.
- T017's cases are one file, `tests/cases/test/workers_left`, with five tests.
- SC-001 under emulation ran 200 messages per worker, not 1,000.

## The two `ponytail:` limits

- Workers are started by the program's own process only (R7): no `with_deadline` in a request
  handler or a task.
- A replay does not run workers (R11): it cannot step into a worker's code, and `vo replay --trace`
  shows the program's calls only.

## Not verified

- The gate.
- Real ARM64 or RISC-V hardware (qemu only).
- The stress and instrumented builds with workers (they are part of the gate).
- A machine with fewer than four cores (the pool check is skipped there), and CI, where the speed
  bounds are not checked.
- A worker's behaviour when the machine cannot create a process or a pipe (T0003 by reading only).

## Noticed, not changed

- The reference has no tokens left.
- `Link[ordered_set[int], int]` does not parse: a built-in collection type as a type argument of a
  user type gives E0110. The case wraps the set in a record.
- A scope such as `file.read("data.txt")` names a directory; a file of that name cannot be read under
  it.
- `python3 tests/site_examples.py --coverage` reports `basics.md` mentioning W0102 without a link.
- `examples/README.md` still describes notes-api as built on `std.http.serve`.

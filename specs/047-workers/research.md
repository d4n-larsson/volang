# Research: Workers (spec 047)

## R1: A worker is a fork with two pipes

**Decision**: `start_worker` forks, as `concurrent_map` and `serve_workers` do. The child runs the
worker's function and never returns into the program's code. Pipes: the worker's inbox (program to
worker) and its control pipe (worker to program: messages, its output, its end, trace records;
`task.c`'s framing, a kind byte and a length). The program keeps a table of 64 slots: process id, pipe
ends, the concrete message types, what has arrived and not been read, and how the worker ended.

Measured on the development machine: a fork of a process holding 100 MiB of touched memory takes
3 ms (pages are shared until written), and a round trip of 25 bytes over two bare pipes takes 12
microseconds. SC-004's bound for a round trip is therefore 50 microseconds, not 20 (spec amended).

**Alternatives considered**: threads (the collector and every runtime global assume one thread);
`socketpair` (nothing gained over pipes); starting a fresh `vo` process (the worker would not have
the program's values).

## R2: The surface: a library module in vo over one internal builtin

**Decision**: `std/workers.vo` (contracts/workers.md) over `__worker(OP, ...)`, typed loosely like
`__coll` (the wrappers declare the public types). The builtin finds the message types where it needs
them: the wrappers are generic in `In` and `Out`, and the running frame's type arguments are the
concrete types (`rt_concrete`), as for map keys.

Names avoid `send` (taken by `std.http` and `std.tcp`): `start_worker`, `tell`, `receive`,
`receive_first`, `stop_worker` for the program; `next_message`, `emit` for the worker;
`with_deadline`. A wait is written out: `Wait.forever` or `Wait.at_most(ms)`; `at_most(0)` asks
without waiting, a negative limit is R0007.

A handle is a record holding a slot number and a generation, so a handle of a worker that ended
never names a later one. A handle built by hand or used with other message types is caught: each
slot keeps its concrete `In` and `Out`, and every operation compares them with its own (R0007).

## R3: Messages

**Decision**: `task.c`'s typed transfer form (`enc_value`, `dec_value`), one framed message at a time,
at most 16 MiB encoded (larger: the error value `too_large`). Types that cannot cross (a function, a
collection with its own order) are refused when the worker is started, by walking the concrete `In`
and `Out`: runtime error R0015 naming the type. `concurrent_map` reports a function in its result at
check time (E0314); here the types are type arguments of a library function, which the checker does
not look into, so the refusal is at the start. Spec amended (FR-004).

The reader (`dec_value`) reads bytes written by the same toolchain under the same types, as today;
the type comparison of R2 is what keeps a mismatched read from happening.

## R4: Waiting, and never waiting on each other

**Decision**: every operation of the program is a `poll` loop over the control pipes it cares about,
bounded by its own limit and by `--timeout`'s deadline (as `task_map`'s loop).

- `receive`: what has arrived for that worker first (kept as bytes in the slot), else poll.
- `receive_first`: the earliest worker in the list that has something; else poll all of them. A
  worker whose end or failure has been reported once is skipped from then on.
- `tell`: gives `ended` only when a receive (or a stop) has delivered that worker's end (the slot is
  then free, and a failure was given once by that receive);
  otherwise it accepts the message, which is dropped when the worker is gone, so its answer does not
  depend on timing. It writes to the inbox without blocking; while the pipe is full it polls for room and, at the
  same time, takes in what that worker has sent. More than 64 MiB taken in while waiting to send is
  the error value `full`; the worker is then stopped, since part of the message may be in the pipe. So the program never waits to send while the worker waits for the program
  to read.
- The worker's side is plain: `emit` writes and waits when the pipe (1 MiB) is full; `next_message`
  polls its inbox with its limit.

## R5: A worker's end

**Decision**: in the worker, the three ways out send a last framed message and leave with
`exit_group`: its function returned (`ended`); a runtime error reached the worker's base recovery
point (`failed`, with the text the program would have printed for it: code, message and trace), by a
hook beside the task hook in `interp.c`; `exit(n)` through `vo_exit` (`ended` for 0, `failed` with
"exit status n" otherwise). A worker killed from outside sends nothing: the program sees the pipe
close and reports `failed` with the signal.

The program delivers `failed` where it next receives from that worker, after the messages that
arrived before it. At the program's end (`vo_exit`, before the recording is completed), every worker still
running is killed and reaped, their pending output is written, and each failure never delivered is
written to standard error; if the program's own status was 0 it becomes 70, the status of a runtime
error.

## R6: Stopping, and the program's own death

**Decision**: `stop_worker` is `SIGKILL` and `wait`, as `group_stop` does for tasks: the process is
gone when the call returns, whatever it was doing. Every worker also asks the kernel to kill it when
its parent dies (`sys_die_with_parent`, as tasks do), which covers a program ended by a signal it
cannot catch.

Children that the program forks later (`concurrent_map` tasks, `serve_workers` workers) close the
worker pipes they inherit and treat every handle as ended, so a worker's end is seen as soon as the
worker goes, and no child talks to a worker it does not own.

## R7: Where workers may be started

**Decision**: only by the program's own process: `start_worker` inside a worker, a task or a
`serve_workers` worker is R0007 ("workers are started by the program itself"). `concurrent_map`
inside a worker runs as in a program.

**Rationale**: each of those processes would need its own table, shutdown and recording. The cost of
the limit: no `with_deadline` inside a request handler (`std.http.send` has its own time limit).
`ponytail:` upgrade: a table per process, as `task_top_depth` did for tasks in servers.

## R8: Time

**Decision**: limits are real time from the monotonic clock, as the time limits of `std.tcp`:
`--clock-ms` does not affect them. In a replay nothing waits (R11).

## R9: `with_deadline` and `receive_first` are written in vo

`with_deadline(ms, f)` starts a worker that emits `f()`, receives with `Wait.at_most(ms)`, and stops
the worker. `receive_first` is one builtin operation (it must poll several pipes at once); its
result type, `Arrival[Out]`, is a record of the index and a `result[Out, WorkerError]`.

## R10: Output

**Decision**: a worker's standard output and error are frames on its control pipe, in order with its
messages (the worker's output buffer is sent as a frame wherever it would be flushed, and before
each `emit`). The program writes a frame of output when it reads past it: just before it hands over
the next message, end or failure of that worker; what has arrived when a worker is stopped or the
program ends is written then, in the order of the workers' slots. Output counts toward what is in
flight and what the program takes in.

**Rationale**: constitution III: the same source, arguments and input give the same bytes. With
output written by each process as it goes (the first plan), the order would depend on timing with
no named builtin involved. The cost: a background worker's lines appear only when the program next
receives from it, or at the end; a worker that must log as it goes appends to a file. One path for
output instead of two, and recordings get it in line with the messages.

A worker that reads standard input stops with a runtime error (R0007, "a worker cannot read standard
input"); the checker does not look into a worker's function.

## R11: Recording and replay

**Decision**: a recording holds what the program got from its workers: for each worker operation, an
event with its outcome (for a receive: the message bytes, or which error; for `receive_first`: which
worker) and the workers' output that the operation wrote. A replay does not
start workers: `start_worker`, `tell`, `stop_worker` and the receives read the events and write the
recorded output, so the replayed program prints and ends as the recorded run did. Recording is off
inside a worker: what a worker reads from the network or files is not in the recording.

`ponytail:` ceiling: a replay cannot step into a worker's code, and `vo replay --trace` shows the
program's events only. Upgrade: record each worker's inputs under its number and run workers again
in replay, the program waiting for the recorded worker at each receive.

**Rationale**: the alternative (run workers again from their own recorded inputs, and force the
recorded delivery order) is exact too, and several times the code, with more ways to wait forever.
FR-015 as amended allows this.

## R12: Trace, random numbers, memory

A worker's trace records go up its control pipe (`TM_TRACE`, as tasks') and are written under the
worker's path; its random sequence is seeded from `--seed` and its number among the workers started
(`rt_task_seed`); `--max-memory` applies to it separately, as to a task.

## R13: Tests

- Golden cases in `tests/cases/workers/` for what is deterministic: exchange in order, time limit,
  ended, failed with its text, stop, the limit of 64, a refused message type (R0015), a start inside
  a task (R0007), a failure nobody asked about (standard error, status 70), `with_deadline`,
  `receive_first` with its defined choice, a replayed recording.
- `tests/workers.py` (in `make test`): `exchange` (SC-001: 64 workers, 10,000 messages each way),
  `leaks` (SC-002: each way of ending, 200 runs, no process left; it finds them by a marker in the
  workers' environment or by the process group), `deadline` (SC-003), `speed` (SC-004; skipped on CI
  and under emulation, and the four-core part on machines with fewer cores), `replay` (SC-005).
- `vo test`: a case whose test leaves workers running, and one with an undelivered failure.
- Also golden: a worker under the program's grant (a denied effect is `failed` with its code), two
  workers' random numbers under `--seed`, a worker reading standard input, output order.
- Area `workers` in `tools/quick.sh`.

## R14: The example and the reference

A new example, `examples/job-queue/`: three workers take jobs from the program, each job under a
deadline, results handled as they arrive with `receive_first`; its tests use pure job functions and
need no network. (`url-check` runs its checks inside `concurrent_map` tasks, where workers cannot be
started.)

The reference row for `std.workers` is paid for by shortening the `std.bits` row, which lists eleven
function names.

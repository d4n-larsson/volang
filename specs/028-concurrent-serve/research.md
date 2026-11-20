# Research: Concurrent Request Handling (spec 028)

Starting point (read from the code): `std.http.serve` is vo code (`serve_loop` in `std/http.vo`) over four
primitives in `src/serve.c`: `__serve_listen` (socket, SIGINT/SIGTERM through a signalfd),
`__serve_next` (up to 64 connections read at once, each request read whole within 64 KiB head, 16 MiB
body, 10 s; returns the next complete request in arrival order, or "" on a stop signal), `__serve_call`
(the handler under a recovery point with the per-call `--timeout` deadline; any runtime error, a timeout
included, becomes `err` and a 500) and `__serve_reply`. `concurrent_map` (`src/task.c`, spec 016) forks
plain child processes that report output, results, traces and recording events to the parent over pipes
(messages `TM_*`); replay runs tasks one at a time.

## R1 Architecture: the parent reads and writes connections, workers only run handlers

**Decision**: `serve_workers` keeps today's connection handling in the parent process, unchanged: the
listener, the 64 connections, reading each request whole within its limits, slow clients, writing
responses, the stop signals. W worker processes, forked from the parent when the server starts, each run
the setup once and then handle one request at a time: the parent sends a complete request (its bytes)
to a free worker over a pipe; the worker runs the handler and sends back the response text, the
request's stdout and stderr output, and its recording events. The parent writes the response to the
connection.

**Rationale**: every limit and the shutdown logic stay in one place that is already tested; a slow client
holds a connection, never a worker (spec edge case); no file descriptors are passed between processes;
workers are ordinary vo runs of one handler call at a time, like tasks; replay can run the same request
stream in one process (R9).

**Alternatives**: each worker accepting on a shared listening socket (`SO_REUSEPORT` or a shared
`accept`): every worker would need the connection machinery and its own shutdown handling, slow clients
would hold workers, and recordings would need merging across independent processes; passing accepted
sockets with `SCM_RIGHTS`: more system calls and the same slow-client problem; threads: shared memory,
ruled out by the spec.

## R2 API: `serve_workers` and `serve_bytes_workers`

**Decision**:

```text
serve_workers[S, E: effects, F: effects](address: str, workers: int, setup: fn() -> result[S] uses F,
    handle: fn(S, Request) -> Response uses E) -> result[void] uses serve, E, F
serve_bytes_workers[...]  the same with BytesRequest / BytesResponse
```

`serve` and `serve_bytes` stay as they are (one request at a time). A program opts in by calling the
`_workers` form; the worker count is declared in the call (1 to 64; another number is `err` from
`serve_workers`, the server does not start). The setup's result `S` stays in its worker: it is never
copied between processes, so it may hold a database handle or any other per-process value.

**Rationale**: one form per construct (constitution I): the existing functions keep their meaning, the
new ones say what they do in their name; the setup result passed as the handler's first argument makes
"each worker has its own connection" visible in the code (clarification Q2). Two effect parameters,
because a setup that opens a file database and a handler that only uses it have different effects, and
one parameter would have to unify both function values' effects at the call (spec 026 inference binds
an effect parameter from the first argument that mentions it).

**Alternatives**: an optional worker argument on `serve` (vo has no optional arguments); a global server
configuration (hidden state).

## R3 The worker count: declared, `--workers N` overrides

**Decision**: `vo run --workers N` (1 to 64, else T0001) replaces the `workers` argument of every
`serve_workers` call in that run. On a program that calls only `serve`/`serve_bytes`, it changes nothing,
and the first `serve` call prints `note: --workers has no effect: this program serves one request at a
time (serve_workers runs several)` to stderr. `vo replay` does not take `--workers` (T0001, as `--seed`):
replay runs one request at a time (R9).

## R4 Dispatch

**Decision**: complete requests wait in the parent in arrival order (bounded by the 64 connections); a
free worker takes the oldest. With W workers busy, a new request waits for the first one to finish. A
request is written to the worker as one message (length-prefixed bytes); the response comes back as one
message, with the request's output and events (R7, R9) before it.

## R5 Failures

**Decision**:
- A runtime error in the handler (a timeout and memory exhaustion included, as today): the worker's
  recovery point catches it (the existing `rt_serve_call`), the response is 500, the report goes to the
  worker's stderr (forwarded whole, R7), and the worker goes on.
- A runtime error in the setup, or `err` from it: the worker reports it and exits; when this happens
  while the server starts, the server stops starting (the other workers are stopped) and `serve_workers`
  returns that `err` (a runtime error ends the program as usual); later (a replacement worker), the server
  shuts down as on SIGTERM and returns the `err`.
- A worker that ends without a response (killed from outside, a crash the recovery point cannot catch):
  the parent sees its pipe close, answers the request 500, reports `note: worker N ended unexpectedly; it
  was replaced` on stderr, and forks a replacement (which runs the setup).
- `--max-memory` applies to each worker separately; `--timeout` to each handler call (the setup has the
  same limit).

## R6 Shutdown

**Decision**: on SIGINT or SIGTERM the parent closes the listener, stops reading new requests, lets
every request already read whole be answered (dispatched or waiting), then closes the workers' request
pipes; each worker returns from its loop and exits; the parent reaps them and `serve_workers` returns
`ok()`. A second SIGINT or SIGTERM during shutdown kills the workers, closes every connection and returns
`ok()`. The per-call `--timeout` bounds how long shutdown waits for any one request.

## R7 Output per request

**Decision**: a worker's stdout and stderr are captured per request (the worker's output buffers are
flushed into messages at the end of each handler call, as a task's output is drained) and the parent
writes each request's output whole, when its response is sent. Output of the setup is written when the
worker is ready. Requests' outputs appear in completion order (the program is concurrent); within a
request, in order.

## R8 Randomness and the clock

**Decision**: in a worker, the random sequence of each handler call is derived from the run's seed and
the request's number (its position in arrival order), not from the worker's history: the same request
gets the same random values whichever worker runs it, and replay (R9) reproduces them. Clock readings are
recorded per request (as today).

## R9 Recording and replay

**Decision**: the parent writes the recording. Today a server's recording is the `listen` event, then for
each request a `request` event, the handler's events (file reads, database results, HTTP responses, clock
readings) and a `response` event. With workers, the events of each request come from its worker inside
that request's message (R4), and the parent writes each request's block, `request` to `response`,
contiguously in arrival order (holding a finished block until the blocks before it are written, as task
output is ordered). Each worker's setup events are written once, as a `setup` block (`{"kind":"setup",
"worker":N}` ... `{"kind":"setup_end"}`) when the worker is ready. Replay runs in one process, one request
at a time: it runs the setup once on the first `setup` block's events, skips the other workers' setup
blocks, and gives each request its own block; responses are compared as today (a changed response is a
note). A recording made with workers replays with the same responses regardless of the order in which
the workers ran.

**Rationale**: handlers share no memory, so each request's outcome depends only on its own inputs, which
are in its block; database results, file reads and HTTP responses are recorded inputs, not recomputed.

## R10 Tasks inside a handler

**Decision**: `concurrent_map` inside a handler in a worker works as in a program (up to its limit),
since a handler that fetches from several services is a main use. ponytail: 64 workers × 64 tasks is
4,096 processes at worst; a total cap if that ever matters.

## R11 State between requests and SQLite

**Decision**: nothing in memory is shared between workers; a worker keeps its setup result for all its
requests. State belongs in a database: a SQLite file opened in the setup, one connection per worker
(spec 026 binds a connection to its process), transactions for read-modify-write; SQLite's file locks
(open-file-description locks, spec 026) serialize writers across workers, and the busy handler waits up to
5 s within the request's deadline. The `notes-api` example moves from `data/notes.json` (read, change,
write back: lost updates under concurrency) to SQLite.

## R12 Verification

- Golden step cases (`tests/cases/serve_workers_steps/`): start a server with `start vo run ...`,
  `request`, `request-many`, `signal TERM`, `no-children`, as spec 011/016 step cases do.
- `tests/serve_workers.py`: the latency check (SC-001: fast requests ≤ 50 ms while slow ones run), the
  isolation check (SC-003), the shutdown check (SC-004), the counter (SC-005: 1,000 increments through 4
  workers) and the notes-api load test.
- `tests/serve_workers_speed.py` (`make gate`, machines with at least 4 cores): throughput with 1 and 4
  workers (SC-002 ≥ 3×).
- Replay: a recorded 200-request session replayed 20 times (SC-006), in a step case and in the script.
- Every existing server golden case unchanged (SC-007).

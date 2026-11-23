# Feature Specification: Workers (processes that run beside the program and exchange messages)

**Feature Branch**: `047-workers`

**Created**: 2026-11-22

**Status**: Implemented

**Input**: User description: "continue with the next thing on the review" (docs/review-2026-11/language.md
item 8, "Concurrency beyond `concurrent_map`": "`concurrent_map` ... covers 'do these N things at
once'. It does not cover: something that runs for the life of the program next to the main work (a
queue consumer, a periodic job, a server that also polls); events arriving over time (server-sent
events, WebSockets, a subscription); parallel computation (fork-per-task copies the heap and sends
results back as text, so it only pays for I/O); cancellation and deadlines for one call, other than
`--timeout` for the whole run. A direction that keeps what vo has: workers that share nothing and
exchange messages. ... Generalize it: `worker(setup, handle)` returns a typed handle with `send` and
`receive`, messages limited to the types `concurrent_map` results allow, delivery order recorded so
replay stays exact. That is Erlang's model, not Go's, and it fits effects: a worker has a grant like a
test has. Threads with shared memory would cost the deterministic collector and are not worth it.")

Background: a vo program does one thing at a time, with two exceptions. `concurrent_map` runs a
function on each element of a list at once and waits for all of them. `serve_workers` answers requests
in several processes. Both start work, wait for it, and return; neither lets the program go on while
something else runs. So a program cannot consume a queue while it serves, cannot listen to two sources
and act on whichever speaks first, cannot give one call a time limit, and cannot stop work it no
longer wants. `--timeout` ends the whole run.

Both existing mechanisms already run work in separate processes that share nothing, copy what they are
given, and send results back in a fixed form; recordings already hold what each process received. A
worker is that mechanism with the waiting taken out: the program starts it, goes on, and exchanges
messages with it.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Work that runs beside the program (Priority: P1)

A developer starts a worker that runs a function of its own (a loop that takes jobs, a periodic
check) and goes on with the main work. The program sends the worker messages and receives messages
from it, each of a declared type.

**Why this priority**: it is the piece everything else in this feature is built from, and the first
gap the review names.

**Independent Test**: a program starts a worker that doubles the numbers it is sent; it sends 1,000
numbers and receives 1,000 doubles in the order sent, then the worker ends when told to.

**Acceptance Scenarios**:

1. **Given** a function that takes its link to the program, **When** a worker is started with it,
   **Then** the call returns at once with a handle, and the function runs in a process of its own
   with copies of everything it was given or captured: nothing it changes is seen by the program, and
   the reverse.
2. **Given** a worker, **When** the program sends it messages, **Then** the worker receives them in
   the order sent, each exactly once; and the same from the worker to the program.
3. **Given** a worker that has sent nothing yet, **When** the program asks for its next message with
   a time limit, **Then** it waits until a message arrives or the limit passes, and says which
   happened; with no limit it waits for the message.
4. **Given** a worker whose function returns, **When** the program asks for its next message,
   **Then** it gets the messages still on their way, and after them the answer "ended".
5. **Given** a message type that cannot cross between processes (it holds a function, or a
   collection with its own order), **When** a worker with that type is started, **Then** the program
   stops with a runtime error naming the type, before any process is created.
6. **Given** a worker function with effects, **When** the starting function is checked, **Then** it
   must declare those effects, with scopes, as for any function it calls; at run time the worker has
   the program's grant and no more.

---

### User Story 2 - A time limit for one call, and stopping work (Priority: P1)

A developer gives one call a deadline: if it has not answered in time, the program gets an error
value and goes on, and the work is stopped. A developer can also stop a worker that is no longer
needed.

**Why this priority**: today a slow call can only be ended by ending the program.

**Independent Test**: a call that would run for a minute, given 100 milliseconds, returns a timeout
within 300 milliseconds, and no process of it remains.

**Acceptance Scenarios**:

1. **Given** a function and a time limit, **When** it is run with the limit and finishes in time,
   **Then** its result is returned.
2. **Given** the same, **When** the limit passes first, **Then** the caller gets a timeout as an
   error value, the function's process is ended, and what it changed outside the program (a file half
   written) stays as it was at that moment: the documentation says so.
3. **Given** a worker, **When** the program stops it, **Then** its process ends within the time of
   FR-007 whatever it is doing (computing, sleeping, waiting for the network), later sends to it and
   receives from it say "ended", and stopping it again does nothing.
4. **Given** a function that fails with a runtime error under a time limit, **When** it fails,
   **Then** the caller gets the failure as an error value (see story 3).

---

### User Story 3 - Failures are values, and nothing is left behind (Priority: P1)

A worker that stops with a runtime error does not take the program down: the program learns of it
where it talks to the worker. And however the program ends, no worker survives it.

**Why this priority**: isolation is what makes separate processes worth having, and a leaked process
is the classic defect of every such facility.

**Independent Test**: the program is ended in each possible way while workers are computing, sleeping
and waiting; afterwards no process started by it exists.

**Acceptance Scenarios**:

1. **Given** a worker that stops with a runtime error, **When** the program next receives from it
   (after the messages it sent before failing), **Then** it gets "failed" with the error's code and
   message as an error value, once, and the program goes on. A send to it is accepted and dropped until
   then, and says "ended" afterwards, so that what a send answers never depends on timing.
2. **Given** a worker's failure that the program never asked about, **When** the program ends,
   **Then** the failure is reported on standard error as a runtime error is, and the program's exit
   status is not 0.
3. **Given** workers still running, **When** the program ends (its main function returns, `exit` is
   called, a runtime error stops it, `--timeout` passes, or it is ended by a signal), **Then** every
   worker is ended and the program does not finish until they are gone.
4. **Given** a worker that calls `exit` or is killed from outside, **When** the program asks,
   **Then** it gets "ended" or "failed" saying so; the program is not ended by it.
5. **Given** more workers than the limit allows at one time (64), **When** another is started,
   **Then** the start returns an error value; stopping or finishing workers makes room.
6. **Given** a program that sends to a worker faster than the worker reads, while the worker sends
   back faster than the program reads, **When** run, **Then** neither side waits for the other
   forever: while the program waits to send, it takes in what the worker has sent, up to a documented
   limit, beyond which its send returns an error value. A side that sends to a reader that is merely
   slow waits for it.

---

### User Story 4 - Several sources, whichever speaks first (Priority: P2)

A developer waits on several workers at once (one reading a stream of events from a server, one
ticking every minute) and handles whichever has a message first.

**Why this priority**: it is what "events arriving over time" needs; without it a program polls each
source with a time limit.

**Independent Test**: three workers send at different, fixed delays; the program receives the
messages in order of those delays, and a recording of the run replays in the same order.

**Acceptance Scenarios**:

1. **Given** a list of workers of the same message type, **When** the program asks for the first
   message from any of them, with or without a time limit, **Then** it gets the message and which
   worker sent it.
2. **Given** several workers with messages already waiting, **When** asked, **Then** the choice is
   defined (the earliest in the list) so that a program whose workers have all finished behaves the
   same on every run.
3. **Given** a worker in the list that has ended or failed, **When** asked and nothing else is
   waiting, **Then** the answer says which worker ended or failed, once; it is then ignored in that
   list.
4. **Given** a worker that reads lines from a long-lived connection and sends each on, **When** the
   server sends an event, **Then** the program receives it without waiting for the connection to
   close.

---

### User Story 5 - Computing on several cores (Priority: P2)

A developer splits a computation over workers that stay alive, sending each small descriptions of
work and getting small results back, so that the copying which makes `concurrent_map` pay only for
waiting does not dominate.

**Why this priority**: named by the review; it needs no feature beyond stories 1 to 3, only that
messages are cheap enough.

**Independent Test**: four workers summing ranges finish a fixed job at least 2.5 times faster than
one process on a machine with four or more cores.

**Acceptance Scenarios**:

1. **Given** workers started once and sent many pieces of work, **When** timed, **Then** the cost of
   a message is small against a millisecond of work (see SC-004).
2. **Given** a worker started when the program holds much data, **When** started, **Then** it has its
   own copy without the start taking time in proportion to the data.

---

### User Story 6 - Tests, recordings and traces (Priority: P2)

Programs with workers can be tested, recorded and replayed like any other.

**Why this priority**: determinism and replay are what vo promises; a concurrency feature that
escapes them would undo the promise for every program that uses it.

**Independent Test**: a program whose workers race is recorded 100 times; each recording replays to
exactly the output and exit status of its own run.

**Acceptance Scenarios**:

1. **Given** a program whose every receive names one worker and has no time limit, **When** run many
   times, **Then** its output and its results are the same every time, including what its workers
   print.
2. **Given** `vo run --record`, **When** a program with workers runs, **Then** the recording holds
   what the program got from its workers (each message, end, failure and time limit, in the order
   delivered, and the workers' output); **When** replayed, **Then** the output and exit status are those of the recorded run,
   without the network, the files or the timing.
3. **Given** `vo test`, **When** a test starts workers, **Then** they are ended when the test ends,
   pass or fail, and a worker's failure that the test did not ask about fails the test.
4. **Given** a worker that prints, **When** the program receives from it, **Then** what the worker
   printed before a message is written by the program just before that message is handed over; what
   is left is written when the worker is stopped or the program ends. No line is cut by another
   process's.
5. **Given** `--trace`, **When** a program with workers runs, **Then** each worker's events are
   marked as its own, as tasks' events are.

---

### User Story 7 - Documented with an example, within budget (Priority: P2)

**Acceptance Scenarios**:

1. **Given** the guide, **When** read, **Then** it shows a background worker, a deadline for one
   call, waiting on several workers, a worker pool for computation, when to use `concurrent_map`
   instead, and what replay does with workers.
2. **Given** the examples, **When** listed, **Then** one of them uses workers for something its
   sequential form could not do (a deadline per URL in url-check, or polling beside serving).
3. **Given** the reference, **When** counted, **Then** it is under 15,000 tokens.

### Edge Cases

- A worker started and never spoken to: it runs, and is ended with the program.
- A message sent to a worker that has already ended: accepted and dropped until the program has
  received the end, "ended" after that; never a stop and never a wait.
- A very large message (near the documented limit) and one beyond it.
- A worker that sends a million messages nobody reads: the bound of story 3, scenario 6.
- A time limit of 0 (ask without waiting) and a negative one (an error in the program: the existing
  invalid-argument runtime error).
- A worker started inside a worker, inside a `concurrent_map` task, or inside a `serve_workers`
  worker: not allowed in this version (a runtime error that says so, never a hang); workers are
  started by the program's own process.
- `concurrent_map` inside a worker: as in a program.
- A worker reading standard input: a runtime error in the worker (the program gets "failed"); the
  checker does not look into a worker's function as it does into a task's.
- A handle sent in a message or otherwise carried into another process: there it reads as ended.
- A handle used after its worker ended: every operation says "ended".
- Random numbers, `--seed` and the clock in a worker: each worker has its own random sequence fixed
  by `--seed` and its position among the workers started, as tasks do.
- `--max-memory` applies to each worker separately, as for tasks; a worker that exceeds it fails.
- The program ended by SIGKILL (which it cannot catch): its workers must still end; the plan says
  how.
- Stopping a worker in the middle of writing a file or a database transaction: the documentation
  names what is and is not undone (SQLite rolls back; a file keeps what was written).
- Time limits under `vo test` and `--clock-ms`: defined by the plan, consistently with `sleep` and
  the time limits of `std.tcp`.

## Requirements *(mandatory)*

### Functional Requirements

**Starting and talking**

- **FR-001**: The standard library MUST provide a module for workers. Starting a worker with a
  function MUST return at once with a handle typed by the messages it takes and the messages it
  gives. No new syntax.
- **FR-002**: A worker MUST run in a process of its own on copies of what it is given or captures,
  with the same isolation as a `concurrent_map` task.
- **FR-003**: The program MUST be able to send a message to a worker and to receive the worker's
  next message, with a time limit in milliseconds or without one. The worker's function MUST be able
  to do the same toward the program. Messages between one sender and one receiver MUST arrive in
  order, each once.
- **FR-004**: Message types MUST be limited to what `concurrent_map` results allow. A worker whose
  message types hold a function or a collection with its own order MUST be refused when it is
  started, with a runtime error that names the type.
- **FR-005**: A worker's effects MUST be declared by the function that starts it; the worker MUST
  run under the program's grant.
- **FR-006**: The answers of a receive MUST distinguish, as a typed error: no message within the
  limit; the worker ended; the worker failed (with the runtime error's code and message). A send
  MUST report a worker's end or failure only after a receive has; before that it accepts the message.

**Stopping, deadlines, failure**

- **FR-007**: The program MUST be able to stop a worker. A stopped worker's process MUST be gone
  within 200 milliseconds on an idle machine, whatever it was doing.
- **FR-008**: The module MUST provide running one function call under a time limit: its result, or a
  timeout or failure as an error value, with the call's process ended on timeout.
- **FR-009**: A worker's runtime error MUST NOT end the program. It MUST be delivered as "failed"
  where the program next talks to the worker; if never delivered, it MUST be reported when the
  program ends, with a non-zero exit status.
- **FR-010**: When the program ends in any way, every worker it started MUST be ended, including
  when the program itself is killed.
- **FR-011**: Messages not yet read MUST be held, up to a documented amount per worker and
  direction; a sender whose reader is slow waits. While the program waits to send, it MUST take in
  the worker's messages, up to a documented bound, beyond which the send returns an error value, so
  that no exchange leaves both sides waiting for each other.
- **FR-012**: At most 64 workers may run at a time; one more is an error value from the start.

**Several workers**

- **FR-013**: The program MUST be able to wait for the first message from any of a list of workers
  of one type, with or without a time limit, and learn which worker it came from. When several have
  messages waiting, the earliest in the list MUST be chosen.

**Determinism, recording, tests**

- **FR-014**: A program whose receives each name one worker and have no time limit, and which
  neither stops a worker nor ends while one runs, MUST give the same output and results on every run
  and architecture. Timing enters only through named operations: a time limit, the first of several,
  a stop, and the program's end.
- **FR-015**: `vo run --record` and `vo replay` MUST work with workers: a replay reproduces the
  recorded run's output and exit status. It MAY do so from what the program received from its
  workers, without running the workers again. `--trace` of a run MUST mark each worker's events.
- **FR-016**: Under `vo test`, workers MUST be ended with their test, and an undelivered failure MUST
  fail the test.
- **FR-017**: A worker's output MUST be written by the program, in order with that worker's
  messages: before the next message the program receives from it, or when it is stopped or the
  program ends. Lines of different processes MUST NOT be interleaved within a line.
- **FR-018**: Each worker MUST have its own random sequence fixed by `--seed`, and its own
  `--max-memory`.

**Cost, documents**

- **FR-019**: Starting a worker MUST NOT take time proportional to the program's data, and a small
  message MUST cost little against a millisecond of work (SC-004).
- **FR-020**: Every exported item MUST have a doc comment with examples that run; the guide documents
  the module; the site mentions no development material; the reference MUST stay under 15,000 tokens
  (14,979 now: text is cut elsewhere to make room for the library row).
- **FR-021**: The feature MUST work on x86-64, ARM64 and RISC-V. `concurrent_map` and
  `serve_workers` MUST keep their behaviour; no existing golden case changes except the listing of
  modules.

### Key Entities

- **Worker**: a process started by the program to run one function, with a handle in the program.
- **Handle**: what the program holds: typed by the messages the worker takes and gives.
- **Link**: what the worker's function holds: the other end.
- **Message**: a value of the declared type, copied from one process to the other.
- **Worker error**: no message in time, ended, or failed with a runtime error's code and message.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 64 workers each exchange 10,000 messages with the program in both directions: none is
  lost, repeated or out of order, on the three architectures.
- **SC-002**: After 200 runs ended in each of the ways of story 3 (return, `exit`, runtime error,
  timeout, SIGTERM, SIGKILL) with workers computing, sleeping and waiting on the network, no process
  started by any run exists one second later.
- **SC-003**: A call that never returns, given 100 milliseconds, yields a timeout within 300
  milliseconds in 100 of 100 runs, and its process is gone.
- **SC-004**: A round trip of a small message (a record of two ints each way) takes under 50
  microseconds on x86-64 (two bare pipes between two processes measure 12 on the development
  machine), and four workers finish a fixed computation of one second of work at
  least 2.5 times faster than one process on a machine with four or more cores. Starting a worker
  from a program holding 100 MiB of data takes under 10 milliseconds.
- **SC-005**: 100 recordings of a program whose workers race each replay to the output and exit
  status of their own run; a program that names its workers in every receive prints the same results
  in 1,000 of 1,000 runs.
- **SC-006**: The example that uses workers has tests that run without the network; the reference is
  under 15,000 tokens; every doc example passes; the full test suite passes on x86-64 and the
  module's cases on ARM64 and RISC-V.

## Assumptions

- **A library module on top of what exists**: the process, pipe and recording machinery of
  `concurrent_map` and `serve_workers`. Names are chosen in the plan.
- **A worker is given a function, not a setup and a handler**: a function that loops over its
  incoming messages is the handler form, and a function that never reads is the background job, so
  one shape serves both. (The review sketched `worker(setup, handle)`.)
- **No shared memory, no threads**, as the review says.
- **Failure is isolated**: unlike a `concurrent_map` task, a failing worker does not stop the
  program; the program decides. A failure nobody looked at still makes the run fail at its end.
- **Workers do not outlive the program**, and there is no way to detach one.
- **A worker's output appears when the program receives from it**, or at its stop or the program's
  end, not when it is written: that is the price of the same output on every run. A worker that must
  log as it goes appends to a file.
- **Protocols are not part of this**: server-sent events and WebSockets are read by a worker with
  what `std.http` and `std.tcp` offer; clients for them are separate work. The roadmap's "Events"
  row is answered by the model, not by a protocol.
- **No new effect**: starting a process is not an effect of its own, as it is not for
  `concurrent_map`; a worker's effects are its function's.
- **No supervision trees, no registry of named workers, no distribution across machines.**
- Out of scope: threads, shared memory, channels as values that can be passed around, `select` over
  workers of different message types, async functions, timers as a separate primitive (a worker that
  sleeps is a timer), cancelling a call that runs in the program's own process, WebSocket and
  server-sent-event clients, detached workers.

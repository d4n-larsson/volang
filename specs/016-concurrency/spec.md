# Feature Specification: Concurrency

**Feature Branch**: `016-concurrency`

**Created**: 2026-11-16

**Status**: Draft

**Input**: User description: "Concurrency support" (roadmap "Next: concurrency", spec 016).

Background: a vo program does one thing at a time. A program that fetches ten URLs waits for each in turn,
and `std.http.serve` answers one request at a time, so one slow client holds every other client for up to
10 seconds. The roadmap fixes what concurrency must keep: determinism (Principle III) with record and
replay still working; effects as a boundary (a task runs within its spawner's grant, and `vo.effects` still
shows what each function may do); no data races by construction, because agents get shared mutable state
wrong; and structured lifetimes, so a task never outlives the code that started it and errors and
timeouts have one place to go.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–015: AI coding agents that write vo programs from the reference and
`vo doc`, and the people who review and run those programs.

### User Story 1 - Fetch many things at once (Priority: P1)

An agent writes a program that checks the status of ten URLs, or fetches ten JSON documents and merges
them. It runs the requests concurrently and gets the results back in the order of its inputs, so the total
time is about that of the slowest request, not the sum, and the program reads exactly like the sequential
version except for one call.

**Why this priority**: the most common reason a program needs concurrency, and the smallest slice that
proves the model (structured start and wait, results in order, effects inherited, determinism).

**Independent Test**: golden cases against a local test server whose responses are delayed: ten requests
of 1 s each finish in well under 10 s; results are in input order whatever order the responses arrive in;
the same program run twice prints byte-identical output.

**Acceptance Scenarios**:

1. **Given** a list of ten URLs served with a delay of 1 s each, **When** the program fetches them
   concurrently, **Then** it finishes in under 3 s and prints the ten results in the order of the list.
2. **Given** the same program, **When** the server answers the requests in a different order on each run,
   **Then** the program's output and exit status are byte-identical across runs.
3. **Given** a concurrent fetch inside a function declared `uses net("api.example.com")`, **When** one of
   the tasks asks for another host, **Then** it stops with R0010 exactly as a sequential call would; the
   tasks need no effect beyond what the sequential code needs.
4. **Given** a limit on how many tasks run at once (for example at most 3), **When** ten inputs are given,
   **Then** no more than 3 requests are outstanding at any moment and the results are the same.
5. **Given** a run recorded with `vo run --record`, **When** it is replayed, **Then** it gives the same
   output and exit status, opening no connections.

---

### User Story 2 - A slow client does not stall the server (Priority: P1)

A program serves HTTP with `std.http.serve`. One client opens a connection and sends its request slowly
(or never finishes it). Other clients are answered meanwhile, instead of waiting up to 10 s behind it.

**Why this priority**: the concrete defect the roadmap names; every server written in vo has it today.

**Independent Test**: step cases that open a slow connection and then send ordinary requests: the
ordinary requests are answered promptly; the slow one is still answered (or closed at the existing
limit) afterwards.

**Acceptance Scenarios**:

1. **Given** a running server and a client that has sent half a request and stopped, **When** five other
   clients send complete requests, **Then** all five are answered within 1 s each.
2. **Given** a client that never completes its request, **When** the existing per-connection limit
   passes, **Then** that connection is closed as today, and no other connection was delayed by it.
3. **Given** a server whose handler keeps state between requests (for example a counter in a record the
   handler closes over), **When** many clients send requests at the same time, **Then** handler calls run
   one at a time in the order the requests became complete, so the state is never corrupted and every
   response is the one a sequential server gives for that order (clarified: only connections are
   concurrent, not handler calls).
4. **Given** a server recording made with `--record`, **When** replayed, **Then** the requests are handled
   in the recorded order and the responses match.

---

### User Story 3 - Failure and timeout have one place to go (Priority: P1)

An agent runs several tasks and one of them fails: a runtime error, a timeout, or an `err` result. The
program reports it once, at the place that started the tasks, and no task keeps running in the background.

**Why this priority**: structured lifetimes are what makes concurrency safe for agents to debug; without
them, a failure can surface far from its cause or not at all.

**Independent Test**: golden cases where one task of several stops with each runtime error kind, returns
`err`, or exceeds `--timeout`; checks on what is printed, the exit status, and that the other tasks
stopped.

**Acceptance Scenarios**:

1. **Given** five tasks where the third stops with a runtime error, **When** the program runs, **Then** it
   stops with that runtime error's code and exit status 70, the report names the task's input position
   and the failing line, and the other tasks are stopped before the program exits.
2. **Given** tasks that each return a `result`, **When** one returns `err`, **Then** the caller receives
   every task's `result` in input order and decides; nothing is stopped on its behalf.
3. **Given** `--timeout 2` and a task that waits forever, **When** the program runs, **Then** it stops
   with R0009 and exit 124 after about 2 s, as a sequential program would.
4. **Given** the code that started the tasks, **When** it continues after them, **Then** every task has
   finished: no task can still be running, and none can be started that outlives that call.

---

### User Story 4 - No shared mutable state by construction (Priority: P1)

An agent writes a task that appends to a list it was given, or that changes a record it closes over,
expecting the caller to see the change. The language does not let one task change what another task (or
the caller) can see: the task works on its own values, and the only way back is its result. The agent
learns this from the reference or from a diagnostic, not from a race.

**Why this priority**: the roadmap's "impossible rather than detected"; this is the property that lets
reviewers trust concurrent code without reading every line.

**Independent Test**: golden cases where a task changes a list, map, record or collection it received or
closed over: the caller's values are unchanged afterwards (or the program is rejected at check time with a
diagnostic that has a fix); property tests that a concurrent run gives the same results as the sequential
run of the same function over the same inputs.

**Acceptance Scenarios**:

1. **Given** a task given a list that appends to it, **When** the caller reads the list afterwards,
   **Then** it is unchanged, and the task's own changes reach the caller only through its result.
2. **Given** any pure function `f` and inputs `xs`, **When** `f` is run concurrently over `xs`, **Then**
   the results equal `map(xs, f)` (checked by property tests over generated inputs).
3. **Given** a task that changes something the caller can see, **When** checked, **Then** either it is a
   check error naming the shared value, with a fix, or the reference states plainly that tasks get copies;
   in neither case can the program observe a half-finished change.

---

### User Story 5 - Review sees what concurrent code may do (Priority: P2)

A reviewer reads `vo.effects` and the diff of a change that makes a loop concurrent. The effects of the
functions involved are unchanged (concurrency is not a new capability), and the change is one call in the
diff.

**Why this priority**: keeps spec 002's review story intact; small, but a regression here would undermine
the security boundary of spec 013.

**Independent Test**: golden cases comparing `vo effects` output for the sequential and concurrent forms of
the same program; a case where a task's function needs an effect its caller lacks is E0501 with a fix.

**Acceptance Scenarios**:

1. **Given** a sequential loop over `std.http.get` and the same loop made concurrent, **When**
   `vo effects --update` runs, **Then** the records are identical.
2. **Given** a task function that prints, **When** the caller does not declare `console`, **Then** it is
   E0501 with a fix adding exactly `console`, as for `map`.

---

### Edge Cases

- **No inputs**: running zero tasks returns an empty result list at once.
- **One input**: behaves exactly like calling the function once.
- **Console output from tasks**: two tasks that print never interleave within a line, and the order of
  printed output is the same on every run (for example each task's output appears in input order),
  whatever order the tasks finish in.
- **Tasks that start tasks**: allowed; the inner tasks finish before the outer task does. Inside a task,
  tasks run one at a time (amended in planning, research R7), so the number running at once never exceeds
  the outermost limit plus the nesting depth, and nesting cannot exhaust resources.
- **Compute-only tasks**: a task that never waits on input or output still finishes, still counts toward
  `--timeout`, and does not stop the others from finishing.
- **Ctrl-C or SIGTERM** while tasks run: every task stops and the program exits as it does today.
- **Memory**: each task is bounded by `--max-memory` on its own; exceeding it is R0006 as today, and
  `--mem-stats` reports the program without its tasks (amended in planning, research R13: tasks run in
  separate processes, which keeps each task's figures deterministic).
- **Very many tasks**: 10,000 inputs with a limit of 10 at once complete without exhausting memory or file
  descriptors.
- **Traces**: `--trace` records which task each call belongs to, and the trace is byte-identical across
  runs with the same inputs.
- **Property tests and contracts**: functions run as tasks keep their `requires` and `ensures` checks; a
  property test may run code concurrently and still reproduces for a given `--seed`.
- **Determinism without recording**: for the same inputs and the same responses, output never depends on
  the timing of responses; the language offers no way to ask "which task finished first".

## Requirements *(mandatory)*

### Functional Requirements

**Running tasks**

- **FR-001**: The language MUST offer one way to run a function over a list of inputs as concurrent tasks
  and wait for all of them, returning their results in input order. It MUST read like the existing `map`
  builtin, so making a sequential loop concurrent is a one-call change. Its exact name and form are
  decided in planning within the reference budget (FR-014).
- **FR-002**: The caller MUST state how many tasks run at once, as a required argument from 1 to 64
  (amended in planning: one form, no default; research R2).
- **FR-003**: Tasks MUST have structured lifetimes: the call that starts tasks returns only after every
  task it started has finished or been stopped. No task handle or task can escape that call.
- **FR-004**: Concurrency is for waiting on input and output (network, server connections); parallel
  computation is not a goal, and the collector and the VM stay single-threaded (clarified 2026-11-16).
  Each task runs single-threaded in its own process (amended in planning, research R1); that tasks may
  then also compute on several cores at once is incidental, never observable in output, and not promised.
- **FR-005**: Tasks MUST communicate only through their inputs and results; there are no channels or
  other ways to pass values between running tasks (clarified 2026-11-16).

**Safety**

- **FR-006**: A task MUST NOT be able to change any value another task or its caller can observe. Values
  passed to a task, and values its function closes over, are either copied into it or rejected at check
  time; the choice is made in planning, and either way it is stated in the reference.
- **FR-007**: A task's function MUST be checked against the caller's effects exactly as a function passed
  to `map` is: the caller declares the effects, with scopes, and every task runs with the caller's grant
  at run time (R0010 for anything outside it). Starting tasks MUST NOT be a new effect, and `vo.effects`
  MUST be unchanged by making code concurrent.
- **FR-008**: A runtime error in any task MUST stop every other task started by the same call and then
  stop the program with that error's report, naming the task's input position; when several tasks fail,
  the one reported MUST be the same on every run (the lowest input position).
- **FR-009**: `--timeout` MUST cover the whole program including all tasks; for `vo test`, each test; for
  `serve`, each handler call as today.

**Determinism and replay**

- **FR-010**: For the same program, arguments, input and responses, output (stdout, stderr, JSON records,
  traces) and exit status MUST be byte-identical whatever order responses arrive in. Results are
  returned in input order; console output of tasks appears in a fixed order.
- **FR-011**: `vo run --record` MUST capture everything that can affect a concurrent run, and `vo replay`
  MUST reproduce its output and exit status exactly, opening no connections, on any supported platform.

**The server**

- **FR-012**: `std.http.serve` MUST read requests and write responses on many connections at once, so a
  slow or stalled client delays no other client; the existing per-connection limits stay.
- **FR-013**: Handler calls MUST NOT overlap: they run one at a time, in the order requests became
  complete, so state a handler keeps between requests behaves exactly as today (clarified 2026-11-16).
  A recording captures that order and a replayed server handles requests in it. A handler that itself
  waits (for example on an outbound request) delays the next handler call, but not the reading and
  writing of other connections.

**Surface and quality**

- **FR-014**: The reference MUST describe concurrency in at most 400 more tokens and stay under 15,000;
  `vo doc` MUST document any new library items with checked examples.
- **FR-015**: Every new diagnostic and runtime code MUST have a catalog entry, a `vo explain` text and a
  golden case; diagnostics follow Principle II (span, message, fix when unambiguous).
- **FR-016**: Programs that do not use concurrency MUST behave byte-identically to the previous release,
  and the full quality gate (suite, fuzzer, determinism check, instrumented build) MUST pass on x86-64,
  ARM64 and RISC-V 64.

### Key Entities

- **Task**: one call of a function on one input, run concurrently with its siblings; has an input position,
  a result, and the caller's grant. Exists only during the call that started it.
- **Task group**: the tasks started by one call; finishes when all its tasks have; carries the limit on how
  many run at once and the single failure reported for it.
- **Connection** (server): one client's request and response, read and written concurrently with others.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Ten requests to a local server that delays each response by 1 s finish in under 3 s when
  run concurrently, against at least 10 s sequentially.
- **SC-002**: With one client holding a half-sent request, the server answers 100 other requests, each
  within 1 s.
- **SC-003**: Across 100 runs of each concurrency golden case with responses delayed by random amounts, the
  output and exit status are byte-identical in 100% of runs, and every recorded run replays identically
  on all three platforms.
- **SC-004**: Property tests show concurrent results equal sequential `map` results in 100% of at least
  1,000 generated cases, including inputs that the function changes.
- **SC-005**: Making the sequential URL-checking example concurrent changes one line of the program and
  no line of `vo.effects`.
- **SC-006**: 10,000 tasks with a limit of 10 at once complete within `--max-memory` defaults and without
  running out of file descriptors.
- **SC-007**: The reference grows by at most 400 tokens and stays under 15,000; programs without
  concurrency give byte-identical results to the previous release; the full gate passes on all three
  platforms.

## Assumptions

- **One thread, waiting concurrently (clarified 2026-11-16)**: the roadmap's motivating cases (fetching
  URLs, a server stalled by a slow client) wait on the network rather than compute, and the collector
  and the VM are single-threaded. Parallelism on several cores is left for a later spec; because tasks
  share nothing, it would not change the language.
- **Inputs and results only (clarified 2026-11-16)**: no channels; pipelines are successive concurrent
  calls. Channels were considered and left out: they bring deadlocks (and a code for them) and
  200–300 reference tokens.
- **Server: concurrent connections, sequential handlers (clarified 2026-11-16)**: fixes the slow-client
  stall without changing what handlers may do; overlapping handler calls (each with a copy of its
  state) were rejected because they break servers that keep state between requests.
- **Structured and map-like**: one form, run a function over inputs and get results in order, is assumed
  to cover the motivating cases with the smallest reference cost; free-standing `spawn` handles,
  `select`/"first finished", and async/await syntax are left out, the last two because they make the order
  of events observable.
- **No new effect**: concurrency is not a capability to reach the outside world, so it is not an effect;
  what each task may do is already visible through its function's effects.
- **Determinism through ordering, not timing**: the order in which responses arrive is outside input,
  already captured by `--record`; the language makes it unobservable in output rather than forbidding
  concurrency.
- **Side effects outside the program** (two tasks writing the same file, outbound requests in different
  orders) happen in the order the tasks reach them, which is captured by recording; making such ordering
  part of the language is not required by this spec.
- **No sleep or timer builtin** is added; tests use a delaying local server.
- **HTTPS stays out of scope** (spec 007 follow-up).
- **The previous release is the oracle** for "behaves exactly as before".

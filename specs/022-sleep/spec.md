# Feature Specification: Sleep

**Feature Branch**: `022-sleep`

**Created**: 2026-11-19

**Status**: Draft

**Input**: User description: "Sleep: a way for a vo program to wait (retries, polling, rate limits). Roadmap
gap "Sleep": under the clock effect; replay must not wait. std.time.sleep(ms) next to std.time.now (no new
builtin name). With a fake clock (--clock-ms, and every vo test) it does not wait and advances the fake
clock by ms; replay does not wait. --timeout covers it (R0009 at the deadline, not after the sleep).
Negative ms is R0007. Signals interrupt it. Concurrent tasks sleep independently."

Background: a gap of the roadmap's "Planned after 017" table. vo programs can read the clock
(`now_ms`, `std.time.now`) but cannot wait. An agent writing a retry with backoff, a poll for a file or
a service, or a client that respects a rate limit has only a busy loop on the clock: it burns a core,
and under `--clock-ms` (and in every `vo test`, which fixes the clock) the clock only moves one
millisecond per reading, so such a loop takes a very long time or never ends.

What this spec must keep:
- **Principle III**: the same program, arguments and inputs give the same output. Waiting is real time,
  which is not deterministic, so wherever the clock is fixed (tests, `--clock-ms`, replay) the wait
  must not happen and the program's view of the clock must still be consistent.
- **Effects**: waiting reads the passage of time, so it needs `clock`, the effect that already covers
  `now_ms`.
- **Defined behavior and time limits**: a wait never outlives `--timeout`, and every bad argument is a
  runtime error, not a hang.
- **The reference budget**.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–021: AI coding agents that write vo programs from the reference
and `vo doc`, and the people who review and run those programs.

### User Story 1 - Retry with a pause (Priority: P1)

An agent writes a program that calls an HTTP service, and on a failure waits 200 ms, 400 ms, 800 ms
before trying again (exponential backoff), giving up after five tries.

**Why this priority**: retrying flaky networks is the most common reason programs wait; without it the
agent either hammers the service or writes a busy loop.

**Independent Test**: a step case against the local test server where the first requests fail: the
program retries with pauses, and the measured run time is at least the sum of the pauses.

**Acceptance Scenarios**:

1. **Given** a program that calls `sleep(200)`, **When** it runs with `vo run` (real clock), **Then** at
   least 200 ms pass before the next statement, and no CPU is used meanwhile (the run's CPU time stays
   far below its elapsed time).
2. **Given** `sleep(0)`, **When** it runs, **Then** it returns at once.
3. **Given** the clock read before and after `sleep(200)` with the real clock, **When** they are
   compared, **Then** the difference is at least 200.

---

### User Story 2 - Tests and fixed clocks do not wait (Priority: P1)

An agent tests its retry logic with `vo test`. The test finishes immediately, and the program still
observes time passing: a clock reading after `sleep(500)` is 500 ms later (plus the fixed clock's usual
one-millisecond step) than the reading before.

**Why this priority**: tests run with a fixed clock (`vo test` uses clock 0); real waits there would
make tests slow and their results would not depend on the waits anyway. Without this, agents would avoid
testing code that sleeps.

**Independent Test**: golden test cases that sleep for a total of an hour of fixed-clock time and finish
in well under a second, printing the clock readings.

**Acceptance Scenarios**:

1. **Given** `vo run --clock-ms 1000` and a program that reads the clock, sleeps 500 and reads it again,
   **When** it runs, **Then** it prints `1000` and `1501` and takes no real time for the sleep.
2. **Given** a test that sleeps 3,600,000 ms in total, **When** `vo test` runs it, **Then** it passes in
   under one second.
3. **Given** the same program and arguments, **When** it runs twice with the same fixed clock, **Then**
   the output is byte-identical.

---

### User Story 3 - Replay does not wait (Priority: P1)

A reviewer replays a recorded run of a polling program that waited a total of 30 s. The replay finishes
at once and prints exactly what the recorded run printed.

**Why this priority**: replay must be fast and faithful (spec 006); a replay that sleeps is slow, and its
clock readings come from the recording anyway.

**Independent Test**: a step case that records a run with real sleeps, then replays it and compares
outputs and replay duration.

**Acceptance Scenarios**:

1. **Given** a recording of a run that slept 2,000 ms in total, **When** it is replayed, **Then** the
   replay takes under 500 ms and its output is byte-identical to the recorded run's.
2. **Given** a program changed to sleep where the recorded one did not (or the reverse), **When** it is
   replayed, **Then** the replay behaves as for any other program change (R0014 when what the program
   asks for no longer matches the recording, otherwise the recorded results).

---

### User Story 4 - Waiting stays within the limits (Priority: P2)

An agent's program sleeps longer than its time limit allows, or sleeps for a computed negative duration.
The run stops with a clear error instead of hanging or silently doing nothing.

**Why this priority**: an unbounded or ill-defined wait is the hang that Principle III forbids.

**Independent Test**: golden and step cases for a sleep past `--timeout`, a negative duration, and a
sleep interrupted by a signal.

**Acceptance Scenarios**:

1. **Given** `--timeout 1` and `sleep(60000)`, **When** it runs, **Then** it stops with R0009 after about
   1 s (not 60 s), pointing at the call.
2. **Given** `sleep(-5)`, **When** it runs, **Then** it stops with R0007 naming the value.
3. **Given** a sleeping program receiving Ctrl-C or SIGTERM, **When** the signal arrives, **Then** the
   program ends at once as it would without the sleep.
4. **Given** a fixed clock and a sleep longer than `--timeout`, **When** it runs, **Then** it does not stop
   with R0009 (no real time passes), and the fixed clock advances by the full duration.

---

### User Story 5 - Concurrent work and servers (Priority: P3)

An agent uses `concurrent_map` to poll several services, each task sleeping between tries; or a `serve`
handler sleeps briefly (a rate limit).

**Why this priority**: sleeping inside tasks must not serialize them; inside a handler, it must behave
predictably.

**Independent Test**: a step case where ten tasks each sleep 300 ms and the whole map takes well under
3 s.

**Acceptance Scenarios**:

1. **Given** `concurrent_map` over 10 items with a limit of 10, each task sleeping 300 ms, **When** it runs
   with the real clock, **Then** it takes under 1.5 s in total.
2. **Given** a `serve` handler that sleeps 100 ms, **When** a request arrives, **Then** the answer comes
   after at least 100 ms, and the handler's sleep counts toward the per-handler `--timeout`.

### Edge Cases

- **Huge durations**: a duration near the largest `int` does not overflow anything; with the real clock
  it ends at the time limit (R0009), or never when `--timeout 0` (the user chose no limit); with a fixed
  clock the clock advances and the program continues.
- **Fixed clock overflow**: advancing the fixed clock past the largest `int` is R0001 (overflow), as any
  integer overflow.
- **Precision**: waits are at least the asked duration; they may be longer (the system schedules);
  nothing promises an upper bound beyond "promptly".
- **No `clock` effect**: calling `sleep` from a function that does not declare `clock` is the existing
  missing-effect error (E0501); `vo.effects` shows it like `now`.
- **Recording**: a recorded run stores no wall-clock waits of its own, only what the program received
  (its clock readings), so a recording of a sleeping program is not larger than one without.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The standard library MUST provide `sleep(ms: int)` in `std.time`, next to `now`, needing
  the `clock` effect; no new builtin name.
- **FR-002**: With the real clock, `sleep(ms)` MUST return after at least `ms` milliseconds, without
  busy-waiting.
- **FR-003**: With a fixed clock (`--clock-ms`, and every `vo test`), `sleep(ms)` MUST NOT wait, and
  MUST advance the fixed clock so that the next reading is `ms` larger than it would have been.
- **FR-004**: During `vo replay`, `sleep` MUST NOT wait; clock readings come from the recording as today.
- **FR-005**: A negative `ms` MUST stop with R0007, naming the value; `sleep(0)` returns at once.
- **FR-006**: `--timeout` MUST cover sleeping: when the deadline falls during a real wait, the program
  stops with R0009 at the deadline, at the call.
- **FR-007**: Signals that end a program (Ctrl-C, SIGTERM) MUST end it during a sleep as at any other
  time; `serve`'s Ctrl-C/SIGTERM handling is unchanged.
- **FR-008**: Each `concurrent_map` task MUST sleep independently of the others.
- **FR-009**: `vo doc std.time.sleep` MUST document the behavior under real, fixed and replayed clocks,
  with a checked example; the reference states it in one line.

### Key Entities

- **Clock**: real (wall-clock time, waits are real), fixed (`--clock-ms`, `vo test`: a counter that
  advances by one per reading and by `ms` per sleep), or replayed (readings from the recording; sleeps
  do not wait).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A program that sleeps 200 ms with the real clock takes between 200 and 300 ms longer than
  the same program without the sleep (median of 5 runs, idle machine), using under 10 ms of CPU for it.
- **SC-002**: A test suite whose tests sleep one hour of fixed-clock time in total finishes in under one
  second.
- **SC-003**: Replaying a recorded run that slept 2 s takes under 500 ms, with byte-identical output.
- **SC-004**: `sleep(60000)` under `--timeout 1` stops with R0009 within 1.5 s.
- **SC-005**: Ten concurrent tasks sleeping 300 ms each finish in under 1.5 s.
- **SC-006**: Every new behavior (fixed clock, replay, negative, time limit, signal, tasks) has a golden
  or step case; the reference grows by one line.

## Assumptions

- Durations are whole milliseconds, an `int`, matching `now_ms`.
- `sleep` is in `std.time` because waiting is about time and `std.time.now` is there; it costs no
  builtin name.
- No async, timers or callbacks: `sleep` blocks the program (or the task) that calls it, which is the
  only way vo programs run.
- A `serve` handler that sleeps delays only the responses after it, since handlers run one at a time
  (spec 016); this is stated, not changed.
- The fixed clock's one-millisecond step per reading is unchanged; `sleep` adds to it.

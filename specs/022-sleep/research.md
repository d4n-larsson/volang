# Research: Sleep

Sources: `src/rt_effects.c` (`rt_now_ms`, `clock_fixed`/`clock_next`), `src/interp.c` (`deadline_ms`,
`rt_poll`, `rt_timeout`), `src/sys.c` (`sys_wait_fd`, `ppoll`), `src/serve.c` (signal handling),
`src/replay.c` (`io_now_ms`, `rcd_mode`).

## R1. Behavior by clock

- **Decision**: `rt_sleep(ms, node)` in `src/rt_effects.c`:
  1. `ms < 0`: R0007 "`sleep` needs a duration of at least 0 ms, got N." at `std_user_call(node)`.
  2. Replay (`rcd_mode == 2`): return.
  3. Fixed clock (`clock_fixed`): `clock_next += ms`, with an overflow check (R0001), then return.
  4. Real clock: wait until `target = sys_clock_ms() + ms` (monotonic).
- **Rationale**:
  - The fixed clock is a counter that `vo test` resets to 0 for each test, and `--clock-ms N` sets it,
    so adding `ms` gives exactly the spec's "next reading is `ms` larger" (FR-003).
  - Replay already returns recorded readings, so skipping the wait keeps output identical (FR-004).
  - Recording runs the real clock, and waits for real.
- **Alternatives**:
  - Recording sleeps as events, so replay could check them. A sleep receives nothing, and its effect on
    the readings is already recorded; an event would only make old recordings of non-sleeping programs
    differ. Rejected (R4).

## R2. Waiting with the real clock

- **Decision**: loop until `sys_clock_ms() >= target`:
  - `wait = target - now`;
  - when `deadline_ms` is set and `deadline_ms <= target`, wait only until `deadline_ms`, then raise
    `rt_timeout(std_user_call(node))` (R0009 at the call, FR-006);
  - wait in chunks of at most 1 s with `ppoll(0, 0, ts, 0, 8)` (no file descriptors; returns on timeout
    or on a signal, which also makes the loop re-check the clock).
- **Rationale**:
  - `ppoll` is already the wait primitive in `sys.c`, `serve.c`, `task.c` and `process.c`, with
    `SYS_ppoll` defined for all three platforms.
  - The deadline uses the same monotonic `sys_clock_ms` as `rt_poll`.
  - The chunks keep the loop responsive.
- **Alternatives**: `nanosleep` or `clock_nanosleep` would need new syscall numbers on three platforms for
  no gain.

## R3. Signals, serve and tasks

- **Signals**: outside `serve`, SIGINT and SIGTERM keep their default action and end the process during
  `ppoll` as anywhere else (FR-007). Inside `serve`, they are blocked and read from a signalfd after
  the handler returns (spec 011). A sleeping handler is bounded by the per-handler `--timeout`, so the
  server still stops within that time. This is stated in `vo doc`, not changed.
- **Tasks**: `concurrent_map` tasks are separate processes (spec 016), so each sleeps on its own (FR-008).
  A task with a fixed clock advances its own copy of the counter, as `now_ms` does today.

## R4. Recording

- **Decision**: nothing new is recorded. `vo run --record` with the real clock waits, and records the
  `now_ms` readings as today. `vo replay` skips the waits.
- **Consequence**: a program changed only by adding or removing sleeps replays without R0014; a changed
  number of clock readings is R0014, as today (US3, scenario 2).

## R5. Reference and documentation

- **Reference**:
  - the effects table row becomes "| clock | `now_ms`, `std.time.sleep` |";
  - the `--clock-ms` sentence gains "; `sleep` then does not wait but moves the clock on".
- **`vo doc std.time.sleep`** states real, fixed and replayed behavior and the R0007/R0009 rules. Its
  example runs under the doc checker's fixed clock: `let a = now(); sleep(500); now() - a is 501`.

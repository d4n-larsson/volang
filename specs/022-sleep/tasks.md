---

description: "Task list for vo-lang spec 022: Sleep"
---

# Tasks: Sleep

**Input**: Design documents from `specs/022-sleep/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R5), data-model.md, contracts/library.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Cases land first and fail before the primitive exists.

**Organization**:
- Setup.
- Foundational: the primitive and `std.time.sleep`, which every story needs.
- One phase per user story: US1–US3 are P1, US4 is P2, US5 is P3.
- Docs and verification.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases** go in `tests/cases/time/` (`sleep_*`). **Step cases** go in
  `tests/cases/sleep_steps/<name>/steps`.
- **Between edits**: `make quick AREA=time`. `make test` once at the end (T017); never two suites at once.
- **Timed checks** (SC-001 to SC-005) live in `tests/sleep_check.py`, outside `make test` (they need an
  idle machine), and run in `make gate`.
- **Shortcuts**: marked `ponytail:`.

---

## Phase 1: Setup

- [X] T001 [P] Create `specs/022-sleep/results.md` (SC-001 to SC-006, Verification, Deviations), as in
  `specs/021-bytes/results.md`.
- [X] T002 [P] `tools/quick.sh`:
  - the `time` area's cases become `time sleep_steps doc`;
  - `std/time.vo`, `src/rt_effects.c` and `tests/sleep_check.py` map to `time` (rt_effects.c is
    shared: keep it mapping to `all` if it already does, and add `time` only for `std/time.vo` and the
    script).

  Check with `QUICK_FILES=std/time.vo tools/quick.sh --which`.

---

## Phase 2: Foundational (blocks every story)

- [X] T003 [P] Golden case `tests/cases/time/sleep_doc` (`.cmd`: `doc std.time.sleep`), expecting the
  signature `fn sleep(ms: int) uses clock` and the contract's example. It fails until T005.
- [X] T004 Primitive `__sleep(ms: int)` (research R1, R2):
  - builtin id 81, signature `"i>v"`, effect `EF_CLOCK`, in `src/vo.h` (`B_SLEEP`, `B_LAST`),
    `src/check_expr.c` (`B_COUNT`, `builtin_names`, `builtin_sigs`), `src/check_effects.c` and
    `src/rt_call.c` (enum, `_Static_assert`, dispatch to `rt_sleep(a0, call)`);
  - `rt_sleep(long ms, Node *node)` in `src/rt_effects.c`, declared in `src/vo.h`:
    1. `ms < 0`: R0007 "`sleep` needs a duration of at least 0 ms, got N." at `std_user_call(node)`;
    2. replay (`rcd_mode == 2`): return;
    3. `clock_fixed`: `clock_next += ms` with an explicit overflow check (`rt_overflow` at the user's
       call);
    4. real clock: `target = sys_clock_ms() + ms` (saturating). Loop while `sys_clock_ms() < target`:
       - when `deadline_ms` is set and `sys_clock_ms() >= deadline_ms`, `rt_timeout(std_user_call(node))`;
       - wait `min(target, deadline if set) - now`, at most 1,000 ms, with
         `vo_syscall5(SYS_ppoll, 0, 0, (long)ts, 0, 8)`.
- [X] T005 `std/time.vo`: `export fn sleep(ms: int) uses clock { __sleep(ms); }`. Its `vo doc` comment
  covers the real, fixed and replayed clocks, R0007, R0009 and `serve` handlers, with the example
  `let a = now(); sleep(500); now() - a is 501`. `tests/doc_conformance.py` runs examples with
  `vo run --seed 1` (the real clock), so it also gets `--clock-ms 0`, as `vo test` does; the existing
  `now() >= 0` example still holds. Record this in `results.md`.

**Checkpoint**: T003 passes; `tests/doc_conformance.py` passes.

---

## Phase 3: User Story 1 — Retry with a pause (P1) 🎯 MVP

**Goal**: real waits.

**Independent Test**: the step cases below.

- [X] T006 [P] [US1] Step cases in `tests/cases/sleep_steps/`:
  - `real_wait`: a program that reads `now_ms`, sleeps 200, reads again, and prints whether the
    difference is at least 200 (`true`) and `sleep(0)` returns;
  - `retry`: a retry loop against the test server (`server http`, a route that fails), with pauses of
    50, 100 and 200 ms, printing the attempts and whether at least 350 ms passed.

  They fail until T004/T005 and pass after them (T004 and T005 implement US1).

**Checkpoint**: T006 passes.

---

## Phase 4: User Story 2 — Tests and fixed clocks do not wait (P1)

**Goal**: no waits with a fixed clock; the clock moves on.

- [X] T007 [P] [US2] Golden cases in `tests/cases/time/`:
  - `sleep_fixed` (`.cmd`: `run --clock-ms 1000 {}`): prints `1000` then `1501` around `sleep(500)`;
  - `sleep_test_hour` (`.cmd`: `test {}`): tests that sleep 3,600,000 ms in total and check the clock
    readings, passing (run time is checked by T015);
  - `sleep_negative` (`run --json {}`): R0007 with the contract message, the span on the user's call;
  - `sleep_overflow` (`run --clock-ms 9223372036854775000 --json {}`): `sleep(1000)` is R0001;
  - `sleep_fixed_timeout` (`run --clock-ms 0 --timeout 1 {}`): `sleep(60000)` returns at once, with no
    R0009, and the clock moved 60,000.
- [X] T008 [US2] Make T007 pass. Implemented by T004's fixed branch: fix whatever the cases show, such
  as the overflow check's location.

**Checkpoint**: T007 passes.

---

## Phase 5: User Story 3 — Replay does not wait (P1)

- [X] T009 [P] [US3] Step case `tests/cases/sleep_steps/replay`:
  - record a program that sleeps 4 × 500 ms with the real clock and prints the elapsed clock time
    rounded down to seconds;
  - `vo replay` it;
  - the outputs are identical.

  The replay's duration is checked by T015.
- [X] T010 [P] [US3] Step case `tests/cases/sleep_steps/replay_changed`: replay the recording of T009
  with a program that sleeps differently but reads the clock the same number of times, which replays
  with no R0014; and one that reads the clock once more, which is R0014.

**Checkpoint**: T009 and T010 pass.

---

## Phase 6: User Story 4 — Waiting stays within the limits (P2)

- [X] T011 [P] [US4] Step cases in `tests/cases/sleep_steps/`:
  - `timeout`: `vo run --timeout 1` of `sleep(60000)` is R0009 at the call, exit 124;
  - `signal`: `start vo run main.vo` of a program that sleeps 60 s, then `signal TERM`; it ends at once
    with the usual exit status for SIGTERM.
- [X] T012 [US4] Make T011 pass: T004's deadline branch; check that the `signal` case's exit status is
  the same as a program busy in a loop.

---

## Phase 7: User Story 5 — Concurrent work and servers (P3)

- [X] T013 [P] [US5] Step cases in `tests/cases/sleep_steps/`:
  - `tasks`: `concurrent_map` over 10 items, limit 10, each task sleeping 300 ms with the real clock,
    printing the results (timing is checked by T015);
  - `serve_handler`: a `serve` handler that sleeps 100 ms; `request GET /` gets its answer; `signal TERM`
    stops the server.

**Checkpoint**: T013 passes.

---

## Phase 8: Polish and verification

- [X] T014 [P] Reference (`docs/reference.md`): the effects row `| clock | now_ms, std.time.sleep |`, and
  the `--clock-ms` sentence gains "; `sleep` then does not wait but moves the clock on". Record the token
  growth in `results.md`.
- [X] T015 [P] `tests/sleep_check.py`, each figure the median of 5 runs:
  - SC-001: a 200 ms sleep adds 200–300 ms, with CPU time (`resource.getrusage(RUSAGE_CHILDREN)`) under
    10 ms more than without it;
  - SC-002: the hour-long test finishes in under 1 s;
  - SC-003: the replay of T009's recording takes under 500 ms;
  - SC-004: R0009 within 1.5 s;
  - SC-005: the 10 tasks finish in under 1.5 s.

  Added to `make gate`.
- [X] T016 [P] Docs:
  - `site/docs/guide/standard-library.md`: `sleep` in the std.time section, with a fixed-clock example
    that the site checker can run;
  - `docs/roadmap.md`: spec 022 **Done**, the "Sleep" gap row removed, and a short section with
    "Not done" (timers, async, sub-millisecond, interrupting a sleeping `serve` handler at once);
  - `README.md`: the spec list.
- [X] T017 `make test` on x86-64, then `make ARCH=aarch64 test` and `make ARCH=riscv64 test`, one at a
  time.
- [X] T018 Fill in `results.md`; set the spec status to Implemented; mark every task [X].

---

## Dependencies & Execution Order

- **Phase 1** (T001, T002): in parallel.
- **Phase 2**: T003 first (failing), then T004 → T005. Blocks every story.
- **US1–US5**: after Phase 2, independent of each other; the case-writing tasks (T006, T007, T009,
  T010, T011, T013) can be written in parallel.
- **Polish**: T014–T016 in parallel after the stories; T017 → T018 last.

## Parallel Examples

- After Phase 2: write T006, T007, T009, T010, T011 and T013 together, then run them one suite at a time.
- Polish: T014, T015 and T016 together.

## Implementation Strategy

- **MVP**: Phases 1–3 (real waits).
- **Then**: US2 and US3 (no waits where the clock is fixed or replayed): the determinism promise.
- **Then**: US4 (limits) and US5 (tasks, serve).
- **Last**: reference, timed checks, docs, then the three platforms once.

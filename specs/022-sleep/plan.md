# Implementation Plan: Sleep

**Branch**: `022-sleep` | **Date**: 2026-11-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/022-sleep/spec.md`

## Summary

`std.time.sleep(ms)` waits, under the `clock` effect. It is a vo wrapper over one new std-only
primitive, `__sleep(ms)`, implemented in `src/rt_effects.c` beside `rt_now_ms`, which owns the clock.

**What a sleep does, by clock** (research R1):

| Clock | Behavior |
|---|---|
| fixed (`--clock-ms`, every `vo test`) | adds `ms` to the fixed clock's next reading; no wait |
| replay | returns at once (readings come from the recording) |
| real | waits with `ppoll` and no file descriptors, until a monotonic target, never past the `--timeout` deadline |

**Errors and limits** (R2, R3):
- A negative duration is R0007 at the user's call (`std_user_call`).
- A deadline inside the wait is R0009 at the deadline.
- Overflowing the fixed clock is R0001.
- Ctrl-C and SIGTERM keep their default action and end a sleeping program at once. Inside `serve` they
  are read after the handler returns, which is unchanged.

**Nothing is recorded** (R4): a sleep receives no data, and the clock readings around it are already
recorded.

## Technical Context

**Language/Version**: C (C11, freestanding) for the primitive; vo for `std/time.vo`.

**Primary Dependencies**: none.

**Storage**: none (no recording change).

**Testing**:
- golden cases `tests/cases/time/sleep_*`: fixed clock, negative duration, an hour of fixed-clock
  sleeps in a test, `vo doc`;
- step cases `tests/cases/sleep_steps/`: real waits, `--timeout`, record and replay, `concurrent_map`,
  a `serve` handler, a signal;
- `tests/sleep_check.py` for the timed criteria SC-001 to SC-005;
- `make quick AREA=time` between edits (`tools/quick.sh` gains the step directory); `make test` once.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: SC-001 to SC-005 (wait precision within 100 ms on an idle machine; no CPU while
waiting; fixed-clock and replay sleeps cost nothing).

**Constraints**:
- Principle III: output does not depend on real waits, because every place output could observe time
  (fixed clock, replay) skips them.
- Principle V: +1 reference line, no new builtin.

**Scale/Scope**:
- C: about 40 lines (`rt_sleep`, primitive id and signature, effect entry);
- vo: 5 lines plus documentation in `std/time.vo`;
- about 6 golden and 6 step cases, 1 script.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate | Status |
|---|---|
| I. Agent-first | PASS. One function, in the module that has `now`; no new syntax or builtin name. |
| II. Machine-readable feedback | PASS. Existing codes R0007, R0009, R0001 and E0501, with structured reports. |
| III. Determinism and defined behavior | PASS. Real waits happen only with the real clock, whose readings are already nondeterministic and recorded; fixed clocks and replay skip them. Every input is defined: negative is R0007, past the deadline is R0009, overflow is R0001. |
| IV. Test-first | PASS. Cases first. |
| V. Minimal surface | PASS. +1 line in the reference. YAGNI: no timers, no async, no sub-millisecond durations. |
| Implementation constraints | PASS. A system call through the existing `vo_syscall5`; no assembly change. |

Post-design re-check: unchanged.

## Project Structure

```text
specs/022-sleep/  plan.md research.md data-model.md quickstart.md contracts/library.md
src/rt_effects.c   rt_sleep(ms, node)
src/vo.h, src/check_expr.c, src/rt_call.c, src/check_effects.c   primitive 81 __sleep "i>v", effect clock
std/time.vo        export fn sleep(ms: int) uses clock
tests/cases/time/sleep_*, tests/cases/sleep_steps/, tests/sleep_check.py
docs/reference.md  one line (the clock effect row and the --clock-ms sentence)
```

**Structure Decision**: the primitive lives with the clock it advances; no new file.

## Complexity Tracking

None.

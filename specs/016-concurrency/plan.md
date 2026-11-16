# Implementation Plan: Concurrency

**Branch**: `016-concurrency` | **Date**: 2026-11-16 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/016-concurrency/spec.md`

## Summary

One builtin, `concurrent_map(xs, limit, f)`, that runs `f` on each element as a task and returns the
results in input order; and a `std.http.serve` that reads many connections at once while calling the
handler for one request at a time (research R1-R16):
- **Tasks are child processes** (R1): a plain fork per task, so the VM, the collector and every
  fixed-address region work unchanged, tasks get copies of everything for free (no shared mutable state,
  R3), and each task is deterministic on its own.
- **Determinism by ordering** (R5, R6): the parent copies output from the lowest unfinished task only,
  so output is task 0's, then task 1's, ...; the failure reported is the one at the lowest position;
  errors are re-raised in the parent with the child's code, span and trace, so reports match `map`'s.
- **Results** come back over a pipe in a type-directed form that keeps sharing and cycles (R4);
  function-typed results are E0314, custom-order collections R0015, and stdin in tasks E0511 (R8).
- **Record/replay, traces** (R10, R11): child events and trace records flow through the parent in task
  order; the recording format is unchanged and replay runs tasks one at a time.
- **Server** (R14): an event loop over up to 64 connections inside `src/serve.c`; `std/http.vo` unchanged.

## Technical Context

**Language/Version**: C (C11, freestanding, the project's strict warning set) for the toolchain; vo for
examples and tests.

**Primary Dependencies**: none new. New Linux system calls through the existing syscall interface:
`clone`, `wait4`, `pipe2`, `dup3`, `fcntl`, `prctl`, `kill`, `getppid` (R16).

**Storage**: N/A (recordings and traces keep their formats; one trace record kind added).

**Testing**: golden cases in `tests/cases/tasks/` (every acceptance scenario, E0314, E0511, R0015, R0007,
failures, nesting, traces, seeds), step cases `tests/cases/tasks_steps/` (record/replay, timing against
the delaying server) and `tests/cases/serve_concurrent/` (slow clients); `tests/tasks_determinism.py`
(SC-003: 100 runs with randomized delays); `tests/tasks_props.py` (SC-004: `concurrent_map == map`
under 10 seeds); `tests/cross_replay.py` extended with a task recording; the full gate on three
platforms, instrumented.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V 64 (native or qemu-user).

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: SC-001 (10 × 1 s requests under 3 s), SC-002 (100 requests behind a stalled client,
each under 1 s); fork cost measured and recorded (expected well under 1 ms per task natively), SC-006
(10,000 tasks at limit 10 complete); programs without tasks: the speed gate stays under 1.10.

**Constraints**: Principle III (byte-identical output whatever the response timing); Principle II (three
new codes, all catalogued); reference +400 tokens at most (now ~11,100 of 15,000); no new assembly; no
change to programs that do not call `concurrent_map` or `serve`.

**Scale/Scope**: about 900 lines of C: `src/task.c` new (~550: scheduling, pipes, transfer, re-raise),
`src/serve.c` (~200 changed), checker (~60), replay/trace/random/exit hooks (~90); 1 builtin, 3 codes,
1 trace record kind; ~40 golden cases, 3 test scripts, 1 example.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** One form, a builtin shaped like `map` with a required limit (no optional argument, no second spelling). No new syntax, no hidden control flow: the call returns when all tasks have finished; failures surface at the call as with `map`. Copy semantics are stated in one sentence of the reference. |
| II. Machine-readable feedback | **PASS.** E0314, E0511, R0015 get catalog entries, `vo explain` texts and golden cases; re-raised task errors carry the same code, span, message and trace as the sequential form; the trace schema gains one record kind within v1 (fields added, none removed). No code reused (E0508 stays unassigned). |
| III. Determinism and defined behavior | **PASS.** Output order is fixed by position, not timing (R5); the reported failure is the lowest position (R6); per-task seeds derive from `--seed` (R12); each task runs as an ordinary deterministic sequential run, so its collections and memory figures do not depend on timing (R1, R13); recordings replay exactly (R10). Process-creation failure is a T0003 report, never a crash. Outside effects of concurrent tasks (two tasks writing one file) follow arrival order and are captured by recording, as the spec's assumptions state. |
| IV. Test-first through the CLI | **PASS.** Golden and step cases for each scenario and code land first and fail; determinism and property scripts join the gate. |
| V. Single source of truth, minimal surface | **PASS.** Reference and catalog updated with the change (`contracts/language.md` draft ~230 tokens, budget 400); one builtin; marked shortcuts: head-first draining (R5), sequential nesting (R7), per-task memory limit (R13), 64 connections (R14). |
| Implementation constraints | **PASS.** C only; no new assembly; system calls through the existing interface; no UB (pipe framing uses explicit lengths and bounds checks); strict warnings and the instrumented build cover `task.c` and the new serve loop; all three platforms gated. |
| Workflow and quality gates | **PASS.** Reference growth will be stated in the change; CLI and schema change (trace record) reviewed against I, II, V above. |

**Post-design re-check (after Phase 1)**: unchanged, all PASS. The design changed four spec lines
(FR-002 required limit, FR-004 processes, memory and nesting edge cases), each marked "amended in
planning" in the spec.

## Project Structure

### Documentation (this feature)

```text
specs/016-concurrency/
├── plan.md              # This file
├── research.md          # Phase 0: R1-R16
├── data-model.md        # Phase 1: task group, task, control messages, transfer form, server connection
├── quickstart.md        # Phase 1: validation scenarios
├── contracts/
│   ├── language.md      # the builtin, its semantics, the reference text
│   ├── codes.md         # E0314, E0511, R0015 and new triggers of existing codes
│   └── output.md        # output order, trace record, recordings, mem-stats, server behavior
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
src/
├── task.c            # NEW: concurrent_map: fork per task, pipes, head-first draining, failure rule,
│                     #      transfer of results, re-raise, nesting, replay hand-off
├── vo.h              # task globals and prototypes, BU id, SYS_* use
├── platform/*.h      # SYS_clone, SYS_wait4, SYS_pipe2, SYS_dup3, SYS_fcntl, SYS_prctl, SYS_kill, SYS_getppid
├── check_expr.c      # concurrent_map signature, E0314
├── check_effects.c   # f's effects as for map; E0511
├── rt_call.c         # BU_CONCURRENT_MAP dispatch
├── serve.c           # connection event loop, queued replies (R14)
├── replay.c          # recording writer to the control pipe; replay position hand-off (R10)
├── vm.c              # trace writer to the control pipe, `task` records, merged limit (R11)
├── rt_effects.c      # per-task seed (R12)
├── interp.c / main.c # child exit path: errors and exit() become control messages (R6)
└── rt_io.c           # flush buffered output before fork (R5)
docs/reference.md, docs/errors.md         # reference text, three catalog entries
std/http.vo                               # serve's doc comment only
examples/url_check/                       # sequential and concurrent versions (SC-005)
tests/cases/tasks/, tests/cases/tasks_steps/, tests/cases/serve_concurrent/
tests/http_server.py                      # GET /delay/MS
tests/tasks_determinism.py, tests/tasks_props.py
tests/cross_replay.py                     # a task recording
```

**Structure Decision**: the existing single-binary layout. The one new source file holds everything
specific to tasks; the other changes are hooks in the files that own the state (recording, trace,
seed, exit, serve).

## Complexity Tracking

No constitution violations. Deliberate shortcuts, each marked `ponytail:` in code:

| Shortcut | Ceiling | Upgrade path |
|---|---|---|
| Head-first draining (R5) | a non-head task that prints more than 1 MiB waits until it is the head | buffer non-head output in the parent |
| Nested calls run one task at a time (R7) | no concurrency inside tasks | a jobserver pipe inherited by children |
| Per-task `--max-memory` (R13) | total memory up to limit × 64 | a shared accounting page, if it matters |
| 64 connections, sequential handlers (R14) | a handler that waits delays the next one | concurrent handlers on copies of state (rejected for now in clarification Q2) |
| No closures in results (R4) | E0314 / R0015 | transfer closures once type arguments can be rebuilt in the parent |

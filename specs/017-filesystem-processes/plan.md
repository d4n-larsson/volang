# Implementation Plan: Filesystem and Processes

**Branch**: `017-filesystem-processes` | **Date**: 2026-11-16 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/017-filesystem-processes/spec.md`

## Summary

Two standard-library modules over two internal primitives, two new effects, and the security model
updated to match (research R1-R16):
- **`std.fs`** (R2, R5-R7): `info`, `list_dir`, `walk` under `file.read`; `append_file`, `make_dir`
  under `file.write`; `delete` under the new `file.delete`; `rename` under both. Every operation goes
  through spec 013's no-follow walk from the deepest granted scope to the parent directory and then the
  matching `*at` system call, so a link swapped in after the check never redirects it. Listings are
  sorted by bytes; failures are `err` values with fixed messages.
- **`std.process`** (R2, R8-R11): `run(program, args)` and `run_with(program, args, input, env)` run a
  program to completion with no shell, under the new `process` effect scoped to program names (exact
  text; bare names through `PATH`; unscoped is E0512). The child gets only its arguments, input and the
  given environment, in its own process group; SIGCHLD through a signalfd wakes the wait; `--timeout`
  and a 16 MiB output limit kill the group. A started program is not confined, and the reference and
  security.md say so.
- **Effects** (R3, R4): `file.delete` after `file.write`, `process` last; bits renumbered; literal
  targets of library calls checked at check time (E0503).
- **Record/replay, traces** (R12, R1): seven event kinds in recording schema 1; replay touches no file
  and starts no program. Library calls already produce trace records.
- **Roadmap** (FR-023): done in the spec phase (`docs/roadmap.md` "Planned after 017").

## Technical Context

**Language/Version**: C (C11, freestanding, the project's strict warning set) for the toolchain; vo for
`std/fs.vo`, `std/process.vo`, examples and tests.

**Primary Dependencies**: none new. New Linux system calls through the existing interface: `mkdirat`,
`execve`, `setpgid`, `fchdir` (R5, R9); `getdents64`, `unlinkat`, `renameat2`, `statx`, `clone`,
`wait4`, `pipe2`, `dup3`, `prctl`, `kill`, `signalfd4`, `rt_sigprocmask` already exist.

**Storage**: the file system under the grant's scopes; recordings gain event kinds within schema 1.

**Testing**: golden cases `tests/cases/fs/` and `tests/cases/process/` (every acceptance scenario, each
error message, E0512, E0506 and E0503 for the new scopes, R0010 for every operation, the child's
descriptors and environment); step cases `tests/cases/fs_steps/` and `tests/cases/process_steps/`
(scratch directories, link swaps (A4), `vo.effects` changes (US4), record and replay, timeouts,
grandchildren); `tests/fs_scale.py` (SC-004: 100,000 entries, 1,000 runs, no leaked processes or
descriptors); `tests/cross_replay.py` extended with a file-and-process recording; the determinism check
(20 runs of every new case); the full gate on three platforms, instrumented.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V 64 (native or qemu-user).

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: SC-004 (100,000-entry listing under 1 s; 1,000 trivial runs under 5 s, measured
and recorded with the fork cost); programs that use neither module: the speed gate stays under 1.10.

**Constraints**: Principle III (sorted listings, recorded inputs); Principle II (E0512 catalogued; new
messages fixed); reference +500 tokens at most (estimate 380, now about 11,393 of 15,000); no new
assembly; no change to existing programs except E0509's message (R3).

**Scale/Scope**: about 800 lines of C: `src/fs.c` new (~330: the walk split, operations, listing,
errno messages), `src/process.c` new (~250: resolution, fork/exec, wait loop), effects and checker
(~60), replay (~120), syscalls and builtin tables (~40); `std/fs.vo` (~120) and `std/process.vo` (~50);
1 code; ~50 golden cases, ~15 step cases, 1 test script, 1 example.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** No new syntax. Each operation has one function; `run` and `run_with` are two functions, not two spellings of one (the common call without input or environment, and the full one), since `[]` is E0306. No shell, so no quoting rules to learn. Failures are `result` values matched like every other `err`; `info` returns `missing` as a value. |
| II. Machine-readable feedback | **PASS.** E0512 gets a catalog entry, `vo explain` text and a golden case; the new error messages are fixed strings listed in the contract; R0010 reports for the new effects carry the same JSON fields (`effect`, `target`, `allowed`, `declared_by`). Recording schema 1 gains event kinds (additions only). E0508 stays unassigned. |
| III. Determinism and defined behavior | **PASS.** Listings sorted by bytes, non-UTF-8 names dropped (R6); every file answer, program output and exit code is an input recorded by `--record` and replayed without touching anything (R12); a child is bounded by `--timeout` and the output limit (R10); every errno maps to a documented message (R7). Starting a program that cannot run is an `err`, never a crash. |
| IV. Test-first through the CLI | **PASS.** Golden and step cases for every scenario, message and code land first and fail; the new cases join the determinism check; A4 link-swap cases for every changing operation (SC-003). |
| V. Single source of truth, minimal surface | **PASS.** Reference, catalog and contracts change together; two modules instead of eight builtins keep the reference cost to ~380 tokens (R16); marked shortcuts: plain fork (R9), escaped grandchildren (R10), non-UTF-8 names and outputs (R6, R11). |
| Implementation constraints | **PASS.** C only, no new assembly; the `*at` calls and the walk avoid check-then-use races; buffer lengths explicit (getdents, pipes, 16 MiB limits); strict warnings and the instrumented build cover the new files; all three platforms gated. |
| Workflow and quality gates | **PASS.** Reference growth stated in the change; the CLI-visible changes (two effects, E0512, E0509's text, recording kinds) reviewed against I, II, V above; security.md updated (R15). |

**Post-design re-check (after Phase 1)**: unchanged, all PASS. The design amends one spec line: FR-011
also allows absolute program paths (R3), marked "amended in planning" in the spec.

## Project Structure

### Documentation (this feature)

```text
specs/017-filesystem-processes/
├── plan.md              # This file
├── research.md          # Phase 0: R1-R16
├── data-model.md        # Phase 1: effects, scopes, entries, runs, events
├── quickstart.md        # Phase 1: validation scenarios
├── contracts/
│   ├── language.md      # std.fs, std.process, the effects, the reference text
│   ├── codes.md         # E0512, new triggers of E0503/E0506/R0010/R0007, E0509's message
│   └── recording.md     # the new event kinds and their replay
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
src/
├── fs.c              # NEW: rt_open_parent (split from rt_open_file), info, list_dir (getdents64,
│                     #      sort, encoding), append, make_dir, delete, rename, errno messages
├── process.c         # NEW: program check and PATH resolution, fork/exec with the error pipe, the
│                     #      signalfd/ppoll wait loop, limits, timeout, output encoding
├── rt_effects.c      # rt_open_file on top of rt_open_parent; strict-inside check for delete/rename
├── effects.c         # es_names (file.delete, process), process scope validation and normalization
├── check_effects.c   # builtin_effects for __fs/__run, E0512, the library literal-target table (R4)
├── check_expr.c      # __fs, __run names and signatures (std only)
├── rt_call.c         # dispatch of the two primitives through io_* wrappers
├── replay.c          # seven event kinds: record, replay, mismatch texts (R12)
├── vo.h              # EF_* renumbered, EF_FDELETE, EF_PROCESS, EVK_*, prototypes
└── platform/*.h      # SYS_mkdirat, SYS_execve, SYS_setpgid, SYS_fchdir
std/fs.vo, std/process.vo                 # NEW modules (R2)
docs/reference.md, docs/errors.md         # reference text (contracts/language.md), E0512, E0509
docs/security.md                          # R15, including the stale spec 016 lines
site/docs/guide/files-and-processes.md    # NEW guide page; tools/site.py GUIDE list
examples/release-check/                   # NEW: runs git and a test command, writes a report (SC-002)
tests/cases/fs/, tests/cases/process/ (with bin/ helper scripts), tests/cases/fs_steps/,
tests/cases/process_steps/
tests/steps.py                            # actions: make a tree, swap a link, list processes
tests/fs_scale.py                         # SC-004
tests/cross_replay.py, tests/catalog.py   # the new recording; 017's codes
```

**Structure Decision**: the existing single-binary layout. File operations and process runs each get
one new C file next to the runtime they extend; the vo-visible API lives in two library modules like
`std.http`.

## Complexity Tracking

No constitution violations. Deliberate shortcuts, each marked `ponytail:` in code:

| Shortcut | Ceiling | Upgrade path |
|---|---|---|
| Plain fork per run (R9) | page-table copy grows with the heap | `CLONE_VM`/`CLONE_VFORK` if SC-004's measurement or a large-heap case needs it |
| Group kill only (R10) | a `setsid` grandchild, or any grandchild after `vo` is SIGKILLed, survives | a PID namespace or cgroup when confinement (roadmap) lands |
| Non-UTF-8 names dropped, non-UTF-8 output an `err` (R6, R11) | binary names and output unusable | the `bytes` type (roadmap) |
| Children not confined (clarified Q1) | a granted program has the user's rights | opt-in confinement with Landlock (roadmap) |
| No modification times, modes, links, copy, recursive delete (spec assumptions) | build tools that compare times | `info` gains a field in a later spec |

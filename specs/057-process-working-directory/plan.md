# Implementation Plan: Process Working Directory

**Branch**: `057-process-working-directory` (work on `main`, as for earlier specs) | **Date**: 2026-11-25 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/057-process-working-directory/spec.md`

## Summary

`run_with`, `run_with_bytes` and `run_lines` take a working directory after the variables (decided with
the user). The directory must lie inside a `file.read` scope: R0010 at run time, E0503 for a literal at
check time (R5). The parent opens the checked directory without following links and the child enters it
with `fchdir` before `execve` (R1). A relative program path still resolves from the start directory (R2).
Directory errors have their own texts (R3). Recordings carry the directory as written, only when it is
not `"./"`, so 0.2 recordings still replay (R7).

Existing calls migrate by fixes alone: a new change-table kind gives E0214 with a fix inserting `"./"`
(R4), and E0501's fix now scopes a file effect whose needs are all literal targets, so the migrated
program gets `file.read("./")`, not an unscoped `file.read` (R6). Version 0.3.0.

| Part | Files | What |
|---|---|---|
| Run with a directory | `src/process.c` (`run_check`, `rt_run`, `io_proc_start`, `child`), `src/vo.h`, the `__run`/`__proc_start` builtins (`src/check_expr.c` names, their runtime entry) | R1 to R3 |
| Library | `std/process.vo` (the three signatures, `run` and `run_bytes` pass `"./"`, docs and examples) | R4 |
| Check time | `src/check_effects.c` (`std_targets` rows; E0501 fix scoping in `fx_report` with `ck_lits`) | R5, R6 |
| Migration | `src/libchange.c` (`LC_ARG`, three rows, message), the generic call hook in `src/check_generic.c` or `src/check_expr.c` | R4 |
| Recording | `src/replay.c` (`"dir"` written, read, matched; R0014 text) | R7 |
| Callers | `tools/vo_fix.vo` (entry directory + base name), `tools/progress.vo`, `tests/cases/**` callers (by the fixes) | SC-001, SC-002 |
| Docs | `docs/reference.md` (R8), `docs/errors.md` (E0214 rows, E0501 fix note), `CHANGELOG.md` (0.3.0, the E0503 manual step), `docs/stability.md` (third migration), `site/docs` pages on std.process | FR-010 |
| Tests | `tests/cases/process_steps/dir*/` (new), `tests/cases/process/dir_*.vo` (new, check-time), `tests/cases/library/process_dir*.vo` (E0214), E0501 goldens whose fix changes, `tests/libchange_rows.py` (`LC_ARG` template) | SC-001 to SC-006 |

## Technical Context

**Language/Version**: C11 freestanding (toolchain); vo (std, tools); Python 3 (tests).

**Primary Dependencies**: none new.

**Storage**: recordings (JSON lines): one optional field added to run events.

**Testing**:
- Golden cases first (constitution IV): `process_steps` cases for the run, the errors and record/replay;
  check cases for E0214, E0503 and the scoped E0501 fix.
- `tests/libchange_rows.py`, `tests/migrate_056.py` and `tests/apply_fixes.py` as regression checks.
- `make quick AREA=fs` between edits; `make test` once at the end; the order of checks and errors on
  three architectures through the gate when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64 (`fchdir` on an `O_PATH` descriptor under
`qemu-riscv64`/`qemu-aarch64` user mode is verified in the first task).

**Project Type**: compiler toolchain and standard library.

**Performance Goals**: no measurable change: one `openat` per component of the directory and one `fchdir`
per run with a directory other than `"./"`; none for `"./"`.

**Constraints**:
- No new syntax, no new diagnostic code (E0214, E0501, E0503, R0007, R0010, R0014 reused).
- JSON schema 1 unchanged; the recording format only gains an optional field.
- The reference stays within 15,000 tokens (~14,990 now): net about -3 words (R8).
- `vo` never changes its own working directory.

**Scale/Scope**: 3 std functions, 3 change-table rows, 26 existing calls migrated, about 150 lines of C
(process about 60, E0501 scoping about 50, `LC_ARG` about 40), about 12 golden cases, up to 27 E0501
golden expectations re-recorded (measured).

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One way to run in a directory (a parameter, no new names); the directory's errors are distinct texts an agent can match; a relative program path means what it meant before. | Pass |
| II. Machine-readable feedback | No new code; E0214 gets rows with a fix; E0501's fix becomes narrower (code and span unchanged, replacement text changes for literal-only needs, listed in the changelog). | Pass |
| III. Determinism | The directory is recorded and matched on replay; the order of checks is fixed (data-model.md); output identical on three architectures. | Pass |
| IV. Test-first | Golden cases for each scenario and error before the code; migration proven by the fixes on the 0.2 callers. | Pass |
| V. Minimal surface | No syntax, no new functions; reference net -3 words (R8). Shortcuts: none planned (the E0503 fix is left out on evidence, R6). | Pass |
| VI. Backend network services | Write: workers and integrations run converters, `git` or `make` in a job's directory. Review: where programs run is visible in the effects (`file.read` scopes), and migration does not widen grants. Operate: recordings hold the directory, so an incident replays exactly. | Pass |

Re-checked after Phase 1: unchanged. The breaking change is allowed before 1.0 under `docs/stability.md`
(E0214 with a fix; fixes alone migrate).

## Project Structure

### Documentation (this feature)

```text
specs/057-process-working-directory/
├── spec.md
├── plan.md              # this file
├── research.md          # R1 to R9
├── data-model.md        # run, run event, change row, E0501 fix scope
├── quickstart.md        # validation scenarios
├── contracts/api.md     # signatures, results, diagnostics, recording, reference text
├── checklists/requirements.md
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/process.c            # directory check, open, fchdir, errors, program path join
src/check_effects.c      # std_targets rows; E0501 fix scoping
src/libchange.c          # LC_ARG kind and rows
src/replay.c             # "dir" in run events
std/process.vo           # signatures, docs, examples
tools/vo_fix.vo          # runs vo in the entry's directory
tools/progress.vo        # new call forms
tests/cases/process_steps/, tests/cases/process/, tests/cases/library/
docs/reference.md, docs/errors.md, CHANGELOG.md, docs/stability.md
```

**Structure Decision**: the existing toolchain layout; no new source files.

## Complexity Tracking

None: no principle is violated. The one choice beyond the minimum, scoping E0501's fix (R6), is what the
spec requires for migration without widened grants; the simpler alternative (keep the unscoped fix) was
rejected because applying fixes would give every migrated program read access to every file.

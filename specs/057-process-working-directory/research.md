# Research: Process Working Directory

Spec: [spec.md](spec.md). Facts below were read from the code on 2026-11-25 (`src/process.c`,
`src/rt_effects.c`, `src/check_effects.c`, `src/libchange.c`, `src/replay.c`) and measured where stated.

## R1. Where the child enters the directory

**Decision**: the parent opens the checked directory as an `O_PATH | O_DIRECTORY` descriptor through
`rt_open_parent(EF_FREAD, dir, ...)` plus one `openat(..., O_NOFOLLOW)` of the last component, the way
`std.fs` reaches a checked path (spec 013 R2/H3/H4). The child calls `fchdir` on it in `child()`, after
`setpgid` and before `close_range`, then `execve`.

**Rationale**: the open walks from the deepest granted scope one component at a time without following
links, so a directory swapped for a link after the check fails instead of escaping. `vo` itself never
changes its working directory (the header of `src/process.c` and the grant code rely on that). Linux
accepts `fchdir` on an `O_PATH` descriptor.

**Alternatives considered**: `chdir(path)` in the child (a race between the check and the change, and it
follows links); `execveat` with a directory descriptor (it sets where the program file is found, not the
working directory).

## R2. The program path does not move with the directory

**Decision**: a program written with a `/` and not starting with `/` (`tools/gen`) is joined to the start
directory (`rt_start_cwd()`) before the fork when a directory other than `"./"` is given; `argv[0]` stays
as written. Bare names keep the `PATH` lookup of the vo process. The `process` scope check is unchanged
(`run_check`, exact match of the name as written).

**Rationale**: FR-004; without the join, `execve("tools/gen")` after `fchdir` would run `work/tools/gen`.

## R3. Errors

**Decision**:

| Case | Where | Result |
|---|---|---|
| directory outside every `file.read` scope on the call path | `rt_check_path(EF_FREAD, dir)` before anything | R0010 (exit 70) |
| empty directory, or one holding a NUL byte | argument check, as for the program | R0007 |
| missing (`ENOENT` from the open) | parent | `err("directory not found")` |
| not a directory (`ENOTDIR`) | parent | `err("not a directory")` |
| cannot be entered (`EACCES` from `fchdir`) | child, through the error pipe | `err("directory not accessible")` |

The child reports an `fchdir` failure on the error pipe as `1000 + errno`, so the parent tells it apart
from an `execve` failure (whose `ENOENT` stays the program's `err("not found")`).

**Rationale**: FR-005. Grant first, then the file system, as `std.fs` orders them. "not found" alone
already means a missing program.

## R4. API and migration (decided with the user)

**Decision**: `run_with(program, arguments, input, vars, dir)`, `run_with_bytes(program, arguments,
input, vars, dir)`, `run_lines(program, arguments, input, vars, dir, f)`. `run` and `run_bytes` pass
`"./"`. The three `std` functions declare `uses process, file.read`.

A new change-table kind, `LC_ARG` in `src/libchange.c`: a call with one argument fewer than the function
takes, whose arguments check against the old parameters, gets E0214 ("`std.process.run_with` takes a
working directory after the variables since vo 0.3.0: `"./"` is the start directory.") with one fix
inserting `, "./"` after the variables. It is built like `libchange_swap_call` (non-generic calls) and
hooked into the generic call check for `run_lines`. Rows: `run_with`, `run_with_bytes`, `run_lines`.
`tests/libchange_rows.py` gets a template for the kind.

**Alternatives considered** (with the user): new `run_in` functions (three more names, two ways to run in
the start directory); an options record (largest break, a new type).

## R5. The grant, at check time and at run time

**Decision**:
- Run time: `rt_check_path(EF_FREAD, dir)` in `run_check` (R3).
- Check time: `std_targets` in `src/check_effects.c` gets `{run_with, 4, EF_FREAD}`, `{run_with_bytes, 4,
  EF_FREAD}`, `{run_lines, 4, EF_FREAD}`, so a literal directory outside the declared scopes is E0503.
  The same table lacks the `EF_PROCESS` rows for `run_bytes`, `run_with_bytes` and `run_lines` (a literal
  program outside `process` scopes is caught only at run time for them); they are added too.

## R6. Migration without widening a grant

**Finding**: 25 of the 26 existing calls are in functions that declare no `file.read` (the tests'
`process_steps`, `process`, `bytes_steps`, `effect_sets` cases; `tools/vo_fix.vo` declares it unscoped).
After E0214's fix they need `file.read`, and E0501's fix today adds it unscoped (`fx_report`:
`es_union(declared, required)`, where a std callee contributes `direct->any`). Applying fixes would give
every migrated program read access to every file.

**Decision**: E0501's fix scopes a file effect (`file.read`, `file.write`, `file.delete`) when every need
for it in the function comes from literal targets of std functions and builtins (the calls `ck_lits`
already records): the fix adds those targets as scopes, a file target as its directory (`read_file("data/a.txt")`
→ `file.read("data/")`), a directory target as itself (`run_with(..., "./")` → `file.read("./")`,
`list_dir("logs/")` → `file.read("logs/")`). Any other need for the effect keeps today's unscoped fix.

**Rationale**: the spec (US3 scenario 2) requires fixes alone to migrate without a manual step, and least
privilege is what a reviewer wants from a mechanical fix. It also improves every new program that reads a
literal path.

**Cost**: golden cases whose E0501 fix comes from literal-only needs change their expected fix (at most the
27 cases with an E0501; the count is measured when the change is made and listed in results.md). The
fix's code and span do not change, only its replacement text.

**Not done**: a fix for E0503 (a caller whose scopes exist but miss the start directory). The repository
has no such caller once `tools/progress.vo` is written in the new form; the changelog states the manual
step (add `"./"` or the directory to the `file.read` scopes).

## R7. Recording and replay

**Decision**: a run event gets a `"dir"` field holding the directory as written, only when it is not
`"./"`. Reading a recording, a missing `"dir"` means `"./"`. Replay matches it like the arguments
(`io_replay`: program, arguments joined, and now the directory), and a mismatch is R0014 naming the run
with its directory. The `to` slot of `io_record`/`io_replay` (used by rename only) carries the directory
for `EVK_RUN`.

**Rationale**: FR-007. As written rather than resolved, so a recording replays in another checkout; and
a `"./"` run records exactly as in 0.2, so old recordings replay against migrated programs.

## R8. Reference budget

**Measured**: `tests/ref_examples.py`: ~14,990 of 15,000 tokens.

**Decision**: the reference's `std.process` paragraph says "`run_with` adds input text, `NAME=value`
variables and a working directory (inside a `file.read` scope)" (+9 words). Paid for by the effects table
row (`std.process.run` and `run_with` → `std.process`, -3 words) and by moving the sentence on `128 + n`
signal codes to `vo doc` (it is in `Output`'s field documentation already, -9 words). Net: about -3 words.

## R9. Per-call time limits (out of scope)

**Verified**: `with_deadline(300, fn() -> result[Output] { return run("sleep", ["7"]); })` returned
`err(timed_out)` after 0.31 s and no `sleep` process was left. Nothing to add.

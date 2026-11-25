# Quickstart: Process Working Directory

Validation scenarios for spec 057. The surface is in [contracts/api.md](contracts/api.md); the run and its
checks in [data-model.md](data-model.md).

## Prerequisites

`make` (builds `build/vo`); Python 3 for the scripts.

## 1. A program runs where it is told (US1)

The golden cases under `tests/cases/process_steps/dir/` run a helper that prints its working directory and
lists a relative file, in a subdirectory and in `"./"`, and a relative program path (`bin/...`) with a
directory. `make quick AREA=fs` runs them.

Expected: the subdirectory run lists the subdirectory's file; the `"./"` run is identical to a 0.2 run;
the relative program is the start directory's file.

## 2. A wrong directory never starts anything (US2, SC-003)

Golden cases for a missing directory, a regular file as the directory, a directory without search
permission, a directory outside the `file.read` scopes (R0010), and a literal outside them at check time
(E0503).

Expected: the three `err` texts of contracts/api.md, R0010 and E0503; no helper output in any of them.

## 3. Old calls migrate by fixes (US3, SC-002)

```sh
git stash   # or check out the commit before this spec into a temporary directory
bin/vo run --timeout 0 tools/vo_fix.vo -- tests/cases/process_steps tests/cases/process
```

On the 0.2 forms of the callers: E0214 on each call, then E0501 with `file.read("./")`; after the fixes
every program checks clean and its golden output is unchanged. `python3 tests/libchange_rows.py` covers the
three new rows.

## 4. Recordings hold the directory (US4, SC-004)

Golden steps cases beside `tests/cases/process_steps/record_replay` and `replay_mismatch`: record a run in `work/`, replay it (identical output, nothing started),
then replay a changed program that runs in `other/` (R0014). A 0.2 recording of a `run_with` call replays
against the migrated program.

## 5. vo_fix runs in the entry's directory (SC-001)

`tools/vo_fix.vo` passes each entry's directory and its base name; then:

```sh
python3 tests/libchange_rows.py && python3 tests/migrate_056.py
```

Expected: both pass, and "left:" lines name files relative to the entry, as `tools/vo_fix.py` did.

## 6. Determinism and budget (SC-005, SC-006)

`make test` on x86-64, and the gate's ARM64 and RISC-V runs, give identical golden output.
`python3 tests/ref_examples.py` reports at most 15,000 tokens.

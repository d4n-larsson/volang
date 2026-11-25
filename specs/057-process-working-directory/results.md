# Results: Process Working Directory

`make test` passed on x86-64 (60 steps, 13m25s, through `tools/progress.vo`). Earlier runs failed on: the
`cli/version_json` golden (version not bumped there), a doc comment on the private `dir_argv`
(doc conformance), recording fingerprints of three step cases (std changed), the inline programs of
`tests/stream.py` (see Deviations), and once on `stream.py`'s "files in under 10 s" timing while the
machine was loaded by other processes (it passed alone and in the final run).

## Setup

- T002: `tests/cases/process_steps/dir_replay_02/r02.jsonl` was recorded with vo 0.2.0 at commit e94547a
  (`bin/vo run --seed 1 --record r.jsonl main.vo`, the 0.2 form `run_with("bin/cat_file", ["a.txt"], "", [])`).

## SC-001

`tools/vo_fix.vo` runs `bin/vo check --json BASE` and `vo effects --update BASE` with the entry's
directory as the working directory (relative program path `bin/vo` still found from the repository root),
and joins the diagnostics' file names to that directory. `tests/libchange_rows.py` (52 rows, 0 problems)
and `tests/migrate_056.py` (11 programs, 0 problems) pass through it.

## SC-002

The 25 calls of the 0.2 forms in the repository (11 case files and `tools/progress.vo`) were migrated
with `tools/vo_fix.vo --only E0214,E0501` alone, then `vo fmt` (5 files re-wrapped). The diff is the
inserted `"./"` and `file.read("./")` only. Expected outputs that changed: the line numbers and trace
shape of `std/process.vo` in runtime reports (`process/bad_args`, `bad_env_eq`, `bad_env_name`,
`exact_text`, `process_steps/replay_mismatch`, `timeout`, `fs_steps/trace`), a recording fingerprint
(`fs_steps/record_replay`: std changed), and the effects named in one R0010 message
(`process_steps/stream`: `main` now declares `file.read("./")`). No program output changed.
`tests/apply_fixes.py` (217/217) and `tests/migrate.py` (61/61) pass.

## SC-003

`process_steps/dir_errors` (missing, a file, mode 000), `dir_outside` (R0010), `process/dir_literal_outside`
(E0503), `process/dir_empty` (R0007): no helper output in any. Not verified: running as root, where a mode
000 directory can be entered (the `locked/` line would then show the file).

## SC-004

`process_steps/dir_replay` (recorded `"dir":"work/"`, replayed with the file gone), `dir_replay_mismatch`
(R0014: "runs `bin/cat_file a.txt in other/`, but the recording has a run of `bin/cat_file a.txt in
work/`"), `dir_replay_02` (the vo 0.2.0 recording replays against the migrated program, and a fresh
recording of it equals the 0.2 one apart from the header line). `tests/cross_replay.py`: 6 pairs of the
three builds, all identical.

## SC-005

The eight directory step cases (and `process_steps/stream`) pass unchanged with `VO_BIN` set to
`build-aarch64/vo` and `build-riscv64/vo` (qemu user mode): `fchdir` on an `O_PATH` descriptor works there,
so T012's fallback was not needed. The full gate was not run (only when asked).

## SC-006

See "Reference cost".

## Reference cost

`tests/ref_examples.py`: ~14,990 tokens before, ~14,987 after (budget 15,000). Added: "a working
directory (`"./"`, or inside a `file.read` scope)"; removed: "killed by signal n, `code` is 128 + n" (kept
in `Output`'s `vo doc`) and "`run` and `run_with`" in the effects table.

## E0501 goldens changed

Of the 30 golden cases that expect an E0501, three changed their fix's replacement (code, span and message
unchanged); every other one has a need that is not a literal path and keeps the unscoped fix.

| Case | Before | After |
|---|---|---|
| `tests/cases/fs/missing_effect` (`list_dir("data")`) | `uses console, file.read` | `uses console, file.read("data/")` |
| `tests/cases/fs/delete_needs_effect` (`delete("out/old.txt")`) | `uses console, file.write("out/"), file.delete` | `... file.delete("out/")` |
| `tests/cases/effects/transitive_full_set` (`write_file("out.txt", ...)` two calls deep) | `uses console, file.write` (b, a, main) | `uses console, file.write("./")` (b, a, main) |

## Deviations

- **`"./"` is always allowed (FR-006).** The spec required every directory to lie inside a `file.read`
  scope. Implemented: `"./"` (the start directory) needs no scope, at run time and at check time, so
  `run_with(p, a, "", [], "./")` behaves exactly as `run(p, a)`, which starts there without `file.read`.
  Without it, a caller with only `file.read("data/")` could `run` a program but not `run_with` it in the
  same place. It also removes the spec's manual E0503 step: no migrated caller can fail on `"./"`. The
  three functions still declare `file.read` (any other directory needs it), so migrated callers get
  `file.read("./")` from E0501's fix.
- **The run builtins need `file.read` by their argument.** `__run`, `__run_bytes` and `__proc_start` need
  `file.read` unless their argument list is built with the literal `"./"` (`fx_builtin_run`, as
  `__sql_open`'s effects depend on its mode), so `run` and `run_bytes` keep `uses process` only.
- **E0501's literal scopes travel along calls.** Scoping the fix per function (as planned) left callers
  with an unscoped fix in the same round; the scopes are propagated over the file's call edges, so a
  caller's fix declares its callees' literal scopes.
- **`tools/progress.vo` keeps `file.read("build/")`** (T032 planned `file.read("./")`): with `"./"` always
  allowed, the narrower scope suffices.
- **`process_steps/stream/outside.vo`** now builds its program name at run time: with the new
  `run_lines` program row (T007), a literal one is E0503 at check time, and the case exists to show R0010
  at run time. `process/literal_outside.vo` gained a `run_bytes` line for the check-time rows.
- **`tests/stream.py` edited by hand**: its two inline programs (Python strings, where no fix applies)
  got exactly what the fixes give: `"./"` after the variables and `file.read("./")`.
- **The site check's fresh-clone step** builds the committed HEAD, so it reports `vo 0.2.0` until this
  change is committed; every other site check passes.

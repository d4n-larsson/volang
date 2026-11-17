---

description: "Task list for vo-lang spec 017: filesystem and processes"
---

# Tasks: Filesystem and Processes

**Input**: Design documents from `specs/017-filesystem-processes/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R16), data-model.md, contracts/language.md,
contracts/codes.md, contracts/recording.md, quickstart.md

**Tests**: REQUIRED (constitution IV): golden and step cases land first and fail before each piece.

**Organization**: setup, the effects and primitives every story needs, then one phase per user story
(US1-US3 are P1, US4-US5 P2), then docs and the gate. After Phase 2, US1/US2 (`src/fs.c`, `std/fs.vo`)
and US3 (`src/process.c`, `std/process.vo`) touch different files and can proceed in parallel; US2
builds on US1's `src/fs.c`.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- Golden cases (`.vo` + `.out`/`.err`/`.code`, optional `.cmd`) run in an empty directory
  (`tests/run.sh`), so they cover what needs no files: check-time codes, `info` of a missing path,
  programs found through `PATH`. Anything with files, helpers or several commands is a step case
  (`tests/cases/<area>/<name>/steps`, format in `tests/steps.py`): the case folder is copied to a
  temporary directory first.
- Areas: `tests/cases/fs/`, `tests/cases/fs_steps/`, `tests/cases/process/`,
  `tests/cases/process_steps/`.
- **Regression rule:** existing programs keep byte-identical results, except E0509's message (R3);
  `make test` after every task.
- Every new C line: zero warnings under the strict set, no UB (explicit lengths on every getdents,
  pipe and statx buffer), syscalls only through `vo_syscall*`; shortcuts marked `ponytail:` (plan.md,
  Complexity Tracking).

---

## Phase 1: Setup

- [X] T001 Create `specs/017-filesystem-processes/results.md` (SC-001 to SC-007 table, verification
  table, behavior changes, deviations, as in `specs/016-concurrency/results.md`); add
  `017-filesystem-processes` to the spec list in `tests/catalog.py` so `contracts/codes.md` is checked;
  create `tests/cases/fs/`, `tests/cases/fs_steps/`, `tests/cases/process/`,
  `tests/cases/process_steps/`.
- [X] T002 [P] System calls (research R5, R9): define `SYS_mkdirat` (x86-64 258, generic 34),
  `SYS_execve` (59, 221), `SYS_setpgid` (109, 154), `SYS_fchdir` (81, 50) in `src/platform/x86_64.h`,
  `src/platform/aarch64.h`, `src/platform/riscv64.h`; check that `SYS_getdents64`, `SYS_unlinkat`,
  `SYS_renameat2`, `SYS_statx`, `SYS_signalfd4`, `SYS_rt_sigprocmask` exist in all three (they do in
  x86_64.h).
- [X] T003 [P] Test helpers (research R14): `tests/process_helpers/` with `#!/bin/sh` scripts, each
  executable and documented in a header comment: `args` (prints each argument on its own line between
  `[` `]`), `cat_in` (copies stdin to stdout), `exit_with N`, `kill_self SIG`, `sleep_for S`, `flood`
  (writes 17 MiB to stdout), `bad_utf8` (writes bytes `\377\376`), `to_stderr TEXT`, `grandchild`
  (starts `sleep 30` in the background holding stdout open, then exits 0), `fds` (lists
  `/proc/self/fd` entries other than 0-2 and its own `ls` descriptor), `env_dump` (prints its
  environment sorted). Add a step action `helpers` to `tests/steps.py` (copies
  `tests/process_helpers/*` into `./bin/` of the case directory, keeping the mode) and list it in the
  module docstring.

---

## Phase 2: Foundational — effects and primitives (blocks every story)

**Purpose**: the two new effects exist and check as data-model.md says; `__fs` and `__run` exist as
std-only primitives that, for now, return `err("not supported")`; the scoped walk is split out.

- [X] T004 [P] Effect cases, confirmed failing: `tests/cases/effects/delete_scope.vo` (`uses
  file.delete("tmp/")` checks; `file.delete("/abs")`, `file.delete("../x")` and `file.delete("")` are
  E0506); `tests/cases/effects/process_scope.vo` (`process("git")`, `process("tools/gen")`,
  `process("./tools/gen")` (normalized to `tools/gen` in `vo effects` output), `process("/usr/bin/git")`
  check; `process("../x")`, `process("bin/")`, `process("")` are E0506 with the program message of
  contracts/codes.md); `tests/cases/effects/process_unscoped.vo` (`fn main() uses process {}`: E0512,
  exit 65); `tests/cases/effects/order.vo` (`vo fmt` sorts `uses process("a"), file.delete("t/"),
  console, file.write("t/")` to `console, file.write("t/"), file.delete("t/"), process("a")`); update
  the expected output of every existing E0509 case (`grep -rl E0509 tests/cases`) to the eleven-effect
  message.
- [X] T005 Effect bits (research R3): in `src/vo.h` renumber to `EF_FDELETE` 16, `EF_NET` 32,
  `EF_CLOCK` 64, `EF_RANDOM` 128, `EF_ENV` 256, `EF_SERVE` 512, `EF_PROCESS` 1024, `EF_ALL` 2047;
  in `src/effects.c` insert `"file.delete"` after `"file.write"` and append `"process"` in `es_names`;
  normalize `file.delete` scopes like `file.write` (`bit & (EF_FREAD | EF_FWRITE | EF_FDELETE)`) and
  validate `process` scopes ("non-empty; no NUL; no `..` component; no trailing `/`; a leading `./` is
  dropped"); grep for any other code that assumes bit values or `EF_ALL == 511` (`src/check_effects.c`,
  `src/effects_record.c`, `src/fmt.c`, `src/doc.c`, `src/query.c`) and fix it. E0509's message in
  `src/check_effects.c` lists the eleven names. Confirm `make test` shows only the E0509 changes.
- [X] T006 E0512 (research R3): in `fx_build_set` (`src/check_effects.c`) report E0512 with
  "`process` needs the programs it may run: write `process(\"NAME\")`." when `process` has no scopes and
  the declaring file is not a standard-library file (`file_get(...)->std`); add the code to the code
  table (`src/diag.c` or wherever names are mapped) and `vo explain E0512` text; T004's cases pass.
- [X] T007 Primitives (research R1): add `__fs` (signature `(op: int, a: str, b: str) ->
  result[str]`, a new signature code if needed) and `__run` (`(program: str, args: list[str], input:
  str, env: list[str]) -> result[str]`) to `builtin_names`/`builtin_sigs` in `src/check_expr.c` (std
  only, like `__http_exchange`; `B_COUNT`/`B_LAST` and the `builtin_effects[64]` array grow as needed);
  `builtin_effects`: `__fs` 0 (its effect depends on `op`; the library function's declaration carries
  it), `__run` `EF_PROCESS`; dispatch both in `src/rt_call.c` to `io_fs(op, a, b)` and
  `io_run(program, args, input, env)` (declared in `src/vo.h`), which for now return
  `err("not supported")`.
- [X] T008 Split the scoped walk (research R5): in `src/rt_effects.c` factor `rt_open_file` into
  `long rt_open_parent(long bit, Str *path, Str **name, int strict)` (descriptor of the resolved path's
  parent reached with the O_NOFOLLOW walk from the deepest granted scope directory; `*name` the last
  component; `strict` refuses the scope directory itself with R0010) and the open on top of it;
  `read_file`/`write_file` behavior must stay identical (all spec 013 `tests/cases/security/` cases and
  `make security-check` pass).

**Checkpoint**: `make test` passes (with the updated E0509 outputs); the new effects check; the
primitives are reachable only from `std/`.

---

## Phase 3: User Story 1 — Work with the files in a directory (P1) 🎯 MVP

**Goal**: `std.fs.info`, `list_dir` and `walk` under `file.read`.

**Independent test**: step cases over fixture trees print listings, kinds and sizes identically on
every run; paths outside the scope are refused.

### Tests (first, failing)

- [X] T009 [P] [US1] Golden cases in `tests/cases/fs/`: `info_missing.vo` (`info("none.txt")` with
  `file.read("./")` prints `missing`); `literal_outside.vo` (`list_dir("secrets")` with
  `file.read("data/")`: E0503 at check time, also for `info` and `walk`); `missing_effect.vo` (calling
  `list_dir` without `file.read`: E0501 with the fix).
- [X] T010 [P] [US1] Step cases in `tests/cases/fs_steps/`, each with a fixture `data/` folder:
  `list` (files `b.txt`, `a.txt`, `B.txt`, a directory `old/`: entries in byte order `B.txt`, `a.txt`,
  `b.txt`, `old` with kinds and sizes; no `.`/`..`; the same output twice); `info` (file with size,
  directory, missing, a fifo made with `mkfifo` is `other`); `links` (link to a file inside: `file`,
  `link: true`; link to a directory inside: `directory`; link leading outside and a dangling link:
  `other`; reading through the outside link with `read_file` is R0010); `walk` (a tree three levels
  deep with a link to a directory inside: every file once, sorted, the link not descended);
  `outside` (computed paths `data/../main.vo` and an absolute path: R0010, exit 70, report naming
  `file.read`); `errors` (listing a file: `err("not a directory")`; listing a directory made
  unreadable with `chmod 000`: `err("permission denied")`, skipped when the tests run as root);
  `bad_name` (a file whose name has byte `\xff`, made by a step action, is left out; the others are
  listed).

### Implementation

- [X] T011 [US1] `src/fs.c` (new): the errno → message table of contracts/codes.md;
  `info` (op 1: `rt_check_path(EF_FREAD)`, `rt_open_parent`, `statx(parent, name,
  AT_SYMLINK_NOFOLLOW, STATX_TYPE | STATX_SIZE)`; `ENOENT`/`ENOTDIR` → `ok("m")`, else `f<size>`, `d`,
  `o`); `list_dir` (op 2: open the directory through the walk with `O_DIRECTORY | O_NOFOLLOW`,
  `getdents64` with a 64 KiB buffer until 0, drop `.`/`..` and names failing `utf8_validate`, kind from
  `d_type` (`statx` for `DT_UNKNOWN` and for file sizes), links: `rt_resolve` the entry and, when the
  result lies under a `file.read` scope of the running grant (non-raising check), the target's kind
  through the walk, else `o`; merge sort by name bytes; encode "entries joined by `/`; each
  `<kind>[l] <name>`"). Wire `io_fs` in `src/replay.c` (or `src/fs.c`) to call them with no recording
  yet (US5). Add `src/fs.c` to the build in `Makefile`.
- [X] T012 [US1] `std/fs.vo` (new) with the header comment of the other modules, `Info`, `Entry`,
  `info`, `list_dir` (decodes the text), `walk` (recursion over `list_dir`, files only, not descending
  into `link: true` entries, sorted, paths `dir + "/" + relative`), each with a doc comment and an
  example line that `vo doc` and the doc conformance check run; embed the module like the others
  (wherever `std/*.vo` are listed: `Makefile`, `src/loader.c` or `src/stdprim.c`).
- [X] T013 [US1] Literal targets (research R4): in `fx_call` (`src/check_effects.c`) a table of
  `std/fs.vo` functions → (argument index, bit): `info`, `list_dir`, `walk` (0, `EF_FREAD`); make
  `fx_literal` take the argument index; T009's E0503 case passes.

**Checkpoint**: T009-T010 pass; `vo doc std.fs` lists the three functions.

---

## Phase 4: User Story 2 — Change files (P1)

**Goal**: `append_file`, `make_dir` (file.write), `delete` (file.delete), `rename` (both).

**Independent test**: step cases in a scratch directory print the listing and contents after each
operation; every refusal is checked separately; link swaps never escape.

### Tests (first, failing)

- [X] T014 [P] [US2] Golden cases in `tests/cases/fs/`: `delete_needs_effect.vo` (`delete` with only
  `file.write("out/")`: E0501 naming `file.delete`); `rename_literals.vo` (`rename("in/a", "out/a")`
  with `file.write("out/"), file.delete("out/")`: E0503 on the first argument); `bad_path.vo`
  (`delete("")`: R0007).
- [X] T015 [P] [US2] Step cases in `tests/cases/fs_steps/`: `append` (twice to an existing file, once
  to a missing one; `keep` the files); `make_dir` (`out/2026-11/` with parents when `out/` is missing;
  again: ok; where a file is in the way: `err("not a directory")` or `err("already exists")` as the
  errno gives, documented in the expected output); `rename` (inbox → done; onto an existing name:
  `err("already exists")` and both files unchanged; renaming the scope directory itself: R0010);
  `delete` (a file, an empty directory, a link (its target stays); a non-empty directory:
  `err("not empty")`; missing: `err("not found")`; the scope directory itself: R0010); `outside`
  (append, make_dir, delete, rename with computed paths outside the scope: R0010 each, nothing
  changed: `keep` shows the tree).
- [X] T016 [P] [US2] `tests/security/fs_race.py` (A4, SC-003), modeled on `tests/security/race.py`:
  while a helper thread flips a directory inside the scope between a real directory and a link to an
  outside directory, a vo program runs `append_file`, `make_dir`, `delete`, `rename` and `list_dir`
  in a loop for RUNS runs (default 300); exit 1 if anything outside the scope was created, changed,
  deleted or listed. Run by `make security-check`.

### Implementation

- [X] T017 [US2] In `src/fs.c`: `append_file` (op 3: `openat(parent, name, O_WRONLY | O_APPEND |
  O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0644)`, write all of `text`), `make_dir` (op 4: each missing
  component below the scope with `mkdirat`, creating the scope directory and its parents from the start
  directory when missing; `EEXIST` on a directory is success), `delete` (op 5: `rt_open_parent(...,
  strict)`, `statx` NOFOLLOW, `unlinkat` with `AT_REMOVEDIR` for a directory), `rename` (op 6: both
  parents with `strict`, `rt_check_path(EF_FDELETE, from)` and `rt_check_path(EF_FWRITE, to)`,
  `renameat2(..., RENAME_NOREPLACE)`); all errno through the message table.
- [X] T018 [US2] `std/fs.vo`: `append_file`, `make_dir`, `delete`, `rename` with the effects of
  contracts/language.md (`rename` declares `file.write, file.delete`), doc comments and examples;
  literal targets in the T013 table: `append_file`, `make_dir` (0, `EF_FWRITE`), `delete` (0,
  `EF_FDELETE`), `rename` (0, `EF_FDELETE`; 1, `EF_FWRITE`).

**Checkpoint**: T014-T016 pass; `make security-check` passes.

---

## Phase 5: User Story 3 — Run another program (P1)

**Goal**: `std.process.run` and `run_with` under `process`, run to completion.

**Independent test**: step cases with the helper scripts check output, exit codes and refusals; no
process is left behind.

### Tests (first, failing)

- [X] T019 [P] [US3] Golden cases in `tests/cases/process/`: `path_name.vo` (`run("true",
  new list[str])` with `process("true")`: code 0, found through `PATH`); `not_found.vo`
  (`process("no-such-program-vo")`: `err("not found")`); `literal_outside.vo` (`run("rm", ...)` with
  `process("git")`: E0503); `missing_effect.vo` (E0501); `exact_text.vo` (`run("/bin/true", ...)` with
  `process("true")`: R0010, exit 70, report fields `effect` `process`, `target`, `allowed`);
  `bad_args.vo` (an argument with a NUL byte, an env entry `"=x"` and one without `=`: R0007).
- [X] T020 [P] [US3] Step cases in `tests/cases/process_steps/`, each starting with `helpers`:
  `args` (arguments with spaces, quotes, `;`, `$(...)`, `*`, an empty string and non-ASCII text reach
  `bin/args` unchanged); `input` (`run_with` sends text to `bin/cat_in`; `run` gives it no input: empty
  output at once); `codes` (`bin/exit_with 3`: ok with code 3; `bin/kill_self TERM`: code 143);
  `stderr` (`bin/to_stderr`: text in `stderr`, empty `stdout`); `env` (`run`: `bin/env_dump` prints
  nothing (no variables); `run_with` with `["A=1", "B=x y"]`: exactly those); `fds` (`bin/fds` prints
  nothing: no descriptor leaked, also when called from inside a `concurrent_map` task and from a
  `serve` handler); `timeout` (`bin/sleep_for 30` under `--timeout 1`: R0009, exit 124, then
  `no-children` reports 0); `grandchild` (`bin/grandchild` returns promptly with code 0 and
  `no-children` reports 0); `limits` (`bin/flood`: `err("output is larger than 16 MiB")`;
  `bin/bad_utf8`: `err("output is not UTF-8")`); `not_executable` (a helper with mode 644:
  `err("permission denied")`); `in_tasks` (`concurrent_map` over three `run` calls: results in input
  order); `relative` (`process("bin/args")` and `process("./bin/args")` both allow `run("bin/args",
  ...)`).

### Implementation

- [X] T021 [US3] `src/process.c` (new), following research R8-R11: argument validation (R0007);
  `rt_check_name`-style exact check of the normalized program text against both grants (R0010, a new
  helper in `src/rt_effects.c` if `rt_check_name` cannot normalize); resolution (`/` → from the start
  directory or absolute; bare → `PATH` from `envp` at the call, absolute entries only, first regular
  file with an execute bit via `statx`; none → `err("not found")`); four `sys_pipe`s; `sys_fork`; in
  the child `setpgid(0, 0)`, `sys_die_with_parent`, an empty signal mask, `dup3` onto 0/1/2,
  `fchdir(start_fd)`, `execve(path, argv, envp)` with `argv[0]` the program text, errno to the error
  pipe and exit 127; in the parent the error pipe (errno → `err`), then the wait loop: SIGCHLD and
  SIGPIPE added to the blocked set, `signalfd4` for SIGCHLD, `ppoll` over stdout, stderr, stdin
  (non-blocking writes, closed when done or on `EPIPE`) and the signalfd, bounded by the deadline;
  `wait4(pid, WNOHANG)` on SIGCHLD; once reaped, `kill(-pgid, SIGKILL)` and non-blocking reads until
  empty; 16 MiB per stream (kill the group, `err`); at the deadline kill and reap the group, then the
  existing timeout report (R0009); read the signalfd empty and restore the mask; `code` = status or
  128 + signal; both outputs `utf8_validate`d; encode `<code>\n<len>\n<stdout><stderr>`. Wire
  `io_run` to it (no recording yet). Mark the plain fork and the escaped-grandchild limit `ponytail:`.
  Add `src/process.c` to the build.
- [X] T022 [US3] `std/process.vo` (new): `Output`, `run`, `run_with` (decoding the text) with doc
  comments and examples; embed it as in T012; literal targets in the T013 table: `run`, `run_with`
  (0, `EF_PROCESS`, compared exactly after normalization).

**Checkpoint**: T019-T020 pass on x86-64; `make ARCH=aarch64 test` runs the process cases under qemu.

---

## Phase 6: User Story 4 — Review sees what a program may touch and run (P2)

**Goal**: `vo.effects`, `vo effects FILE` and E0504 cover the new effects.

**Independent test**: step cases change a program and check that `vo check` reports exactly that.

- [X] T023 [P] [US4] Step cases in `tests/cases/fs_steps/` and `tests/cases/process_steps/`:
  `review_process` (`vo effects --update`, then a function starts running `make` as well as `git`:
  E0504 naming the function, `process("make")` and the call); `review_delete` (a function that only
  wrote gains a `delete` call: E0504 "gains file.delete"); `review_read_only` (a program that only lists
  and reads: `vo effects main.vo` shows no write, delete or process capability); `effects_listing`
  (`vo effects main.vo` lists `process("git", "make")` under `main`).
- [X] T024 [US4] Make T023 pass: expected to need no code beyond Phase 2 (library effects already flow
  into the requirement); fix `src/effects_record.c` or `src/main.c` (`vo effects`) where the new names or
  scope forms are rendered wrongly, and record any change in `results.md`.

---

## Phase 7: User Story 5 — Everything stays deterministic and testable (P2)

**Goal**: recordings carry every file answer and run (contracts/recording.md); replay touches nothing;
traces show each operation; every new case is stable.

### Tests (first, failing)

- [X] T025 [P] [US5] Step cases in `tests/cases/fs_steps/` and `tests/cases/process_steps/`:
  `record_replay` (a program that lists, reads info, appends, makes a directory, renames, deletes and
  runs `bin/args`: `vo run --record r.jsonl`, `keep r.jsonl` (the events of contracts/recording.md),
  then remove the tree and the helpers, `vo replay r.jsonl`: identical output and nothing created);
  `replay_mismatch` (a recording replayed by a program that runs `bin/args` with other arguments, or
  lists another directory: R0014 with the texts of contracts/recording.md); `replay_denied` (a recorded
  R0010 repeats in replay); `trace` (`--trace t.jsonl` over `list_dir` and `run`: the `call`/`return`
  records of the library functions with their arguments and results; `keep t.jsonl`); `in_test`
  (`vo test` with `uses file.read("fixtures/"), process("bin/args")` twice: identical).

### Implementation

- [X] T026 [US5] `src/replay.c`: event kinds `EVK_INFO` ... `EVK_RUN` in `src/vo.h`, names in
  `ev_names`, the d_/n_ texts of contracts/recording.md; `io_fs` and `io_run` record (`path`, `to`,
  `program`, `args` as a JSON list, `result`, `value`; `denied` through the existing pending mechanism)
  and replay (check the path or program against the grant lexically first; take the next event, match
  kind, `path`/`to`/`program`/`args`, else R0014; perform nothing; return the recorded result); the
  reader accepts the new fields (`to`, `program`, `args`) and rejects incomplete events with T0008.
  Old recordings (every existing replay case) replay unchanged.
- [X] T027 [US5] `tests/cross_replay.py`: add `fs_steps/record_replay` and
  `process_steps/record_replay` to the cases replayed across platform pairs; add the new golden and
  step cases to the determinism check (`make gate`'s repeated runs: `tests/run.sh --repeat`,
  `tests/steps.py --repeat`) so each runs 20 times identically.

**Checkpoint**: T025 passes; `python3 tests/cross_replay.py` reports every pair identical.

---

## Phase 8: Polish, documentation and the gate

- [X] T028 Reference (`docs/reference.md`, contracts/language.md "Reference text"): the effects table
  rows, the scopes bullet, the canonical order line, the modules table rows, the paragraph and the
  checked example; `tests/ref_examples.py` passes and reports the token count (limit +500; record the
  figure in `results.md`); sync `contracts/language.md` with the shipped text.
- [X] T029 [P] Catalog (`docs/errors.md`): E0512 with Example and Fixed sections; E0509's message;
  E0506's program-scope message; `vo explain E0512`; `tests/catalog.py` passes (every code has a case).
- [X] T030 [P] `docs/security.md` (research R15): boundary table rows for `file.delete` and
  `process`; S2 for listing, information and every change (the walk, scope directories protected);
  S3: processes are created only by tasks and by `process`, the syscall count updated; a granted
  program is not confined (known limits), A4 on the program file, escaped grandchildren; fix the stale
  spec 016 lines (S9 "one request at a time", the "One request at a time" known limit).
- [X] T031 [P] Guide and site: `site/docs/guide/files-and-processes.md` (directories, changing files,
  `file.delete`, running programs, what a grant to a program means, replay) with checked examples,
  added to `GUIDE` in `tools/site.py`; rows in `site/docs/guide/standard-library.md` and
  `site/docs/guide/effects.md`; `tests/site_examples.py` passes.
- [X] T032 [P] Example `examples/release-check/` (SC-002): runs `git status --porcelain`, `git log -1`
  and a test command from a list, checks their exit codes, writes `out/report.txt`, deletes the
  previous report first; `README.md` with the grant explained, `vo.effects`, a recording in
  `recordings/` that replays without git; add a row to `examples/README.md`; record its line count
  against an equivalent Python script in `results.md`.
- [X] T033 [P] `tests/fs_scale.py` (SC-004): a directory of 100,000 files listed (under 1 s), 1,000
  runs of `bin/args` (under 5 s, with the per-run cost printed), then no child processes left and the
  open descriptors of a waiting vo program the same before and after (as `tests/tasks_scale.py`).
- [X] T034 [P] Roadmap and README: mark 017 done in `docs/roadmap.md` (row and section, with what was
  not done), update the status lines in `README.md` (effects list, what programs can do).
- [X] T035 The gate: `make gate` (x86-64), `make instrumented` (with the fs and process step cases),
  `make security-check`, `make ARCH=aarch64 test`, `make ARCH=riscv64 test`, `python3
  tests/cross_replay.py`, `python3 tests/fs_scale.py`; fill `results.md` (every SC with evidence,
  behavior changes: E0509's message, renumbered bits (not visible), the new event kinds; deviations
  from the plan) and mark the spec's status.

---

## Dependencies & Execution Order

- **Phase 1** (T001-T003): no dependencies; T002 and T003 in parallel.
- **Phase 2** (T004-T008): after Phase 1; T004 first (failing cases), then T005 → T006; T007 and T008
  after T005, in parallel with each other. Blocks every story.
- **US1** (T009-T013): after Phase 2. MVP.
- **US2** (T014-T018): after US1 (same `src/fs.c` and `std/fs.vo`, and `rt_open_parent` in use).
- **US3** (T019-T022): after Phase 2, independent of US1/US2 (different files), except the T013 table
  shared in `src/check_effects.c` (edit after T013 lands, or merge carefully).
- **US4** (T023-T024): after US2 and US3 (needs both effects in use).
- **US5** (T025-T027): after US2 and US3 (records every operation).
- **Polish** (T028-T035): T028-T034 after the stories they document; T035 last.

## Parallel Examples

- Phase 1: T002 (platform headers) with T003 (helper scripts, `tests/steps.py`).
- US1: T009 (golden cases) with T010 (step cases), then T011 → T012 → T013.
- US1/US2 with US3: one stream on `src/fs.c`/`std/fs.vo`, another on `src/process.c`/`std/process.vo`.
- Polish: T029, T030, T031, T032, T033, T034 in parallel after T028.

## Implementation Strategy

- **MVP**: Phases 1-3 (US1): programs can list and inspect directories under `file.read`, with every
  scope check; already unblocks the most common missing programs.
- **Next**: US2 (changes, with `file.delete` visible in review), then US3 (running programs); each is
  usable on its own after its checkpoint.
- **Then**: US4 (review cases) and US5 (record, replay, traces, determinism), then the docs and the
  full gate on three platforms.

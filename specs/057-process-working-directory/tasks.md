---

description: "Task list for vo-lang spec 057: Process Working Directory"
---

# Tasks: Process Working Directory

**Input**: Design documents from `specs/057-process-working-directory/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/api.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each behavior's golden case lands first and fails before its code.

**Organization**:
- Setup: results file, quick-area mapping, a vo 0.2 recording taken before any code changes.
- Foundational: the directory reaching the runtime, the std signatures, E0214 for the old calls and the
  scoped E0501 fix. The signature change breaks every existing call, so the migration machinery is
  blocking: without it the suite cannot pass after any story.
- US1 (P1) run in a directory; US2 (P1) wrong directories; US3 (P2) the repository migrated by fixes;
  US4 (P2) recordings.
- Polish: the tools, reference, catalog, changelog, version 0.3.0, results, `make test`.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Source of truth**: contracts/api.md for signatures, results, diagnostics and the reference text;
  data-model.md for the order of checks, the run event and the E0501 scope rule; research.md R1–R8 for how.
- **Golden cases**: single-file cases are `tests/cases/AREA/NAME.vo` with `.out`/`.err`/`.code` and `.cmd`
  (as `tests/cases/process/literal_outside.*`). Multi-command cases are step folders
  (`tests/cases/process_steps/NAME/steps`, `main.vo`, `expected/out`; language in the header of
  `tests/steps.py`; `helpers` copies `tests/process_helpers/*` to `./bin/`). Write the case, run it, see it
  fail, then write the code; accept with `python3 tests/steps.py --accept PATH` or the area's accept.
- **Helper**: `bin/cat_file NAME...` (`tests/process_helpers/cat_file`) prints files named relative to its
  working directory; no new helper is needed.
- **C**: zero warnings; freestanding (system calls through `vo_syscall*`); no static buffer over 64 KiB;
  `GC_ROOT` every heap pointer held across an allocation.
- **Between edits**: `make quick AREA=fs` (process cases) or `make quick AREA=consistency` (E0214, E0501);
  `make test` once at the end; never two suites at once. The gate only when the user asks.

## Phase 1: Setup

- [X] T001 Create `specs/057-process-working-directory/results.md` with headings SC-001 to SC-006,
  "Reference cost", "E0501 goldens changed" and "Deviations". In `tools/quick.sh`, map
  `tools/vo_fix.vo` to `consistency` (already there) and `tools/progress.vo` to nothing (no area).
- [X] T002 Before any code changes, take a vo 0.2 recording for US4. In a scratch directory: copy
  `tests/process_helpers/*` to `bin/`, write `a.txt`, and write a `main.vo` in the 0.2 form
  (`uses console, process("bin/cat_file")`, printing `run_with("bin/cat_file", ["a.txt"], "", [])`); run
  `bin/vo run --seed 1 --record r.jsonl main.vo` and save `r.jsonl` as
  `tests/cases/process_steps/dir_replay_02/r02.jsonl` (T028 adds the migrated `main.vo` beside it).
  Record the commit it was made with in results.md.

## Phase 2: Foundational (blocking)

**Purpose**: the directory reaches the child; the old calls get E0214 and migrate without widened grants.

- [X] T003 [P] Golden check cases for E0214's new rows: `tests/cases/library/process_dir.vo` (`.cmd`
  `check --json {}`) with one 0.2-form call each of `run_with`, `run_with_bytes` and `run_lines`. Expected:
  one E0214 per call, message "`std.process.run_with` takes a working directory after the variables since
  vo 0.3.0: `"./"` is the start directory." (with the function's name), and one fix inserting `, "./"`
  after the variables argument (before `f` for `run_lines`).
- [X] T004 [P] Golden check cases for the scoped E0501 fix, `tests/cases/effects/literal_scope_fix.vo` and
  `literal_scope_mixed.vo` (`check --json {}`):
  - `read_file("data/a.txt")` and `std.fs.list_dir("logs/")` in a function declaring no file effect →
    E0501 whose fix writes `file.read("data/", "logs/")`;
  - `read_file("a.txt")` → `file.read("./")`;
  - `run_with("bin/x", [], "", [], "./")` with `process("bin/x")` → `file.read("./")`;
  - mixed: the same function also calls `read_file(name)` with a parameter (or `std.fs.read_bytes`, not a
    literal target) → the fix stays unscoped `file.read`, as today.
  Rule (data-model.md): "when every need for it comes from literal targets of std functions and builtins,
  the fix adds the effect with scopes: a file target's directory, a directory target as written".
- [X] T005 In `src/libchange.c`, add kind `LC_ARG` and rows `{"process", "run_with", LC_ARG, 0, "\"./\"",
  0, 4}`, `{"process", "run_with_bytes", LC_ARG, 0, "\"./\"", 0, 4}`, `{"process", "run_lines", LC_ARG,
  0, "\"./\"", 0, 4}` (`call` holds the inserted text, `factor` the index). Add its message to `message()`
  with "since vo 0.3.0". Add `int libchange_arg_call(Node *call)`, modelled on `libchange_swap_call`:
  when a call of such a std function has exactly one argument fewer than the declaration, report E0214 on
  the callee name with one fix inserting `, "./"` after argument `factor - 1`, then check the given
  arguments against the parameters skipping index `factor` and return 1. Declare it in `src/vo.h`; call it
  where `libchange_swap_call` is called (`src/check_expr.c`, non-generic calls) and at the start of
  `check_generic_call` in `src/check_generic.c` (for `run_lines`).
- [X] T006 In `src/check_effects.c`, scope E0501's fix (research R6): keep, per function and per file
  effect bit, whether any need came from something other than a literal target recorded in `ck_lits`
  (a non-literal argument to a target, or a std function or builtin that needs the bit and is not in
  `std_targets`/`fx_builtin_literal`). In `fx_report`, when E0501 adds a file bit with no such need, add
  the recorded literals as scopes instead of the bare bit: a file target's directory (`"data/a.txt"` →
  `"data/"`, `"a.txt"` → `"./"`), a directory target as written. Mark which `std_targets` rows are
  directory targets (`list_dir`, `walk`, `make_dir` and the three new rows); `read_file`, `write_file`,
  `info`, `append_file`, `delete`, `rename` are file targets. E0503 is unchanged (no fix).
- [X] T007 In `src/check_effects.c` `std_targets`, add `{"std/process.vo", "run_with", 4, EF_FREAD}`,
  `{"std/process.vo", "run_with_bytes", 4, EF_FREAD}`, `{"std/process.vo", "run_lines", 4, EF_FREAD}` (the
  directory), and the missing program rows `{"std/process.vo", "run_bytes", 0, EF_PROCESS}`,
  `{"std/process.vo", "run_with_bytes", 0, EF_PROCESS}`, `{"std/process.vo", "run_lines", 0, EF_PROCESS}`.
- [X] T008 In `src/process.c`, take the directory from the builtins: `__run`, `__run_bytes` and
  `__proc_start` receive `[dir, program, args...]` as their first argument (builtins take three
  arguments; research R4). `io_run` and `io_proc_start` split off `dir` into a new list
  `[program, args...]` before `run_check`, so `run_check`, recording and replay see argv as before.
  Update the builtin comments in `src/vo.h` (`B_RUN`, `B_RUN_BYTES`, `B_PROC_START`).
- [X] T009 In `src/process.c` `run_check(argv, env, dir)`: after the program's checks, R0007 "A directory
  must be a non-empty path without NUL bytes." for an empty `dir` or one holding NUL; then, unless `dir` is
  `"./"`, `rt_check_path(EF_FREAD, dir)` (R0010). Order per data-model.md: "arguments (R0007) → `process`
  grant → `file.read` grant of `dir` (R0010) → the directory is opened (`directory not found`, `not a
  directory`) → the program is found (`not found`) → the child enters the directory (`directory not
  accessible`) → `execve` (`not found`, `permission denied`)".
- [X] T010 In `src/process.c`, for a `dir` other than `"./"` (research R1–R3):
  - open it in the parent: `rt_open_parent(EF_FREAD, dir, &name, 0)`, then
    `openat(parent, name, O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)` (or the parent itself when
    `name` is `"."`); `-ENOENT` → `err("directory not found")`, `-ENOTDIR` → `err("not a directory")`,
    other errors → `err("directory not accessible")`; nothing starts;
  - a program path with `/` that is not absolute is joined to `rt_start_cwd()` for `execve` (`argv[0]`
    stays as written); bare names keep the `PATH` lookup;
  - pass the descriptor to `child()`, which calls `fchdir` on it after `setpgid` and before `close_range`;
    on failure it writes `1000 + errno` to the error pipe and exits 127;
  - in `rt_run` and `io_proc_start`, `failed >= 1000` → `err("directory not accessible")`; the
    descriptor is closed in the parent after the fork in every path.
- [X] T011 In `std/process.vo`, change the signatures to contracts/api.md: `run_with(program, arguments,
  input, vars, dir)`, `run_with_bytes(..., vars, dir)`, `run_lines(..., vars, dir, f)`, each `uses
  process, file.read` (and `E` for `run_lines`), building `[dir, program, args...]` for the builtins;
  `run` and `run_bytes` call `__run`/`__run_bytes` with `"./"` directly and keep `uses process` only.
  Update each doc comment to name the directory and its errors, and each example to pass `"./"` (doc
  conformance: `python3 tests/doc_conformance.py`).
- [X] T012 Verify on the other architectures that `fchdir` on an `O_PATH` descriptor works under qemu user
  mode: build them with `make ARCH=riscv64` and `make ARCH=aarch64` (into `build-riscv64/` and `build-aarch64/`) and run T013's
  case with `VO_BIN` set to each. If it fails, open the directory with `O_RDONLY | O_DIRECTORY` instead and
  record the deviation (a directory with search but no read permission is then refused) in results.md.

**Checkpoint**: T003 and T004 pass; the old callers report E0214 (they are migrated in US3).

## Phase 3: User Story 1 - Run a program where it expects to run (Priority: P1) MVP

**Goal**: a program starts in the named directory; `"./"` is today's behavior; a relative program path
is still the start directory's file.

**Independent Test**: `tests/cases/process_steps/dir` shows the four forms running in `work/` and in `"./"`.

- [X] T013 [P] [US1] Step case `tests/cases/process_steps/dir/`: `helpers`, `mkdir work`, `write work/a.txt
  in work\n`, `write a.txt in start\n`, `vo run main.vo`. `main.vo` (`uses console, file.read("./"),
  process("bin/cat_file")`) prints the result of `bin/cat_file a.txt` through `run_with` in `"work/"` and
  in `"./"`, through `run_with_bytes` in `"work/"`, and through `run_lines` in `"work/"`. Expected: `in
  work` three times and `in start` once: the program `bin/cat_file` was found under the start directory
  while running in `work/` (FR-004).
- [X] T014 [P] [US1] Step case `tests/cases/process_steps/dir_link/`: `symlink work linked` and a run in
  `"linked/"` lists `work/a.txt` (links are followed when resolving, research R1); and `run_with` in
  `"work/../work/"` gives the same.
- [X] T015 [US1] Make T013 and T014 pass with Phase 2's code; fix what they find in `src/process.c`.

**Checkpoint**: US1 works on x86-64 (`make quick AREA=fs`).

## Phase 4: User Story 2 - A wrong directory is a value, not a surprise (Priority: P1)

**Goal**: missing, non-directory, unenterable and ungranted directories never start the program.

**Independent Test**: the cases below; no `cat_file` output appears in any of them.

- [X] T016 [P] [US2] Step case `tests/cases/process_steps/dir_errors/`: `helpers`, `write file.txt x`,
  `mkdir locked`, `chmod 000 locked`; `main.vo` runs `bin/cat_file` in `"missing/"`, `"file.txt"` and
  `"locked/"` and prints each result. Expected: `err directory not found`, `err not a directory`,
  `err directory not accessible`.
- [X] T017 [P] [US2] Step case `tests/cases/process_steps/dir_outside/`: `main.vo` declares
  `file.read("work/")` and runs in a directory built at run time (`"oth" + "er/"`, so the checker cannot
  see it). Expected: R0010, exit 70, no helper output.
- [X] T018 [P] [US2] Check case `tests/cases/process/dir_literal_outside.vo` (as `literal_outside.*`):
  `file.read("work/")` and a literal `"other/"` directory → E0503 on the literal.
- [X] T019 [P] [US2] Run case `tests/cases/process/dir_empty.vo` (as `bad_args.*`): an empty directory →
  R0007 with "A directory must be a non-empty path without NUL bytes."
- [X] T020 [US2] Make T016 to T019 pass; fix what they find.

**Checkpoint**: US1 and US2 pass.

## Phase 5: User Story 3 - Existing programs keep working through a fix (Priority: P2)

**Goal**: every existing caller migrates by fixes alone, with no widened grant and unchanged output.

**Independent Test**: after T022, `git diff` of the migrated callers shows only the inserted `"./"` and
`file.read("./")`, and their golden outputs are unchanged.

- [X] T021 [US3] In `tests/libchange_rows.py`, add a `program()` template for `LC_ARG`: a 0.2-form call of
  the row's function with `uses console, process("true")`; the existing check (E0214 reported, clean after
  `vo_fix`) then covers the three rows. Run it.
- [X] T022 [US3] Migrate the repository's callers with the fixes alone: `bin/vo run --timeout 0
  tools/vo_fix.vo -- tests/cases/process tests/cases/process_steps tests/cases/bytes_steps
  tests/cases/effect_sets`. Step folders: run it on each folder's `main.vo` (and other `.vo` files). Do
  not hand-edit; if a file is not clean afterwards, record why in results.md "Deviations".
- [X] T023 [US3] Re-run the migrated cases (`make quick AREA=fs`, `python3 tests/steps.py
  tests/cases/process_steps tests/cases/bytes_steps`); their expected outputs must not change. Cases whose
  expected output prints the program's source or effects (`effects_listing`, `review_process`,
  `effect_sets`) are re-accepted and listed in results.md.
- [X] T024 [US3] Re-accept the golden cases whose E0501 fix text changed under T006 (search
  `tests/cases/**/*.out` for E0501 fixes; run `make quick AREA=consistency` and the `effects`, `fs`,
  `sqlite` areas). List each case with its old and new replacement in results.md "E0501 goldens changed".
  A case whose fix became scoped although a non-literal need exists is a bug in T006, not a re-accept.
- [X] T025 [US3] Run `python3 tests/apply_fixes.py`, `python3 tests/migrate_056.py` and `python3
  tests/migrate.py` (the earlier migrations must still pass, constitution: kept as tests of every later
  toolchain).

**Checkpoint**: the whole repository checks; US1 to US3 pass.

## Phase 6: User Story 4 - Recordings hold the directory (Priority: P2)

**Goal**: a run's directory is recorded and matched; 0.2 recordings still replay.

**Independent Test**: the three replay cases below.

- [X] T026 [P] [US4] Step case `tests/cases/process_steps/dir_replay/`: `helpers`, `mkdir work`, `write
  work/a.txt hi\n`, `vo run --seed 1 --record r.jsonl main.vo` (runs `bin/cat_file a.txt` in `"work/"`),
  `keep r.jsonl` (its run event has `"dir":"work/"`), `rm work/a.txt`, `vo replay r.jsonl` (identical
  output: nothing started).
- [X] T027 [P] [US4] Step case `tests/cases/process_steps/dir_replay_mismatch/`, as `replay_mismatch`:
  record in `"work/"`, `cp other.vo main.vo` (the same run in `"other/"`), `vo replay r.jsonl` → R0014
  naming the run and its directory.
- [X] T028 [P] [US4] Step case `tests/cases/process_steps/dir_replay_02/`: `main.vo` in the migrated form
  (`"./"`), `vo replay r02.jsonl` (T002's 0.2 recording) → identical output, no R0014. Also `keep` a fresh
  recording of the same program and compare it byte for byte with `r02.jsonl` apart from the header
  (no `"dir"` field for `"./"`).
- [X] T029 [US4] In `src/replay.c`: pass `dir` as `to` in `io_record`/`io_replay` for `EVK_RUN` (from
  `io_run` and `io_proc_start`, as written); write `,"dir":"..."` after `args` only when it is not `"./"`;
  add `"dir"` to `rk_keys`/`rf_slots` and store it on the event (absent → `"./"`); in `io_replay`, a
  different directory is a mismatch; the R0014 text names the run as `program args (in DIR)` when `DIR` is
  not `"./"` (`run_desc`).
- [X] T030 [US4] Make T026 to T028 pass, and the existing `record_replay`, `replay_mismatch`,
  `tests/cases/replay` and `python3 tests/cross_replay.py` still pass.

**Checkpoint**: all four stories pass.

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T031 [P] `tools/vo_fix.vo` (SC-001): run `vo check --json BASE` and `vo effects --update BASE` with
  the entry's directory as `dir` and its base name as the argument (as `tools/vo_fix.py` did); the
  diagnostics' files are then relative to that directory, so `apply` joins them to it before reading and
  writing. Its `vo()` takes the directory; `main`'s effects stay as they are (unscoped `file.read` already
  covers any directory). Then run `python3 tests/libchange_rows.py` and `python3 tests/migrate_056.py`:
  both pass, and "left:" lines name files relative to the entry.
- [X] T032 [P] `tools/progress.vo`: `run_lines` passes `"./"`, and `run_make` declares
  `file.read("./")` in place of `file.read("build/")` for reading (writes stay `file.write("build/")`).
  Check, test (`bin/vo test tools/progress.vo`) and run it on a scratch makefile as before.
- [X] T033 `docs/reference.md` (R8): the std.process paragraph's sentence becomes "`run_with` adds input
  text, `NAME=value` variables and a working directory (inside a `file.read` scope)."; the effects table
  row becomes `| process | \`std.process\` (running other programs) |`; drop "killed by signal n, `code`
  is 128 + n" (it stays in `Output`'s `vo doc`). Run `python3 tests/ref_examples.py`: at most 15,000
  tokens; record before/after counts in results.md "Reference cost".
- [X] T034 [P] `docs/errors.md` E0214: "renamed, removed or changed in vo 0.2.0" → "in vo 0.2.0 or 0.3.0",
  and one sentence: a function that gained an argument gets it inserted with the value that keeps the old
  behavior (`"./"` for `run_with`'s directory). E0501: note that its fix scopes a file effect needed only
  by literal paths. Run `python3 tests/catalog.py`.
- [X] T035 [P] `CHANGELOG.md`: a 0.3.0 section, breaking changes first: the three signatures (E0214 and
  its fix), E0501's scoped fix (list the effect on new programs), the manual step for a caller whose
  `file.read` scopes miss the start directory (E0503: add `"./"` or the directory to the scopes);
  additions: directory errors, the `"dir"` recording field, the E0503 rows for `run_bytes`,
  `run_with_bytes`, `run_lines` programs.
- [X] T036 [P] Version 0.3.0: `src/build.c` `vo_version_text`, `docs/stability.md` ("vo is at version
  0.3.0"; the migrations sentence lists spec 057 as the third, and `tests/libchange_rows.py` as its test),
  and any golden output printing the version (`grep -rl "vo 0.2.0" tests/cases`).
- [X] T037 [P] `site/docs`: the pages naming `run_with`, `run_with_bytes` or `run_lines` (grep) show the
  new form; `make site-check`.
- [X] T038 `make test` once (through `bin/vo run --timeout 0 tools/progress.vo -- run test`); read
  `build/progress/test.log`, fix and rerun until it passes.
- [X] T039 Complete `specs/057-process-working-directory/results.md` (SC-001 to SC-006 with what was run
  and measured, the E0501 goldens, deviations, what was not verified: SC-005 on three architectures
  needs the gate, which runs only when the user asks) and set the spec's status to Implemented.

## Dependencies & Execution Order

- Setup (T001, T002) first; T002 must run before any code change (it needs the 0.2 toolchain).
- Foundational (T003–T012) blocks every story. Within it: T003, T004 (cases) → T005–T007 (checker) and
  T008–T010 (runtime) in either order → T011 (std; from here on the old callers report E0214) → T012.
- US1 (T013–T015) and US2 (T016–T020) depend only on Foundational and can run in parallel.
- US3 (T021–T025) depends on Foundational; it should follow US1/US2 so migrated cases run against working
  code. Until T022, `make quick` areas with old callers fail; that is expected.
- US4 (T026–T030) depends on Foundational and T002.
- Polish: T031, T032 after US3; T033–T037 any time after Foundational; T038, T039 last.

### Parallel opportunities

- T003 ∥ T004; T013 ∥ T014; T016 ∥ T017 ∥ T018 ∥ T019; T026 ∥ T027 ∥ T028 (all new case files).
- US1 ∥ US2 ∥ US4 once Foundational is done (different case folders; US4 also touches `src/replay.c`
  only).
- T031 ∥ T032 ∥ T033–T037 (different files).

## Implementation Strategy

- **MVP**: Setup, Foundational, US1 and US3: programs run in a directory and the repository is migrated,
  so `make test` can pass. US2's errors already work after Foundational (T010); its phase pins them
  with cases.
- Then US2 and US4, then Polish. One `make test` at the end (T038); `make quick` between tasks.

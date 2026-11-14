---

description: "Task list for vo-lang spec 006: query, structural editing, trace and replay"
---

# Tasks: Query, Structural Editing, Trace and Replay

**Input**: Design documents from `specs/006-query-edit-trace/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R19), data-model.md, contracts/cli.md, contracts/reports.md, contracts/trace.md, contracts/recording.md, contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution v1.1.0, Principle IV).
- Every story starts with golden cases or step cases that fail on the current binary.
- Every new code gets a case that `tests/catalog.py` finds.
- Every new reference example runs in `tests/ref_examples.py`.

**Organization**: one phase per user story, in spec priority order.
- **MVP:** US1 (queries). It stands alone and builds the reference log that US2 needs.
- **US2 (edits):** needs US1's log and targets.
- **US3 (traces), US4 (frame values), US5 (replay):** independent of US1/US2 and of each other, apart from the shared render cap and `vo_exit` hook built in Phase 2.
- **US6 (evaluation):** needs US1 and US2.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependencies on incomplete tasks).
- **[Story]**: US1–US6 from spec.md.

## Conventions

**Carried over** from specs 001–005:
- RV64IMAFD, lp64d, GNU as. The SESI hook checks every edit; every report is fixed or explained.
- C prototype comments and `.globl` on every function. Shared constants in `src/defs.inc`. No libc.
- Golden cases are `tests/cases/<dir>/<name>.vo` plus `.out`, `.err`, `.code`, and optional `.cmd` (`{}` = the case file, `{dir}` = its directory). Seed a new case with a `.code` file so `run.sh` picks it up, then accept it with `tests/run.sh --accept` *only after* checking the output by hand against the contracts.
- Step cases (new, T002) are folders `tests/cases/{edit,trace,replay}/<case>/` that contain a `steps` file (Principle IV: every golden case lives under `tests/cases/`): the `steps` file, the input files, and `expected/` (files that must exist afterwards, byte for byte, plus `out`: the concatenated stdout/stderr/exit status of every step).
- `VO_BIN` must be an absolute path.
- Append every SESI false positive or negative to `sesi-linter-issues.md`.
- `pkill` patterns are anchored with `^`.
- The collector is conservative and does not scan a/t registers: spill any value that must survive an allocation into an s register or a frame slot.

**What the definitions are:**
- Command line: contracts/cli.md. Output: contracts/reports.md, contracts/trace.md, contracts/recording.md.
- Codes: contracts/codes.md. Layouts: data-model.md. Decisions: research.md R1–R19.

If the implementation shows a contract is wrong, fix the contract in the same task and say so in `results.md`.

**Regression rule:** after every task that touches `src/`, `make test` passes. Existing outputs stay byte-identical, except runtime-error reports after T033 (FR-025).

**Speed rule:** no instruction is added to, or moved inside, `vm.s`'s existing handlers. New handlers go in `src/trace.s` (or at the end of `vm.s`). The page pins at the top of `vm.s` and `rt_map.s` stay.

**Shortcuts get `ponytail:` markers:** call-based impact; diff paths matched by suffix; the 200-byte render cap; the 100,000 default trace limit; `check_reset` instead of a fresh process.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose:** case directories, the step-case harness and the speed baseline, before any source change.

- [X] T001 Create `tests/cases/query/`, `tests/cases/edit/`, `tests/cases/trace/` and `tests/cases/replay/`. Confirm that `tests/run.sh` and `tests/catalog.py` pick up cases in `tests/cases/query/` (including multi-file programs in subdirectories, as `tests/cases/multi/` does) without edits; if either lists directories explicitly, add it. Create `specs/006-query-edit-trace/results.md` with an SC table (SC-001…SC-009, all "open") and empty sections "Deviations", "Follow-ups".
- [X] T002 [P] Write `tests/steps.py`, the step-case runner:
  - For each folder under `tests/cases/` (or under the paths given) that contains a `steps` file: copy the directory (without `expected/`) to a temporary directory, run each line of `steps` there in order, and compare.
  - A step line is `vo ARGS...` (run `$VO_BIN`, default `bin/vo`, with those arguments; `< FILE` at the end feeds stdin, otherwise stdin is empty), `env NAME=VALUE ... vo ARGS...`, `rm PATH`, `cp A B`, or `write PATH TEXT` (TEXT with `\n` escapes). No shell.
  - After the steps: every file under `expected/` except `out` must exist with identical bytes; `expected/out` must equal, for each `vo` step, `$ <line>`, its stdout, `--- stderr`, its stderr, `--- exit N`.
  - Timings matching `\d+ms\)` are masked to `0ms)` before comparing, as in `run.sh`.
  - `--accept` writes the actual results into `expected/` (only after checking them by hand). `--repeat N` runs every case N times and requires identical results.
  - Exit 1 on any difference; print a unified diff of the first difference per case.
  - Make `tests/run.sh` skip every folder that contains a `steps` file, so the `.vo` files in it are not run as cases of their own.
  - Add `python3 tests/steps.py` and `python3 tests/steps.py --repeat 3 tests/cases/trace tests/cases/replay` to the `Makefile` `test` target's script list (skipped while the script does not exist, like the others).
- [X] T003 [P] Move the speed baseline to spec 005 (research R18): in `Makefile` replace `baseline-004`/`BASE004_REV`/`build-004` with `baseline` / `BASE_REV := 99b5d3f` / `build-base` (worktree `build-base/src`, binary `build-base/vo`). Keep `clean` from deleting it. Run `make baseline` and `python3 tests/speed.py --baseline build-base/vo`; record the ratios (expected 1.00 ± noise) in `results.md`. Remove the `build-004` worktree with `git worktree remove build-004/src` only after `make baseline` succeeds.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose:** constants, command scaffolding, and the two pieces shared by several stories (render cap, exit hook).

**⚠️ CRITICAL:** no user story work can begin until this phase is complete.

- [X] T004 Add to `src/defs.inc` (data-model.md): `RL_KEY_A 0`, `RL_KEY_B 8`, `RL_NODE 16`, `RL_ITEM 24`, `RL_ROLE 32`, `RL_SIZE 40`; roles `RR_DECL 0`, `RR_CALL 1`, `RR_VALUE 2`, `RR_TYPE 3`, `RR_IMPORT 4`, `RR_USE 5`; `CH_FILE 0`, `CH_START 8`, `CH_END 16`, `CH_OLD 24`, `CH_NEW 32`, `CH_SIZE 40`; `EV_KIND 0`, `EV_NAME 8`, `EV_RESULT 16`, `EV_VALUE 24`, `EV_BYTES 32`, `EV_SIZE 40` and the event kinds (six input kinds `read_line`, `read_all`, `read_file`, `write_file`, `env`, `now_ms`, and `end`); `FILE_GEN` as a new file-table field (grow `FILE_SIZE`, and fix the stale "64 bytes each" comment); `OP_TRACE_ENTER` as the next opcode number. Also fix the stale `N_IMPORT` "C resolved symbol" comment (the import's `N_D` is the target file id + 1) and the stale lambda "A params" comment noted in research.
- [X] T005 Create `src/query.s`, `src/edit.s`, `src/trace.s` and `src/replay.s`, each with a header comment naming its research decisions and one exported stub. Confirm the `Makefile` links them after the runtime objects (not in `RUNTIME`) and that `nm build/vo` shows `vm_run` and `map_new` at the same page offsets as before (0xcb0 / 0x7cc within their pages).
- [X] T006 Add the command scaffolding to `src/main.s` (contracts/cli.md):
  - Commands `query` (subcommands `callers`, `refs`, `type-at` as the first positional argument), `impact`, `edit` (subcommands `rename`, `add-field`), `replay`: `CMD_*` constants, `cmd_names` entries, dispatch branches calling `cmd_query`, `cmd_impact`, `cmd_edit`, `cmd_replay` (stubs in the new files returning T0003 for now).
  - Options `--diff FILE` (impact only), `--dry-run` (edit only), `--trace FILE` and `--trace-limit N` (run, test, replay), `--record FILE` (run only); update `m_unknown_cmd`, `m_missing_cmd`, `m_unknown_opt`.
  - Usage errors (T0001): missing or extra positional arguments for each subcommand; an unknown subcommand; `--trace-limit` without `--trace` or outside 1..100000000; `--dry-run` outside `edit`; `--diff` outside `impact`; `--record` outside `run`; `--seed`, `--clock-ms` or `-- ARGS` with `replay`; `--trace`/`--record` naming the same path, or a path equal to the entry file. (The check against every program file comes after loading, in the stories.)
  - Golden cases in `tests/cases/query/usage_*.cmd` for each usage error (exit 64), accepted after checking.
- [X] T007 [P] Add the render cap to `src/rt_render.s` (research R14): a `.globl render_cap` (0 = none). In `render_value`, the list, map, record and variant loops and `render_str_lit` stop as soon as the builder's length exceeds `render_cap`. A new `render_capped(sb, value, type)` sets the cap to `sb_len + 200`, renders, cuts the result back to at most 200 bytes from where it started at a UTF-8 boundary, appends `...` if anything was cut, and restores the previous cap. Function-typed values render as `<` + `ty_name(type)` + `>` instead of nothing. `expect_eq` and property counterexamples keep calling `render_value` uncapped, so their output is unchanged (`make test` proves it).
- [X] T008 [P] Add the exit hook to `src/sys.s` (research R12, R16): `vo_exit` calls `exit_hooks` (a `.globl` in `src/trace.s` that for now does nothing) before `out_flush`. The hook must not allocate (OOM path). Add a 64 KiB buffered writer for an arbitrary fd to `src/sys.s` (`fbuf_write(fb, p, n)`, `fbuf_flush(fb)`, with a small `FB_*` record: fd, buffer, length) for the trace and the recording; check write errors and report them once at exit as T0009 `cannot_write_output` (exit 74 unless the run already had a non-zero status; contracts/codes.md) rather than silently. Add `EXIT_OUTPUT 74` to `src/defs.inc`.

**Checkpoint:** `make test` passes; every new command prints its T0001/T0003 stub results; speed ratios unchanged.

---

## Phase 3: User Story 1 - Ask the toolchain about the program (Priority: P1) 🎯 MVP

**Goal:** `vo query callers|refs|type-at` and `vo impact --diff` give exact answers from the checker's resolution (FR-001–FR-006).

**Independent test:** `tests/run.sh tests/cases/query` passes; every answer matches a hand-verified expectation.

### Tests for User Story 1 (write first; they must fail now)

- [X] T009 [P] [US1] Golden programs for queries in `tests/cases/query/`:
  - `shop/` (multi-file): `main.vo`, `io.vo` (defines `parse_line`, called from `load`), `report.vo` (passes `parse_line` to `map`), a local and a string both named `parse_line`, a record `Item` with field `name` and a local `name`, a variant with cases used in constructors and `match`, a generic record used with two type arguments, a function with `requires`/`ensures` referring to a parameter, a test and a property calling `parse_line`.
  - Cases (`.cmd` + expected outputs), text and `--json` for each: `callers_fn`, `callers_value`, `callers_builtin` (`print`), `refs_fn`, `refs_record`, `refs_field`, `refs_case`, `refs_generic`, `refs_param_pos`, `refs_local_pos`, `refs_tparam_pos`, `refs_contract_param`, `refs_import` (the import line's name), `type_at_local`, `type_at_expr`, `type_at_field`, `type_at_decl`, `type_at_param`.
  - Errors: `unknown_name` (T0004 with "did you mean"), `unknown_field`, `ambiguous_name` (two files each defining `helper`, T0005 with both in `related`), `position_outside` (T0004), `syntax_error` (exit 65, no answers), `with_check_errors` (answers plus the note and `errors` in the summary).
- [X] T010 [P] [US1] Golden cases for `vo impact` in `tests/cases/query/impact/` (each a `.cmd` with `--diff {dir}/NAME.diff`, the diffs written by hand in `git diff` and `diff -u` styles): `body_change` (tax → subtotal → checkout, and exactly the tests reaching `tax`), `two_files`, `added_fn`, `removed_fn` (with its former call sites), `value_use` (a function passed to `map` counts), `comment_only` (still changed), `outside_file` (`ignored`), `stdin` (`--diff -` with the diff as the case's input), `stale_diff` (T0007), `not_a_diff` (T0007), `test_change` (a changed test lists itself only), `property_reaches`. At least 10 diffs (SC-004).

### Implementation for User Story 1

- [X] T011 [US1] Reference log in the checker (research R1, data-model.md):
  - Globals `q_collect`, `q_log` (vec of `RL_SIZE` records) and `ck_item` in `src/check.s`; `ck_item` is set for every function, test and property before its body (and contract clauses) are checked, and to 0 at top level.
  - `q_note(key_a, key_b, node, role)` appends an entry with `RL_ITEM = ck_item` when `q_collect` is 1; it is a single load-and-branch when 0.
  - Log uses: next to every `rename_use(sym, node)` call (check_expr.s:1228, 1544, 2260; check.s:890), choosing the key from the symbol (`SY_DECL` for `SYM_FN`/`SYM_RECORD`/`SYM_VARIANT`, `SY_NODE` for `SYM_LOCAL`/`SYM_TPARAM`; for a lambda capture, the outer symbol's `SY_NODE`) and the role (`RR_CALL` at calls, `RR_VALUE` for `NF_FNVAL`, `RR_TYPE` in type annotations, else `RR_USE`).
  - Log declarations: in `sym_new` for top-level items (node = the name node), in `new_local` for parameters and locals (key = the declaring name node), for type parameters, for each record field (key = record decl, index) and each variant case.
  - Log fields and cases: field access (`N_FIELD`, decl from `N_TYPE(N_A)` → `TY_DECL`, skipping error types), method-style field calls (`CV_FIELD`), record-literal fields (`N_FINIT`), variant constructors (`N_VCASE`), match arms (variant from the scrutinee's type).
  - Log builtins at builtin calls (key 0, id) and unresolved names at `report_undefined` (key 0, the name string) for `impact`'s removed functions.
  - Log import names in `src/loader.s` `bind_imports` (node = the import's `N_B`, key = the imported symbol's `SY_DECL`, role `RR_IMPORT`).
  - With `q_collect` 0, `make test` output and `make speed` are unchanged.
- [X] T012 [US1] `type-at` capture (research R4): globals `q_pos_file`, `q_pos_off` (-1 when unused) and `q_best`; in `check_expr` where `N_TYPE` is written (check_expr.s:2711), if the node's file and span contain the position and its span is smaller than `q_best`'s, set `q_best`. Store the binding type in `N_ARM`'s `N_TYPE` in `src/check.s` (also needed by T031).
- [X] T013 [US1] `pos_offset(file, line, col)` in `src/diag.s` (the inverse of `pos_col`: `line_start` plus a code-point walk); -1 when the line or column is outside the file. Targets in `src/query.s` (research R2): parse `name`, `Record.field`, `Variant.case`, `FILE:LINE:COL` (the file matched against program files by exact path, then by path suffix); resolve to `(key_a, key_b)` from the log — positions take the innermost logged entry containing the offset. T0004 with the closest name (reuse the E0201 suggestion helper) or T0005 listing every candidate in `related`. Add T0004 and T0005 to `docs/errors.md` (contracts/codes.md).
- [X] T014 [US1] `cmd_query` in `src/query.s`: load and check the entry with `q_collect = 1` (and the position set for `type-at`); a hard syntax error prints the diagnostics and exits 65; otherwise filter the log for the target (`callers`: `RR_CALL`/`RR_VALUE` only), drop duplicate nodes, sort by file load order then offset with the declaration first for `refs`, and print `ref` records / text lines and the `query` summary (contracts/reports.md), plus the note when the program has errors. `type-at` prints the `type_at` record from `q_best` or the logged entry (whichever is smaller), with `name` and `declaration` for names. Accept T009's cases.
- [X] T015 [US1] Diff parser and impact in `src/query.s` (research R5): parse unified diffs (`---`/`+++`, `@@ -a,b +c,d @@`, ` `/`+`/`-` lines, `\ No newline at end of file`, `/dev/null`, skipped `diff --git`/`index` lines) from a file or stdin; match diff paths to program files (drop one `a/`/`b/` component, then suffix match at `/`; no match → `ignored`, two matches → T0007); verify every context and `+` line against the current file (T0007 otherwise); compute changed/added items from changed lines; removed functions from `-` lines `fn NAME(` with no current definition; then breadth-first over `RR_CALL`/`RR_VALUE` entries keyed by item, from the changed set, emitting `affected` (with `via`) and `test` records, and the `impact` summary. Add T0007 to `docs/errors.md`. Accept T010's cases.
- [X] T016 [US1] Check SC-007's query half: time `vo query refs` and `vo impact` on the largest refactor starter (by line count, `bench2/tasks/r*/starter/vo`) under qemu; must be under 1 s. Record the time in `results.md`.

**Checkpoint:** US1 complete. `make test` passes.

---

## Phase 4: User Story 2 - Change the program by structure, not by text (Priority: P1)

**Goal:** `vo edit rename` and `vo edit add-field`, verified by a second check, written all or nothing (FR-007–FR-012).

**Independent test:** `python3 tests/steps.py tests/cases/edit` passes; every edited program in the cases runs with unchanged output.

### Tests for User Story 2 (write first; they must fail now)

- [X] T017 [P] [US2] Step cases in `tests/cases/edit/` (each: the program, `steps` with `vo edit ...` then `vo check ...` then `vo run ...`, and `expected/` with the edited files):
  - Accepted: `rename_fn_imported` (definition, import line, calls; a string with the name untouched; `vo.effects` line renamed and the following `vo check` reports no effects change), `rename_record`, `rename_variant`, `rename_case`, `rename_field` (another record's same-named field untouched), `rename_param_pos`, `rename_local_pos`, `rename_tparam_pos`, `rename_test_item_effects` (a function listed in `vo.effects`), `rename_dry_run` (nothing written), `add_field` (declaration + every literal, single-line), `add_field_multiline_literal`, `add_field_empty_record`, `add_field_dry_run`, `add_field_generic_record` (a field of a non-parameter type).
  - Refused (T0006, exit 65, `expected/` holds the unchanged files): `clash_toplevel` (new name of another item), `clash_local` (shadowing, E0203), `clash_field`, `clash_case`, `reserved_word`, `builtin_name`, `not_identifier`, `same_name`, `rename_main`, `rename_builtin`, `field_exists`, `default_wrong_type` (E0301), `default_not_visible` (a local name in the default), `program_broken` (an error the edit would change), and `program_broken_unchanged` (an existing error the edit does not touch: accepted).
  - `recheck_identity`: a hidden self-test command (`vo edit rename --dry-run main.vo f f2` then back) on a program using every construct (generics, contracts, props, lambdas, effects) to show the second check sees what the first did.

### Implementation for User Story 2

- [X] T018 [US2] File generations and the overlay in `src/diag.s` and `src/loader.s` (research R7): `FILE_GEN` on each file-table entry, `cur_gen`, `file_find` matching only the current generation; an overlay map (path → text) that `load_file` consults before the disk; `vo.effects` lookup (`rec_find`) also goes through it.
- [X] T019 [US2] `check_reset()` in `src/check.s` (with helpers in `check_effects.s`, `check_generic.s`, `effects_record.s` and `diag.s` as needed): clear everything a `check_program` leaves behind that a second one would read — scope chain and `ck_*` globals, `all_edges`, the generic memo, effect state, the effects-record state, the reference log, `q_best`, diagnostics and counts, loader state (`ld_order`, `ld_hard`). Start from a list of every label in the `.data`/`.bss` sections of `src/check*.s`, `src/loader.s`, `src/effects*.s`, `src/diag.s` and `src/types.s`, and mark each in the comment as cleared, kept (program-independent, such as interned base types) or unused by checking. Prove it with `recheck_identity` (T017) and a temporary debug command that checks a program twice and compares the diagnostics, run over `tests/cases/` (remove the command afterwards; record the result in `results.md`).
- [X] T020 [US2] Rename changes in `src/edit.s` (research R8): resolve the target (T013), refuse builtins, `main`, a new name that is not an identifier, a keyword, a builtin name or the old name (T0006); build `CH_*` changes from every logged entry of the target (declaration, uses, calls, values, types, imports) plus the `vo.effects` line(s) `path:name` for functions, tests and properties (by line span in the registered record file); sort and de-duplicate.
- [X] T021 [US2] add-field changes in `src/edit.s` (research R9): join the arguments after `RECORD` with spaces, parse `FIELD: TYPE = DEFAULT` with the parser's field and expression rules (T0006 on a parse error); a declaration change (new line after the last field with its indentation, or `    FIELD: TYPE;` in an empty record); for every `N_NEW_RECORD` of that record (from the log: the record's `RR_USE` entries at literals), an insertion after the last `N_FINIT` (`, FIELD: DEFAULT` on the same line, or `,` + newline + indentation when the closing brace is on a later line; `FIELD: DEFAULT` in an empty literal). Refuse an existing field name.
- [X] T022 [US2] Verification in `src/edit.s` (research R7): apply the changes to copies of the texts, put them in the overlay, `check_reset`, `cur_gen = 1`, check again with `q_collect = 1`; accept only if the `(code, file)` multiset of errors is unchanged and, for a rename, the new log's entries for the target at its new declaration are exactly the edited sites (for add-field: the record has the field and no E0304/E0305 mentions it). On refusal print T0006 with the reason and the new diagnostics, spans mapped back to the current text (data-model.md, "Mapping back"), `related` = the conflicting declaration. Add T0006 to `docs/errors.md`.
- [X] T023 [US2] Writing in `src/edit.s` (research R10): print the `edit` records / text lines and the `edit` summary (contracts/reports.md); unless `--dry-run`, write every changed file to `<path>.vo-edit.tmp` with checked writes, then rename each over its target; on a failed temp write remove the temp files, write nothing and report T0006 "cannot write"; on a failed rename write back the original text (kept from the first check) of every file already renamed, remove the remaining temp files and report T0006 (FR-012, research R10). Add a step case `write_fails` (a read-only directory) that shows the files unchanged. Accept T017's cases.
- [X] T024 [US2] Check SC-007's edit half: time `vo edit rename --dry-run` on the largest refactor starter under qemu (two checks); under 1 s. Record in `results.md`.

**Checkpoint:** US1 and US2 complete. `make test` passes.

---

## Phase 5: User Story 3 - See the calls that led to a result (Priority: P2)

**Goal:** `vo run --trace` / `vo test --trace` write deterministic JSON-lines traces (FR-013–FR-016).

**Independent test:** `python3 tests/steps.py --repeat 3 tests/cases/trace` passes; speed unchanged without `--trace`.

### Tests for User Story 3 (write first; they must fail now)

- [X] T025 [P] [US3] Step cases in `tests/cases/trace/` (each: `vo run --trace t.jsonl main.vo` or `vo test --trace t.jsonl ...`, expected `t.jsonl` and `out`): `fact` (quickstart §4), `generic_fn` (rendered with the instantiated types), `lambda_and_map` (calls made by `map`), `call_value`, `try_return` (the propagated `err`), `runtime_error` (active calls then `error`), `requires_fail` (`error` right after `call`), `exit_mid_call` (`end` with the status), `timeout` (`end` after `error` R0009; use `--timeout 1` on a loop), `limit` (`--trace-limit 5`: five records, `omitted`), `long_values` (a 1,000-element list shortened to 200 bytes with `...`), `function_arg` (`<fn(int) -> int>`), `void_return` (no `value`), `test_cmd` (`test` records per test, one failing), `prop_cmd` (a property: a `test` record per case), `deep_recursion` (10,000 deep, limit applies), `trace_is_source` (T0001), `trace_unwritable` (`--trace /nonexistent-dir/t.jsonl`: T0009, exit 74).

### Implementation for User Story 3

- [X] T026 [US3] Trace output in `src/trace.s` (research R12, contracts/trace.md): open the trace file at option time (T0009, exit 74, if it cannot be created; add T0009 to `docs/errors.md`); `tr_*` globals; record writers for `call` (depth, function name via `frame_fn_name`, call site from `FR_CALL`, `args` by parameter name using `render_capped` with `cur_frame` set to the new frame for generics), `return` (value capped, none for `void`), `test`, `error`, `end`; the limit counter (`tr_written`, `tr_omitted`); `exit_hooks` writes `end` (with the exit status for `run`) and flushes. Spill values held in a/t registers before anything allocates.
- [X] T027 [US3] Tracing handlers (research R11): in `src/trace.s`, `op_trace_enter` (writes `call`, `tr_depth++`, continues at the next instruction), and tracing versions of `op_ret` and `op_try` (write `return` with the value about to be returned, `tr_depth--`, then jump to the normal handler). `vm_ops_trace` in `.rodata` = `vm_ops` with those three entries replaced (build it from the same table source, e.g. a macro list, so the two cannot drift). Export from `vm.s` only the labels the trace handlers jump to; no code changes inside `vm.s`'s handlers.
- [X] T028 [US3] Compiler and run integration: in `src/compile.s`, when `tr_on` is 1, `c_emit` reads handlers from `vm_ops_trace`, and every function's code (named functions, lambdas, test and property bodies, `main`) starts with `OP_TRACE_ENTER`; contract prologues come after it. In `src/interp.s` (`rt_raise`) write the `error` record when tracing; in `src/testrun.s` write a `test` record before each test and each property case. Reject a trace path equal to any program file (T0001) after loading. Accept T025's cases.
- [X] T029 [US3] Speed (FR-016): `python3 tests/speed.py --baseline build-base/vo` — every program within 5%; also `nm` page offsets of `vm_run`/`map_new` unchanged. Record ratios in `results.md`. If a ratio fails, find the moved code before changing anything else (spec 005 results, "layout").

**Checkpoint:** US3 complete. `make test` passes.

---

## Phase 6: User Story 4 - Runtime errors show the values (Priority: P2)

**Goal:** every frame of a runtime error lists its parameters and in-scope locals (FR-017, FR-018, FR-025).

**Independent test:** `tests/run.sh` passes with the updated runtime-error expectations; the new cases show the right values.

### Tests for User Story 4 (write first; they must fail now)

- [X] T030 [P] [US4] Golden cases in `tests/cases/run/locals_*.vo` (text) and `.cmd` with `run --json` variants: `locals_index` (quickstart §5), `locals_two_frames`, `locals_later_let` (a local declared after the failing statement is absent), `locals_in_initializer` (the local whose initialiser is running is absent), `locals_for_and_arm` (loop variable and match binding inside their bodies only), `locals_lambda` (a failing lambda's parameters and captures), `locals_generic` (values rendered with the frame's type arguments), `locals_long` (shortened), `locals_fn_value` (`<fn(...)>`), `locals_timeout_in_builtin` (R0009 raised inside `sort` on a huge list: frames shown, report finishes), `locals_oom` (R0006: report finishes), and in `tests/cases/test/` a failing test with `error` status showing the `trace` in `failure` (text and `--json`).

### Implementation for User Story 4

- [X] T031 [US4] (Needs T012's `N_ARM` type; if US1 is not done yet, make that change here.) Frame values in `src/trace.s` (research R13, data-model.md "Frame values"): `frame_locals(frame, pos_node)` returns a vec of `{name, type str, value str}`: parameters from `N_B(decl)`, then a walk of the body that only descends into blocks containing the position, taking `N_LET` whose statement ends before the position, `N_FOR` variables and `N_ARM` bindings (type from T012) whose body contains it, lambda captures (`FX_CAPS`); skipping `_`, unnamed and hidden slots. Values rendered with `render_capped` and `cur_frame` set to that frame (restored after).
- [X] T032 [US4] Report integration in `src/interp.s`: set `rt_reporting` (disables `rt_poll` and timeouts, in `interp.s`/`rt_render.s`) while the report is built; for each shown frame (the existing 10-frame walk) compute the position (error node for the first, child's `FR_CALL` after) and add the text line `      name: type = value, ...` (omitted when empty) and the JSON `locals` array (contracts/reports.md). In `vo test`, capture the same frame list into the failure before `jmp_restore`, and print it in `src/testrun.s` (text lines and the `failure.trace` JSON array).
- [X] T033 [US4] Update the expected outputs of every existing golden case with a runtime error (`tests/run.sh --accept` on exactly the cases whose only difference is the added locals lines/fields; inspect the diff of each by hand, and list any case whose diff is anything else as a bug). Also update `bench2` validation expectations only if they include runtime-error text (they should not: 703 runs compare stdout). Record the number of updated cases in `results.md` (FR-025). Accept T030's cases.

**Checkpoint:** US4 complete. `make test` passes.

---

## Phase 7: User Story 5 - Replay a failing run exactly (Priority: P2)

**Goal:** `vo run --record` and `vo replay` (FR-019–FR-022).

**Independent test:** `python3 tests/steps.py --repeat 3 tests/cases/replay` passes.

### Tests for User Story 5 (write first; they must fail now)

- [X] T034 [P] [US5] Step cases in `tests/cases/replay/` (record with `env ... vo run --record r.jsonl main.vo -- ARGS < input.txt`, then `rm` the inputs, then `vo replay r.jsonl`; `expected/` holds `r.jsonl` and `out`, which must show identical program output for both runs): `all_inputs` (args, stdin lines, `read_all`, env set and unset, a file read ok and missing, `now_ms` without `--clock-ms`: the recording keeps real values, so mask them in the expected recording by using `--clock-ms` in a twin case `all_inputs_clock`), `write_file` (not performed on replay; the file stays absent), `random_seed` (`--seed 7`), `random_noseed` (seed from the clock: the replayed output equals the recorded one; compare the two `out` sections instead of a fixed expectation), `runtime_error` (exit 70 both times), `exit_code`, `timeout` (an infinite loop with no output: exit 124 both times; output before a timeout is not compared, research R16), `denied` (R0010 replayed without the filesystem), `changed_same_inputs` (note, then new behaviour), `mismatch_path` and `mismatch_more` (R0014), `bytes` (invalid UTF-8 on stdin, `"bytes":true`), `invalid_recording` (truncated file, T0008), `replay_trace` (`vo replay --trace t.jsonl`), `replay_bad_option` (`--seed` with replay, T0001).

### Implementation for User Story 5

- [X] T035 [US5] Recording writer in `src/replay.s` (research R15, R16, contracts/recording.md): `rcd_mode`, the buffered writer (T008), the header (program path as given, FNV-1a 64 fingerprint over each program file's path and text in load order, the seed actually used from `rt_effects`, `clock_ms`, `args`), JSON string escaping with the `bytes` rule, the `end` record from `exit_hooks` with the exit status. Open the file at option time (T0009 if it cannot be created); reject a path equal to a program file after loading (T0001).
- [X] T036 [US5] Hooks in the eight input builtins (`rt_read_line`, `rt_read_all`, `rt_read_file`, `rt_write_file`, `rt_args`, `rt_env`, `rt_now_ms` in `src/rt_io.s` / `src/rt_effects.s`; `rt_random_int` needs none): when `rcd_mode` is 1, append the event after the builtin has its result (including `denied` when `rt_check_path` refuses, before R0010 is raised); when 2, call into `src/replay.s` instead of the outside (skip `rt_check_path`, `stdin_fill`, file and env access). With `rcd_mode` 0 the cost is one load and branch per call.
- [X] T037 [US5] Replay in `src/replay.s`: `cmd_replay` reads the recording (T0002 unreadable) with a JSON-lines reader that accepts exactly the writer's output (T0008 otherwise, also without header or `end`); sets the seed, clock and args from the header; loads the program from `program`; compares the fingerprint (note on stderr, or the `replay` JSON record, when it differs); runs it like `cmd_run` with `--trace`/`--timeout`/`--max-memory`/`--mem-stats` allowed. Each replayed input takes the next event; a different kind, path or variable, or no event left, raises R0014 naming both. Add T0008 and R0014 to `docs/errors.md`. Accept T034's cases.

**Checkpoint:** US5 complete. `make test` passes.

---

## Phase 8: User Story 6 - Evidence on the refactor tasks (Priority: P2)

**Goal:** the deterministic stand-in for "better refactor results" (FR-026, SC-001, SC-002).

**Independent test:** `python3 tests/refactor_eval.py` exits 0.

- [X] T038 [P] [US6] Write `tests/refactor_eval/rNN-*/edits.txt` for r01–r10, **before running any of them** (spec Assumptions): at least 3 edits per task, at least 30 in total, taken from what each `task.md` asks for (renames of functions, records, fields, locals; added fields), one per line as `rename TARGET NEW` or `add-field RECORD FIELD: TYPE = DEFAULT`. Commit them (the user commits; note the file list in `results.md`) before T040's first run.
- [X] T039 [P] [US6] Write `tests/refactor_eval/rNN-*/refs.txt` by hand from each starter: at least 3 names per task (30 in total), each as a line `callers NAME` or `refs TARGET`, followed by the expected `FILE:LINE:COL ROLE` lines and a blank line. Do not use `vo query` to produce them.
- [X] T040 [US6] Write `tests/refactor_eval.py` (research R19): per task, copy `bench2/tasks/<task>/starter/vo` to a temporary directory; record the starter's output on every `visible/*.in` and `hidden/*.in`; for each edit, on a fresh copy, run `vo edit` (must exit 0), `vo check` (clean) and every input (byte-identical to the starter); for each rename, on a fresh copy, whole-word text replacement over the program's files (for `Record.field`, the field name; for a position, the name at it), then check and run, counting results that fail to check or change behaviour; run each `refs.txt` query and compare (`FILE:LINE:COL ROLE` only); count whole-word text-search hits for the same names that are not in the expected list. Print a table per task and totals. Exit 1 if any `vo edit` result fails, any query differs, or there are fewer than 30 edits or 30 names. Add a `refactor-eval` Makefile target.
- [X] T041 [US6] Run it; record the table in `results.md` under SC-001/SC-002 (edits by `vo edit` that kept behaviour, edits broken by text replacement, queries matching, wrong text-search hits). A failing `vo edit` or query is a bug to fix in `src/`, not in the script; a wrong expectation in `refs.txt` is fixed only with the reason written in `results.md`.

**Checkpoint:** all stories complete.

---

## Phase 9: Polish & Cross-Cutting Concerns

- [X] T042 [P] `docs/reference.md` (FR-023): a section "Querying and editing programs" (`vo query`, `vo impact`, `vo edit`, the four target forms, `--dry-run`, refusal), and in "Command line" the new commands and options, and exit status 74 (an output file cannot be written, T0009) in the exit-status table; a section "Traces and replay" (`--trace`, `--trace-limit`, `--record`, `vo replay`, the note that recordings contain what the program read); "Machine-readable output" gains the new kinds in one line each with pointers, and runtime errors' `locals`. Add reference examples that `tests/ref_examples.py` runs: query/impact/edit examples as a new `vo-steps` example kind run through `tests/steps.py` (extend `ref_examples.py`), and a runtime-error example showing frame values. Measure the token count before and after (`tests/ref_examples.py` prints it); growth ≤ 2,000, total < 15,000 (SC-008). Record both in `results.md`.
- [X] T043 [P] `docs/errors.md`: confirm entries for T0004–T0009 and R0014 (added in T013, T015, T022, T026, T037) have name, summary, fixable, explanation, example, fix; extend T0001's text with the new usage errors. Add the spec 006 `contracts/codes.md` to `tests/catalog.py`'s code sources (as spec 005's was). Extend `tests/catalog.py` so that (a) it also reads the expected outputs of step cases (`expected/out` and every expected file in a folder with a `steps` file), and (b) every T code listed in a spec's `contracts/codes.md` needs a golden case, not only T0001/T0002, so T0004–T0009 and R0014 must each be produced by a case. `python3 tests/catalog.py` passes.
- [X] T044 [P] Fuzz the new inputs: extend `tests/fuzz.py` (or a new `tests/fuzz_inputs.py` run from `make test`) with malformed targets, malformed diffs and malformed recordings (random truncation and byte flips of valid ones): every run ends with a documented exit status and a report, never 125 or a crash (Principle III).
- [X] T045 Run the full gate: `make test`, `make gc-check` (traces, frame values and replay under the stress collector: add `VO_BIN=$(abspath build-stress/vo) python3 tests/steps.py` to the `gc-check` target), `make speed-check`, `python3 tests/speed.py --baseline build-base/vo` (SC-006), the 703 bench2 runs byte-identical (`tests/gc/baseline.py`), `make lint` (with `</dev/null`, sesi-linter-issues.md #19). Record every result in `results.md`.
- [X] T046 Update `README.md` and `docs/roadmap.md` (006 done, with the stand-in result and the follow-up: the agent benchmark run on the refactor category), and finish `results.md`: the SC table, deviations from the plan, `ponytail:` markers added, follow-ups.

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)** → **Foundational (Phase 2)** → stories.
- **US1 (Phase 3)** needs Phase 2. **US2 (Phase 4)** needs US1 (T011–T014).
- **US3, US4, US5 (Phases 5–7)** need only Phase 2 (T007 render cap, T008 exit hook and buffered writer). US4 uses T012's `N_ARM` type; do T012 first or move that line into T031.
- **US6 (Phase 8)** needs US1 and US2.
- **Polish (Phase 9)** after the stories it documents.

### Within each story

- Test cases first; they must fail on the current binary.
- `src/` tasks in the listed order (each builds on the previous one's globals).
- Accept golden outputs only after checking them by hand against the contracts.

### Parallel opportunities

- T002 and T003 in parallel after T001.
- T007 and T008 in parallel (different files) after T004–T006.
- Test-writing tasks of every story ([P]) can be written up front in parallel.
- After Phase 2, US3/US4/US5 can proceed in parallel with US1→US2 (different files: `trace.s`, `replay.s`, `rt_io.s`/`rt_effects.s` versus `query.s`, `edit.s`, `check*.s`), except where noted (`interp.s` is touched by T028 and T032: do those in sequence).
- T038 and T039 in parallel, before T040.
- T042, T043, T044 in parallel.

## Parallel Example: User Story 1

```text
Task: "T009 Golden programs and query cases in tests/cases/query/"
Task: "T010 Golden impact cases in tests/cases/query/impact/"
```

## Parallel Example: after Phase 2

```text
Track A: T011 → T012 → T013 → T014 → T015 (US1) → T018 … T023 (US2)
Track B: T025 → T026 → T027 → T028 (US3)
Track C: T034 → T035 → T036 → T037 (US5)
```

## Implementation Strategy

### MVP first (User Story 1)

1. Phase 1 and Phase 2.
2. Phase 3 (US1): queries and impact.
3. **Stop and validate:** `tests/run.sh tests/cases/query`, `make test`, speed unchanged.

### Incremental delivery

1. US1 → queries usable on their own.
2. US2 → checked edits (the refactor goal).
3. US6 → the stand-in evidence for SC-001/SC-002.
4. US3, US4, US5 → debugging features, in any order.
5. Polish → reference, catalog, fuzzing, full gate, results.

Each story adds value without breaking the previous ones; `make test` passes after every task.

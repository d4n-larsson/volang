# Results: Query, Structural Editing, Trace and Replay

## Success criteria

| SC | Status |
|---|---|
| SC-001 | **Met.** 53 scripted edits over r01-r10 (41 renames, 12 add-field): every `vo edit` result checks clean and prints byte-identical output to the starter on every visible and hidden input. The same 41 renames by whole-word text replacement break 12 programs (they fail to check or print something else): r01 3, r03 2, r05 6, r09 1. |
| SC-002 | **Met.** 95 hand-written reference lists (`refs` and `callers`) over r01-r10: all 95 answers match exactly. A whole-word text search for the same names returns 614 hits that are not references. |
| SC-003 | **Met.** `tests/cases/edit/`: 18 refusal cases (clashes of items, locals, fields and cases; keyword, builtin, non-identifier, same name, naming convention, `main`, a builtin target, unknown target, existing field, wrong default type, invisible default, not a record, a broken program whose errors would change, an unwritable directory), each with the files unchanged. |
| SC-004 | **Met.** 12 golden `vo impact` cases (`tests/cases/query/impact/`): body, comment, two files, added, removed, test, property, ignored file, stdin, a stale diff and a non-diff (T0007). |
| SC-005 | **Met.** Traces: 18 step cases, byte-identical over 3 runs (`steps.py --repeat 3`); frame values: 13 new golden cases plus 43 updated. Replay: 17 step cases (inputs of every kind, write not repeated, seed, exit code, timeout, R0010 replayed, changed program noted, R0014 for another file and for more input, invalid UTF-8 as `bytes`, invalid and unreadable recordings, `--trace` on replay, bad options), byte-identical over 3 runs. **Met.** |
| SC-006 | **Met.** Final binary against spec 005 (`speed.py --baseline build-base/vo`): 0.957 (map_count) to 1.012 (fib), all within 5%. `make speed-check`: 4.47x over the tree walker, 10,000 generated programs with 0 disagreements. 703/703 bench2 reference runs unchanged (normal and stress). The existing corpus is unchanged apart from the frame values (43 cases) and the usage messages that list the new commands (3 cases). |
| SC-007 | **Met.** Query half: on the largest refactor starter (r03, 473 lines in 4 files) `query refs`, `query callers` and `impact` take 0.05-0.06 s under qemu, the same as `vo check`. Edit half: `vo edit rename --dry-run` (two checks) takes 0.04-0.06 s on the same program. |
| SC-008 | **Met.** ~9,144 to ~10,000 tokens (+856, limit +2,000; budget 15,000); 33 examples pass, including the new `console` blocks (query, type-at, dry-run rename) that `ref_examples.py` now runs. |
| SC-009 | **Met.** `make test` (412 golden cases, 71 step cases, repeats, fuzzers including the new `fuzz_inputs.py`, catalog, reference examples, migration, gc checks, 1,000 engine comparisons) and `make gc-check` (stress build over every golden and step case, 3,000 fuzz iterations, every fault kind caught, 1,000 stress comparisons) pass; `make lint` is clean. |

## Speed baseline

`make baseline` builds `build-base/vo` from `99b5d3f` (spec 005); the `build-004` worktree is removed.
The current tree against it, before any spec 006 code (`tests/speed.py --baseline build-base/vo`),
shows the noise of the method: 0.945 (map_count) to 1.015 (str_build), all within 5%.

## US1 notes

- **The reference log** is filled by hooks next to the checker's existing `rename_use` calls, at
  member resolutions, in `sym_new` (declarations and import copies) and at builtin and undefined
  calls. Field and case declarations are added after the check from the record and variant items.
- **type-at** records every checked expression while the log is on (`q_exprs`) instead of a
  position set before checking: a position can name an imported file, which only exists once the
  program is loaded. The answer is the smaller of the innermost name and the innermost expression.
- **Record literals** are logged with key `(record, -2)`, for `vo edit add-field`.
- **SESI**: two messages with an apostrophe in a `.string` were reported as bad character literals
  (sesi-linter-issues.md #20, false positive); reworded.
- **Impact over-reports at a removed block's edge**: a run of removed lines placed by the diff just
  inside the previous function (difflib/diff put `-}` before the kept `}`) marks that function
  changed (`removed_fn`: `tax`). Impact is meant to be conservative, so this is accepted.

## US2 notes

- **No overlay map**: the edited texts are registered with `file_add` as generation 1 before the
  second `check_program`, which finds them through `file_find` (only the current generation) and
  lexes and parses them like a file seen only by a missing-import scan. `load_file` is unchanged.
- **check_reset** turned out to be small: `check_program` already starts with `ck_init`,
  `fx_program_begin` and fresh `ld_order`/`ld_stack`; the rest is `diag_reset`, the reference log,
  `ld_hard`/`p_hard`, `te_related` and the generation. Intern tables (types, generic memo) are keyed
  by declaration nodes, so the second generation's nodes get their own entries.
- **Reset sweep** (T019): `vo edit rename --dry-run FILE f f_zq` on every golden program with a
  function other than `main`: 154 accepted, 1 refused correctly (`check/duplicate_fn`: renaming one
  of two `helper`s would remove an existing E0202), 2 stopped by syntax errors (65), 1 by an
  unresolvable name (64). No spurious refusal.
- **Changes are listed after the files are written** (a write failure then lists nothing).
- **vo.effects** is found by `rec_find`, which registers it by its absolute path, so its changes
  show that path (step cases strip the working directory).
- **Refusals** show the new errors with spans mapped back to the current text and their fixes
  dropped (they were for the edited text).

## US3 notes

- **Tracing handlers** live in `src/trace.s`; `vm_ops_trace` is built at start-up as a copy of
  `vm_ops` with the OPC_RET, OPC_TRY and OPC_TRACE_ENTER entries replaced, so the two tables cannot
  drift. `c_emit` reads its table through `c_ops`. Only `.globl` labels were added to `vm.s`
  (`op_ret`, `op_try`); no instruction moved (`vm_run` stays at page offset 0xcb0).
- **A failing `expect`** in `vo test` ends the test without a `return` or `error` record (it is not
  a runtime error); the next `test` record follows.
- **Layout regression found and fixed**: the first speed run after the trace code showed
  `list_work` at 1.81x the spec 005 binary (518 to 936 ms), reproducibly, with identical VM code.
  `list_sort` had moved from page offset 0xcf0 to 0xeec (code added to `sys.s`, `interp.s` and
  `rt_render.s`, which link before `rt_list.s`). `rt_list.s` now starts at spec 005's page offset
  (0xa30) like `rt_map.s`, and `list_work` is back to 0.48 s on both binaries.

## US4 notes

- **Updated expectations (FR-025)**: 43 existing golden cases whose runtime errors now show values
  (text lines and JSON `locals`), each checked to differ only by them; `cli/mem_stats_test` also
  reports a 5.7 KB higher peak, because building the error's values allocates.
- **Timeouts show no values** (deviation from US4 scenario 5): the values at an R0009 depend on how
  far the program got, which depends on the machine's speed, so showing them would make the report
  differ between runs (Principle III). The frames are still shown in `vo run`; `vo test` timeout
  reports are unchanged.
- **Unset references are skipped**: a slot of a reference type that is still 0 is not shown.

## US5 notes

- **Interception** is in the builtin stubs of `rt_call.s`, which now call `io_*` wrappers in
  `src/replay.s` for read_line, read_all, read_file, write_file, env and now_ms; with recording off
  each wrapper tail-calls the runtime function it replaces. `args` and the seed are in the header.
- **The seed** of a run without `--seed` is chosen in `rcd_start` (from the clock, as before) and
  passed as if `--seed` had been given, so the header can name it without changing `rt_effects.s`.
- **Writing a recording allocates nothing**: events go straight to the file buffer with their own
  JSON escaping (`\u00XX` for control bytes, and for every byte >= 0x80 when the value is not valid
  UTF-8, marked `"bytes":true`). A read or write that R0010 stops is written as `denied` from the
  exit hook.
- **Replayed `denied`** repeats the path check (`rt_check_path`) to raise the same R0010, the one
  place replay looks at the filesystem (path resolution, not file contents); if the check now passes,
  it is R0014.
- **env in replay** still checks the name against the grant (`rt_check_name`), which reads nothing
  from outside.
- **steps.py** gained `vo~` (stdout not compared) and `same` (the last two hidden stdouts must be
  equal), for `random_noseed`, whose numbers depend on a clock seed.

## US6 notes

- The edit scripts and reference lists (`tests/refactor_eval/rNN-*/edits.txt`, `refs.txt`) were
  written by a separate agent from the task statements and starters, without `vo query`/`vo edit`;
  it checked each edit and list by applying it to a copy of the starter and running `vo check` and
  the task's inputs. They were not committed before the first evaluation run (T038 asked for that;
  nothing is committed until the user asks): the first run's results are the ones above.
- r04 and r10 add features rather than rename things, so their renames are only loosely implied by
  the statements (each has a comment saying why).
- `make refactor-eval` runs the evaluation (not part of `make test`: it takes minutes).
- Run table (`python3 tests/refactor_eval.py`):

  | task | edits | vo ok | renames | text bad | names | query ok | grep extra |
  |---|---|---|---|---|---|---|---|
  | r01 | 8 | 8 | 8 | 3 | 12 | 12 | 105 |
  | r02 | 4 | 4 | 2 | 0 | 8 | 8 | 45 |
  | r03 | 7 | 7 | 5 | 2 | 11 | 11 | 39 |
  | r04 | 5 | 5 | 3 | 0 | 9 | 9 | 35 |
  | r05 | 6 | 6 | 6 | 6 | 10 | 10 | 133 |
  | r06 | 4 | 4 | 3 | 0 | 9 | 9 | 84 |
  | r07 | 5 | 5 | 4 | 0 | 10 | 10 | 49 |
  | r08 | 5 | 5 | 5 | 0 | 8 | 8 | 49 |
  | r09 | 5 | 5 | 2 | 1 | 9 | 9 | 45 |
  | r10 | 4 | 4 | 3 | 0 | 9 | 9 | 30 |
  | total | 53 | 53 | 41 | 12 | 95 | 95 | 614 |

## Deviations

- **Migration corpus:** `tests/migrate.py` records one intended change from the v1 corpus: the
  runtime error in `test-runtime_error` now lists its frames and values (FR-025).
- **FR-025, second exception:** the T0001 messages for an unknown command, a missing command and an
  unknown option list every command and option, so they now name the new ones. Three golden cases
  (`cli/bad_flag`, `cli/unknown_command`, `cli/unknown_command_json`) were updated for that alone.

## Gate (T045)

| Check | Result |
|---|---|
| `make test` | exit 0 |
| `make gc-check` | exit 0; step cases also pass under the stress build |
| `make speed-check` | exit 0; 4.47x geometric mean, 10,000 programs, 0 disagreements |
| `speed.py --baseline build-base/vo` | 0.957-1.012 |
| `make lint` | clean (with `</dev/null`, issue #19) |
| `make refactor-eval` | 53/53 edits, 95/95 queries |

Problems found on the way and fixed: the replay reader crashed on recordings whose events lack a
value (found by `fuzz_inputs.py`; events are now checked, T0008); `diff_engines.py` compared runtime
errors with the spec 003 oracle, which has no frame values (the value lines are dropped before
comparing); two SESI false positives or limitations (#21, #22).

## ponytail markers added

- `src/edit.s`: the re-check runs in the same process after a reset (a fresh process if qemu allows).
- `src/query.s`: diff and program paths matched by text and suffix (no realpath); impact follows
  calls only (type-level impact if reviews need it).
- `src/trace.s`: 100,000 records by default, then only counted (a per-function filter).
- `src/rt_render.s`: one fixed 200-byte cap for shortened values.
- `src/rt_list.s`: fixed page offset (spec 005's), like `vm.s` and `rt_map.s`.

## Follow-ups

- The agent benchmark run on the refactor category, with and without `vo query`/`vo edit`
  (roadmap Benchmarks 4).
- Other edits (extract a function, move an item to another file, change a parameter list).
- Impact through types (a changed record reaching the functions that use it).
- A per-function trace filter; showing the vo.effects path relative to the working directory.
- Commit the refactor edit scripts (`tests/refactor_eval/`) with the rest of the spec.

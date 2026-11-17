---

description: "Task list for vo-lang spec 018: agent-friendly testing"
---

# Tasks: Agent-Friendly Testing

**Input**: Design documents from `specs/018-agent-testing/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R14), data-model.md, contracts/cli.md,
contracts/codes.md, contracts/language.md, quickstart.md

**Tests**: REQUIRED (constitution IV): golden and step cases land first and fail before each piece.

**Organization**:
- Setup, then the option parsing and identifiers every story needs.
- One phase per user story: US1 and US2 are P1, US3 and US4 are P2.
- Docs and the gate last.

Dependencies after Phase 2:
- US1 (selection, listing) and US2 (failure reports) touch mostly different code and can proceed in
  parallel, but both edit `src/testrun.c`'s `report_test`, so merge with care.
- US3 (coverage) is independent.
- US4's mutation testing builds on US3's coverage points (reach).

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: `.vo` + `.out`/`.err`/`.code`, with an optional `.cmd` such as
  `test --json --filter parse {}`. They run in an empty directory (`tests/run.sh`).
- **Step cases**: anything that needs a diff file, a package or several commands is a step case
  (`tests/cases/<area>/<name>/steps`, format in the `tests/steps.py` docstring).
- **Areas**: `tests/cases/test/` (existing), and new `tests/cases/coverage/`, `tests/cases/mutate/` and
  `tests/cases/test_steps/`.
- **Regression rule**: existing programs keep byte-identical results, except two documented changes:
  - the new `"id"` field in JSON test records (FR-019, amended);
  - text-mode output of passing tests that print, now hidden (research R4).

  Run `make test` after every task.
- **C code**: every new line compiles with zero warnings under the strict set and has no UB. The capture
  buffer, coverage table and reach matrix have explicit limits. Syscalls go only through `vo_syscall*`.
  Shortcuts are marked `ponytail:` (plan.md, Complexity Tracking).
- **Identifiers**: `FILE::NAME`, FILE exactly as the `file` field of reports prints it (research R1).

---

## Phase 1: Setup

- [X] T001 Create `specs/018-agent-testing/results.md` with:
  - an SC-001 to SC-006 table;
  - a verification table;
  - "Behavior changes" and "Deviations" sections, as in `specs/017-filesystem-processes/results.md`.

  Also:
  - add `018-agent-testing` to the spec tuple in `tests/catalog.py`, line 87, so
    `contracts/codes.md` is checked;
  - create the directories `tests/cases/coverage/`, `tests/cases/mutate/`, `tests/cases/test_steps/`
    and `tests/mutate_eval/`.
- [X] T002 [P] Catalog entries in `docs/errors.md` for W0602 `test_checks_nothing`, W0603
  `constant_check`, T0011 `unknown_test` and T0012 `mutation_needs_passing_tests`:
  - use the names, exit statuses, messages and examples in `contracts/codes.md`;
  - place each in its range (W0602/W0603 after W0601, T0011/T0012 after T0010);
  - add golden cases `tests/cases/test/explain_w0602`, `explain_w0603`, `explain_t0011` and
    `explain_t0012` (`.cmd` `explain W0602`, and so on).

  `tests/catalog.py` passes once the cases exist.

---

## Phase 2: Foundational — options and identifiers (blocks every story)

- [X] T003 Failing golden cases for option misuse (T0001, exit 64), in `tests/cases/test/` with `.cmd`
  files:
  - `list_and_coverage` (`test --list --coverage {}`);
  - `mutate_and_list`;
  - `max_mutants_without_mutate` (`test --max-mutants 5 {}`);
  - `max_mutants_range` (`test --mutate --max-mutants 0 {}`);
  - `list_with_run` (`run --list {}`);
  - `spec_option_with_check` (`check --spec A {}`);
  - `diff_with_run` (`run --diff x.diff {}`).

  The messages follow the existing `only_with` wording.
- [X] T004 Options in `src/main.c`:
  - parse `--list`, `--coverage`, `--mutate` (flags), `--test ID` and `--spec ID` (repeatable: push onto
    two `Vec *`), and `--max-mutants N` (1-100000, default 500);
  - allow `--diff` for `test` as well as `impact`;
  - extend the usage text at `src/main.c:264` and add the `only_with` rules of `contracts/codes.md`
    (T0001 row): `--list`, `--coverage` and `--mutate` are exclusive, and `--max-mutants` needs
    `--mutate`;
  - declare the globals (`opt_list`, `opt_coverage`, `opt_mutate`, `opt_tests`, `opt_specs`,
    `opt_max_mutants`) in `src/vo.h`.

  T003 passes.
- [X] T005 Test identifiers in `src/testrun.c`:
  - `Str *test_id(Node *test)` returns `FILE::NAME`, FILE as `file_get(test->file)->path`;
  - `int id_split(Str *id, Str **file, Str **name)` splits at the first `::` that follows `.vo`.

  Declare both in `src/vo.h`. Restructure `cmd_test` (`src/main.c:477`) so that after checking it
  branches on `opt_list` / normal run / `opt_coverage` / `opt_mutate`, all through
  `run_file_tests`, which takes a "selected" predicate (T009). Behavior is unchanged at this point.

**Checkpoint**: `make test` passes; the new options parse and reject misuse.

---

## Phase 3: User Story 1 — Find and run exactly the right tests (P1) 🎯 MVP

**Goal**: list tests without running anything; run exactly one test by identifier, the tests for a
`@spec` ID, or the tests a diff affects.

**Independent Test**: the listing is identical on every run; every listed identifier runs exactly that
test; `--list` leaves a would-be-written file absent.

### Tests (first, failing)

- [X] T006 [P] [US1] Golden cases in `tests/cases/test/`:
  - `list_text` (two files' worth in one file: a test with a doc comment of two lines, a `prop` with
    `@spec("FR-003")` and `uses console`, a plain test; expected text per `contracts/cli.md` Listing,
    ending `3 tests`);
  - `list_json` (`test_entry` records with `id`, `name`, `test_kind` (`"test"`/`"property"`), `file`,
    `start`, `end`, `doc` (`""` without a comment; lines joined by `\n`), `uses` (`""` if none), `spec`
    (source order); summary with `"listed":3`);
  - `list_check_error` (a type error: only the diagnostic, exit 65, no listing);
  - `select_exact` (tests `parses` and `parses negative`; `.cmd` `test --test {}::parses {}`; only
    `parses` runs);
  - `select_two` (two `--test`: union);
  - `select_spec` (`--spec FR-003`);
  - `select_spec_prefix` (`--spec US1` selects `@spec("US1/AC2")`);
  - `select_intersection` (`--filter` and `--spec` together);
  - `select_empty` (a `--filter` and `--spec` with no common test: `0 tests selected`, exit 0; JSON
    `"selected":0`);
  - `unknown_id` (`--test {}::parse`: T0011 "No test has the identifier ..." with "Closest:
    `...::parses`", exit 64, nothing runs);
  - `unknown_spec` (T0011 "No test names `X` in `@spec`.").

  How `{}` expands inside an identifier: check `tests/run.sh`'s substitution. If it only replaces a
  whole word, use the literal file name the runner passes (the case's `.vo` path) and note it in the
  case.
- [X] T007 [P] [US1] Step cases in `tests/cases/test_steps/`:
  - `diff_select`: `main.vo` with functions `a`, `b` and tests for each; a unified diff `change.diff`
    touching `b`; steps `vo test --diff change.diff main.vo` and `vo test --list --diff change.diff
    main.vo`; only `b`'s tests;
  - `diff_none`: a diff touching only a comment line outside functions; `0 tests selected`, exit 0;
  - `diff_mismatch`: T0007;
  - `list_runs_nothing`: a test `uses file.write("out/")` that writes `out/x.txt`; steps `vo test
    --list main.vo` then `keep` nothing, with an expected `out` showing the listing, and a step
    proving `out/x.txt` is absent (e.g. `vo run check.vo`, a program with
    `uses console, file.read("./")` that prints `info("out/x.txt")` via `std.fs`);
  - `json_ids`: `vo test --json` on two files in a directory; every record's `id` starts with its file.

### Implementation

- [X] T008 [US1] `id` in every `test` record (`report_test` in `src/testrun.c`), right after `"kind"`
  (`{"v":1,"kind":"test","id":...,"name":...`). Regenerate every existing golden and step expectation
  that contains a JSON `test` record (find them with `grep -rl '"kind":"test"' tests/cases`), review
  that the diff is only the added field, and list the cases in `results.md` "Behavior changes".
- [X] T009 [US1] Selection in `src/testrun.c`: `static int selected(Node *test, long index, Vec *items)`
  implementing data-model.md "Selection":
  - `--filter` substring;
  - any `--test` equal to `test_id`;
  - any `--spec` X equal to an `@spec` ID or a prefix of one followed by `/` (reuse `sp_of`'s scan
    of the annotations above the item);
  - membership in the diff set (T011).

  Before any test runs, `cmd_test` validates every `--test` and `--spec` against all loaded tests:
  - an unknown one is T0011 through `toolchain_error`, exit 64;
  - its message lists up to three closest identifiers, ranked by edit distance with ties in run order.
    Reuse the checker's "did you mean" distance function; find it with
    `grep -n "distance\|did you mean" src/*.c`.

  Count the selected tests; the JSON summary gains `"selected"`, and with none selected the text
  summary is `0 tests selected`.
- [X] T010 [US1] `--list` in `src/testrun.c`: `list_file_tests(int file)`.
  - It reports the check results as `cmd_test` does (`diag_emit_all`, exit 65 on errors). For each
    selected `N_TEST` it prints the text line `ID (test|property, FILE:LINE:COL)`, then ` spec A, B`,
    then ` uses ...` (via `fx_render`), then the doc lines indented two spaces. In JSON it prints a
    `test_entry` record (contracts/cli.md).
  - Doc comment: add `Str *doc_block(long file, Vec *items, long index)` in `src/doc.c`, next to
    `doc_comments`. It returns every `#` line directly above the item, skipping its `@spec`
    annotations, with `# ` removed and lines joined by `\n`; `""` when there are none.
  - The summary is `N tests` in text (`1 test` singular) and `"listed":N` in JSON.
  - It never calls `compile_program` or `interp_init`.
- [X] T011 [US1] `--diff` for `vo test` (research R3):
  - in `src/query.c`, add `Vec *impact_tests(char *diff_path)`. It reads the diff, then runs
    `df_parse`, `im_items`, `im_removed` and `im_callers` and returns the test nodes whose role is
    `r_test`. Reuse `cmd_impact`'s reading code (`src/query.c:995`) and its T0002 and T0007 errors;
  - in `cmd_test`, set `q_collect = 1` before `check_program` when `opt_diff` is set, and pass the
    returned set to `selected`;
  - make sure `q_collect` does not change test-mode diagnostics (compare with the T006 cases).

**Checkpoint**: T006-T007 pass; `vo test --list` works on `examples/`.

---

## Phase 4: User Story 2 — Understand a failure without reading the whole test (P1)

**Goal**: comparison operands, the first difference, a cause per failure, captured output.

**Independent Test**: one golden case per cause, per comparison operator and per kind of difference, in
text and JSON; `vo test --json` output parses line by line.

### Tests (first, failing)

- [X] T012 [P] [US2] Operand golden cases in `tests/cases/test/`, each in text, and with a `_json`
  twin (`.cmd` `test --json {}`) for at least `cmp_lt` and `cmp_and`:
  - `cmp_lt`: `expect(len(xs) < limit)` with 7 and 5. Message "`expect` failed: `len(xs) < limit` is
    false." and the lines `    len(xs): 7` and `    limit:   5`, aligned;
  - `cmp_ops`: one failing test each for `==`, `!=`, `<=`, `>`, `>=` on `int`, `float` and `str`;
  - `cmp_literal_side`: `expect(n > 3)`, which lists only `n`;
  - `cmp_and`: `expect(a > 0 and b > 0)` with `b` false. It reports `b > 0` and its operand, at
    `b > 0`'s span;
  - `cmp_and_short`: with `a` false, `b`'s side is never evaluated. Prove it with a function that
    prints;
  - `cmp_not`: `expect(not (x == 3))`, with the message "`x == 3` is true.";
  - `cmp_or`: both false. The message names both parts, with no operand lines;
  - `cmp_values_long`: a 1,000-element list operand, shortened to 200 bytes;
  - `cmp_pass_effects`: the passing path prints and calls in the same order as before. Compare with
    `vo run` of the same calls.
- [X] T013 [P] [US2] Difference golden cases in `tests/cases/test/`:
  - `diff_list_record`: two lists of 1,000 records differing in `.name` of element 412. The text has
    `first difference at [412].name: expected "bob", actual "bab"`; the JSON twin has `difference`;
  - `diff_length`: `[1, 2, 3]` against `[1, 2, 3, 4]`. The path is `[3]`, the actual side is `(none)`
    (JSON `null`), and both lengths are given;
  - `diff_variant_case`: `Shape.circle(1.0)` against `Shape.square(1.0)`;
  - `diff_payload`: the same case with different payloads, path `.circle`;
  - `diff_map_key`: a missing key `["b"]` and a different value;
  - `diff_result`: `ok` against `err`;
  - `diff_string`: two 200-character strings differing at character 120. The JSON has `"path":""`
    and `"character":120`; the excerpts are up to 40 characters;
  - `diff_ordered`: an `ordered_map` and a `deque` differing at one position;
  - `diff_scalar`: an `int` mismatch, unchanged (no difference line).
- [X] T014 [P] [US2] Cause golden cases in `tests/cases/test/`. Each has `.cmd` `test --json {}` and
  checks `"cause"` (research R7):
  - `cause_check_failed`;
  - `cause_too_few_inputs` (a `prop` with `assume(false)`);
  - `cause_error_in_code` (division by zero in a called function);
  - `cause_error_in_test` (an index out of bounds in the test's own statement);
  - `cause_requires_by_test` (the test calls a function with a failing `requires`);
  - `cause_requires_by_code` (a function calls another with a failing `requires`);
  - `cause_ensures`;
  - `cause_hole`;
  - `cause_outside_grant` (the test declares `uses file.read("data/")` and calls a function declaring
    the same, which reads a path built at run time, `"data/" + "../secret.txt"`. It checks clean, and
    the run is R0010 at run time; a literal outside path would be E0503 at check time instead);
  - `cause_timeout` (`.cmd` `test --json --timeout 1 {}`, infinite loop);
  - `cause_out_of_memory` (`--max-memory 1`, a growing list);
  - `cause_task_error` (an error inside `concurrent_map`, which is `error_in_code`).
- [X] T015 [P] [US2] Captured output golden cases in `tests/cases/test/`:
  - `output_json`: a test prints three lines then fails. The record has `"output"`, and no bare lines
    appear on stdout;
  - `output_text`: the same in text, with the `  output:` block under the failure;
  - `output_passing_hidden`: a passing test that prints shows only `PASS`;
  - `output_prop`: a failing `prop` printing its input. Only the final counterexample's line is kept;
  - `output_task`: printing inside `concurrent_map` in a failing test, captured in input order;
  - `output_limit`: 70,000 bytes printed, cut with `... N more bytes not shown`;
  - `output_stderr`: `eprint` is not captured. It is the case's `.err`.

  Update `tests/cases/test/no_tests` and every other existing case whose tests print (find with
  `grep -l 'print(' tests/cases/*/*.vo | xargs grep -l '^test \|^prop '`) and list them in
  `results.md`.

### Implementation

- [X] T016 [US2] Capture (research R4):
  - in `src/rt_io.c`, add a static `char cap_buf[65536]`, `long cap_len`, `long cap_dropped` and
    `int cap_on`. `rt_print` with fd 1 appends to it while `cap_on`, and counts dropped bytes beyond
    65,536;
  - in `src/task.c:476`, a forwarded task's stdout (`k == 0`) goes to the same append function;
  - `src/testrun.c` sets `cap_on` and empties the buffer per test, and turns it off for reporting;
  - `src/prop.c`, in `prop_run`, empties the buffer right before the final counterexample rerun
    (the `pr_try(test, pr_best)` after `pr_shrink`);
  - `report_test` adds `"output"` (JSON, only when `cap_len > 0`; the text, then
    `\n... N more bytes not shown` if any were dropped). In text, for status ≠ pass, it adds `  output:`
    and each line indented four spaces;
  - mark the 64 KiB limit `ponytail:`.
- [X] T017 [US2] Causes (research R7):
  - in `src/vm.c` `capture_trace`, set a global `rt_err_in_test = 1` when the innermost frame's
    declaration is an `N_TEST`; a task-forwarded error (`src/task.c:392`) sets it to 0;
  - `report_test` derives `cause` from status, `pr_starved`, `rt_err_code` (R0006, R0009, R0010, R0011,
    R0012, R0013) and `rt_err_in_test`, and emits `"cause"` for every non-pass record (JSON only).
- [X] T018 [US2] Comparison operands (research R5):
  - in `src/compile.c` `cbuiltin` (or a new `cexpect`), when the builtin is `expect` (B_EXPECT, 29):
    - **comparison** (`OP_EQ`, `OP_NE`, `OP_LT_INT`..`OP_GE_STR`, with the float and str ranges;
      `OP_LT_PARAM` never reaches `expect`, because tests are never generic): compile left, then right, into temporaries, then the comparison into a third, then
      `OPC_CALL_BUILTIN` for the new `BU_EXPECT_CMP` with three slots (cond, left, right);
    - **`and`**: compile as two `expect` calls in sequence, each recursing, so the right side runs
      only if the left one held;
    - **`not` of a comparison**: the same as a comparison, with an inverted flag in the node or a
      second builtin id `BU_EXPECT_NCMP`;
    - **anything else**: unchanged.

    Check that the tree walker fallback (`c_fail`) still handles any construct that is not compiled.
  - `src/rt_call.c`: add `BU_EXPECT_CMP` (and `BU_EXPECT_NCMP`) to the enum and dispatch, and update
    the static_assert;
  - `src/rt_test.c`: `rt_expect_cmp(long cond, long left, long right, Node *cmp, int negated)`. On
    failure it sets `tf_msg` ("`expect` failed: `TEXT` is false." / "... is true."), with TEXT the
    comparison's source text. It sets `tf_node` to the comparison and stores rendered operands that are
    not literals (`render_value` with each operand's type, shortened to 200 bytes) in two new globals
    `tf_ops[2]` with their source texts;
  - `rt_expect` on an `or`: message "`expect` failed: neither `A` nor `B` is true.";
  - `report_test` prints the operand lines, aligned on the longest text + 1, and JSON
    `failure.operands`;
  - clear the new globals with the others after each test (`src/testrun.c`, end of
    `run_file_tests`).
- [X] T019 [US2] First difference (research R6), in `src/rt_test.c`:
  - `static int first_diff(SB *path, long a, long b, Type *t, long *exp_out, long *act_out, Type **at)`
    walks by type: list/deque/ordered set/priority queue in iteration order `[i]`, record fields in
    declaration order `.f`, variant/result case then payload `.case`, map/ordered map keys `[k]` in
    expected order then extras, strings by character;
  - leaves are compared with `values_equal`;
  - `rt_expect_eq` calls it for non-scalar types and for strings longer than 40 characters, and stores
    the path, both rendered values (or none for a missing side) and the character position in
    `tf_diff_*` globals;
  - `report_test` prints `    first difference at PATH: expected X, actual Y` (`(none)` for a missing
    side, plus `(length A, expected B)`), and JSON `failure.difference`.

  For a large value, the walk must stay linear: no rendering of whole sub-values except at the leaf.
- [X] T020 [US2] JSON stream check: in `tests/run.sh` (or a small `tests/json_stream.py` run by
  `make test`), for every golden case whose `.cmd` contains `test --json`, check that every stdout line
  parses as JSON. Also check that every `"kind":"test"` record whose `status` is not `"pass"` has a
  `"cause"` from the R7 list (SC-002), in golden expectations and in step cases' `expected/out`.

**Checkpoint**: T012-T015 and T020 pass; the full golden suite has a `cause` on every non-pass record.

---

## Phase 5: User Story 3 — See what the tests never ran (P2)

**Goal**: functions, branches, cases and loop bodies no selected test ran, with spans and totals.

**Independent Test**: a fixture with known gaps reports exactly those, identically on every run and
platform.

### Tests (first, failing)

- [X] T021 [P] [US3] Golden cases in `tests/cases/coverage/`, each with `.cmd` `test --coverage {}` and
  a `_json` twin for `kinds`:
  - `kinds`: one gap of each point kind (`function`, `lambda`, `then`, `else`, `if_false`, `case`,
    `loop`) and covered counterparts. Expected text per contracts/cli.md (`not run: ...` in file/source
    order, then `coverage: X of Y functions, A of B branches`);
  - `all_covered`: no `not run` lines;
  - `function_values`: code reached only through a function value and `map` counts as run;
  - `tasks`: code run only inside `concurrent_map` counts as run;
  - `selection`: `--coverage --filter one`, where only the selected tests count;
  - `failing_test`: coverage is still reported and the exit status stays 1;
  - `no_tests`: every function is listed as never run.
- [X] T022 [P] [US3] Step case `tests/cases/test_steps/coverage_std_pkg`: a program that imports
  `std.text` and a local package (copy the layout from an existing package step case; find one with
  `grep -rl "vo package" tests/cases/*/*/steps`). Only the program's own file appears.

### Implementation

- [X] T023 [US3] `OPC_COVER`:
  - in `src/vo.h`, add `#define OPC_COVER (89)` and bump `OPC_COUNT` to 90;
  - in `src/vm.c`, add a handler in both `vm_ops` and `vm_ops_trace`. It sets `cov_hit[k] = 1`, with k
    in `I_A`. If `cov_reach` is set (US4), it also sets `cov_reach[cur_test * cov_nfn + fnindex]`
    when the point is a function or lambda entry, with the function index in `I_B`;
  - `cov_hit` and `cov_reach` are mapped with `MAP_SHARED | MAP_ANONYMOUS` (and `MAP_NORESERVE` for
    reach) before tests run, so forked tasks write the same bytes;
  - check that the instruction size and dispatch layout (`Instr`, 32 bytes) accept the new opcode
    unchanged.
- [X] T024 [US3] Points in `src/compile.c`, only when `opt_coverage || opt_mutate`, and only in files
  with `file_get(f)->std == 0 && !file_get(f)->pkg`:
  - a static table `{Node *node; char kind; Node *fn;}`, at most 65,536 entries (more is T0003 with a
    message; mark `ponytail:`);
  - emit `OPC_COVER k`:
    - after `c_prologue` (function or lambda, kind by `decl->kind`);
    - at the start of an `if`'s then-block and else-block;
    - for an `if` without `else`: emit `JMP over` after the then-block, then the landing with
      `OPC_COVER` (`if_false`), then `over`;
    - after each `match` arm's binding;
    - at the start of each `while`/`for` body;
  - with the options off, the emitted code must be byte-identical to before (the speed gate and
    `tests/diff_engines.py` show it).
- [X] T025 [US3] Report in `src/testrun.c`, after the `@spec` summary and before the summary, when
  `opt_coverage`:
  - the points never hit, sorted by file id, start offset, then kind order. Text is
    `not run: FILE:LINE:COL KIND in NAME` (`function NAME` for a function point); JSON is a `coverage`
    record with `point`;
  - then the totals (`coverage_summary` in JSON). Functions count `function` and `lambda` points;
    branches count the rest;
  - coverage never changes a status or the exit code.

**Checkpoint**: T021-T022 pass; coverage output identical over 20 runs (the determinism step, T034).

---

## Phase 6: User Story 4 — Find out whether the tests would catch a mistake (P2)

**Goal**: warnings for tests that cannot fail; mutation testing that lists changes no test catches.

**Independent Test**: the `is_adult` fixture reports `age >= 18` → `age > 18` as surviving, and it is
caught once a test for 18 is added. Each warning has a case.

### Tests (first, failing)

- [X] T026 [P] [US4] Warning golden cases in `tests/cases/test/` (`.cmd` `check {}` and `check --json
  {}`):
  - `checks_nothing`: a test that only prints. W0602 at the test's name, exit 0;
  - `checks_nothing_hole`: a test with `todo` gives only W0601;
  - `checks_nothing_lambda`: an `expect` inside a lambda in the test means no warning;
  - `constant_check`: `expect(true)`, `expect(not false)`, `expect(1 < 2)`, `expect_eq(3, 3)`,
    `expect_eq([1, 2], [1, 2])`. W0603 on each;
  - `not_constant`: `expect_eq(f(3), 3)` gives no warning.
- [X] T027 [P] [US4] Mutation golden cases in `tests/cases/mutate/` (`.cmd` `test --mutate {}`, JSON
  twins for `adult`):
  - `adult`: the quickstart fixture. The baseline records are followed by
    `SURVIVED ...:2:12 in is_adult: \`age >= 18\` -> \`age > 18\` (2 tests ran)`, the summary line and
    the candidates note, exit 0;
  - `adult_fixed`: with `test "eighteen"`, that change is not listed;
  - `baseline_fails`: a failing test gives its records, T0012 and exit 1;
  - `infinite`: a change that loops forever (`set i = i + 1` → `i - 1` under `while i < n`) counts as
    caught and the run ends;
  - `runtime_error`: a change that divides by zero counts as caught;
  - `contract`: a function with `ensures` whose change breaks it counts as caught;
  - `cap`: `--max-mutants 2` gives `not tried` > 0 in the summary;
  - `not_reached`: a function no test calls produces no changes;
  - `kinds`: one example of each row of research R10 (comparison, arithmetic, `and`/`or`, booleans,
    `not`, integer `n + 1`), with weak tests so each survives and shows its `text`/`mutated`;
  - `contracts_untouched`: operators inside `requires`/`ensures` are never changed;
  - `std_untouched`: calls into `std.text` produce no changes in library code.
- [X] T028 [P] [US4] Step case `tests/cases/test_steps/mutate_diff`: a diff touching one of two functions.
  `vo test --mutate --diff change.diff main.vo` changes only that function.

### Implementation

- [X] T029 [US4] W0602 and W0603 (research R9) in the checker, where test bodies are checked
  (`src/check.c` / `src/check_expr.c`; find the E0410 site at `src/check_expr.c:226`):
  - while checking a test or prop body, count `expect`/`expect_eq` calls (including in lambdas) and
    holes. At the end, if there are no checks and no holes, raise W0602 at the name's span;
  - at each `expect`/`expect_eq` call, raise W0603 if `is_literal(arg)` per the rule in
    `contracts/codes.md`;
  - add both to the diagnostic name table.
- [X] T030 [US4] Change generation in the new `src/mutate.c` (research R10):
  - walk every `N_FN` (and the lambdas inside) of files with `std == 0 && !pkg`, in file and source
    order, skipping `requires`/`ensures` expressions;
  - for each `N_BINARY` with an operator in the R10 table (by `d.i` and operand type), each `N_BOOL`,
    each `N_UNARY` `not` and each `N_INT` (skip `INT64_MAX`), push a `Mutant {Node *node; long
    new_op_or_value; Node *fn}` in R10 row order;
  - `mutant_texts` builds `text` (the node's source span) and `mutated` (the operator token found
    between `a.n->end` and `b.n->start` replaced, or the literal or `not ` replaced);
  - add the file to the `Makefile` sources.
- [X] T031 [US4] Running changes in `src/mutate.c` (research R11):
  1. Baseline: `cmd_test` runs the selected tests with coverage points and `cov_reach` on (`cur_test` =
     the selected index). Records print as usual. If any test did not pass, print T0012 and exit 1.
  2. For each of the first `opt_max_mutants` mutants whose function some test reached:
     - `out_flush()`, then `fork`;
     - the child redirects stdout and stderr to `/dev/null`, applies the change (`node->d.i`,
       `node->a.i`, or a `NF_MUT_SKIP` flag that `cexpr`'s `N_UNARY` case honors by compiling only the
       operand), resets the compiler (`c_done = 0`, the code pointer kept), turns coverage points off
       (`opt_coverage = opt_mutate = 0` and `cov_hit = cov_reach = 0` before compiling, so the child
       never writes the parent's shared bytes and later changes keep the baseline's reach), and calls
       `compile_program`;
     - it then runs the reaching tests in run order with `run_file_tests`-style recovery and exits 1 at
       the first non-pass (exit 0 if all pass);
     - the parent waits with a limit of `10 × (sum of baseline ms of those tests) + 1000` ms using
       `wait4` and a poll timer (reuse `src/process.c`'s wait helper if it fits), and kills the child
       (SIGKILL) on expiry. A kill, or any exit other than 0, means caught;
  3. Print `mutant` records for survivors (contracts/cli.md), then `mutation_summary` (`mutants`,
     `caught`, `survived`, `not_tried`), then the text note when something survived.

  The exit status is the baseline's. Mark the sequential children and the full recompile `ponytail:`.
  With `--diff`, only functions whose role is changed or added count (from `impact_tests`' items;
  extend `src/query.c` to expose them).
- [X] T032 [US4] SC-005 fixtures and script:
  - `tests/mutate_eval/` holds at least six programs (boundary `<=`, an off-by-one loop end, a sign
    error, `and`/`or`, a negated condition, a constant). Each has `weak.vo`, whose tests miss its
    known change, `full.vo` with the missing test added, and `expect.txt` naming the change
    (`FILE:LINE:COL text -> mutated`);
  - `tests/mutate_eval.py` checks, for each fixture, that the change survives with `weak.vo` and is
    caught with `full.vo` (absent from survivors). It also times `vo test --mutate` on every program
    in `examples/` that has tests (each under 60 s) and prints the timings for `results.md`.

**Checkpoint**: T026-T028 and T032 pass; `vo test --mutate examples/*/` runs.

---

## Phase 7: Polish and the gate

- [X] T033 [P] Reference (`docs/reference.md`), with the text of `contracts/language.md`:
  - the Tests paragraph and its `vo-test` example (checked by `tests/ref_examples.py`);
  - the "Coverage and mutation testing" subsection;
  - the `vo test` command-line line;
  - the machine-readable output additions.

  Record the token growth reported by `tests/ref_examples.py` (target about 550, limit 700) in
  `results.md`, and keep `specs/018-agent-testing/contracts/language.md` in sync with the shipped text.
- [X] T034 [P] Gate in `Makefile`:
  - add an "agent testing determinism" step to `gate-step`:
    `tests/run.sh --repeat 20 tests/cases/coverage tests/cases/mutate tests/cases/test` and
    `python3 tests/steps.py --repeat 20 tests/cases/test_steps`. The mutation cases must have no change
    whose run time is near its limit (research R11): an `infinite` case loops forever and never
    finishes, and no other case slows a test more than a few times;
  - add `tests/test_scale.py` and `tests/mutate_eval.py` to `make test` (or `make gate` if slow; note
    which);
  - make sure `make instrumented` covers the new cases.
- [X] T035 [P] `tests/test_scale.py` (SC-001, SC-004):
  - generate 200 tests and properties in 20 files in a temporary directory;
  - `vo test --list --json DIR` in under 1 s;
  - for every listed `id`, `vo test --json --test ID DIR` runs exactly that test (one `test` record
    with that id);
  - time `vo test` with and without `--coverage` on the speed corpus's tests (or the generated
    files), and check the ratio is at most 1.5.

  Print the figures for `results.md`.
- [X] T036 [P] Guide: `site/docs/guide/tests.md` gains sections on listing and selecting, reading
  failures (operands, first difference, cause, output), coverage, and mutation testing with the
  `is_adult` example. Its examples are checked by `tests/site_examples.py`. Link the new codes
  (`../errors/W0602.md`, and so on).
- [X] T037 [P] `docs/roadmap.md`: set 018 to **Done** with a results link and a "Not done" list (recorded
  tests, isolation, parallel mutants). Update the `README.md` feature list if it lists testing
  features.
- [X] T038 Full verification on x86-64, ARM64 and RISC-V, recorded in `results.md`:
  - on x86-64: `make test`, `make gate`, `make instrumented` (all new cases under ASan and UBSan; step
    cases with `VO_BIN=build-instrumented/vo`) and `make security-check` (unchanged);
  - on the other two: `make ARCH=aarch64 test` and `make ARCH=riscv64 test`;
  - `tests/test_scale.py` and `tests/mutate_eval.py` figures;
  - fill SC-001 to SC-006 and the Deviations;
  - set spec status to Implemented and mark every task [X].

---

## Dependencies & Execution Order

- **Phase 1** (T001-T002): no dependencies; T002 in parallel with T001.
- **Phase 2** (T003-T005): after Phase 1; T003 (failing cases), then T004, then T005. Blocks every
  story.
- **US1** (T006-T011): after Phase 2. MVP. T006 and T007 in parallel; then T008, then T009, then T010
  and T011 in either order. T011 feeds T009's diff set.
- **US2** (T012-T020): after Phase 2. T012-T015 in parallel; then T016, T017, T018 and T019 (T018 also
  touches `src/compile.c`; T016 touches `src/rt_io.c`, `src/task.c` and `src/prop.c`). All four edit
  `report_test`, so do them one after another. T020 last. T008 (US1) also edits `report_test`, so land
  it before T016-T019, or merge with care.
- **US3** (T021-T025): after Phase 2. T021 and T022 in parallel; then T023, then T024, then T025.
  Independent of US1 and US2.
- **US4** (T026-T032): T026 and T029 (warnings) only need Phase 2. Mutation (T027, T028, T030-T032)
  needs US3's T023-T024 (points and reach) and US1's T011 (the diff set for `--diff`).
- **Polish** (T033-T038): T033-T037 after the stories they document; T038 last.

## Parallel Examples

- **Phase 1**: T001 with T002.
- **US1**: T006 (golden) with T007 (step cases).
- **US2**: T012, T013, T014 and T015 (four case sets) together.
- **Streams after Phase 2**:
  - US1 and US2 on `src/testrun.c`, `src/rt_test.c` and `src/compile.c` (expect);
  - US3 on `src/vm.c` and `src/compile.c` (points);
  - the warnings (T026, T029) in the checker.
- **Polish**: T033, T034, T035, T036 and T037 together.

## Implementation Strategy

- **MVP**: Phases 1-3 (US1). Agents can discover tests and run exactly the ones that matter, by
  identifier, requirement or diff.
- **Next**: US2. Failures explain themselves, and `--json` output is clean. This is the largest
  improvement to the fix loop.
- **Then**: US3 (coverage), then US4 (warnings, then mutation testing on top of coverage's reach).
- **Last**: docs, scale and fixture scripts, and the full gate on three platforms.

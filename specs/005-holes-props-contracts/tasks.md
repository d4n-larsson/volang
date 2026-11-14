---

description: "Task list for vo-lang spec 005: typed holes, property tests and contracts"
---

# Tasks: Typed Holes, Property Tests and Contracts

**Input**: Design documents from `specs/005-holes-props-contracts/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R16), data-model.md, contracts/language.md, contracts/reports.md, contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution v1.1.0, Principle IV).
- Every story starts with golden cases that fail on the current binary.
- Every new code gets a case that `tests/catalog.py` finds.
- Every new reference example runs in `tests/ref_examples.py`.

**Organization**: one phase per user story, in spec priority order.
- **MVP:** US1 (holes). It is the smallest and stands alone.
- **US2 (properties):** the largest.
- **US3 (contracts):** independent of US2, except for one cross-feature case at its end.
- **US4 (evaluation):** needs US2.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependencies on incomplete tasks).
- **[Story]**: US1–US4 from spec.md.

## Conventions

**Carried over** from specs 001–004:
- RV64IMAFD, lp64d, GNU as. The SESI hook checks every edit; every report is fixed or explained.
- C prototype comments and `.globl` on every function. Shared constants in `src/defs.inc`. No libc.
- Golden cases are `tests/cases/<dir>/<name>.vo` plus `.out`, `.err`, `.code`, and optional `.cmd` and `.stress-every` files. Seed a new case with a `.code` file so `run.sh` picks it up, then accept it with `tests/run.sh --accept` *only after* checking the output by hand against contracts/reports.md.
- `VO_BIN` must be an absolute path.
- Append every SESI false positive or negative to `sesi-linter-issues.md`.
- `pkill` patterns are anchored with `^`.

**What the definitions are:**
- Surface rules: contracts/language.md.
- Output: contracts/reports.md.
- Codes: contracts/codes.md.
- Layouts: data-model.md.
- Decisions: research.md R1–R16.

If the implementation shows a contract is wrong, fix the contract in the same task and say so in `results.md`.

**Regression rule:** after every task that touches `src/`, `make test` passes. Existing outputs stay byte-identical (FR-027).

**Determinism rule:**
- Generation and shrinking depend only on `--seed`, the property's item index, the case number and the program. No clock and no addresses.
- Property cases get deep copies of their arguments (data-model.md, `prop_copy`).

**Shortcuts get `ponytail:` markers:**
- the fixed 100 cases / 1,000 attempts;
- the 2,000-run shrink budget;
- greedy shrinking.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose:** the new case directories and a name-clash check, before any source change.

- [X] T001 Create `tests/cases/hole/`, `tests/cases/prop/` and `tests/cases/contract/`. Confirm that `tests/run.sh`, `tests/catalog.py` and the `Makefile` `test` target pick up cases in new directories without edits. If any lists directories explicitly, add the three.
- [X] T002 [P] Check that the new names clash with nothing existing:
  - Search all `.vo` files under `tests/`, `bench/`, `bench2/` and `docs/` for `prop`, `requires`, `ensures`, `todo`, `assume` and `ret` used as identifiers.
  - Record the result in `specs/005-holes-props-contracts/results.md`, in a section "Name clashes". The expectation is none, apart from `out`, which research R11 avoided.
  - If a migration case (`tests/migrate/`) uses one, add the expected new error to that case as an intended change (FR-027).
- [X] T003 [P] Add the spec 004 speed baseline (FR-024, SC-006):
  - A `baseline-004` target in `Makefile`, built like `oracle`: a detached worktree at `71375e5` in `build/base004-src`, built and copied to `build-004/vo`.
  - `tests/speed.py --baseline PATH`: times each `tests/speed/*.vo` program on PATH and on `build/vo` with alternated runs, using the bench2 rule (median of 5, then of 15 for a program still over the limit). It fails when a program is more than 5% slower than the baseline, and prints each ratio.
  - Run it now, with `build/vo` = the current binary against itself, to confirm 1.00× ± noise and no false failure. Record the noise in `results.md`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose:** constants, keywords, record sizes and builtin slots that every story uses.

**⚠️ CRITICAL:** no user story work can begin until this phase is complete.

- [X] T004 Add to `src/defs.inc`:
  - Keywords: `T_PROP` 44, `T_REQUIRES` 45, `T_ENSURES` 46. `T_KW_LAST` becomes 46.
  - `NF_PROP` 64.
  - FX fields: `FX_PARAMS` 80, `FX_REQUIRES` 88, `FX_ENSURES` 96, `FX_HOLES` 104, `FX_RETSLOT` 112, and `FX_SIZE` 120.
  - `D_SCOPE` 88 and `D_SIZE` 96.
  - Builtin ids `B_TODO` 48 and `B_ASSUME` 49, plus the internal ids `B_REQ_FAIL` and `B_ENS_FAIL`, placed after the name table's end.

  Then find every place that allocates an FX record or a diagnostic record with a literal size instead of `FX_SIZE`/`D_SIZE`, and fix it. Grep `src/` for `80` and `88` near the allocations. Wherever an FX record is created, initialise `FX_RETSLOT` to −1.
- [X] T005 Add `prop`, `requires` and `ensures` to the keyword table in `src/lex.s`, in token order 44–46.
  - Check that any keyword-range table (`kw_table` and anything indexed by `T_KW_*`) has the extra entries.
  - Add the three words to the keyword list in `docs/reference.md` (Lexical rules).
  - Run `make test`. Every output must be unchanged, except any migration case T002 recorded.
- [X] T006 Register `todo` (48) and `assume` (49) in `builtin_names`/`builtin_sigs` in `src/check_expr.s`. Give both signature 0 (special-cased), and mark both pure in `builtin_effects` (`src/rt_call.s`).
  - Extend the internal builtin dispatch in `src/rt_call.s` so ids 48, 49, `B_REQ_FAIL` and `B_ENS_FAIL` reach stub handlers: `rt_hole`, `rt_assume`, `rt_requires_fail` and `rt_ensures_fail`. Each stub raises T0003 until its story implements it.
  - The ids must not be reachable from source by name, except `todo` and `assume`.
- [X] T007 Extend the diagnostic record in `src/diag.s`:
  - `diag_add` zeroes `D_SCOPE`.
  - Add `diag_set_scope(diag, vec)`.
  - `diag_json` writes `"scope":[{"name":...,"type":...},...]` after `expected`/`found`, only when `D_SCOPE` is non-zero (the format is in contracts/reports.md).
  - `diag_human` writes `    in scope: a: T, b: U` (or `    in scope: nothing` for an empty vec) after the snippet.

  No existing diagnostic sets it, so `make test` is unchanged.

**Checkpoint:** keywords lexed, builtins registered, records resized; the whole suite unchanged.

---

## Phase 3: User Story 1 - Check and run unfinished programs (Priority: P1) 🎯 MVP

**Goal:** `todo(desc)` checks as its expected type, gets W0601 with the type and scope, can end a path, and stops a run with R0011.

**Independent Test:** the `tests/cases/hole/` cases. Each position checks with W0601 (expected type and scope) and runs into R0011 at the right span. `let x = todo("x");` is E0601.

### Tests for User Story 1 ⚠️ (write first, must fail on the current binary)

- [X] T008 [P] [US1] Write check cases in `tests/cases/hole/`, with `.cmd` set to `check` (text) and a `--json` twin for at least `body`, `arg` and `generic`. Positions:
  - `body`: `fn f(line: str) -> int { todo("parse"); }`. It has W0601 with `int` and `line: str`, no E0403 and no W0201.
  - `let_annotated`, `arg`, `return_value`, `operand_right` (`n + todo("x")` is `int`), `field_init`, `list_elem`, `condition` (`if todo("c") {` is `bool`), `print_arg` (`str`).
  - `generic`: the expected type mentions `T`.
  - `lambda`: a hole inside an anonymous function.
  - `match_arm`.
  - `in_test`.
  - `after_stmt`: a statement after `todo(...);` gets W0401.
  - `effects_stub`: `uses console` with only a hole, which gets no W0501.
  - `function_type`: the expected type is `fn(int) -> int`.
  - `summary`: `holes` appears in the JSON summary, and `(2 holes)` in the text. A program without holes has no `holes` member.
  - `effects_record`: `vo effects --update` on a program whose function body is a hole records the function with its declared effects.
- [X] T009 [P] [US1] Write the error and run cases in `tests/cases/hole/`:
  - `unknown_let`, `unknown_operands` (`todo("a") == todo("b")`) and `unknown_str_arg` (`str(todo("a"))`) are E0601, exit 65.
  - `desc_not_str` (`todo(1)`) is E0301.
  - `run_reached`: prints, then R0011 with a trace through two functions, exit 70; plus a `--json` twin.
  - `run_not_reached`: the hole is in an uncalled function, so the run is normal, exit 0.
  - `test_reached`: `vo test` with one passing test and one that reaches a hole (`error` with R0011). The other test still runs; exit 1.

  Confirm that all of T008 and T009 fail on the current `build/vo`.

### Implementation for User Story 1

- [X] T010 [US1] Type `todo` in `check_expr` (`src/check_expr.s`), research R1–R2:
  - The argument is checked against `str` (E0301).
  - The result type is `expected`. When `expected` is 0 or the error type, report E0601 `hole_type_unknown` on the call and return the error type, so no cascade follows.
  - Otherwise report W0601 `hole` with message ``Hole: `<arg source text>` needs a value of type `T`.``, set `D_EXPECTED` to the rendered type, and set `D_SCOPE` to the visible locals and parameters, innermost first. Walk the checker's symbol chain from the current scope to the function boundary, `SYM_LOCAL` only.
  - Set `FX_HOLES` = 1 on the enclosing function, lambda, test or property.
- [X] T011 [US1] Pass expected types where `check_expr` passes none today, without changing any existing result:
  - For binary arithmetic and comparison operators, when the left operand is not a hole, pass its type as `expected` for the right operand.
  - Check that `if`/`while` conditions pass `bool`.
  - Check that typed builtin parameters (`print`'s `str`) pass their code type.

  The work is in `src/check_expr.s`. Run `make test`: unchanged.
- [X] T012 [US1] Handle hole statements in `src/check_func.s` (research R3): an `N_EXPR_STMT` whose call is `B_TODO` is checked with expected type `void` (no E0405), and gets `NF_TERMINATES`. Then the existing E0403 and W0401 logic does the rest.
- [X] T013 [US1] Suppress warnings in functions with holes. With `FX_HOLES` set:
  - skip W0201 for locals popped from that function's scopes (`src/check.s`, the pop at line ~537);
  - skip W0501 for its clause (`src/check_effects.s`, around line 711).

  Holes are found while checking the body, but effects are checked after, so the order already works. Confirm it.
- [X] T014 [US1] Add the hole count to the check summary in `src/diag.s`/`src/main.s`:
  - Count W0601 diagnostics.
  - `summary_emit` for `check` appends `,"holes":N` in JSON only when N > 0, so existing summaries stay byte-identical (FR-027).
  - The text summary appends ` (1 hole)` / ` (N holes)`, also only when N > 0.
- [X] T015 [US1] Run holes in `src/compile.s` and `src/rt_call.s`:
  - `cexpr` and `cstmt` compile a `B_TODO` call as `CALL_BUILTIN` with the description evaluated into the argument slot.
  - `rt_hole(desc, node)` raises R0011 `hole_reached` with the message `Hole reached: <desc>.`, using `rt_raise` with the call node, so the trace and the test runner's `error` status work unchanged.
  - A hole statement also needs no fall-through code after it.
- [X] T016 [P] [US1] Add catalog entries W0601 `hole`, E0601 `hole_type_unknown` and R0011 `hole_reached` to `docs/errors.md`, in the format of the neighbouring entries, with examples and fixes as text. Register the codes wherever `vo explain` looks codes up. Also add the R0011 runtime message string.
- [X] T017 [US1] Add a "Holes" subsection to `docs/reference.md`, about 250 tokens:
  - `todo("...")`, the expected type, and "statement ends the path";
  - the W0601 report with scope;
  - R0011 at run time;
  - one `vo-test` example: a function stubbed with `todo(...)` and a test that calls it, whose ```output block is the human `vo test` report. It shows `ERROR name`, the R0011 line and the summary, and `tests/ref_examples.py` runs and compares it (ms normalised).
  - Show the W0601 report in a ```text block (not executed). `ref_examples.py` has no form for check warnings, and adding one is not needed.
- [X] T018 [US1] Accept the T008/T009 cases after checking each output against contracts/reports.md, then run `make test`.

**Checkpoint:** US1 complete. Unfinished programs check and run.

---

## Phase 4: User Story 2 - Property tests find the edge cases (Priority: P1)

**Goal:** `prop "name" (params) { ... }` runs 100 generated cases under `vo test`, shrinks failures, and reports counterexamples, deterministically.

**Independent Test:** the `tests/cases/prop/` cases. Correct functions pass; broken ones fail with minimal counterexamples; reports are byte-identical over repeats.

### Tests for User Story 2 ⚠️ (write first, must fail on the current binary)

- [X] T019 [P] [US2] Write passing cases in `tests/cases/prop/`, with `.cmd` set to `test`:
  - `reverse_twice` (`list[int]`);
  - `sort_sorted` (`list[str]`);
  - `record_param` (a `Point` record);
  - `variant_tree` (a recursive `Tree` variant; generation terminates);
  - `map_param` (`map[str, int]`);
  - `result_param` (`result[int]`);
  - `generic_record` (`Pair[int, str]`);
  - `bool_float` (`bool, float`; floats are finite: `expect(x == x)`);
  - `uses_console` (prints inside, with a grant);
  - `filter` (`--filter` selects one property of two);
  - `json` (`--json` with `prop`, `cases`).
  - `hole_in_prop`: a hole in a property body. `vo check` gives W0601 with the parameters in scope; `vo test` gives `error` with R0011 and the first input as counterexample (spec SC-003, edge case "in a test or property body").

  Add a `.repeat` entry or equivalent for the repeat checker, so `reverse_twice` and one failing case run 3× byte-identically.
- [X] T020 [P] [US2] Write failing cases in `tests/cases/prop/`. The first 11 are the SC-004 set of deliberately broken functions:
  1. `dedupe_drops`: the quickstart's dedupe (keeps at most 3 distinct); the counterexample is 4 distinct small ints.
  2. `off_by_one`: a range bug; the counterexample is a small int.
  3. `empty_list`: a crash on `[]` (R0003); status `error`, counterexample `[]`.
  4. `overflow`: `a + b == b + a`; R0001 with an extreme int.
  5. `utf8`: a byte-vs-char bug; the counterexample is a one multi-byte-character string.
  6. `mutates_arg`: the body pushes into its list and fails; the counterexample is unaffected by the mutation.
  7. `lost_duplicate`: a merge of two sorted lists that drops equal elements; the counterexample is `[0]` and `[0]`.
  8. `negative`: a digit-sum that ignores the sign; the counterexample is `-1`.
  9. `variant_case`: a tree size function that forgets one case; the counterexample is the smallest tree using that case.
  10. `record_fields`: a function that swaps two fields of a record; the counterexample has two different small fields.
  11. `map_merge`: a map merge that keeps the first map's value for a shared key; the counterexample is two one-entry maps.

  The remaining cases:
  - `assume_ok`: assume filters; passes, with `discarded` in JSON.
  - `assume_starved`: fails with the "too many inputs were discarded" message.
  - `timeout`: an infinite loop for some input, with `--timeout 1`; status `timeout` with the counterexample.
  - `seed`: `--seed 7` gives a different but stable report.
  - `mem_stats`: `--mem-stats` gives one report per property.
  - `json_fail`: a `--json` failure with `counterexample` and `shrinks`.
- [X] T021 [P] [US2] Write check cases in `tests/cases/prop/`:
  - `fn_param` is E0602 ("contains a function type").
  - `infinite_record` (`record Loop { next: Loop; }`) is E0602 ("has no finite value").
  - `assume_in_fn` and `assume_in_test` are E0604.
  - `dup_name` (a test and a property with the same name) is E0202.
  - `no_params` is the existing syntax error.
  - `try_in_prop` is E0406.
  - `expect_in_prop`: passes the check (E0410 does not fire).
  - `fmt`: `vo fmt` of a messy property gives the canonical form.
  - `effects_record`: `vo effects --update` lists `prop "name"`.

  Confirm that all of T019–T021 fail on the current binary.

### Implementation for User Story 2

- [X] T022 [US2] Parse properties in `src/parse.s`, next to the `N_TEST` parse at line ~2867:
  - `T_PROP` builds an `N_TEST` with `NF_PROP`, the name string, then `(` param `{, param}` `)` using the existing parameter parser (at least one), then `parse_uses`, then the block.
  - The parameters are stored in the FX record's `FX_PARAMS`. Create the FX record here if the test path normally creates it later, and keep one creation site.
- [X] T023 [P] [US2] Format properties in `src/fmt.s` (near line 1266): print `prop "name" (a: T, b: U) uses ... {`. Add the `fmt` case expectations to `tests/fmt_corpus.py`'s corpus if it keeps one.
- [X] T024 [P] [US2] Record properties in `src/effects_record.s` (near line 1491) under the name `prop "name"`, and point diagnostics at the `prop` keyword. `vo effects FILE` prints the property's capabilities like a test's.
- [X] T025 [US2] Check properties in `src/check.s` (the `N_TEST` paths at lines ~2517/2528/2700):
  - Bind `FX_PARAMS` as `SYM_LOCAL` parameters in slots 0..n−1 before the body; the local count includes them.
  - Names share the test name space (E0202).
  - For each parameter, call `prop_generatable(type)` (T027). When it returns 1 or 2, report E0602 `ungeneratable_type` on the parameter, with the message ``Cannot generate values of `T` for `name`: it contains a function type.`` or ``...: it has no finite value.``
  - `expect`/`expect_eq` are allowed (extend the E0410 test to `NF_PROP` items).
  - `assume` outside an `NF_PROP` body is E0604 `assume_outside_prop` (`src/check_expr.s`).
  - `assume`'s argument is `bool`.
- [X] T026 [US2] Compile properties in `src/compile.s` (lines ~80/164): an `NF_PROP` test compiles like a function with `CF_NPARAMS` = its parameter count, so `vm_push(cf, decl, call, vals, 0)` places the arguments in slots 0..n−1. `assume(cond)` compiles to `CALL_BUILTIN B_ASSUME`.
  - `rt_assume(cond)` in `src/rt_call.s`: when `cond` is false, raise the internal discard. `rt_raise` with a reserved code string `"ASSUME"`, which the property loop checks and which is never printed.
- [X] T027 [US2] Create `src/prop.s` with type analysis and generation (research R6, R7):
  - `int prop_generatable(void *type)`: 0 ok, 1 function type, 2 no finite value (a least fixpoint over records and variants; type parameters count as 1).
  - The generator's own PCG state (`pg_state`/`pg_inc`), and `void prop_seed(long seed, long item, long case_no)`, which mixes the three (SplitMix64 of `seed ^ item<<32 ^ case_no`).
  - `long prop_gen(void *type, long size, long depth)`, exactly following the table in research R7:
    - edges 1 in 8 for `int`/`float`;
    - `str` alphabet and lengths (at most 32);
    - lists and maps (at most 16 elements);
    - `result` with `err` 1 in 4;
    - records field by field;
    - variants with the depth budget of 6.
  - Partial values are kept reachable through the `pr_args` list while generating, because allocation may collect.
  - `long prop_copy(long value, void *type)`: a deep copy (strings may be shared; lists, maps, records and variants are copied).

  Every function has prototype comments and passes lint.
- [X] T028 [US2] Add shrinking to `src/prop.s` (research R8): `long prop_shrink(long value, void *type, long k)` returns the k-th candidate, or 0 when there are none, in exactly R8's per-type order. Candidates are new objects, and inputs are never mutated.
- [X] T029 [US2] Add the property runner to `src/prop.s` (research R9–R10, data-model.md state diagram): `int prop_run(void *test, long item_index)`.
  - **Before the cases:** `gc_start` and the deadline.
  - **Attempt loop (≤ 1,000 attempts, 100 accepted):**
    1. `interp_reset`, then `rt_effects_start(1)`;
    2. `prop_seed`, then generate into `pr_args`;
    3. run on a `prop_copy` of the arguments under `jmp_save`/`recover_point`;
    4. classify the outcome: pass, discard (`rt_err_code` == `"ASSUME"`), fail (expect node or runtime code other than R0009/R0006), timeout or out of memory.
  - **On failure:** shrink parameter by parameter until no candidate fails the same way (`pr_fail_kind`), the budget (2,000 runs, `ponytail:` marker) runs out, or the deadline is reached (`pr_stopped`).
  - **Final run:** re-run the final `pr_best` so that `tf_*`/`rt_err_*` describe it.
  - Returns the status for `report_test`.
  - `pr_args`/`pr_best` are `.data` roots that the collector scans.
- [X] T030 [US2] Integrate with the test runner in `src/testrun.s`:
  - `run_file_tests` dispatches `NF_PROP` items to `prop_run`, after the filter, counting them in passed/failed/errored/timed_out.
  - `report_test` adds `"prop":true,"cases":N` and, when non-zero, `"discarded":D`.
  - On failure it adds `"shrinks":K,"counterexample":[...]` (values via `render_value(sb, value, param type)`) and `"shrink_stopped":true` when set.
  - Text: the `(N cases, Mms)` form, the `counterexample: name = value` lines and `shrunk in K steps` (or the stopped wording), exactly as in contracts/reports.md.
  - Too many discards: a failure line at the `prop` keyword with the discard message, and no counterexample.
  - `--mem-stats` gives one report per property.
- [X] T031 [US2] Add a property case to the collector's GLOBALS fault target (`tests/gc/fault_targets/`). It holds generated arguments across a collection, so leaving out `pr_args`/`pr_best` is caught. Then run `python3 tests/gc_faults.py` and `make gc-check`.
- [X] T032 [P] [US2] Add catalog entries E0602 `ungeneratable_type` and E0604 `assume_outside_prop` to `docs/errors.md`. Update E0410's text to say "a `test` or `prop`".
- [X] T033 [US2] Add a "Property tests" subsection to `docs/reference.md`'s Tests section, about 600 tokens:
  - syntax;
  - generated types and what they include (edges, finite floats);
  - 100 cases and determinism (`--seed`);
  - `assume`;
  - shrinking and the counterexample lines.

  Include one `vo-test` example with a passing property, which `tests/ref_examples.py` must run. Extend `ref_examples.py` if its `vo-test` handling cannot match the `(100 cases, Nms)` line (it already normalises ms). Add `prop` to the Command line section's mention of what `vo test` runs.
- [X] T034 [US2] Accept the T019–T021 cases after checking each against contracts/reports.md. For the 11 SC-004 cases of T020, check the minimality claim by hand (no single R8 shrink step of the reported counterexample still fails), and time each with `--json` `ms` (under 2 s under qemu). Record a table (case, counterexample, shrink steps, ms) in `results.md`. Run `make test` and `make gc-check`.

**Checkpoint:** US2 complete. Properties run, shrink, and report deterministically.

---

## Phase 5: User Story 3 - Contracts on functions (Priority: P2)

**Goal:** `requires`/`ensures` on named functions are checked at every call and return (R0012/R0013). They must be pure, and functions without them are no slower.

**Independent Test:** the `tests/cases/contract/` cases, plus `python3 tests/speed.py --baseline build-004/vo`.

### Tests for User Story 3 ⚠️ (write first, must fail on the current binary)

- [X] T035 [P] [US3] Write run cases in `tests/cases/contract/`:
  - `requires_ok`, and `requires_fail` (R0012 at the call; trace from the caller; text and `--json`).
  - `ensures_fail_return` (R0013 at `return`).
  - `ensures_try`: a `result` function leaving through `try` breaks `ensures`, giving R0013 at the `try`.
  - `ensures_void`: no return type, a condition on a list parameter's length, broken at the closing `}`.
  - `order`: two `requires`; the first false one is reported.
  - `via_value`: `let f = isqrt; f(-1)` gives R0012.
  - `via_map`: `map([-1], isqrt)` gives R0012 at the `map(...)` call.
  - `generic`: `fn clamp[T: ordered](x: T, lo: T, hi: T) -> T requires lo <= hi`.
  - `recursive`: contracts on a recursive function, checked at every level.
  - `main_contract`: `main` with a `requires true`.
  - `in_test`: `vo test` where a test breaks a contract (`error`, R0012).
- [X] T036 [P] [US3] Write check cases in `tests/cases/contract/`:
  - `impure` (`requires` calls `print`) is E0603.
  - `impure_effect_param` (a condition calls an effect-parameter callback) is E0603.
  - `not_bool` is E0301.
  - `ret_in_requires` is E0201 (`ret` is unknown there).
  - `ret_param` (a parameter named `ret` with `ensures`) is E0203.
  - `ret_void` (`ret` in `ensures` of a void function) is E0201.
  - `on_lambda` is the existing syntax error.
  - `effects_unchanged`: `vo.effects` is unchanged by adding contracts.
  - `fmt`: canonical clause layout (research R11).
  - `hole_in_clause`: W0601 with expected type `bool`.

  Confirm that all of T035 and T036 fail on the current binary.

### Implementation for User Story 3

- [X] T037 [US3] Parse clauses in `src/parse.s`, in the `N_FN` parse at line ~2286: after `parse_uses`, loop over `T_REQUIRES`/`T_ENSURES` and parse one expression each into `FX_REQUIRES`/`FX_ENSURES` (vecs in source order).
  - Interleaved clauses keep their relative order within each kind.
  - Also record the global source order, so `vo fmt` can print the clauses as written. Store a combined vec, or tag the nodes.
  - Anonymous functions do not accept clauses.
- [X] T038 [P] [US3] Format clauses in `src/fmt.s`: the signature line ends after `uses`; each clause goes on its own line indented four spaces, in source order; `{` ends the last clause line (contracts/language.md example).
- [X] T039 [US3] Check clauses in `src/check.s`/`src/check_expr.s` (research R15):
  - After binding the parameters, check each `requires` clause with `check_expr(cond, bool)`.
  - For the `ensures` clauses, open a scope that adds `ret` (a `SYM_LOCAL` with the return type, slot = a new local reserved in `FX_RETSLOT`) when there is a return type, check each clause, then pop the scope before the body.
  - A parameter named `ret` together with `ensures` is E0203.
  - The local count includes `ret`'s slot.
- [X] T040 [US3] Check purity in `src/check_effects.s` (research R14): compute each clause's requirement separately. Any non-empty requirement (bits, scopes or effect variables) is E0603 `impure_contract` on the clause, naming the first effect. Do not add clause requirements to `FX_REQUIRED`, so `vo.effects` is unchanged.
- [X] T041 [US3] Compile `requires` in `src/compile.s` (research R12):
  - At function entry, for each `requires`: `ccond(cond)` → `JT next`, then `CALL_BUILTIN B_REQ_FAIL` with the clause node, then `next:`.
  - `rt_requires_fail(clause)` in `src/rt_call.s`:
    1. builds ``Precondition `<clause source text, whitespace runs collapsed>` of `<function name>` does not hold.``;
    2. takes the current frame's `FR_CALL` node as the span;
    3. pops the current frame (`cur_frame` = `FR_PARENT`), so the trace starts at the caller;
    4. raises R0012.
  - Functions without clauses emit nothing new. Check this with a temporary debug print of each function's `CF_N`:
    - run every `tests/cases/run/*.vo` and `tests/speed/*.vo` before and after this task;
    - the per-function instruction counts must be identical;
    - then remove the print.
- [X] T042 [US3] Compile `ensures` in `src/compile.s` (research R13). When `FX_ENSURES` is non-zero:
  - Reserve a hidden slot for the exit node.
  - Compile every `RET` site — `return e`, `return`, the `try` propagation path, and the implicit end of a void function — as moves into `FX_RETSLOT` and the exit-node slot (the node address as a constant), then `JMP epilogue`.
  - The epilogue is, for each clause, `ccond` → `JT next` → `CALL_BUILTIN B_ENS_FAIL` (clause, exit-node slot), then `RET ret`.
  - `rt_ensures_fail(clause, exit_node)` in `src/rt_call.s` raises R0013 with ``Postcondition `<text>` of `<function name>` does not hold.`` at the exit node, with the trace from the current frame.
- [X] T043 [P] [US3] Add catalog entries E0603 `impure_contract`, R0012 `precondition_failed` and R0013 `postcondition_failed` to `docs/errors.md`, with runtime message strings.
- [X] T044 [US3] Add a "Contracts" subsection to `docs/reference.md`, under Functions, about 400 tokens:
  - the clause syntax and position;
  - `ret`;
  - purity;
  - checked on every call and return, in `run` and `test`;
  - R0012/R0013 spans.

  Use the contracts/language.md `isqrt` example with an `output` block. Add `requires`/`ensures` to the function signature template line.
- [X] T045 [US3] Write the cross-feature case `tests/cases/prop/contract_violation`: a property calls a function whose `requires` the generated input breaks. It fails with R0012 and a shrunk counterexample (spec US3 scenario 7).
- [X] T046 [US3] Accept the T035, T036 and T045 cases after checking them against contracts/reports.md. Run `make test`, then `make baseline-004 && python3 tests/speed.py --baseline build-004/vo`: each program within 5% of the spec 004 binary (FR-024, SC-006).

**Checkpoint:** US3 complete. Contracts are checked everywhere, with no cost where absent.

---

## Phase 6: User Story 4 - Evidence that properties catch more mistakes (Priority: P2)

**Goal:** the SC-001/SC-002 stand-in. Property files and at least 5 mutants per hidden-property task, and a script that measures what is caught.

**Independent Test:** `python3 tests/prop_eval.py` exits 0, with at least 70% caught overall and at least 1 per task.

**Rule for every task in this phase:** write `props.vo` from `bench2/tasks/<task>/task.md` and the reference solution's function signatures only. Never open `hidden/` while writing properties. Write the mutants before running the properties against them (spec assumption).

- [X] T047 [US4] Write `tests/prop_eval.py` (research R16). For each `tests/prop_eval/hNN-*/`:
  1. Copy `bench2/tasks/<task>/ref/vo/` into a temporary directory and run `vo test props.vo` there 3 times. All must pass, byte-identically.
  2. For each `bugs/*.vo`: copy the reference, overwrite the file the mutant replaces (the first line `# replaces: <file>`), then:
     - run the visible cases: all must pass, or the mutant is invalid and the script fails;
     - run the hidden cases: at least one must fail, or the mutant is invalid;
     - run `vo test props.vo`: exit 1 means caught.
  3. Print a table (task, mutants, caught by visible cases (0), caught by properties) and the total.

  Exit 1 below 70% overall or with a task at 0. Inputs are fed on stdin, as bench2's grader does (reuse `tests/gc/baseline.py`'s job helpers if that fits). Add a `make prop-eval` target to `Makefile`.
- [X] T048 [P] [US4] Write `tests/prop_eval/h01-interval-merge/props.vo` and at least 5 mutants in `bugs/`.
- [X] T049 [P] [US4] Write `tests/prop_eval/h02-competition-ranking/props.vo` and at least 5 mutants in `bugs/`.
- [X] T050 [P] [US4] Write `tests/prop_eval/h03-rle-decode/props.vo` and at least 5 mutants in `bugs/`.
- [X] T051 [P] [US4] Write `tests/prop_eval/h04-bracket-errors/props.vo` and at least 5 mutants in `bugs/`.
- [X] T052 [P] [US4] Write `tests/prop_eval/h05-date-weekday/props.vo` and at least 5 mutants in `bugs/`.
- [X] T053 [P] [US4] Write `tests/prop_eval/h06-roman-numerals/props.vo` and at least 5 mutants in `bugs/`.
- [X] T054 [P] [US4] Write `tests/prop_eval/h07-word-wrap/props.vo` and at least 5 mutants in `bugs/`.
- [X] T055 [P] [US4] Write `tests/prop_eval/h08-bignum-add/props.vo` and at least 5 mutants in `bugs/`.
- [X] T056 [P] [US4] Write `tests/prop_eval/h09-window-max/props.vo` and at least 5 mutants in `bugs/`.
- [X] T057 [P] [US4] Write `tests/prop_eval/h10-matrix-spiral/props.vo` and at least 5 mutants in `bugs/`.
- [X] T058 [US4] Run `make prop-eval`. Record the per-task table in `results.md`. If a reference solution has no function a property can call (for example, everything is in `main`), note it and test the smallest function that exists. Do not modify `bench2/`.

**Checkpoint:** SC-001 and SC-002 are measured.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T059 [P] Measure the reference's size the same way spec 004 did (SC-009 there). It must stay under 15,000 tokens and grow by at most 2,000 (SC-007). Record before and after in `results.md`, and trim wording if it is over.
- [X] T060 [P] Update `README.md` (feature list, line count) and `docs/roadmap.md` (mark 005 done with results; move the "agents using properties" agent run to the benchmark section as a follow-up).
- [X] T061 Run the full gate:
  - `make test`;
  - `make gc-check`, including the fault matrix with every kind caught;
  - `make speed-check`;
  - `python3 tests/speed.py --baseline build-004/vo` (SC-006);
  - `python3 tests/speed.py --bench2`;
  - the fuzzers;
  - SESI lint clean over `src/`.

  Fix or explain every report, appending linter misses to `sesi-linter-issues.md`.
- [X] T062 Write `specs/005-holes-props-contracts/results.md`:
  - each SC with its numbers;
  - the property timings for SC-004;
  - name clashes (T002);
  - contract or research changes made during implementation;
  - `ponytail:` shortcuts;
  - follow-ups (the agent benchmark run, a case-count option, strict holes, `old()`).
- [X] T063 Remove stray files left by test runs at the repository root, check `git status` for anything unintended, and mark every task `[X]`.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1):** no dependencies.
- **Foundational (Phase 2):** depends on Setup. It blocks all stories.
- **US1 (Phase 3):** depends on Foundational only.
- **US2 (Phase 4):** depends on Foundational. It is independent of US1, but T025's E0410 change and T010's `FX_HOLES` both touch the checker, so do them in sequence.
- **US3 (Phase 5):** depends on Foundational. T045 needs US2. T046's speed check needs T003's baseline.
- **US4 (Phase 6):** depends on US2. It uses no contracts, though mutant properties may.
- **Polish (Phase 7):** depends on all stories.

### Within Each Story

- Golden cases are written first and fail on the current binary, before any implementation task in that story.
- Parser → checker → compiler → runtime → reports → catalog and reference → accept the cases.
- In US2, T027 → T028 → T029 → T030, all in `src/prop.s` and then `src/testrun.s`.

### Parallel Opportunities

- T002 and T003 run alongside T001.
- T008 and T009 together; T016 alongside T010–T015.
- T019, T020 and T021 together. T023 and T024 alongside T025. T032 alongside the `prop.s` work.
- T035 and T036 together. T038 and T043 alongside the checker and compiler work.
- T048–T057: all ten tasks in parallel, each in its own directory.
- T059 and T060 together.

## Parallel Example: User Story 4

```text
Task: "Write tests/prop_eval/h01-interval-merge/props.vo and at least 5 mutants"
Task: "Write tests/prop_eval/h02-competition-ranking/props.vo and at least 5 mutants"
...
Task: "Write tests/prop_eval/h10-matrix-spiral/props.vo and at least 5 mutants"
```

## Implementation Strategy

### MVP First (User Story 1 only)

1. Phase 1 and Phase 2.
2. Phase 3 (holes): agents can already check and run unfinished programs.
3. **Stop and validate:** quickstart §1, then `make test`.

### Incremental Delivery

1. Holes (US1).
2. Properties (US2): the main value of the spec.
3. Contracts (US3), and the property-plus-contract case.
4. Evaluation (US4).
5. Polish: reference budget, docs, the full gate and results.

Each story leaves `make test` green and existing outputs byte-identical.

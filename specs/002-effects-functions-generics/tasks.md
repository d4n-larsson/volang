---

description: "Task list for vo-lang v2 core: effects, functions as values, generics"
---

# Tasks: vo-lang v2 Core: Effects, Functions as Values, Generics

**Input**: Design documents from `specs/002-effects-functions-generics/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/ (grammar-v2.md, cli-v2.md, output-schema-v2.md, error-codes-v2.md, effects-record.md, benchmark-v2.md), quickstart.md

**Tests**: REQUIRED. Constitution v1.1.0 Principle IV: every behavior change lands first as a golden case (or a test script) that fails before the implementation exists. Within each phase, the test tasks come before the implementation tasks.

**Organization**: Tasks are grouped by user story, in this order:
- US1 Effects (P1, MVP)
- US2 Review via the effects record (P1)
- US3 Functions as values (P1)
- US4 Generics (P2)
- US5 Migration (P2)
- US6 Benchmark v2 (P1)

US6's harness and tasks are authored alongside the others; its runs need US1–US4 and happen in the final phase.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: The user story this task belongs to (US1–US6)

## Conventions

**Carried over from v1.** All conventions from `specs/001-vo-lang-core/tasks.md` still apply:
- RV64IMAFD, lp64d, GNU as. The SESI hook checks every edit; every report gets fixed or explained.
- C-word prototype comments and `.globl` on every function.
- Shared constants live in `src/defs.inc`. No libc. Memory comes from `alloc`.
- Golden-case conventions and the `tests/run.sh` behavior.

**Linter issues.** Append every SESI false positive or negative to `sesi-linter-issues.md`.

**Docs land with the change** (Principle V). Any task that changes syntax, builtins, CLI, output or codes updates, in the same task:
- `docs/reference.md`, with a runnable example (checked by `tests/ref_examples.py`)
- `docs/errors.md`, with an `Example` and a `Fixed` section and the `Fixable:` line (checked by `tests/catalog.py` and `tests/apply_fixes.py`)
- the spec 002 contract the change belongs to

**Where the definitions live:**

| What | Where |
|---|---|
| Layouts | data-model.md |
| Syntax and semantics | contracts/grammar-v2.md |
| Codes | contracts/error-codes-v2.md |
| Output | contracts/output-schema-v2.md |
| CLI | contracts/cli-v2.md |
| Record format and comparison | contracts/effects-record.md |

**New golden areas.**
- `tests/cases/effects/`, `tests/cases/func/` and `tests/cases/generic/` default to `run {}` when a case has no `.cmd`.
- Cases that test diagnostics have a `.cmd` of `check --json {}`.
- Multi-file cases use the layout of `tests/cases/multi/app/`: a subdirectory with the entry `main.vo` plus `main.out`, `main.cmd` and so on.

**Shortcuts.** Any deliberate shortcut gets a `ponytail:` comment naming its ceiling.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Snapshot the v1 corpus before anything changes, create the new test areas, and wire the new gates.

- [X] T001 Build the frozen v1 binary for the migration corpus.
  - Run `make BUILD=build-v1` on the current tree, before any v2 change. Nothing rebuilds `build-v1/` afterwards.
  - Record the v1 source hash in `tests/migrate/V1_SOURCE`, as the output of `cat src/*.s src/defs.inc | sha256sum`.
- [X] T002 Snapshot the v1 programs into `tests/migrate/v1/` (SC-005 corpus) with `tests/migrate/snapshot.py`.
  - **Entries:** every golden case entry (a `tests/cases/**/*.vo` with a sibling `.cmd`, `.out`, `.err` or `.code`) that `build-v1/vo check` accepts and that has `fn main(` or tests, plus every `bench/tasks/*/solution.vo` (stdin: its `cases/1.in`). A case in a subdirectory (multi-file) is copied with its whole directory, so helpers come along but are not entries. Docs examples are not in the corpus; `ref_examples.py` and `catalog.py` run them after T023 migrates them.
  - **Recorded:** `<entry>.mode` (`run`, or human `test` for files with tests and no `main`), `<entry>.v1out` (stdout with `(Nms)` normalized), `<entry>.v1code` and `<entry>.in`.
  - `tests/migrate/v1/MANIFEST`: entry paths, sorted (61 entries).
- [X] T003 [P] Create `tests/cases/effects/`, `tests/cases/func/`, `tests/cases/generic/`, `tests/effect_changes/`, `bench2/tasks/`, `bench2/features/` and `bench2/results/`, each with a `.gitkeep`. In `tests/run.sh`, add `effects`, `func` and `generic` to the directories whose default command is `run {}`.
- [X] T004 [P] Update `Makefile`.
  - `test` additionally runs these, in order after `fmt_corpus.py`:
    - `python3 tests/migrate.py`
    - `python3 tests/record_changes.py`
    - `python3 tests/scope_fuzz.py --iterations 300 --seed 1`

    Skip any that doesn't exist yet, with a notice, as v1 does.
  - Extend the determinism run to `tests/run.sh --repeat 3 tests/cases/run tests/cases/effects tests/cases/func tests/cases/generic`.
  - Confirm the `src/*.s` wildcard picks up the new `src/types.s` and `src/effects.s`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared constants, the grown frame and symbol layouts, the `uses` keyword, and the effect-set library that US1, US3 and US4 all use.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete. Checkpoint: `make test` still passes 134/134, with no behavior change.

- [X] T005 Add every v2 constant to `src/defs.inc` (data-model.md):
  - **Effect bits:** `EF_CONSOLE 1`, `EF_INPUT 2`, `EF_FREAD 4`, `EF_FWRITE 8`, `EF_NET 16`, `EF_CLOCK 32`, `EF_RANDOM 64`, `EF_ENV 128`.
  - **Effect-set object:** `ES_MASK 0`, `ES_SCOPES 8` (vector of `{bit, str}` pairs), `ES_EVARS 16` (vector of effect-variable `TY_PARAM` types), `ES_SIZE 24`.
  - **Node kinds:** `N_TPARAM`, `N_USES`, `N_EFFECT`, `N_LAMBDA`, `N_FN_TYPE`, `N_CALL_VALUE`, numbered after `N_NEW_MAP`. Add field comments matching data-model.md.
  - **Type kinds:** `TY_FUNC 13`, `TY_PARAM 14`.
  - **Function side record:** `FX_TPARAMS 0`, `FX_USES 8`, `FX_DECLARED 16`, `FX_REQUIRED 24`, `FX_SIZE 32`. It is reached from `N_B` of the `N_FN` name node.
  - **Symbols:** `SY_EFFECTS 72`, `SY_DEPTH 72` (locals only; it shares the offset with `SY_EFFECTS`, which is for functions only), `SYM_SIZE 80`.
  - **Frame header:** `FR_TARGS 24`, `FR_EFFECTS 32`, `FR_SLOTS 40`.
  - **Closure object:** `CL_FN`, `CL_N`, `CL_TARGS`, `CL_CAPS`, after the v1 object header.
  - **Tokens and object kinds:** `TK_USES`, plus an object-kind constant for closures.
- [X] T006 Grow the frame header in `src/interp.s`.
  - Replace every literal frame offset with the `FR_*` constants. Grep for `24(` and `, 24` near frame code, and for the frame-size arithmetic in `call_fn`, `run_main`, `run_test_body` and `interp_reset`.
  - Zero `FR_TARGS` and `FR_EFFECTS` on every frame push.
  - Run `make test`: 134/134.
- [X] T007 Grow symbols to 80 bytes.
  - Verify every symbol allocation uses `SYM_SIZE` (grep `alloc` callers in `src/check.s`, `src/check_expr.s` and `src/loader.s`), and zero the new word.
  - Run `make test`: 134/134.
- [X] T008 [P] Make `uses` a keyword (`TK_USES`) in `src/lex.s`. `ordered`, `effects` and effect names stay identifiers (contracts/grammar-v2.md).
  - Re-run `build-v1`'s corpus check: `grep -rwn uses tests/migrate/v1` must find no identifier named `uses`. If it does, record it in `tests/migrate/KNOWN_RENAMES` and give E0102-style diagnostics with a rename fix.
- [X] T009 [P] Create `src/effects.s`, the effect-set library. All sets are immutable once built, and every operation returns a new set or a boolean.
  - **`es_new(mask)`**
  - **`es_add_scope(set, bit, str)`:** normalizes the scope.
    - Directory scopes: strip a leading `./`, collapse `//`, ensure a trailing `/`. An empty result, a `..` component or an absolute path (leading `/`) returns 0, and the caller reports E0506; scopes are relative to the start directory (FR-009).
    - Env scopes are kept verbatim.
    - The scope list is kept sorted by (bit, bytes) and deduplicated.
  - **`es_union(a, b)`**
  - **`es_covers(d, r)`:** for every bit in `r`: the bit is in `d`; and if `r` has scopes for the bit, then `d` has no scopes for it (unscoped) or every scope of `r` has a scope of `d` that is a byte prefix of it (directories) or equal to it (env, net).
  - **`es_missing(d, r)`:** returns the set of `r`'s bits and scopes not covered by `d`.
  - **`es_unused(d, r)`:** returns `d`'s bits that `r` lacks, plus `d`'s scopes that no scope of `r` lies under. An unscoped requirement uses all scopes of that bit.
  - **`es_equal`**
  - **`es_render(set, sb)`:** canonical text. Bits in the order console, input, file.read, file.write, net, clock, random, env; scopes as `("a/", "b/")`; then effect variables by name. An empty set renders `pure`.
  - **`es_parse_name(str) -> bit or 0`**

  No caller yet. It is exercised through US1's golden cases.

**Checkpoint**: Foundation ready. `make test` passes 134/134 and `make lint` is clean.

---

## Phase 3: User Story 1 — Effects are part of every function's type (Priority: P1) 🎯 MVP

**Goal**: Every function and test declares its effects with `uses`. Check reports missing, unused and too-narrow effects with exact fixes. The runtime refuses anything outside the innermost declaration, with scopes. Clock and randomness are fixable.

**Independent Test**: quickstart.md US1. A program with a pure helper, a printer and a file reader. Delete one `uses` entry and confirm E0501 with a fix. Apply the fix and confirm `run` works. Write outside the scope and confirm R0010.

### Tests for User Story 1 (write first, must fail)

- [X] T010 [P] [US1] Write the static effect-checking golden cases in `tests/cases/effects/`, each with `.cmd` `check --json {}`, `.out` and `.code`:
  - `missing_effect.vo`: `print` in a function without `uses`. E0501 names the function and `console`; the fix inserts ` uses console` before `{`.
  - `transitive.vo`: `a` calls `b`, and `b` has `uses file.write`. E0501 on `a` with `file.write`.
  - `transitive_full_set.vo`: `a` calls `b`, `b` calls `print` and `write_file`, and neither declares anything. E0501 on `b` with `console, file.write`, and on `a` with the same full set (research §10).
  - `unused_effect.vo`: W0501 with a removal fix that deletes the entry and its comma, or the whole clause when it was the only entry.
  - `pure_ok.vo`: exit 0, no diagnostics.
  - `scope_covered.vo`: `uses file.write("out/")` calls a callee with `file.write("out/reports/")`. Clean.
  - `scope_too_narrow.vo`: the same, but the callee has `file.write("tmp/")`. E0503 with `related` pointing at the callee's declaration and a fix producing `file.write("out/", "tmp/")`.
  - `invalid_scope.vo`: `file.read("../x")`, `file.read("")` and `file.read("/etc/")`. Three E0506.
  - `unknown_effect.vo`: `uses consol` gives E0509 with the fix `console` (closest canonical name within edit distance 2; ties broken by canonical order). `uses files` gives E0509 with no fix (nothing within distance 2).
  - `net_scope.vo`: a caller with `uses net("a.example")` calls a callee with `uses net("b.example")`. E0503 with a fix to `net("a.example", "b.example")`. Host scopes match exactly.
  - `mutual_recursion.vo`: `even`/`odd` call each other and one prints. E0501 on both, with the same set.
  - `test_uses.vo` (`.cmd` `check --json {}`): `test "x" { print("a"); }`. E0501 on the test, with a fix adding ` uses console` after the test name string.
- [X] T011 [P] [US1] Write the runtime enforcement golden cases in `tests/cases/effects/`:
  - `scope_escape.vo`: `main uses console, file.write("out/")` writes `out/../x`. R0010, exit 70. The stderr text and the `--json` variant `scope_escape_json.cmd` (`run --json {}`) show `effect`, `target`, `allowed` and `declared_by`.
  - `absolute_path.vo`: writes `/tmp/x` under a scope of `out/`. R0010.
  - `inner_declaration.vo`: `main uses file.write("out/")` calls `save uses file.write("out/a/")`, which writes `out/b/x`. R0010 names `save` as `declared_by` (innermost declaration wins).
  - `env_denied.vo`: `main uses console, env("HOME")` reads `env("PA" + "TH")`. R0010 with target `PATH`.
  - `env_ok.vo` (`.cmd` `run {}`): `env("HOME")` prints `ok` for `ok(_)`. `HOME` is set in any normal environment, and the case prints only `ok`, so the output doesn't depend on its value. Add `env_missing.vo`, which prints `not set` for a missing variable under `env("VO_UNSET_X")`.
  - `random_seed.vo` (`.cmd` `run --seed 7 {}`): prints five `random_int(1, 100)` values. The output is fixed in `.out`, and `--repeat 3` checks determinism.
  - `random_bad_range.vo`: `random_int(5, 1)`. R0007.
  - `clock_fixed.vo` (`.cmd` `run --clock-ms 100 {}`): prints `now_ms()` three times: `100`, `101`, `102`.
  - Under `tests/cases/test/`, `effect_denied_test.vo`: a test with `uses file.read("fixtures/")` that reads `other/x`. The test fails with R0010 in its JSONL `test` record, and the record has `uses`. Also `clock_in_test.vo`: a test reading `now_ms()` gets 0, then 1.
- [X] T012 [P] [US1] Write `tests/scope_fuzz.py` (SC-003a): `--iterations N --seed S`.
  - **Tree:** create a temp tree containing `in/`, `in/sub/`, `secret/f`, `in/link_out -> ../secret`, `in/link_in -> sub` and `in/sub/f`.
  - **Programs:** for each iteration, generate a random path out of `in`, `sub`, `..`, `.`, `link_out`, `link_in`, `f`, absolute prefixes and repeated `/`. Write a one-line program, `fn main() uses console, file.read("in/") { … read_file(<path>) … }`, and run it from the tree root.
  - **Expectation:** compute in Python whether `os.path.realpath(path)` lies under `realpath("in")`. Inside: the run must not report R0010 (a missing file gives a normal `err`). Outside: it must exit 70 with R0010.
  - **env:** do the same for `env` with random names against `env("A_OK")`.
  - Print the counts. Exit 1 on any mismatch.
- [X] T013 [P] [US1] Write the fmt golden case `tests/cases/fmt/uses_order.vo` and its `.expected.vo`. Out-of-order effects (`uses file.write("b/", "a/"), console`) become `uses console, file.write("a/", "b/")`; a lowercase spacing variant is normalized too.

### Implementation for User Story 1

- [X] T014 [US1] Parse `uses` clauses in `src/parse.s`.
  - **Where:** after the optional `-> type` of `fn` declarations, and after the name string of `test`.
  - **Nodes:** build `N_USES` (A = vector of `N_EFFECT`). Each effect is a name, `file.read` or `file.write` as two identifiers joined by `.`, and an optional parenthesized list of string literals (`N_STR`). Store the clause in the `FX` side record (`FX_USES`) hung off `N_B` of the fn's name node, and in `N_C` for `N_TEST`.
  - **Unknown name:** keep an `N_EFFECT` with bit 0 so the checker reports E0509.
  - **Recovery:** on a malformed clause, sync to `{` with `p_sync_item` rules.
- [X] T015 [US1] Declared sets in `src/check.s`.
  - **Build the declared set:** while declaring functions and tests (`check_items` first pass), build `FX_DECLARED` from `FX_USES` with `es_new`/`es_add_scope`.
  - **Report bad entries:** E0506 for scopes rejected by `es_add_scope`. E0509 for unknown names, with a fix replacing the name by the closest of the 8 canonical names when `name_distance` ≤ 2.
  - **Symbol:** store the set pointer in `SY_EFFECTS`.
  - **Docs:** add E0506 and E0509 to `docs/errors.md`.
- [X] T016 [US1] Add the builtin effect table to `src/check_expr.s`: a byte table indexed by builtin id giving the effect bit (contracts/grammar-v2.md "Effects of v1 builtins"). While checking a function body, every builtin call ORs its bit into the current function's direct-requirement set. Every user call records a call edge `{caller decl, callee decl, call node}` into a per-program vector.
- [X] T017 [US1] Requirement fixpoint in `src/check.s` (after all bodies are checked).
  - **Fixpoint:** `need(f) = direct(f) ∪ ⋃ over calls f→g of (declared(g) ∪ need(g))`. Iterate over the call-edge vector until no set changes; sets only grow, so this terminates. Store the result in `FX_REQUIRED`. Mark it `ponytail: O(edges × rounds); SCC order if programs get large`.
  - **Diagnostics, per function and test:**
    - **E0501** when `es_missing(declared, need)` has bits the declaration lacks. The message lists the missing effects in canonical order. The fix rewrites the whole `uses` clause to `es_render(es_union(declared, need))`, or inserts ` uses …` before the body's `{` (functions) or after the name string (tests) when there is no clause.
    - **E0503** when every bit is present but scopes are missing. `related` = the callee's `uses` entry that needs the scope. The fix widens the clause the same way.
    - **W0501** when `es_unused(declared, need)` is non-empty. The fix rewrites the clause without the unused part, or deletes ` uses …` when nothing remains.
  - **Ordering:** diagnostics go in source order, so the golden output is stable.
  - **Docs:** add E0501, E0503 and W0501 to `docs/errors.md` (`Fixable: yes`).
- [X] T018 [US1] Effect frames and runtime enforcement in `src/interp.s` and `src/rt_io.s`.
  - **Frames:** `call_fn` sets `FR_EFFECTS` of the new frame to the callee's `FX_DECLARED`. `run_main` and `run_test_body` set it to `main`'s or the test's declared set; that set is the grant.
  - **`rt_check_path(effect_bit, path) -> 0 or raises R0010`:**
    1. Read the innermost frame's set. No scopes for the bit means it is allowed.
    2. Make the path absolute against the start working directory (captured once at startup with `getcwd`).
    3. Resolve every component with `readlinkat` (symlink targets are followed, at most 40 links, else deny), and resolve `.` and `..` lexically after each resolution. For `file.write` of a path that doesn't exist, resolve the parent directory and append the final name.
    4. Resolve each scope the same way (relative to the start directory), and allow when a resolved scope is a byte prefix of the resolved path.
  - **Call sites:** call `rt_check_path` from `read_file` (`EF_FREAD`) and `write_file` (`EF_FWRITE`). Call `rt_check_env(name)` (exact match against the env scopes) from `env`.
  - **Effect bits (US1 scenario 5, "enforced again at run time"):** every effectful builtin (`print`, `eprint`, `write`, `read_line`, `read_all`, `read_file`, `write_file`, `now_ms`, `random_int`, `env`) first calls `rt_check_bit(bit)`, which raises R0010 when the innermost frame's set lacks the bit. Checked programs can't reach this path, because the checker already guarantees the bit. It is defense in depth, and R0010's reporting path is covered by the scope cases. Mark it `ponytail: one mask test per effectful builtin call`.
  - **R0010:** raise it with the fields from contracts/output-schema-v2.md (`effect`, `target`, `allowed` as a JSON string array, `declared_by` = the innermost frame's function name).
  - **Docs:** add R0010 to `docs/errors.md`.
- [X] T019 [US1] New builtins `env`, `random_int`, and clock control in `src/rt_call.s` and `src/rt_str.s` (or the closest existing runtime file), plus checker typing in `src/check_expr.s`.
  - **`env(name: str) -> result[str]`:** scans the `envp` saved by `_start` in `src/sys.s` (save it there if it isn't saved yet). A missing variable is `err("not set")`.
  - **`random_int(lo, hi) -> int`:** a PCG-XSH-RR 64/32 state in `.data`, combining two outputs into 64 bits, mapped to `[lo, hi]` by rejection sampling. `lo > hi` is R0007.
  - **`now_ms`:** when `clock_fixed` is set, returns `clock_next++`.
  - **Builtin ids:** add the new ones to the `rt_call.s` jump table and to the effect table: env→`EF_ENV`, random_int→`EF_RANDOM`.
- [X] T020 [US1] `--seed N` and `--clock-ms N` in `src/main.s` for `run` and `test`.
  - `test` defaults to seed 1 and clock 0.
  - `run` without `--seed` seeds from `clock_gettime` nanoseconds, and without `--clock-ms` uses the real clock.
  - Usage errors follow v1's E0001 style.
  - Update the `vo help` text and contracts/cli-v2.md if the wording changes.
- [X] T021 [US1] Test grants in `src/testrun.s`.
  - Each test runs with `FR_EFFECTS` = its declared set. Reset the PRNG to the seed and the fixed clock to its start value before every test, so tests don't affect each other.
  - Add `"uses":"<es_render>"` to the JSONL `test` record. The human `PASS`/`FAIL` lines are unchanged.
- [X] T022 [P] [US1] Canonical printing of `uses` clauses in `src/fmt.s`: ` uses ` plus `es_render` of the declared set, rebuilt from the clause's `N_EFFECT` nodes so the order and spacing are canonical. Scope strings are printed as written after normalization (`./out` becomes `"out/"`). Run `tests/fmt_corpus.py`.
- [X] T023 [US1] Migrate the in-repo corpus to v2. This is what makes `make test` pass again with effects mandatory.
  - **Write mode:** add `tests/migrate.py --write PATH...`. It runs `bin/vo check --json` on each entry file and applies the first fix of every E0501 diagnostic, non-overlapping and back to front, exactly once. The US5 verification mode builds on it.
  - **Corpus:** run it over `tests/cases/**` (except cases whose purpose is a lex or parse error), `bench/tasks/*/solution.vo`, `tests/fmt_corpus` inputs and the `Example`/`Fixed` programs in `docs/errors.md`.
  - **Docs:** add `uses` clauses by hand to `docs/reference.md` examples.
  - **Expected outputs:** regenerate a `.out` only where its sole difference is the expected v2 change, and review each diff.
  - **Target:** 134/134 plus the new US1 cases.
- [X] T024 [US1] Write the reference section "Effects" in `docs/reference.md`:
  - the 8 effects
  - `uses` syntax with scopes and the canonical order
  - "no clause = pure, same result for the same arguments"
  - the grant rule
  - runtime refusal and R0010
  - `--seed` and `--clock-ms`
  - the builtin effect table
  - one runnable example

  Keep it within budget: `tests/ref_examples.py` reports the token count.

**Checkpoint**: The US1 golden cases, `scope_fuzz.py`, catalog, `apply_fixes` and `ref_examples` all pass. Effects work end to end without US2–US6.

---

## Phase 4: User Story 2 — Review what a change allows, not every line (Priority: P1)

**Goal**: `vo.effects` records every top-level function's and test's effects. `vo check` fails on growth (E0504, with call paths), reports reductions and renames, and `vo effects [--update] [--json]` prints the capability summary and rewrites the record.

**Independent Test**: quickstart.md US2 and `tests/record_changes.py` (SC-004).

### Tests for User Story 2 (write first, must fail)

- [X] T025 [P] [US2] Write `tests/record_changes.py` and change sets `tests/effect_changes/NN/`: 20 sets that alter effects (SC-004), plus the no-change sets marked below as extras. Each has `before/` (a multi-file project with `main.vo`), `after/` (the changed files only, overlaid on `before/`) and `expected.txt` (one `<change> <file>:<function>` per line, sorted).
  - **Procedure:**
    1. Copy `before/` to a temp dir, run `vo effects --update main.vo` (exit 0), and check that `vo check main.vo` exits 0.
    2. Overlay `after/` and run `vo check --json main.vo`.
    3. Collect E0504, W0502 and W0503 into `<change> <file>:<function>` lines (growth → `added`, `widened`; reduction → `removed`, `narrowed`; `renamed`) and compare them with `expected.txt` exactly (SC-004: no false positives or negatives).
    4. Run `vo effects --update --json main.vo` and check that its `effect_change` records equal the same set.
    5. Diff the old and new `vo.effects`: the changed lines must be exactly the listed functions, plus `main` and the other transitive callers when they grew.
    6. Check that `vo check` exits 0 again.
  - **Coverage of the sets:**
    - deep helper gains `file.write` (callers listed)
    - scope widening `out/` → `out/`, `tmp/`
    - scope removal (unscoped = growth)
    - narrowing
    - effect removal
    - a new function with effects
    - extra: a new pure function (no change expected)
    - a deleted function with effects
    - extra: a deleted pure function (no change expected)
    - a rename with identical effects (W0503)
    - a rename plus an effect change (shows as removal plus addition)
    - a test gaining an effect
    - an env scope added
    - two functions growing through one shared callee
    - growth through a function value passed to `map` (needs US3; mark that set `REQUIRES=func` and skip it until US3 lands)
    - `clock`/`random` added
    - an import added that brings in an effectful function
    - extra: `--update` on a program with check errors exits 65 and leaves `vo.effects` byte-identical
    - and 5 more mixed effect-altering sets, so 20 sets alter effects
- [X] T026 [P] [US2] Write the golden cases in `tests/cases/effects/`:
  - `record/main.vo` plus helpers plus a committed `vo.effects`. The helper's code grew, so `check --json {}` shows E0504 on the helper and on `main`, with `related` call paths (`main.cmd`, `main.out`, `main.code` = 65).
  - `summary/main.vo` with `main.cmd` `effects {}`: the human capability summary, `main` plus each test.
  - `summary_json/main.vo`: `effects --json {}`, with `capabilities` records.
  - `no_record/main.vo` (two files, no `vo.effects`): `check --json {}` gives one W0504, exit 0.
  - `single_file_no_record.vo`: no W0504.
  - `record_shrink/main.vo` plus a committed `vo.effects` listing a helper with more effects than it now declares: `check --json {}` gives W0502, exit 0.
  - `record_rename/main.vo` plus a committed `vo.effects` with the helper's old name: W0503 and no E0504, exit 0.
  - `record_malformed/main.vo` plus a `vo.effects` with a bad line (`main.vo:main uses consoel`): E0510 with the record's line number, exit 65.


### Implementation for User Story 2

- [X] T027 [US2] Read, write and compare the record in `src/effects.s` (contracts/effects-record.md):
  - **`rec_find(entry_path) -> dir or 0`:** the nearest ancestor of the entry file that contains `vo.effects`.
  - **`rec_parse(text) -> vector of {path, name, kind, set}`:** parses `<path>:<name> uses <effects>` or `pure`, skipping `#` comment lines. It reuses the lexer on the effect text by parsing it as a `uses` clause. A malformed line is diagnostic E0510 `invalid_effects_record` (contracts/error-codes-v2.md) with the line number, and no fix. Add E0510 to `docs/errors.md`.
  - **`rec_build(program) -> vector`:** one entry per top-level function and test across the entry file and its imports, with paths relative to the project root and the declared set. Sorted by (path, name) byte-wise.
  - **`rec_render(vector, comments) -> sb`**
  - **`rec_compare(old, new) -> vector of changes`:**
    - **added:** a new entry with a non-empty set, or an existing one that gained a bit.
    - **widened:** an existing entry whose scope went from scoped to unscoped, or gained a new scope not covered by the old one.
    - **removed and narrowed:** the reverse cases.
    - **renamed:** pairs a removed and an added entry in the same file with an equal set, in file order. Mark it `ponytail: pairs by identical effects; body similarity if mispairing shows up`.
    - New pure functions and removed pure functions produce no change.
- [X] T028 [US2] Comparison in `vo check` (`src/main.s` `cmd_check` and `src/check.s`).
  - After a clean check, when `rec_find` finds a record, compare it:
    - Growth: E0504 per function. The message is "`<fn>` gains <effects>; approve with `vo effects --update`". `related` lists one call path per contributing callee: walk the call-edge vector from the grown function to a callee whose declared set supplies the new bit, and give each hop as a `related` entry with the call node's position.
    - Reduction: W0502.
    - Rename: W0503.
  - With no record and more than one source file: one W0504.
  - Add E0504 and W0502–W0504 to `docs/errors.md` (`Fixable: no`; the `Fixed` section shows the record line after `--update`). Teach `tests/catalog.py` that E0504's `Fixed` example needs a `vo.effects` file beside it. The simplest way is a fenced `vo.effects` block in the entry, written next to the example.
- [X] T029 [US2] `cmd_effects` in `src/main.s` (contracts/cli-v2.md).
  - **Arguments:** parse `[--json] [--update] FILE` and load and check the program like `cmd_check`. Check errors: print them and exit 65 without writing.
  - **Without `--update`:** print the summary. Human form: `main uses …` and `test "<name>" uses …` lines, then the record lines. `--json`: one `capabilities` record per entry point (output-schema-v2.md), then `summary`.
  - **With `--update`:** compute `rec_compare` against the existing record (or an empty one) and write `vo.effects` with `write_file_safely`. Leading comment lines are kept; a new file gets the header comment from contracts/effects-record.md. Print each change as a human line `+ save_report: file.write("out/reports/")` or as an `effect_change` JSONL record (with `before`, `after`, `via`). Exit 0.
  - Add `effects` to `vo help` and to the reference.
- [X] T030 [US2] Write the reference subsection "Reviewing effects" in `docs/reference.md`: `vo.effects`, `vo effects`, `vo effects --update`, E0504 and how a reviewer reads the diff. Runnable where possible; the CLI lines are prose.

**Checkpoint**: `record_changes.py` passes every set except the `func` one, which is pending until US3. The US2 golden cases pass.

---

## Phase 5: User Story 3 — Functions as values (Priority: P1)

**Goal**: Anonymous functions, function types, closures capturing `let` values by value, named functions as values, calls on any expression, and the HOF builtins `sort_by`, `map`, `filter`, `fold`, `any` and `all`, with effects flowing through.

**Independent Test**: quickstart.md US3. Sort records by a field with a tie-break, filter, fold, and call a function stored in a record field.

### Tests for User Story 3 (write first, must fail)

- [X] T031 [P] [US3] Write the runtime golden cases in `tests/cases/func/` (default `run {}`):
  - `sort_by_field.vo`: records sorted by `age`, stable on ties, with a second `sort_by` for the tie-break order.
  - `map_filter_fold.vo`: map to squares, filter evens, fold sum.
  - `any_all.vo`: short-circuit, shown by printing inside the predicate (`main uses console`).
  - `closure_let.vo`: `make_adder(n)` returns `fn(x: int) -> int { return x + n; }`. `make_adder(1)(2)` prints 3.
  - `field_call.vo`: a record field `on_save: fn(str) -> void uses console`, called as `h.on_save("x")`.
  - `list_of_fns.vo`: `list[fn(int) -> int]` called as `fs[1](3)`.
  - `named_fn_value.vo`: `map(xs, double)`.
  - `capture_snapshot.vo`: a captured `let` copy is unaffected by later shadowing.
  - `fn_value_render.vo`: `print` of a function value prints `<fn double>` and `<fn anonymous at 3:13>`.
- [X] T032 [P] [US3] Write the diagnostic golden cases in `tests/cases/func/`, each with `.cmd` `check --json {}`:
  - `capture_var.vo`: E0505 with the fix inserting `let total_now = total;` before the statement containing the anonymous function and renaming the uses inside it.
  - `too_many_effects.vo`: a printing lambda passed where `fn(int) -> int` is expected (a user parameter type written in the file). E0502 names `console`, with a fix adding ` uses console` to that function type.
  - `map_with_print.vo`: `map(xs, fn(x: int) -> int { print("x"); return x; })` in a pure caller. E0501 on the caller with `console`.
  - `uses_on_lambda.vo`: E0507 with a removal fix.
  - `lambda_missing_return_type.vo`: `fn(x: int) { return x * 2; }` gives v1's return-type error (no inference; one spelling).
  - `call_value_arity.vo` and `call_value_types.vo`: the same codes and messages as direct calls (FR-022).
  - `fn_type_mismatch.vo`: E0301 with readable types `fn(int) -> int` vs `fn(str) -> int`.
  - `method_call_still_e0110.vo`: `xs.len()` on a list still gives v1's E0110 with its rewrite fix. Keep the existing v1 E0110 cases passing unchanged.
  - `compare_fns.vo`: `f == g` is a type error with the existing operand-type code.
- [X] T033 [P] [US3] Write `tests/cases/fmt/lambda.vo` and its `.expected.vo`: canonical spacing for lambdas, function types and calls on call results. Write `tests/cases/parse/lambda_errors.vo`: missing parameter type, a missing body, `fn` in a type position without parentheses, each with a structured error and no crash. Write `tests/cases/parse/fn_statement.vo` (`fn` at the start of a statement in a block gives E0110) and `tests/cases/func/call_statements.vo` (`make_adder(1)(2);`, `handlers[0](x);` and `r.on_save(x);` as statements run).

### Implementation for User Story 3

- [X] T034 [US3] Function types in `src/types.s` (new file). Includes `defs.inc`.
  - **`ty_func(params_vec, ret, effset) -> type`:** interned by structural equality of params, return and `es_equal` effects.
  - **`ty_render`** extended for `TY_FUNC`: `fn(int, str) -> bool uses console`, with `-> void` omitted. Move the existing type rendering here if it is small. Otherwise call into it, and leave a one-line note on where rendering lives.
  - **`ty_fits(value_type, expected_type)`:** equal types, except that for `TY_FUNC` the params and return are equal and `es_covers(expected.effects, value.effects)`.
- [X] T035 [US3] Parser in `src/parse.s`:
  - **Function types:** `fn (types) [-> type] [uses …]` in type position, producing `N_FN_TYPE`.
  - **Anonymous functions:** `fn (params) [-> type] block` in expression position, producing `N_LAMBDA`. A `uses` after it is parsed and kept for E0507.
  - **Calls on any expression:** postfix `(args)` after any postfix expression produces `N_CALL_VALUE`. A plain `name(args)` stays `N_CALL`. An expression statement may be any call, including `N_CALL_VALUE`. `fn` at the start of a statement inside a block is E0110 with the message from contracts/grammar-v2.md.
  - **Method syntax:** `x.f(args)` produces `N_CALL_VALUE` with an `N_FIELD` callee. The checker reports E0110 for it now, with the same JSON. Keep the parser's `p_method_call` E0110 only for files that have parse errors (the checker never runs on them), so v1 golden cases with both a parse error and a method call keep their output.
  - All within `p_enter`/`p_leave` depth limits.
- [X] T036 [US3] Check anonymous functions and captures in `src/check_expr.s` and `src/check.s`:
  - **Depth:** a lambda opens a new scope at depth `d+1` (`SY_DEPTH` on locals).
  - **Captures:** an identifier resolving to a local of smaller depth is a capture. A `let` or parameter is added to the lambda's capture vector `{symbol, outer slot, inner slot}` once and gets an inner slot after the params. A `var` is E0505 with its fix.
  - **Return type:** the lambda's `N_E` local count includes the capture slots. A missing `->` means `void`, exactly as for named functions, so returning a value from it gives v1's existing return-type error (FR-017: one spelling).
  - **Effects:** the lambda's effect set is the union of its body's direct builtin bits and the declared sets of what it calls (the same accounting as US1, collected into the lambda instead of the enclosing function). Its type is `ty_func(params, ret, effects)`.
  - **What reaches the enclosing function:** creating a lambda contributes nothing to the enclosing function. Calling a function value contributes its type's effects. Passing a function to a HOF builtin contributes that function's effects.
- [X] T037 [US3] Function values and calls through values in `src/check_expr.s`:
  - **Named functions as values:** an `N_IDENT` resolving to `SYM_FN` has type `ty_func` of its signature and `SY_EFFECTS`.
  - **`N_CALL_VALUE`:**
    - If the callee is `N_FIELD` on a record with a field of that name and function type, it's a field call.
    - If the object's type has no such field (or isn't a record), report v1's E0110 with its existing rewrite fix and message.
    - Otherwise the callee type must be `TY_FUNC`.
  - **Arguments:** check them against the params with the same diagnostics as direct calls; factor out the shared argument checker instead of duplicating it. The call's effects are added to the current function's direct requirements.
  - **Assignment compatibility:** every place that checks assignment compatibility (args, `let` annotations, returns, field inits, list elements) uses `ty_fits`. A function value with too many effects is E0502, naming the extra effects. The fix is offered only when the expected type comes from an `N_FN_TYPE` written in the program: append to or create its `uses` clause.
  - **Equality:** `==`/`!=` on function types is rejected with the existing operand-type error.
  - **Docs:** add E0502, E0505 and E0507 to `docs/errors.md`.
- [X] T038 [US3] Higher-order builtins, typing in `src/check_expr.s`: `sort_by`, `map`, `filter`, `fold`, `any`, `all` (signatures in contracts/grammar-v2.md). These are the first builtins with type variables. Implement them with a tiny local binding of `T`, `U` and `A` (no user generics needed yet). The `sort_by` key must be `int`, `float` or `str`, otherwise E0313 `not_ordered` (add it to `docs/errors.md` here, with a message that names the type and says it is not ordered; US4 reuses it). The argument function's effects are added to the caller's direct requirements. Effect table entry: 0 (the effects flow from the argument).
- [X] T039 [US3] Closures at run time in `src/interp.s`:
  - **Creation:** evaluating `N_LAMBDA` allocates a closure object: `CL_FN` = the node, captures copied from the current frame's outer slots, and `CL_TARGS` = the current `FR_TARGS`.
  - **Named functions as values:** a closure with `n = 0`, cached per function.
  - **`call_value(closure, args)`:** pushes a frame with `FR_FN` = `CL_FN` and `FR_TARGS` = `CL_TARGS`. `FR_EFFECTS` is the named function's `FX_DECLARED`, or for a lambda the calling frame's `FR_EFFECTS`; the static check guarantees the caller's declaration covers the lambda's type, and this is also correct for lambdas returned out of the function that created them. It then copies args and captures into slots, and runs the body like `call_fn`. Factor the common part out of `call_fn`.
  - **Field calls** evaluate the field, then `call_value`.
  - **Rendering:** `render_value` for closures prints `<fn NAME>` or `<fn anonymous at L:C>`.
- [X] T040 [US3] Higher-order builtins at run time in `src/rt_call.s` and `src/rt_list.s`:
  - `map`, `filter`, `fold`, `any` and `all` loop with `call_value`. `any` and `all` short-circuit.
  - `sort_by` computes the keys once into a parallel array, then runs a stable merge sort on (key, index) with the existing comparison for `int`/`float`/`str`, and permutes in place.
  - A runtime error inside the callback propagates normally.
- [X] T041 [P] [US3] `src/fmt.s`: print lambdas (`fn(x: int) -> int {` plus the block with the existing indentation rules), function types and `N_CALL_VALUE` chains. Run `tests/fmt_corpus.py`.
- [X] T042 [US3] Write the reference section "Functions as values" in `docs/reference.md`: anonymous functions (one syntax, capture rules), function types with `uses`, named functions as values, the HOF builtins with a `sort_by` plus `map` plus `fold` example, and the E0505 copy-to-`let` idiom. Enable the `REQUIRES=func` change set in `tests/record_changes.py`.

**Checkpoint**: The US3 golden cases pass, `record_changes.py` passes every set, and the US1 and US2 cases still pass.

---

## Phase 6: User Story 4 — User-defined generics (Priority: P2)

**Goal**: `[T]`, `[T: ordered]` and `[E: effects]` on functions, records and variants. Type arguments are inferred only, and bodies are checked once. Readable types such as `Stack[int]` and `Tree[list[str]]`.

**Independent Test**: quickstart.md US4. `Stack[T]` with `int` and `str`, `Tree[T]` matched exhaustively, `max_of[T: ordered]` with `int` and `str` and rejected for a record.

### Tests for User Story 4 (write first, must fail)

- [X] T043 [P] [US4] Write the runtime golden cases in `tests/cases/generic/`:
  - `stack.vo`: `Stack[int]` and `Stack[str]` with `push`/`pop` (`result[T]` on empty).
  - `swap.vo`: `Pair[A, B]`.
  - `tree.vo`: `variant Tree[T] { leaf; node(Node[T]); }` with an exhaustive `match` and a recursive sum.
  - `max_of.vo`: `[T: ordered]` with `int` and `str`.
  - `generic_eq_render.vo`: `==` on `Pair[int, str]` values, plus `print` of a generic record.
  - `generic_sort.vo`: `fn sorted[T: ordered](xs: list[T]) -> list[T]` using key-less `sort`.
  - `each_effects.vo`: `fn each[T, E: effects](xs: list[T], f: fn(T) uses E) uses E`, called with a printing lambda from `main uses console`.
  - `deep_nesting.vo`: `Stack[Tree[list[str]]]` works.
- [X] T044 [P] [US4] Write the diagnostic golden cases in `tests/cases/generic/`, each with `.cmd` `check --json {}`:
  - `mixed_elements.vo`: pushing `str` onto `Stack[int]`. E0301 shows `Stack[int]`.
  - `cannot_infer.vo`: `let s = empty_stack();` with no later use. E0310 names `T`, with no fix.
  - `cannot_infer_fixable.vo`: `let s = empty_stack(); push(s, 1);`. E0310 with a fix inserting `: Stack[int]`. The fix is offered only when the first later use of the name in the same block passes it as an argument whose parameter type becomes concrete by unifying the other arguments.
  - `needs_ordered.vo`: `<` on an unconstrained `T`. E0311 is reported in the generic body, with the fix `[T: ordered]`.
  - `not_ordered.vo`: `max_of` with a record. E0313 at the call.
  - `wrong_arg_count.vo`: `Stack[int, str]`. E0312.
  - `effect_param_missing.vo`: `each` called from a pure `main` with a printing lambda. E0501 on `main`.
  - `deep_message.vo`: the error message shows `Tree[list[str]]`.

### Implementation for User Story 4

- [X] T045 [US4] Parser in `src/parse.s`:
  - **Type-parameter lists:** `[tparam, …]` after the name of `fn`, `record` and `variant`, producing `N_TPARAM` (A = name, B = constraint 0/1/2, from the contextual words `ordered`/`effects` after `:`). Stored in `FX_TPARAMS` for functions, and in a free field of `N_RECORD`/`N_VARIANT` (`N_C`).
  - **Type arguments:** in type position, `Type[type, …]`, producing the existing `N_TYPE_NODE` with a type-argument vector in a free field.
  - **`new`:** `new Type[args] { … }`.
  - **Effect parameters:** a `Type`-shaped identifier inside `uses` is an effect variable (`N_EFFECT` `C`).
- [X] T046 [US4] Generic types in `src/types.s`:
  - **`ty_param(owner, index, constraint)`**
  - **Record and variant instances:** interned with their argument vector in `TY_B`. `ty_render` prints `Name[args]`.
  - **`ty_subst(type, owner, args)`:** walks through list, map, result, func and instance arguments, and substitutes effect variables in `TY_FUNC` effect sets via `es_subst`. Add `es_subst` to `src/effects.s`.
  - **`ty_unify(param_type, arg_type, bindings) -> ok`:** structural, binding `TY_PARAM`s of the callee. Effect variables bind to the argument function's effect set.
- [X] T047 [US4] Checking generic declarations in `src/check.s`:
  - Type parameters are in scope in the signature and body as `TY_PARAM`.
  - Field types of generic records and variants reference them.
  - Bodies are checked once: an unconstrained parameter allows only `==`/`!=`; `<`, `<=`, `>`, `>=`, `min`, `max` and key-less `sort` on it give E0311, with a fix adding `: ordered` at the declaration (FR-026).
  - Wrong instance argument counts are E0312.
- [X] T048 [US4] Inference at use sites in `src/check_expr.s`.
  - **Generic calls:** unify params with argument types, then the return type with the expected type when one exists. All parameters must be bound, otherwise E0310 naming the first unbound one, with the fix above when its rule applies. Then check arguments with the substituted types. The call's type is the substituted return type, and its effects are the substituted `uses` set (effect variables replaced by the argument functions' sets).
  - **Field access and `new`:** substitute the instance arguments.
  - **Variant cases with no payload:** take the arguments from the expected type, else E0310.
  - **`ordered` instantiated with a non-ordered type:** E0313.
  - **Recording:** store the inferred argument vector on the `N_CALL` node (a free field) for the runtime.
  - **Docs:** add E0310–E0312 to `docs/errors.md` (E0313 was added in T038).
- [X] T049 [US4] Generics at run time in `src/interp.s` and `src/rt_render.s`.
  - Calls to generic functions set `FR_TARGS` to the call node's inferred argument vector, with any `TY_PARAM` inside it resolved through the caller's `FR_TARGS`.
  - `values_equal`, `render_value`, key-less `sort` and map hashing receive types that may contain `TY_PARAM`. Resolve them through the current frame's `FR_TARGS` before dispatching. Mark it `ponytail: substitution walk per op; monomorphize hot paths if generic code is slow`.
  - Record and variant values need no change, because their instance type carries the arguments.
- [X] T050 [P] [US4] `src/fmt.s`: type-parameter lists, instance types and `new Type[args]`. Run `tests/fmt_corpus.py`.
- [X] T051 [US4] Write the reference section "Generics" in `docs/reference.md`: declarations, `ordered`, `effects` parameters with the `each` example, the "type arguments are inferred; annotate the `let` when needed" rule, and a `Stack[T]` example.

**Checkpoint**: The US4 golden cases pass, and all earlier phases still pass.

---

## Phase 7: User Story 5 — v1 programs move to v2 in one round of fixes (Priority: P2)

**Goal**: Every v1 program checks cleanly after one application of all first fixes and behaves byte-identically (SC-005).

**Independent Test**: `python3 tests/migrate.py` passes over `tests/migrate/v1/`.

- [X] T052 [US5] Verification mode in `tests/migrate.py` (the default, no arguments). For every entry in `tests/migrate/v1/MANIFEST`:
  1. Copy the entry's directory to a temp dir.
  2. Run `bin/vo check --json`. Assert that every diagnostic is E0501 (or W0504 for multi-file programs), as FR-033 says: migration only.
  3. Apply the first fix of every diagnostic once, non-overlapping.
  4. Assert that `check` exits 0 with no errors.
  5. Assert that `vo fmt --check` exits 0, meaning the inserted clauses are already canonical (US5 scenario 2).
  6. Run the program with the recorded `.in`, and compare stdout and the exit code with `<name>.out` and `<name>.code` byte for byte.

  Print `migrated N/N`. Exit 1 on any failure, listing the entry and the step that failed.
- [X] T053 [US5] Run `tests/migrate.py` and fix every failure at its root in the checker. Typical causes:
  - an incomplete transitive set
  - a fix that isn't canonical
  - a diagnostic other than E0501 on v1 code, such as a new keyword collision (if so, ensure `KNOWN_RENAMES` is handled)

  Repeat until N/N.

**Checkpoint**: `migrate.py` passes N/N.

---

## Phase 8: User Story 6 — A benchmark that can separate languages (Priority: P1)

**Goal**: `bench2/` with 40+ tasks in 4 categories plus a 20-task feature set, and a harness that runs vo and Python identically (contracts/benchmark-v2.md).

**Independent Test**: `bench2/validate.py` passes (every reference solution passes its hidden cases, and every capability task's violating variant is caught in both languages). The runs themselves happen in Phase 9.

The authoring tasks below are independent of US1–US5 except where they need v2 syntax for vo solutions, and can start right after Phase 2. Write the vo references after the features they use exist.

- [X] T054 [P] [US6] Write `bench2/agent.py` and `bench2/run_agent.sh`, starting from `bench/agent.py` (same `claude -p` invocation, model and limits). The protocol is in contracts/benchmark-v2.md:
  - **Arguments:** `--lang vo|python`, `--set tasks|features`, `--only TASK`, `--attempts 3`.
  - **Prompt:** the task's `task.md`, the starter tree (each file as a `# file:` block), the visible cases and, for vo, `vo reference`.
  - **Reply parsing:** fenced blocks each starting with `# file: <path>`. Reject absolute paths and `..`.
  - **Feedback:** after a failed attempt, the toolchain output plus the failing visible cases, and "N hidden checks failed" (never their contents).
  - **Result:** one JSONL record per task appended to `bench2/results/<lang>[-features].jsonl`, with the fields `{task, category, lang, model, attempts, solved, hidden_passed, hidden_total, declared_violation, behavioral_violation, seconds}`.
- [X] T055 [P] [US6] Write `bench2/grade.py`, used by `agent.py` and `validate.py`.
  - `grade(task_dir, solution_dir, lang) -> {visible_failures, hidden_passed, hidden_total, declared_violation, behavioral_violation, toolchain_output}`.
  - **Running:** for vo, `bin/vo check --json` then `bin/vo run` on the entry; for Python, `python3 -I bench2/py_sandbox.py` on the entry. Each run happens in a fresh temp copy of the solution tree with the task's fixture files.
  - **Capability tasks:** parse `allowed_effects` with a small Python parser of the `uses` syntax.
    - **vo, declared:** run `vo effects --json`; `main`'s set not covered by the allowance (same rules as `es_covers`) is a declared violation.
    - **vo, behavioral:** run the hidden cases in a temp tree where only the allowed directories exist and are writable, and record any R0010 and any file created or changed outside them.
    - **Python, behavioral:** run through `py_sandbox.py`, which writes its observation log to a file named by an environment variable. Any entry outside the allowance is a behavioral violation.
- [X] T056 [P] [US6] Write `bench2/py_sandbox.py`. It installs `sys.addaudithook` and records these events:
  - `open` with its path and mode (write when the mode contains `w`, `a`, `x` or `+`)
  - `os.remove`, `os.rename`, `os.mkdir`
  - `socket.*`, `subprocess.Popen`, `os.system`
  - `time.time`/`datetime.now`, by wrapping them; `random.*`, by wrapping the module
  - reads of `sys.stdin`

  It replaces `os.environ` with a recording mapping, then runs the target with `runpy.run_path`. Paths are resolved with `os.path.realpath` against the start directory before logging. Mark it `ponytail: audit hooks miss C extensions/ctypes; seccomp or strace if that matters`.
- [X] T057 [P] [US6] Write `bench2/validate.py`. For every task:
  - each `ref/<lang>/` solution passes all visible and hidden cases;
  - for capability tasks, each `ref_violating/<lang>/` variant (one extra effect: a stray file write, a log to a file outside the allowance, an env read) is graded with a violation (declared or behavioral for vo, behavioral for Python);
  - the task tree is complete: `task.md`, `category`, `visible/`, `hidden/`, and `starter/` where the category needs one.

  Exit 1 on any problem.
- [X] T058 [P] [US6] Write `bench2/summarize.py`, which reads the result JSONL files.
  - **SC-001:** first-attempt success overall and per category per language. Flag "at ceiling" when a language is above 85% overall, or when fewer than two categories are below 95% for both.
  - **SC-002:** vo against Python on solved within 3 attempts, mean attempts and the relative difference, with a pass/fail line. For capability tasks this comparison uses behavioral violations in both languages.
  - **SC-003:** for capability tasks, the share of vo violating solutions caught by either kind (must be 100%) and Python's undetected-violation rate.
  - **`--set features`:** SC-006 success within 3 attempts (≥ 90%) and on the first attempt (≥ 75%).
- [X] T059 [P] [US6] Author 10 `multi_file` tasks in `bench2/tasks/m01..m10/`, each with:
  - `task.md` (the statement names the required module split: at least 3 files)
  - `category`
  - `visible/` (3 cases)
  - `hidden/` (at least 8 edge-case cases)
  - `ref/vo/` and `ref/python/`

  Topics differ from bench v1, e.g. an inventory ledger with parser, store and report modules, a tiny expression evaluator, a todo CLI over stdin commands, a CSV join, a bank with transaction log replay.
- [X] T060 [P] [US6] Author 10 `refactor` tasks in `bench2/tasks/r01..r10/`.
  - **Starter tree:** in both languages, 3+ files and 150+ lines, behaviorally equivalent across the two languages.
  - **Instruction:** e.g. rename `Item` to `Product` everywhere; add a `currency` parameter to `format_price` and update all 9 callers; extract the duplicated validation into one function; split a module.
  - **Grading:** `hidden/` cases cover the changed behavior plus the "unchanged behavior" cases.
  - `ref/<lang>/` is the refactored tree.
- [X] T061 [P] [US6] Author 10 `hidden_property` tasks in `bench2/tasks/h01..h10/`: a short statement plus 3 visible cases, and at least 15 hidden cases probing overflow, empty input, unicode, large n (within the 10 s timeout under qemu), duplicates and boundary values. `ref/` in both languages.
- [X] T062 [P] [US6] Author 10 `capability` tasks in `bench2/tasks/c01..c10/`.
  - **Statement:** an explicit allowance, e.g. "may read `data/`, may print; nothing else", plus `allowed_effects` in `uses` syntax and fixture files.
  - **Tempting extras:** the statements include things like "log progress" (to console only), "cache results" (not allowed), "read config from the env var `APP_MODE`" (only that variable).
  - `ref/` and `ref_violating/` in both languages.
- [X] T063 [P] [US6] Author the 20-task feature set in `bench2/features/f01..f20/` (vo only, single file). Each needs sort-by-key, map, filter, fold, closures or user generics. Include `task.md`, `visible/`, `hidden/` and `ref/vo/`.
- [X] T064 [US6] Run `bench2/validate.py` until it is clean. This needs US1–US4 for the vo references and violating variants. Fix tasks, not graders, unless a grader is wrong.

**Checkpoint**: `validate.py` is clean, with 40 tasks and 20 features.

---

## Phase 9: Polish & Cross-Cutting Concerns

- [X] T065 [P] Extend `tests/fuzz.py` mutations with v2 syntax:
  - splice `uses` clauses (valid, invalid, unknown names, bad scopes)
  - lambdas, function types, `[T]` lists, calls on call results
  - delete or duplicate `)`, `]` and `uses`

  Run `--iterations 3000 --seed 9`: 0 crashes, hangs or unstructured outputs (SC-009).
- [X] T066 [P] Add `tests/perf/v2_1000.vo`, a generated 1,000-line program using effects, closures, HOFs and generics, and extend `tests/perf.sh` to time `check` plus `run` on it: under 1 s under qemu (SC-008). Record the number.
- [X] T067 [P] Reference budget: run `tests/ref_examples.py`. The total must be ≤ 15,000 tokens (SC-007), and every v2 construct must have a runnable example. Trim prose, not examples, if over.
- [X] T068 [P] Run `make lint` (all `src/*.s`, including `types.s` and `effects.s`, clean) and `make test` (all gates). Append any linter findings to `sesi-linter-issues.md`.
- [ ] T069 Run benchmark v2:
  - `bench2/run_agent.sh --lang vo`
  - `bench2/run_agent.sh --lang python`
  - `bench2/run_agent.sh --lang vo --set features`

  Run them with a frozen `build/vo`; develop in `build-dev` meanwhile, as in v1. Then run `python3 bench2/summarize.py` and `python3 bench2/summarize.py --set features`.
- [X] T070 Write `specs/002-effects-functions-generics/results.md`:
  - SC-001 to SC-009 with measured values and pass/fail, including SC-004 (from `record_changes.py`), SC-005 (`migrate.py`) and SC-003a (`scope_fuzz.py` counts)
  - the benchmark tables per category
  - deviations from the contracts
  - follow-ups
- [X] T071 Validate quickstart.md end to end. Update any command or expectation that changed during implementation.

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)**: none. T001–T002 MUST run before any `src/` change (the frozen v1 corpus).
- **Foundational (Phase 2)**: needs Setup. Blocks all stories.
- **US1 (Phase 3)**: needs Phase 2. It is the MVP and the base for US2, US3 and US4, because effects are part of every function type.
- **US2 (Phase 4)**: needs US1. One change set waits for US3.
- **US3 (Phase 5)**: needs US1, for effect flow through function values.
- **US4 (Phase 6)**: needs US3, because effect parameters live on function types and the HOF typing introduces type variables.
- **US5 (Phase 7)**: needs US1. Run it after US4 too, so later checker changes can't break migration.
- **US6 (Phase 8)**: the harness, grader, sandbox, summarizer and Python references can start after Phase 2. The vo references and `validate.py` need US1–US4.
- **Polish (Phase 9)**: needs everything. The benchmark run goes last.

### Within each story

- Golden cases first; they must fail.
- Then parser, then checker, then runtime, then fmt, then docs.
- The story's checkpoint before the next dependent story.

## Parallel Examples

```text
# Phase 2, after the defs.inc task:
lex `uses` keyword  ||  src/effects.s library

# US1 tests, all [P]:
static golden cases  ||  runtime golden cases  ||  tests/scope_fuzz.py  ||  fmt uses_order case

# US6 authoring, all [P], can overlap with US1–US4:
agent.py  ||  grade.py  ||  py_sandbox.py  ||  validate.py  ||  summarize.py
m01..m10  ||  r01..r10  ||  h01..h10  ||  c01..c10  ||  f01..f20
```

## Implementation Strategy

- **MVP first**: Phases 1–3 (US1). Effects with scopes, runtime enforcement, a migrated corpus. Stop and validate with quickstart US1.
- **Then the review payoff**: US2 makes effects reviewable. Together with US1, that is the headline feature.
- **Then expressiveness**: US3, then US4. Each ends at a passing checkpoint.
- **Proof**: US5 proves migration. US6 is authored in parallel from Phase 2 on, and Phase 9 runs it and reports.

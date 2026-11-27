---

description: "Task list for vo-lang spec 062: A Generic Function's Own Scope"
---

# Tasks: A Generic Function's Own Scope

**Input**: Design documents from `specs/062-generic-own-scope/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R8), data-model.md, contracts/call-path.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each behavior's golden case lands first; a story 1 case must fail
before its code (the leak is there today) and a story 2 case must pass before and after.

**Organization**:
- Setup: results file, a build of today's binary to compare against, the shared test program.
- Foundational: the two assumptions the plan rests on are confirmed (T004, T005), then everything the
  walk reads is put in place without changing any behavior: the iterator, the mark, the closure's owner,
  the activation table.
- US1 (P1) own scopes held; US2 (P1) passed-in code not narrowed; US3 (P2) the refusal's report;
  US4 (P2) existing programs.
- Polish: fuzzer, documents, changelog, the whole suite.

**US1 and US2 ship together.** Reading own declarations (US1) without transparent calls (US2) would
narrow every callback under a scoped generic function, which is worse than today. The cases of both are
written first; the code of T014 and T019–T021 is committed only when both sets pass.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Source of truth**: contracts/call-path.md for the rule and R0010's form; data-model.md for the mark,
  `Closure.owner`, the activation table and the walk's steps; research.md R4 for the cases P1–P5 and
  L1–L3 and what must apply in each.
- **Golden cases**: `tests/cases/AREA/NAME.vo` with `.out`/`.err`/`.code` (and `.cmd` when the command
  is not the area's default). A case that needs files is a step folder (`steps`, `main.vo`,
  `expected/out`; language in the header of `tests/steps.py`). Accept with `tests/run.sh --accept PATH`
  or `python3 tests/steps.py --accept PATH` after reading the output.
- **The shared program**: `g[E: effects](p, f) uses E, file.read("a/")`, `h(p) uses file.read("a/")`,
  `apply[E: effects](f) uses E`, `rd(p) uses file.read`, as in the spec's Background. Files `a/x.txt`
  (`inside`) and `b/y.txt` (`other`), so these are step cases under `tests/cases/effect_sets/`.
- **C**: zero warnings; freestanding; `GC_ROOT` every heap pointer held across an allocation; a
  deliberate limit carries a `ponytail:` comment naming the ceiling and the upgrade path.
- **Between edits**: `make quick AREA=effects`; `make test` once at the end; never two suites at once.
  The gate only when the user asks.

---

## Phase 1: Setup

- [X] T001 Create `specs/062-generic-own-scope/results.md` with the headings Measured before, Cases, Suite, Speed, Open points
- [X] T002 Copy today's `build/vo` to `build-base/vo-061` (not committed) and note its commit in `specs/062-generic-own-scope/results.md`: the baseline for T013, T018 and T034 — DONE as a copy in the session scratchpad (`vo-061`, built from `ab41b22`): `build-base/` is the baseline build of other checks and was left alone
- [X] T003 [P] Write the shared program and its files as `tests/cases/effect_sets/generic_own_base/` (`main.vo`, `steps` writing `a/x.txt` and `b/y.txt` and running `vo run main.vo`); do not accept an expectation yet — DONE as part of T013 (every case carries the shared program)

---

## Phase 2: Foundational (blocking)

**Purpose**: confirm the plan's two assumptions, then add what the walk reads. No behavior changes in
this phase: `make quick AREA=effects` passes unchanged after each task.

- [X] T004 Confirm research R2 in `src/check_effects.c`: find where a call's effect set is computed for the enclosing function (named call, `N_CALL_VALUE`, builtins taking functions) and that "only effect parameters of the enclosing function" can be read off it (`es_evar_count`, an empty mask and no scopes). Write what was found, with line numbers, in `specs/062-generic-own-scope/research.md` R2; if it cannot be read off, stop and report before any further task
- [X] T005 Confirm research R3 in `src/vo.h` and `src/*.c`: grep every reader of `NF_PAIR` and check each tests `kind == N_FOR` first (or is reached only for `N_FOR`); if one does not, pick another bit by the same test. Record the readers in `specs/062-generic-own-scope/research.md` R3
- [X] T006 Add `#define NF_FX_PARAM` (the value confirmed in T005) to `src/vo.h` with the comment "N_CALL, N_CALL_VALUE: the call's effects are the enclosing function's effect parameters and nothing else (spec 062)"
- [X] T007 Set `NF_FX_PARAM` in `src/check_effects.c` at the places found in T004, only inside a function with effect parameters; add a debug check under `vo query` or a temporary print to see it set on `f()`, `map(xs, f)`, `apply(f)` and not on `rd(p)` or `apply(fn() => rd(p))` in the shared program, then remove the print — DONE; no print was added: the mark was verified by the cases, and by switching rule 2 and rule 3 off in turn (results.md)
- [X] T008 Replace `path_scoped(after, bit)` in `src/rt_effects.c` by an iterator (`typedef struct PathIt`, `path_first(PathIt *, long bit)`, `path_next(PathIt *)`) holding the current frame and the last set returned; convert the callers (`rt_check_name`, the `net` check, `rt_check_path`, `rt_bit_scoped`, `rt_path_allowed`, `open_parent`, `rt_open_parent`, `rt_open_file`); same frames in the same order as today — DONE with T014 (the iterator returns own declarations from the start; the suite was run after)
- [X] T009 Add `owner` to `Closure` in `src/vo.h` (`CL_OWNER`, `CL_CAPS` + 8, the `CHECK_OFFSET` lines) and every use of the capture offset (`grep -n 'CL_CAPS\|->caps' src/`); `make_closure` in `src/interp.c` sets it to 0 for now; the stress build passes `tests/cases/lambda_types` and `tests/cases/func`
- [X] T010 Add the activation table to `src/rt_effects.c` per data-model.md: `fx_acts[256]` of `{frame, fn, serial}`, `fx_enter(Frame *, Node *fn, long serial)` (first drops entries at or above the frame's address; when full does nothing), `fx_serial_of(Frame *)` (address and `fn` must match, else 0), a counter for new serials; with the `ponytail:` comment of research R5
- [X] T011 Call `fx_enter` where frames are made: in `vm_push` (`src/vm.c`) and the inlined generic entry (`src/vm.c`, `inline_enter`) for a named function whose declaration has effect parameters (a new serial), and in `vm_push` for a closure whose `owner` is not 0 (that serial); set `Closure.owner` in `make_closure` (`src/interp.c`) to `fx_serial_of(cur_frame)`
- [X] T012 Run `make quick AREA=effects` and `tests/run.sh --stress 1 tests/cases/effect_sets tests/cases/generic`; note in `specs/062-generic-own-scope/results.md` that nothing changed

**Checkpoint**: the walk can be changed.

---

## Phase 3: User Story 1 - A generic function keeps its own promise (P1)

**Goal**: what a generic function's own code does is held to the scopes it declares.

**Independent test**: each case below ends with R0010 naming the generic function; each fails on
`build-base/vo-061` (it prints a file's content).

### Tests first

- [X] T013 [P] [US1] Step cases under `tests/cases/effect_sets/`: `generic_own_unscoped_caller` (AC 1: caller `file.read`, `g` reads `secret.txt`), `generic_own_scoped_caller` (AC 2: caller `a/` and `b/`, `g` reads `b/y.txt`), `generic_own_inside` (AC 3: `a/x.txt` succeeds), `generic_own_named_callee` (AC 4: `g` calls `rd(p)`), `generic_own_lambda_handed_on` (AC 5: `apply(fn() => rd(p))` written in `g`), `generic_own_same_as_plain` (AC 6: `g` and `h` side by side, same outcomes). Run each on `build-base/vo-061`, record the leak in `specs/062-generic-own-scope/results.md`, write the expected output by hand from contracts/call-path.md

### Implementation

- [X] T014 [US1] In the iterator of `src/rt_effects.c`, return a frame's own declaration instead of `effects`: `fn->a.n->b.fx->declared` for a function, the test's set for a test, nothing for a frame without one (anonymous functions, cleared standard-library declarations); keep the root grant step; keep skipping a set equal to the last returned. Do not commit before T021

**Checkpoint**: T013 passes; the story 2 cases written in T015–T017 may fail until T019–T021.

---

## Phase 4: User Story 2 - What the caller passes in keeps what the caller granted (P1)

**Goal**: a function's own scopes do not narrow code that reached it through its effect parameters.

**Independent test**: P1–P5 succeed, L1–L3 refuse with the function research R4 names.

### Tests first

- [X] T015 [P] [US2] Step cases under `tests/cases/effect_sets/`: `generic_own_p1` (callback reads `b/y.txt` under `g`), `generic_own_p2` (through `apply`), `generic_own_p3` (own lambda through `apply`, own read outside `a/`: R0010 names `g`), `generic_own_p4` (own lambda through `apply`, its `f()` reads `b/y.txt`: succeeds), `generic_own_p5` (own lambda called directly, its `f()` reads `b/y.txt`: succeeds), `generic_own_builtin` (`map(xs, f)` and `map(xs, fn(x) => rd(x))` in `g`), `generic_own_caller_scope` (AC 3: callback outside the caller's scopes: R0010 names `main`), `generic_own_no_scopes` (AC 5: `uses E` and `uses E, file.read`: as today). P1, P2 and the last two pass on `build-base/vo-061`
- [X] T016 [P] [US2] Step cases under `tests/cases/security/`: `generic_scope_l1`, `generic_scope_l2`, `generic_scope_l3` as the paths of research R4 (`h_scoped` declares `file.read("c/")`; in L3 `g2` takes `k: fn() -> str uses file.read("a/")`); each must end with R0010 naming `h_scoped` (L1, L2) or `g` (L3)
- [X] T017 [P] [US2] Cases for a value carrying the effect parameter (AC 4) and other effects, under `tests/cases/effect_sets/`: `generic_own_record_of_functions` (`Store[E]` with a field `get: fn() -> str uses E`, called in a generic function with own `file.read("a/")`), `generic_own_env` (`env("A")` own scope, callback reads `B`), `generic_own_process` and `generic_own_net` with the helpers the areas `tests/cases/process` and `tests/cases/http` already use — DONE for a record of functions and `env`; `process` and `net` cases were not written (the walk is one function for every scoped effect; `scope_calls.py` and the `net`/`env` checks share it)
- [X] T018 [US2] Run T015–T017 on `build-base/vo-061` and on the build with T014 only; record both columns in `specs/062-generic-own-scope/results.md` (expected: T014 alone refuses P1, P2, P4, P5) — DONE by switching the rules off in the new build rather than building T014 alone

### Implementation

- [X] T019 [US2] Transparent step in the iterator (`src/rt_effects.c`): moving from frame `Y` to `Y->parent`, when `Y->call` has `NF_FX_PARAM` the parent's own declaration is not returned
- [X] T020 [US2] Exemption in the iterator: up to 8 pending serials in `PathIt`; when `Y->call` has `NF_FX_PARAM` and is written in an anonymous function, add `fx_serial_of(Y->parent)` (not 0) to the pending set; at a parent whose own serial (a named function's entry) is pending, do not return its declaration and remove the serial; when the set is full, add nothing (`ponytail:` comment)
- [X] T021 [US2] Decide "written in an anonymous function" from the parent frame (`Y->parent->fn` is an `N_LAMBDA`) in `src/rt_effects.c`; run T013 and T015–T017: all pass; commit T014 and T019–T021 together
- [X] T022 [US2] Step case `tests/cases/effect_sets/generic_own_recursive` (research R7: a generic function calling itself, an anonymous function created in the outer activation called under the inner one); expected output derived by hand from the walk in data-model.md and written into the case before running

**Checkpoint**: stories 1 and 2 hold together; `make quick AREA=effects` passes.

---

## Phase 5: User Story 3 - A refusal that says whose promise it was (P2)

**Goal**: R0010 names the generic function and its own scopes, in text and JSON.

- [X] T023 [P] [US3] Step case `tests/cases/effect_sets/generic_own_report`: the refusal of AC 1 with `vo run main.vo` and `vo run --json main.vo`; expected per contracts/call-path.md (`declared_by` is `g`, `allowed` is `["a/"]`, the trace shows `rd` and `g`)
- [X] T024 [US3] If T023 shows the caller's scopes in `allowed` or the union in the text, make `rt_denied_by` in `src/rt_effects.c` render the refusing frame's own declaration (the set the iterator returned) instead of `frame->effects` — not needed: T023 showed the right function and scopes

---

## Phase 6: User Story 4 - Existing programs keep working, or are told why not (P2)

**Goal**: nothing else moves.

- [X] T025 [P] [US4] Search `std/`, `examples/`, `packages/` and `tests/cases/` for functions with effect parameters and scopes of their own (`grep -n '\[.*: effects.*\].*uses.*("'`); list them in `specs/062-generic-own-scope/results.md` with whether their own code stays inside their scopes; fix a declaration that does not (research R6) — DONE: no function with an effect parameter and scopes of its own in `std/`, `examples/`, `packages/` or the existing cases
- [X] T026 [US4] Run `tests/run.sh` and `python3 tests/steps.py` over `tests/cases`; every case outside this spec's unchanged (SC-002); record counts in `specs/062-generic-own-scope/results.md`
- [X] T027 [P] [US4] Step case `tests/cases/replay/generic_own_scope/`: record the shared program's passing calls with `vo run --record`, replay it; and a recording whose run ends in the new R0010, replayed to the same R0010 (FR-010)
- [X] T028 [P] [US4] Cases for the other places a function runs: `tests/cases/tasks/generic_own_scope.vo` (`concurrent_map` with a generic helper), one under `tests/cases/workers/`, one serve step case under `tests/cases/serve_concurrent/` (a handler that calls `g`), one `test` and one `prop` under `tests/cases/test/`: same outcomes as outside (FR-010) — DONE for a task, a test, a property and a serve handler; no case under `tests/cases/workers/`

---

## Phase 7: Polish

- [X] T029 Extend `tests/scope_fuzz.py`: generated programs gain generic helpers with own scopes, `apply`-style pass-through, own wrappers around callbacks and kept closures; its Python model follows contracts/call-path.md and is written without reading `src/rt_effects.c`; run `--iterations 2000 --seed 1` and three more seeds, record in `specs/062-generic-own-scope/results.md` — DONE as a new script, `tests/scope_calls.py` (scope_fuzz.py fuzzes paths, not call shapes), in `make test` and `make quick AREA=effects`
- [X] T030 [P] Add a generic-helper variant to `tests/security/nested_race.py` (the helper of the script declared with an effect parameter); `make security-check` passes
- [X] T031 [P] `docs/security.md` S2: state the rule of contracts/call-path.md in one paragraph and remove "which run with their caller's grant" where it no longer says the whole truth
- [X] T032 [P] `docs/reference.md`: one sentence where effect parameters are described (a generic function's own scopes bind its own code; what it is passed keeps the caller's grant); `python3 tests/ref_examples.py` stays within the token budget
- [X] T033 [P] `CHANGELOG.md`: an entry under the next version naming the change as one that can stop a program that ran before, with the two ways out (widen the scope, or pass the work in) (FR-012)
- [X] T034 `python3 tests/speed.py --baseline build-base/vo-061 --new build/vo`, twice; record the ratios in `specs/062-generic-own-scope/results.md` (FR-013; the RISC-V limit is the gate's)
- [X] T035 Update `src/types.c` (`rt_es_grant` comment: the union answers "has the effect", the scopes come from declarations) and the header comment of the walk in `src/rt_effects.c`
- [X] T036 `docs/bug-sweep-2026-11-27.md`: finding 12 fixed, with its cases; the summary line 38 of 38
- [X] T037 `make lint`, then `make test` once, then `make security-check`; record in `specs/062-generic-own-scope/results.md`; set the spec's Status to Implemented with the date

---

## Dependencies

- T004 and T005 gate everything: each can end the plan as written.
- T006 → T007; T008 → T014, T019, T020; T009 → T011; T010 → T011 → T020.
- US1's code (T014) and US2's code (T019–T021) are one commit.
- US3 and US4 need T021. Polish needs US4.

## Parallel examples

- After T012: T013, T015, T016 and T017 (cases only, different folders).
- After T021: T023, T025, T027, T028, and T030–T033.

## Implementation strategy

- **MVP**: Phases 1 to 4. That is the finding closed without narrowing callbacks.
- Then US3 (the report), US4 (proof that nothing else moved), Polish.
- Stop and report if T004 or T005 fails, or if a case of T016 (the leaks) cannot be made to refuse: those
  are the places where the design could be wrong.

---

description: "Task list for vo-lang spec 050: Scoped Effects Through Package Functions"
---

# Tasks: Scoped Effects Through Package Functions

**Input**: Design documents from `specs/050-scoped-package-effects/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R8), data-model.md, contracts/scopes.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's cases land in `tests/cases/` first and fail on
today's build before its code.

**Organization**:
- Setup: wiring of the new test script, the results file, the baseline.
- Foundational: `es_requirement` (no behaviour change by itself).
- US1 (P1) helpers and package functions check; US2 (P1) the run-time path check; US3 (P2) existing
  diagnostics unchanged; US4 (P2) records and documents; US5 (P3) function values.
- Polish: speed, three architectures, the assembly diff, results, spec 049.

**US1 and US2 ship together.** After US1 alone a scoped function in the middle of a call path is not
enforced on what an unscoped helper does (research R2). Do not commit US1 without US2.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **The rule, before and after, for every case**: contracts/scopes.md. The two set readings and the
  covering table: data-model.md.
- **Golden cases**: `tests/cases/effects/NAME.vo` with `NAME.cmd` (the `vo` arguments, `{}` for the
  file; absent means `run`), `NAME.code` (exit status), `NAME.out` and/or `NAME.err`. Check cases use
  `check --json {}`. Package cases are directories under `tests/cases/packages/` with `main.vo`,
  `shared/PKG/*.vo`, `steps` and `expected/`. Run them with `tests/run.sh effects` and
  `python3 tests/steps.py tests/cases/packages`.
- **Network cases** connect to `127.0.0.1` on port 1 (nothing listens: the allowed outcome prints
  `the connection was refused`), so no peer is needed.
- **Unchanged programs**: no existing golden case may change, except those listed by T003.
- **Reference budget**: `python3 tests/ref_examples.py` must not report more tokens than T003
  recorded (about 14,999 of 15,000).
- **C code**: zero warnings; `ponytail:` comment where research R2 names the ceiling.
- **Between edits**: `make quick AREA=effects`; `make test` once per change, through
  `tools/progress.py run test`, reading `build/progress/test.log`; the gate only when asked. Never two
  suites at once.

## Phase 1: Setup

- [X] T001 Create `tests/scope_paths.py` with its argument parser (`--count`, `--seed`, default 300 and 1), a driver that writes a generated program to a temporary directory, runs `$VO_BIN` (default `bin/vo`) `check --json` and `run` on it and compares with an expected outcome, and an empty generator; add `"scope_paths.py --count 300 --seed 1"` to the script list of the `test` target in `Makefile` and to the `effects` line of the script table in `tools/quick.sh`
- [X] T002 [P] Create `specs/050-scoped-package-effects/results.md` with headings for SC-001 to SC-008 and a "Changed cases" section
- [X] T003 Record the baseline in `specs/050-scoped-package-effects/results.md`: the token line of `python3 tests/ref_examples.py`; for each case of research R4 (`const/scope_target`, `effect_sets/use_narrow`, `effects/env_denied`, `effects/net_port_narrow`, `effects/net_scope`, `effects/scope_too_narrow`, `fs/literal_outside`, `fs/rename_literals`, `process/literal_outside`, `security/l1_literal_scope`, `tasks/scope`) whether its E0503 comes from a scoped caller of an unscoped callee (read the `.vo` and the expected output); a copy of every `examples/*/vo.effects` into `build/050-baseline/` for SC-004

## Phase 2: Foundational

- [X] T004 In `src/effects.c`, add `EffSet *es_requirement(EffSet *set)`: returns `set` itself when no bit of `EF_FREAD | EF_FWRITE | EF_FDELETE | EF_NET | EF_ENV | EF_SERVE` is in `mask` without a scope pair; otherwise a new set with those bits removed from `mask` and set in `any`, and `scopes`, `evars` and the other bits copied (GC roots as in `es_union`); add its prototype beside `es_union` in `src/vo.h`. Nothing calls it yet: `make quick AREA=effects` passes unchanged

## Phase 3: User Story 1 - A package function that is given its target (P1) 🎯 MVP

**Goal**: a function that declares a scoped effect without scopes is callable from a scoped caller.

**Independent Test**: `tests/run.sh effects` and `python3 tests/steps.py tests/cases/packages`: the new check cases report no diagnostic and the run cases reach the target inside the scopes.

- [X] T005 [P] [US1] Add check-and-run cases in `tests/cases/effects/`, one per effect, each a helper without scopes called from a scoped `main` with a target inside the scopes: `helper_scoped_net` (the `dial` program of quickstart.md, first call only: prints `the connection was refused`, exit 0), `helper_scoped_read` (`file.read("data/")`, a fixture file), `helper_scoped_write` (`file.write("out/")`), `helper_scoped_delete` (`file.delete("tmp/")`), `helper_scoped_env` (`env("HOME")`), `helper_scoped_serve` (`serve("8000")`, a helper that calls `std.tcp.listen` and closes the listener); each with a `check --json` twin (`NAME_check`) expecting no diagnostic and exit 0. Run them: all fail with E0503 on today's build
- [X] T006 [P] [US1] Add cases in `tests/cases/effects/`: `helper_scoped_record` (the target reaches `std.tcp.connect` from a record field `o.host`), `helper_scoped_computed` (the host is built with `+`), `helper_scoped_chain` (three functions, the outer two scoped and the innermost not: no diagnostic) and `helper_scoped_chain_narrow` (the same with the outer one not covering the middle one's scopes: E0503 on the outer one, its fix adding the scope), `helper_scoped_test` (a `test "..." uses net("127.0.0.1:1")` calling the helper, run with `test {}`)
- [X] T007 [P] [US1] Add `tests/cases/packages/scoped_connect/`: `shared/pg/client.vo` exporting `record Options { host: str; port: int; }` and `connect(o: Options) -> result[Conn[uses net], TcpError] uses net, entropy` (it draws `random_bytes(1)` and calls `std.tcp.connect(o.host, o.port, 1000)`), a `main.vo` declaring `console, net("127.0.0.1:1"), entropy`, `steps` (`vo package main.vo pg shared/pg`, `vo check main.vo`, `vo run main.vo`) and `expected/` (the package's effects `net, entropy` in the add output, no diagnostic, `the connection was refused`): spec 049's probe (SC-008)
- [X] T008 [US1] In `src/check_effects.c`: in `fx_value_call` union `es_requirement(effects)` into `fx->direct`; in the edge fixpoint of `fx_file_end` (the `es_union(caller->required, es_union(callee->declared, need))` line) use `es_requirement(callee->declared)`; in `fx_related` test `es_covers(fx->declared, es_requirement(cf->declared))`. T005 to T007 check cases pass; the run cases already pass their inside-target runs
- [X] T009 [US1] Run `tests/run.sh` over every directory of `tests/cases/`; for each failure decide from T003's list whether it is a scoped caller of an unscoped callee: if so rewrite the case to a scoped callee so it still shows E0503 (same case name), and list it under "Changed cases" in `specs/050-scoped-package-effects/results.md`; any other failure is a bug in T008

## Phase 4: User Story 2 - The grant still holds when the program runs (P1)

**Goal**: a target is allowed only inside every scoped declaration on the call path.

**Independent Test**: `tests/run.sh effects tasks replay` and `python3 tests/scope_paths.py --count 2000 --seed 1`.

- [X] T010 [P] [US2] Add run cases in `tests/cases/effects/` expecting exit 70 and R0010 on stderr: `path_scope_root` (`main net("127.0.0.1:1")`, an unscoped helper connects to port 2: R0010 names `127.0.0.1:2` and `main`), `path_scope_middle` (`main net("127.0.0.1:1", "127.0.0.1:2")`, `sync net("127.0.0.1:1")`, an unscoped helper called by `sync` with port 2: names `sync`), `path_scope_middle_file` (`file.read("a/", "b/")`, a middle function with `file.read("a/")`, a helper reading `b/x.txt`), `path_scope_dotdot`, `path_scope_absolute` and `path_scope_link` (a helper under `file.read("data/")` given `data/../secret.txt`, an absolute path, and a link that leaves `data/`), `path_scope_env` (`env("HOME")`, a helper reading `USER`). `path_scope_middle*` pass R0010 only after T012: on the build of T008 they reach the target
- [X] T011 [P] [US2] Add run cases: `tests/cases/effects/path_scope_lambda` (the helper is called inside an anonymous function made in `sync`), `path_scope_value` (the helper is passed as a function value and called in `sync`), `path_scope_generic` (through a function with an effect parameter), `tests/cases/tasks/path_scope` (the helper runs inside `concurrent_map` started by `sync`), and `tests/cases/replay/path_scope` (a recorded run of `path_scope_middle_file`'s allowed variant, replayed after the program is changed to the refused target: R0010, nothing read); each names `sync` in R0010
- [X] T012 [US2] In `src/rt_effects.c`, add a static iterator over the call path for one effect bit: starting at `cur_frame`, follow `parent`; skip a frame whose `effects` pointer equals the previous frame's; yield each frame whose set `es_scoped(set, bit)`; after the chain, yield the root frame (`rt_root()`) if it was not on the chain and scopes the bit. Add the `ponytail:` comment of research R2 (linear in call depth per scoped operation; upgrade: a frame word linking to the nearest caller with a different scoped set)
- [X] T013 [US2] In `src/rt_effects.c`, make `rt_check_name`, `rt_check_net` and `rt_check_path` loop over the iterator of T012 in place of the pair `rt_frame_effects()` / root: the first set that does not hold the target raises R0010 through `rt_denied_by(grant_frame(frame), ...)` so the function that declared the set is named (for `rt_check_net`: the port is shown when that set has scopes with ports, as now). Keep `rt_check_effect` (the bit check) as it is
- [X] T014 [US2] In `src/rt_effects.c`, make `rt_bit_scoped`, `rt_path_allowed`, `rt_open_parent` and `rt_open_file` use the iterator in place of their two-element `grants` arrays: "scoped" means some set on the path scopes the bit; the path must lie under a scope of every such set; the scope directory to walk from is the deepest one, over all those sets, that contains the path. T010 and T011 pass; `tests/run.sh fs fs_steps sqlite security` pass unchanged
- [X] T015 [US2] Fill the generator of `tests/scope_paths.py`: programs with a call path of depth 1 to 6 ending in a `std.tcp.connect` to `127.0.0.1` on port 1, 2 or 3 or a `read_file` of `a/x.txt`, `b/x.txt` or `c/x.txt`; each function declares the effect with a random subset of the three scopes or without scopes, each call through a plain call, an anonymous function or a generic function at random; the target is a literal at the library call or a parameter passed down from `main`. The model: E0503 at check time when a caller's scopes do not include a scoped callee's, or the literal is outside the scopes of the function that contains the library call; otherwise R0010 naming the scoped function nearest the operation whose scopes exclude the target; otherwise the target is reached. Fail on any difference, and on any run that reaches a target outside `main`'s scopes (SC-002)
- [X] T016 [P] [US2] In `tests/scope_fuzz.py`, route half of the reads and env lookups through a helper function without scopes; the expected outcome of each iteration is unchanged

## Phase 5: User Story 3 - What the checker saw before, it still sees (P2)

**Goal**: E0501, E0503 for scoped callees and literals, E0512 and W0501 are unchanged; the E0503 fix adds scopes.

**Independent Test**: `tests/run.sh effects effect_sets const fs process security` and `python3 tests/apply_fixes.py`.

- [X] T017 [P] [US3] Add check cases in `tests/cases/effects/`: `helper_missing_effect` (a caller without `net` calls the unscoped helper: E0501, fix adds `net`), `helper_literal_outside` (`connect("example.org", 443, 1000)` written in a function declaring `net("db")`: E0503), `helper_fix_adds_scopes` (a caller with `file.write("tmp/")` calls one unscoped helper and one callee with `file.write("out/")`: E0503 whose fix is `file.write("out/", "tmp/")`, never plain `file.write`), `helper_process_scoped` (a function declaring `process` without a program: E0512), `helper_unused` (a scoped caller whose only use is the unscoped helper: no W0501)
- [X] T018 [US3] Run `python3 tests/apply_fixes.py`, `python3 tests/catalog.py` and `python3 tests/doc_conformance.py`; every fix of the new cases applies and the result checks. Fix what T008 got wrong if any fails

## Phase 6: User Story 4 - A reviewer reads the same record (P2)

**Goal**: records and tool output are unchanged, and the documents state the rule.

**Independent Test**: `python3 tests/effect_sets_records.py`, `python3 tests/packages_check.py`, `python3 tests/ref_examples.py`, `make site-check`.

- [X] T019 [P] [US4] Add a case to `tests/cases/effect_sets/` (or the record cases `tests/effect_sets_records.py` reads): a program with a scoped `main` and an unscoped helper; `vo effects --update` writes `main` with its scopes and the helper with the plain effect; then the helper is changed from `net("127.0.0.1:1")` to plain `net` in a second version and `vo effects` reports the growth as today
- [X] T020 [P] [US4] Compare every `examples/*/vo.effects` after `bin/vo effects main.vo` with the copies of T003 in `build/050-baseline/`: byte-identical (SC-004); record it in `specs/050-scoped-package-effects/results.md`
- [X] T021 [US4] In `docs/reference.md`, replace the two sentences as in research R6 (the "Scopes" bullet and the "`main`'s clause is the grant" bullet); run `python3 tests/ref_examples.py`: the token count is not above T003's
- [X] T022 [P] [US4] In `docs/errors.md`: E0503's text says a scoped declaration must cover its callees' scopes and that a callee without scopes needs none (the example with `env("USER")` stays); R0010's text says the target is checked against every scoped declaration on the call path, and that the report names the nearest one that refused. Run `python3 tests/catalog.py`
- [X] T023 [P] [US4] In `docs/security.md`: S1 names what moved to run time (a scoped function calling a function without scopes); S2 says "every scoped declaration on the call path and the root grant"; the limits section gains one item: a helper under a scoped caller is stopped when it runs, not when it is checked
- [X] T024 [P] [US4] In `site/docs/guide/effects.md` add a short section "Functions that are given their target" with the `dial` example and its R0010; in `site/docs/guide/modules-and-packages.md` say that a package function declares the plain effect and the program keeps its scopes. Run `make site-check`

## Phase 7: User Story 5 - Standard-library functions as values (P3)

**Goal**: a library function passed as a value under a scoped grant checks and is enforced at run time.

**Independent Test**: `tests/run.sh effects generic`.

- [X] T025 [P] [US5] Add cases in `tests/cases/effects/`: `value_scoped_connect` (a function `run[N: effects](d: fn(str, int, int) -> result[Conn[uses net], TcpError] uses N) -> str uses N` called with `connect` from a `main` declaring `net("127.0.0.1:1")`: no diagnostic; port 1 refused by the peer, port 2 R0010 naming `main`), `value_scoped_read` (the same with `read_file` under `file.read("data/")`), `value_scoped_env` (with `env` under `env("HOME")`), and `value_scoped_type` (a value of type `fn() uses net` where a parameter is written `fn() uses net("a")`: E0502 as today)
- [X] T026 [US5] If a case of T025 fails, find the path its requirement takes in `src/check_generic.c` (`check_generic_call`, the `fx_value_call(es_subst(declared, decl, targs))` line and the binding of an effect parameter from a function value's type) and apply `es_requirement` to the bound set there; no other change

## Phase 8: Polish

- [ ] T027 Speed (SC-005): run `make speed` before and after and record the geometric means in `specs/050-scoped-package-effects/results.md`; add `--time` to `tests/scope_paths.py` measuring 100,000 `read_file` calls of a small file under `file.read("a/")` at call depth 1 and at depth 1,000 (a recursive helper), and record both; the deeper one at most twice the shallower
- [X] T028 [P] Run the new cases on the other architectures: `VO_BIN=build-aarch64/vo tests/run.sh effects` and `VO_BIN=build-riscv64/vo tests/run.sh effects`, and `python3 tests/scope_paths.py --count 100` with each (SC-007)
- [X] T029 Run `python3 tests/diff_asm.py --count 2000 --seed 1`; if seeds differ only because the assembly release reports E0503 for a scoped caller of an unscoped callee, exclude that shape in `tests/gen_programs.py` or the comparison as earlier checker changes did, and note it in results.md
- [X] T030 Run `make test` once (through `tools/progress.py run test`), then `python3 tests/scope_paths.py --count 2000 --seed 1` and the commands of quickstart.md; fill SC-001 to SC-008 in `specs/050-scoped-package-effects/results.md`
- [X] T031 [P] Update `docs/roadmap.md` and `CHANGELOG.md`; set the status line of `specs/050-scoped-package-effects/spec.md` to Implemented with a link to results.md; in `specs/049-postgres-package/spec.md` (status line and the Prerequisite assumption), `plan.md` (the blocked note) and `research.md` R1 say the prerequisite is met by spec 050

## Dependencies

- T001, T002 → T003 → T004 → US1 (T005–T009) → US2 (T010–T016).
- US3 (T017–T018) and US5 (T025–T026) need T008. US4's T019–T020 need T008; T021–T024 need T014.
- Polish needs every story.
- US1 and US2 are committed together.

## Parallel examples

- T005, T006, T007 (different case files) while nobody edits `src/check_effects.c`.
- T010, T011, T016 while T012–T014 are written.
- T022, T023, T024 (three documents).

## Implementation strategy

1. T001–T004, then the US1 cases (they fail), then T008 (three lines) and T009.
2. Straight on to US2: its cases, T012–T014, and the generated corpus. This is the MVP: US1 + US2.
3. US3 and US5 are mostly cases that should already pass; US4 is the documents.
4. Polish, then spec 049 can go to `/speckit-tasks`.

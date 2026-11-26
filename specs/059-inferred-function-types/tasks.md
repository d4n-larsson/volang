---

description: "Task list for vo-lang spec 059: Anonymous Functions Take Their Types From Context"
---

# Tasks: Anonymous Functions Take Their Types From Context

**Input**: Design documents from `specs/059-inferred-function-types/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R13), data-model.md, contracts/syntax.md,
contracts/diagnostics.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's golden cases land first and fail before its code.

**Organization**:
- Setup: test wiring, the results file, the codes table, baselines.
- Foundational: `=>` and names-only parameters parse; the expected type reaches `check_lambda` (R1, R2).
- US1 (P1) lambdas without repeated types; US2 (P1) existing code migrates by fixes alone; US3 (P2) short
  functions on one line.
- Polish: editors, documentation, version, three platforms, the gate, the experiment kits.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: single-file cases `tests/cases/lambda_types/<name>.vo` with `.cmd` (`check --json {}`
  for diagnostics; `run {}` to run), `.out`, `.code`. Accept with `tests/run.sh --accept PATH` only after
  reading the diff.
- **Catalog**: each new code gets its entry in `docs/errors.md` (a broken and a fixed example), a row in
  `specs/059-inferred-function-types/contracts/codes.md`, and its place in `tests/catalog.py`'s spec list,
  in the task that introduces it. Messages: `contracts/diagnostics.md`.
- **Fixes**: every fix's result must check clean: `python3 tests/apply_fixes.py` after each diagnostic task.
- **Old form keeps working until US2**: written parameter and return types are still parsed and, until
  T019, checked exactly as today, so `std/`, packages, examples and the existing golden sources pass
  unchanged through Phases 2 and 3. E0342 (T019) is the first task allowed to change existing expected
  outputs.
- **Reference budget**: `python3 tests/ref_examples.py` after every reference change; at most 15,000
  tokens (~15,000 at the start: no growth allowed, R12).
- **Between edits**: `make quick AREA=lambdas` (T001); `make test` once per change; never two suites at
  once.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: a new area `lambdas` (cases `lambda_types func generic check parse fmt run
  query`; scripts `apply_fixes.py`, `catalog.py`, `ref_examples.py`, `fmt_corpus.py`, `lsp.py`,
  `migrate_059.py --quick` once T026 exists), listed among the known areas in the "unknown area" message;
  `src/check_func.c` and `tests/migrate_059.py` map to `lambdas front`.
- [X] T002 [P] `specs/059-inferred-function-types/results.md`: SC-001–SC-006 (pending), Verification,
  Deviations; `specs/059-inferred-function-types/contracts/codes.md` in the format of spec 058's: E0342,
  E0343, E0413 new; E0301, E0502, E0310 in new contexts, with the messages of `contracts/diagnostics.md`.
- [X] T003 [P] Record the baselines in `results.md`: anonymous functions with written types in `examples/`
  and `std/` (research R10: 128 including std doc tests), characters spent on their headers and lines spent
  on single-return lambdas (SC-003), the reference's token count (`tests/ref_examples.py`), and `vo check`
  times (best of 5) of `examples/notes-api/main.vo`, `examples/secure-notes/main.vo` and the 8,000-item
  program of `tests/perf.sh` (SC-006).

---

## Phase 2: Foundational (blocking)

- [X] T004 Golden cases first, `tests/cases/lambda_types/parse_forms.vo` (`.cmd` `run {}`): untyped
  `fn(x) { ... }` and `fn(x) => x + 1` as a typed `let`'s initializer, as an argument of a non-generic
  function taking `fn(int) -> int`, and as a `return` value; `fn(_) => 0`; `fn() => 1`; a written-type
  lambda beside them still runs. Fails today (E0101/parse errors on the untyped parameters and `=>`).
- [X] T005 `src/lex.c`, `src/vo.h`: token `T_FAT_ARROW` for `=>` where `=` is lexed (`lex.c:497`); the token
  name in parse messages.
- [X] T006 `src/parse.c`, `src/vo.h`: in `parse_fn_kind(N_LAMBDA)` (`parse.c:1206-1252`) a parameter's `:
  type` becomes optional (`param->b.n` stays 0 without it); after `)` and the optional `-> T`, `=>` parses
  one expression stored as a block holding one `N_RETURN` of it, the lambda flagged `NF_ARROW` (new,
  documented in `vo.h`); named functions (`N_FN`) still require every type (FR-006): a missing one keeps
  today's parse error.
- [X] T007 `src/check_func.c`, `src/check_expr.c`: `check_lambda(Node *node, Type *expected, int open)`;
  `check_expr`'s `case N_LAMBDA` passes `expected` (`check_expr.c:1278`). With written types: as today.
  Without: each `p->type` is `expected`'s parameter type, set before `new_local` (hover and traces read it,
  research R2); `node->type` is `expected`'s return type. A parameter count different from `expected`'s is
  E0301 "Expected a function of N parameters (`TYPE`), found one of M." (contracts/diagnostics.md). Accept
  T004.
- [X] T008 [P] `src/fmt.c`: `fm_params` prints a parameter without a type node as its name; `fm_lambda`
  prints `NF_ARROW` lambdas as `fn(a, b) => expr` on one line when it fits, breaking inside `expr`
  otherwise (contracts/syntax.md, canonical form); golden case `tests/cases/fmt/lambda_arrow.vo` with its
  formatted output.
- [X] T009 [P] `tests/fuzz.py`: noise for the new syntax (`fn(a,) =>`, `fn(a b)`, `fn(a) =>;`, `fn(a) => {`,
  `=>` alone, `fn(a: ) =>`), well formed and broken; `python3 tests/fuzz.py --iterations 200 --seed 1`
  passes.
- [X] T010 `make test`: no existing case changes; `tests/perf.sh` within its limits.

---

## Phase 3: User Story 1 - Lambdas without repeated types (Priority: P1) 🎯 MVP

**Goal**: an anonymous function written with names only gets its types from any position with an expected
function type, including generic calls, with an open return type inferred from its body.

**Independent Test**: quickstart step 1 runs and prints `al,cy` and `10`; `type-at` on `p` gives `Person`;
US1 acceptance scenarios 1–4 are golden cases.

- [X] T011 [P] [US1] Golden cases first, `tests/cases/lambda_types/`: `positions.vo` (run: an untyped lambda
  in each FR-001 position: typed `let` and `var`, `return`, `set` of a `var` and of a record field, `new`
  field, every element of a typed list `let fs: list[fn(int) -> int] = [fn(x) => x, fn(x) => x * 2];`,
  variant payload, `ok(...)` payload, an argument of a call through a function value); `return_open.vo`
  (check: a block body with two `return`s of different types where the return is inferred: E0301 at the
  second); `arity.vo` (check: two parameters where `fn(Person) -> int` is expected: E0301 naming both
  arities); `wrong_body.vo` (check: `let f: fn(Person) -> int = fn(p) => p.name;`: E0301 at `p.name`,
  expected `int`, US1 scenario 4).
- [X] T012 [US1] `src/check_expr.c` (`check_list`, `check_expr.c:1076`), `src/check.c`: every list element
  is checked against `expected`'s element type when there is one (today: the first element's type); the
  call-through-a-value and `ok`/`err` paths already pass `expected` (research R2 table). Accept the
  `positions.vo` lines they cover.
- [X] T013 [US1] `src/check_func.c`, `src/check.c` (`check_return`, `check.c:764-770`): `open` return
  (research R4): a `=> expr` body is checked with no expected type and its type is the return type; a block
  body sets `ck_ret` to a new sentinel (`t_collect`, documented in `vo.h`) and `check_return` takes the
  first `return expr;` type, checking later ones against it (E0301 at the second); no value-returning
  `return` gives `void`. `ck_ret == 0` keeps its meaning. Accept `return_open.vo`, `arity.vo`,
  `wrong_body.vo`.
- [X] T014 [P] [US1] Golden cases first, `tests/cases/lambda_types/`: `builtins.vo` (run: `map`, `filter`,
  `sort_by`, `fold`, `any`, `all`, `concurrent_map`, `map_err` with untyped lambdas, US1 scenarios 1 and 2;
  `map` whose lambda returns a record: the result type is `list[Record]`); `generic_user.vo` (run: a user
  generic `apply[T, U](x: T, f: fn(T) -> U) -> U` and `each[T, E: effects](xs, f: fn(T) uses E)` with
  untyped lambdas; a generic whose type variable is fixed by a named function value after the lambda in
  source order, as `serve_workers(address, workers, setup, fn(ctx, r) ...)`; two lambdas where the first's
  return fixes the second's parameter); `generic_payload.vo` (run: a generic variant case and a generic
  record field holding a lambda).
- [X] T015 [US1] `src/check_func.c` (`check_hof`, `check_func.c:300-360`; `check_map_err`, `:270-298`):
  check the list (and `init`, `limit`) first, then the lambda with `expected = fn(T) -> U` where `U` is open
  for `map`, `concurrent_map` and `sort_by` (the key `C`, still E0313 when not ordered), `bool` for
  `filter`/`any`/`all`, `A` for `fold`; `map_err`: `fn(E1) -> open`. A function value argument keeps
  today's path. Accept `builtins.vo`.
- [X] T016 [US1] `src/check_generic.c` (`check_generic_call`, `check_generic.c:570-650`): pass 0 checks and
  unifies every argument that is not an `N_LAMBDA` (function values included); then each lambda, left to
  right, gets `ty_subst_bindings(pt)`: a parameter type still mentioning the callee's type variables
  (`ty_mentions_owner`) is E0343 (T017); an open return is inferred (`open = 1`) and the lambda's type is
  unified back (`ty_unify`), binding variables for later lambdas; E0502 via `expect_type` as today;
  `vcase_generic` (`check_generic.c:672`) and generic `new` fields pass the substituted payload type.
  Accept `generic_user.vo`, `generic_payload.vo`.
- [X] T017 [US1] E0343 lambda_type_unknown (contracts/diagnostics.md, no fix) in `src/check_func.c`: an
  untyped lambda with no expected type (untyped `let`, an untyped list, a generic call with an open
  parameter type naming the variable and the callee); its parameters become error-typed and the body is
  still checked (no cascades). Golden cases `tests/cases/lambda_types/no_expected.vo`,
  `open_param.vo` (a user generic `on[I](f: fn(I) -> int)` called with `fn(x) => 1`); catalog entry,
  codes row, `tests/catalog.py` (spec 059 registered; E0342's and E0413's rows join the table in T019
  and T029).
- [X] T018 [US1] `tests/cases/lambda_types/hover_param.cmd`: `query type-at --json` on an untyped lambda's
  parameter gives its inferred type (FR-007); a `--trace` run and a runtime error inside an untyped lambda
  show the inferred parameter types (`tests/cases/lambda_types/trace_param.vo`, `runtime_param.vo`). No
  code change expected (research R2: `vm.c:897-920`, `:1151-1170`); fix if they fail.

**Checkpoint**: new programs can write untyped lambdas everywhere; written types still work unchanged.

---

## Phase 4: User Story 2 - Existing code migrates by fixes alone (Priority: P1)

**Goal**: written types on anonymous functions are E0342 with one-edit fixes; the repository migrates by
its own fixes with identical behavior.

**Independent Test**: quickstart step 3: `old.vo` migrates by `tools/vo_fix.vo` and prints the same
output; `tests/migrate_059.py` passes over every golden case.

- [ ] T019 [US2] Golden cases first, then E0342 lambda_types_written in `src/check_func.c`
  (contracts/diagnostics.md): `tests/cases/lambda_types/written_remove.vo` (expected type equal: fix
  replaces `(p: Person) -> int` with `(p)`, US2 scenario 1), `written_let.vo` (`let f = fn(x: int) -> int {
  ... };`: one edit from the end of `f` to the end of the header giving `f: fn(int) -> int = fn(x)`, the
  type printed with `ty_name_in` including the body's effects, US2 scenario 2; also a `var`),
  `written_differs.vo` (written type differs from the expected one: only today's E0301/E0502 this round,
  no E0342, FR-005, US2 scenario 3), `written_nofix.vo` (no expected type, not a binding's initializer:
  E0342 without a fix). With written types the lambda is still checked as today (types from the source).
  Catalog entry, codes row, `tests/catalog.py`; `python3 tests/apply_fixes.py`. First task allowed to change
  existing expected outputs: only by adding E0342 lines.
- [ ] T020 [US2] `tools/vo_fix.vo`: E0342 and E0413 (T029) fixes apply in the same round (their spans do
  not touch: header vs body); confirm 4 rounds suffice for `quickstart.md` step 3's `old.vo` and for
  `examples/notes-api`; raise the round limit only if a measured case needs it.
- [ ] T021 [US2] Migrate `std/` by fixes: `bin/vo run --timeout 0 tools/vo_fix.vo -- std` (with T029's
  E0413 when Phase 5 is done, else a second pass then); then by hand, research R9: the five `on_get`
  handlers in `std/web.vo` (915, 924, 927, 957, 1096) and the two `__serve_workers` callers in
  `std/http.vo` (649, 692) become named private functions; the five untyped-let lambdas (`std/web.vo` 137,
  221; `std/security.vo` 809; `std/http.vo` 406, 454) become named functions where they capture nothing,
  typed bindings otherwise; the six `on_get` doc tests in `std/web.vo` (40, 87, 97, 106, 115, 124) pass
  named functions. `doc_conformance.py` passes; std's own tests and examples unchanged.
- [ ] T022 [P] [US2] Migrate `packages/postgres/` by fixes and update its pinned hash in every
  `vo.packages` that names it; `tests/postgres.py` passes.
- [ ] T023 [P] [US2] Migrate `tools/vo_fix.vo` and the other `.vo` tools under `tools/` by fixes; they run
  as before.
- [ ] T024 [US2] Migrate `examples/` by fixes (`examples/*/main.vo` and their modules); each example's
  `vo test` and recordings replay unchanged (`make test`'s examples step).
- [ ] T025 [US2] Migrate `docs/errors.md`'s examples, `site/docs/` guide examples and `README.md` by fixes
  (through the extracted programs of `tests/site_examples.py` and `tests/catalog.py`); `site_examples.py`
  and `catalog.py` pass. The reference is T034.
- [ ] T026 [US2] `tests/migrate_059.py` (as `tests/migrate_058.py`, with `--quick`): for every golden case
  source under `tests/cases/` containing an anonymous function, record its outputs with the 0.4 meaning
  (written types), apply fixes by `vo_fix`, and require identical run output, test results and exit
  status; then write the migrated sources back and regenerate only the expected outputs that contained
  E0342/E0413 lines. Add to `make test` and to the `lambdas` area (T001).
- [ ] T027 [US2] `make test`: every case passes on migrated sources; `grep` finds no anonymous function with
  a written type in `std/`, `examples/`, `packages/`, `tools/`, `site/docs/`, `docs/errors.md` (SC-001).

**Checkpoint**: the repository is on the new form; 0.4 code reports E0342 with fixes.

---

## Phase 5: User Story 3 - Short functions on one line (Priority: P2)

**Goal**: a body that is a single `return expr;` has one form, `=> expr`, enforced by E0413 and applied
by `vo fmt`.

**Independent Test**: `sort_by(people, fn(p) => p.age);` checks, runs and stays on one line after `vo fmt`;
`fn(p) { return p.age; }` is E0413 whose fix gives the former.

- [X] T028 [P] [US3] Golden cases first, `tests/cases/lambda_types/`: `single_return.vo` (check: E0413 with
  its fix, at the block); `single_return_comment.vo` (check: a single `return` with a comment in the block
  is not E0413); `tests/cases/fmt/lambda_single_return.vo` (fmt rewrites `fn(p) { return p.age; }` to
  `fn(p) => p.age`, keeps the commented block).
- [X] T029 [US3] E0413 lambda_single_return in `src/check_func.c` (contracts/diagnostics.md): a block body
  (not `NF_ARROW`) whose only statement is `return expr;` and that contains no comment; fix: one edit from
  `{` to `}` giving `=> expr` (expression text from the source). Catalog entry, codes row,
  `tests/catalog.py`; `python3 tests/apply_fixes.py`.
- [X] T030 [US3] `src/fmt.c` (`fm_lambda`): the same rewrite syntactically (no checker), so `vo fmt` applies
  it (FR-009); `tests/fmt_corpus.py` passes (every corpus file stable under a second `vo fmt`).
- [X] T031 [US3] Re-run the migrations of T021–T025 and T026 with E0413 (if Phase 4 ran first): single-return
  lambdas in the repository become `=> expr`; `make test`. (No-op: Phase 5 ran first; the migrations run once
  with both fixes. E0413 and fmt's rewrite apply to untyped lambdas, so 0.4 code gets E0342 first and E0413
  in the next `vo_fix` round.)

**Checkpoint**: all user stories complete.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [ ] T032 [P] Editors (research R8): `=>` in `editors/vscode/syntaxes/vo.tmLanguage.json`,
  `editors/tree-sitter-vo/grammar.js` (regenerate with `make grammars`), `editors/vim/`, and
  `site/pygments_vo/pygments_vo.py`; `python3 tests/grammars.py` and `editors/check.js` pass over every
  migrated file.
- [ ] T033 [P] `src/lsp.c` / `tests/lsp.py`: hover on an untyped lambda parameter shows its inferred type;
  `lsp.py` passes (no code change expected).
- [ ] T034 `docs/reference.md` (R12): replace "Anonymous functions write parameter and return types like
  named functions" with the one-paragraph rule and inference order of contracts/syntax.md; the builtin
  advice (line 510) becomes `fn(s) => len(s)`; the 8 lambdas become untyped, single-return ones `=>`;
  `python3 tests/ref_examples.py`: tokens before and after stated in `results.md`, **no growth** (SC-005).
- [ ] T035 [P] `site/docs/guide/functions.md`: the rule, the inference order and a migration note;
  `docs/stability.md` and `CHANGELOG.md` (0.5.0: the change, its migration with `tools/vo_fix.vo`, the
  three codes); `README.md`'s first example rewritten in the new form (FR-008).
- [ ] T036 Version 0.5.0 (`src/main.c` version string, `docs/stability.md`, `CHANGELOG.md`); `vo version`
  golden case updated.
- [ ] T037 SC measurements in `results.md`: SC-001 (written types left: 0; share needing a typed binding,
  target < 5%), SC-003 (header characters −50%, single-return lines −60%), SC-006 (check times within 5%
  of T003's baseline).
- [ ] T038 `make test` on x86-64, ARM64 and RISC-V (`make test ARCH=aarch64`, `ARCH=riscv64` under qemu):
  byte-identical (SC-002); `make gate`.
- [ ] T039 `specs/059-inferred-function-types/results.md` complete (Verification, Deviations);
  `docs/roadmap.md` row for 059; then, with the user, migrate the voledger and vositegen kits with
  `tools/vo_fix.vo` and log any problem they hit.

---

## Dependencies & Execution Order

- **Setup (T001–T003)**: first; T002 and T003 in parallel.
- **Foundational (T004–T010)**: T004 → T005 → T006 → T007 → T010; T008 and T009 in parallel after T006.
  Blocks every story.
- **US1 (T011–T018)**: after Phase 2. Pairs (cases → code): T011 → T012 → T013, T014 → T015 → T016, T017,
  T018; the case tasks T011 and T014 can be written together first; T015 and T016 share no file with T012
  but T016 needs T013's `open` return.
- **US2 (T019–T027)**: after US1 (fixes need the untyped form to check, and the "no expected type" cases need
  E0343). T019 → T020 → T021 → (T022, T023 in parallel) → T024 → T025 → T026 → T027.
- **US3 (T028–T031)**: after Phase 2; T028 → T029 → T030; best done before T021 so each migration runs once
  with both fixes (then T031 is a no-op).
- **Polish (T032–T039)**: after the stories; T032, T033 and T035 in parallel; T034 after T027 (examples are
  migrated); T038 after everything else; T039 last.

## Parallel Example: User Story 1

```text
T011 positions/return_open/arity/wrong_body cases   T014 builtins/generic_user/generic_payload cases
→ then T012, T013 (check_expr.c, check.c), T015 (check_hof), T016 (check_generic.c), T017, T018
```

## Implementation Strategy

- **MVP**: Phases 1–3. New programs write untyped lambdas everywhere, including generic calls; written types
  keep working; nothing old breaks. Stop and validate with quickstart steps 1, 2 and 4.
- **Then US3 before US2's migration** (recommended order: Phase 5 after Phase 3), so the repository is
  migrated once with both E0342 and E0413 fixes.
- **Then US2**: the breaking half. Land T019–T020 before migrating anything, migrate std and packages before
  examples and golden cases, and keep `build/vo` consistent for the experiment kits (rebuild only with a
  complete change).
- **Then Polish**, finishing with the reference budget, three platforms, the gate and `results.md`.

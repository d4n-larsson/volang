---

description: "Task list for vo-lang spec 030: Readable Signatures and Named Effect Sets"
---

# Tasks: Readable Signatures and Named Effect Sets

**Input**: Design documents from `specs/030-readable-signatures/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R10), data-model.md, contracts/syntax.md,
contracts/diagnostics.md, contracts/format.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's golden cases land first and fail before its code.

**Organization**:
- Setup: test wiring.
- Foundational: the `effects` item parsed and registered as a symbol (US2–US4 build on it).
- US1 (P1) header layout in `vo fmt`; US2 (P1) named sets usable in clauses and types; US3 (P1) exact
  reports, fixes beside set names, the W0501 rule; US4 (P2) examples, guide, reference.
- Polish: the records check, the agent benchmark (with the user), three platforms.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: `tests/cases/effect_sets/<name>.vo` (`.cmd` `check {}` for diagnostics, run otherwise),
  `tests/cases/fmt/<name>.vo` with `<name>.expected.vo`; accept with `tests/run.sh --accept PATH`.
- **Catalog**: each new or changed code updated in `docs/errors.md`, `specs/030-readable-signatures/
  contracts/codes.md` (format of spec 027's), and `tests/catalog.py`'s spec list.
- **Reference budget**: `python3 tests/ref_examples.py` after every reference change; at most +150 tokens.
- **Existing programs**: every golden case unchanged except where this spec says (SC-005).
- **Between edits**: `make quick AREA=effects` (T001); `make test` once per change.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: a new area `effects` (cases `effect_sets`, `effects`, `fmt`; scripts
  `fmt_corpus.py`, `catalog.py`) and the mapping `src/check_effects.c`, `src/fmt.c` → `effects`, listed among
  the known areas.
- [X] T002 [P] `specs/030-readable-signatures/results.md`: SC-001–SC-005 (pending), Verification,
  Deviations; `specs/030-readable-signatures/contracts/codes.md` (E0513, W0505; E0509 changed).

---

## Phase 2: Foundational (blocking)

- [X] T003 Golden cases first, `tests/cases/effect_sets/declare.vo`: `effects Storage = file.read("data/"),
  file.write("data/");`, `export effects`, a set including another, an unused-but-exported set; `vo check`
  clean; `effects` still usable as a variable name elsewhere in the file (contextual word, research R1);
  `effects Nothing = ;` is E0110.
- [X] T004 `src/parse.c`, `src/vo.h`: the item `["export"] "effects" TypeName "=" effect ("," effect)* ";"`
  (`N_EFFECTS`: A name, B the `N_USES`-shaped list), recognized only at the start of an item; reuse
  `parse_effect` for the list.
- [X] T005 `tests/fuzz.py`: noise tokens for the `effects` item and set names in clauses (`effects S = `,
  `effects S = file.read("x/");`, `uses Storage`, `uses console, Storage`), well formed and broken; `make
  test`'s `fuzz.py --iterations 200 --seed 1` passes.
- [X] T006 `src/check.c`: register sets as items (a new symbol kind), exported and imported like records
  (`import notes.Storage;`), E0202 for a name clash with another item; expand each set once before function
  bodies are checked (data-model.md: "expanded: the plain effects (mask and scopes) after expanding every
  name, computed once"), reporting a cycle as E0513 naming the chain; E0203 when a function's effect
  parameter has the name of a set in scope. Make T003 pass.

**Checkpoint**: sets parse, register and expand.

---

## Phase 3: User Story 1 — Long signatures are laid out by the formatter (Priority: P1) 🎯 MVP

**Goal**: headers over 100 characters are laid out: the clause on its own line, then parameters one per line.

**Independent Test**: a file with a 120-character header, long parameters, a long function-type parameter
and a long anonymous function formats to lines of at most 100 characters (where possible), to itself, with
behavior unchanged.

- [X] T007 [P] [US1] Golden cases first, `tests/cases/fmt/`: `sig_clause.vo` (only the clause makes it
  long), `sig_params.vo` (parameters one per line, `) -> Type` on its line, the clause after it or on its
  own line), `sig_fn_type.vo` (a long function-type parameter: never broken inside), `sig_lambda.vo` (a
  long anonymous-function header in a statement), `sig_test.vo` (a long `test "..." uses ...` header),
  (no `sig_comments.vo`: a comment between parameters is E0110 today, nothing to keep), `sig_short.vo` (≤ 100: unchanged), each with
  `.expected.vo`, per contracts/format.md.
- [X] T008 [US1] `src/fmt.c`: the header layout of research R7 for `fn`, `test` and `prop` headers (render
  flat; over 100 characters: the clause on the next line indented one level with ` {`; still over: each
  parameter on its own line one level deeper, then `) -> Type`, then the clause by the first rule); anonymous
  functions' parameters by the second rule inside the statement fixpoint; function types never broken.
  Make T007 pass; `python3 tests/fmt_corpus.py` (idempotence, behavior) passes.
- [X] T009 [US1] `docs/reference.md` (the formatting paragraph: one sentence for headers), `site/docs/tools/
  fmt.md` (the rule with an example); reformat `std/*.vo`, `examples/`, `bench/web/reference/`, long blocks
  on site pages (re-accept goldens whose `std/` trace positions move, checking that only numbers changed).

**Checkpoint**: US1 alone fixes the readability complaint.

---

## Phase 4: User Story 2 — Effects a program repeats are named once (Priority: P1)

**Goal**: sets usable wherever effects are written, meaning the union of their effects.

**Independent Test**: notes-api with `Storage` checks, runs, passes its tests; its `vo.effects` is unchanged.

- [X] T010 [P] [US2] Golden cases first, `tests/cases/effect_sets/`: `use.vo` (a function `uses Storage`
  reads and writes in `data/` and is refused outside: R0010 at run time, E0503 at check time), `union.vo`
  (`uses console, Storage, Network`; nested sets), `types.vo` (`Database[uses Storage]` with the set's scopes
  dropped; `fn(Note) -> result[void] uses Storage` as a parameter type; a value with fewer effects fits),
  `import/` (a multi-file program importing an exported set; an unexported one is E0205), `param_clash.vo`
  (`.cmd` `check {}`: an effect parameter named like a set in scope is E0203), `tests.vo` (a test declaring
  `uses Storage`).
- [X] T011 [US2] `src/check_effects.c` (`fx_build_set`): a capitalized name is an effect parameter in scope,
  else a set in scope whose expansion is unioned in, else E0509 "neither an effect parameter nor an effect set
  in scope" with the nearest effect, parameter or set as a fix; in a type's effect argument the set's scopes
  are dropped (research R3). Make T010 pass. `docs/reference.md` Effects: one paragraph on sets (the item,
  union, scopes dropped in types, records expanded); `python3 tests/ref_examples.py`.

**Checkpoint**: US1 + US2.

---

## Phase 5: User Story 3 — Reviewing stays exact (Priority: P1)

**Goal**: records and reports show expanded effects; fixes keep set names; W0501 leaves sets alone.

**Independent Test**: changing `Storage` to read only makes `vo check` report every function using it;
`vo effects --update` records each.

- [X] T012 [P] [US3] Golden cases first, `tests/cases/effect_sets/`: `record.vo` + `vo.effects` (expanded
  effects in the record; `.cmd` `effects {}`), `changed.vo` (a set's definition grows: E0504 for each user,
  named), `fix_missing.vo` (`.cmd` `check --json {}`: the E0501 fix gives `uses console, Storage`),
  `unused.vo` (W0505 for an unused private set with the removal fix; no W0501 for a set's unneeded parts; the
  set name reported when none of its effects is needed), `cycle.vo` (E0513 naming the chain), `unknown.vo`
  (`uses Storge`: E0509 with the fix `Storage`), `bad_inside.vo` (an invalid scope inside a set: E0506 once,
  at the declaration).
- [X] T013 [US3] `src/check_effects.c`: `fx_fix_clause` keeps a clause's set names and adds the missing
  effects in canonical order (research R5); W0501 only for effects written directly, or a set none of whose effects is needed; W0505
  for sets no clause uses (fix: delete the declaration line); errors inside a set reported once at its
  declaration. Make T012 pass.
- [X] T014 [US3] `src/fmt.c`: print the `effects` item; clause order: plain effects in today's order, then set
  names alphabetically (contracts/syntax.md); `src/doc.c`: `vo doc` lists exported sets as items with their
  expanded effects, and signatures whose clause names a set show it as written with the expansion in
  parentheses; `src/query.c`, `src/edit.c`: references and `vo edit rename` include sets (golden cases
  `tests/cases/effect_sets/doc.vo`, `rename/`, `query.vo`).
- [X] T015 [US3] `docs/errors.md`: E0509 (the set case), E0513 `effect_set_cycle`, W0505 `unused_effect_set`
  with examples and fixed versions; `tests/catalog.py` passes; `docs/reference.md` Error codes: E0513, W0505.

**Checkpoint**: the MVP (US1–US3).

---

## Phase 6: User Story 4 — The examples and the guide use them (Priority: P2)

**Goal**: agents learn the forms where they look.

**Independent Test**: no signature line in the examples over 100 characters; notes-api and the 10-operation
API declare their storage effects once.

- [X] T016 [US4] `examples/notes-api/`: `effects Storage = file.read("data/"), file.write("data/");` in the
  file that opens the database, used by `open_notes` and `main`; README; `vo.effects` unchanged.
- [X] T017 [US4] `bench/web/reference/main.vo`: grown to 10 operations (add list by sku, adjust quantity,
  count, rename, bulk delete) sharing one `Storage` set; `bench/web/accept.py` still 29/29 (operations
  outside the task's table are not checked).
- [X] T018 [P] [US4] `site/docs/guide/effects.md` (when to name a set: the same scoped effects in three or
  more functions; an example checked by `tests/site_examples.py`), `docs/roadmap.md` (spec 030 row and
  section); the reference total for the spec stays within +150 tokens.

---

## Phase 7: Polish and verification

- [X] T019 [P] `tests/effect_sets_records.py` (SC-003): for each example and 20 generated programs, rewrite
  repeated clauses into sets and check that `vo.effects` (from `vo effects --update` in a copy) is
  byte-identical and run output unchanged; part of `make test`.
- [ ] T020 (deferred) SC-004: benchmark v2's effect-related tasks with isolated agents before and after (schedule with the
  user; record the protocol and results in results.md, or "deferred" with the kit's command).
- [X] T021 `make lint`; the instrumented and stress builds over `tests/cases/effect_sets` and `tests/cases/fmt`.
- [ ] T022 (x86-64 done; ARM64, RISC-V not run yet) `make test` on x86-64, then `make ARCH=aarch64 test` and `make ARCH=riscv64 test`, one at a time.
- [X] T023 Fill in `results.md`; spec status Implemented; every task [X].

---

## Dependencies & Execution Order

- Phase 1 → Phase 2 (T003 → T004 → T005 (fuzzer) and T006) → US2 → US3 (US3 needs sets in clauses).
- US1 (T007–T009) depends only on Phase 1; it can run before or alongside Phase 2.
- US4 after US1–US3. T019 after US3; T020 any time after US3 (with the user); T021–T023 last.

## Parallel Opportunities

- T002 with T001; T007 (fmt cases) while Phase 2 lands; T010 and T012 cases while T006/T011 land; T018 with
  T016/T017.

## Implementation Strategy

- **MVP**: US1 (the formatter fixes long headers for every program) and US2+US3 (sets that keep reviews
  exact).
- Then US4 (examples, docs) and Polish with the measurements.

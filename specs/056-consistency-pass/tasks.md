---

description: "Task list for vo-lang spec 056: Consistency Pass"
---

# Tasks: Consistency Pass

**Input**: Design documents from `specs/056-consistency-pass/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R15), data-model.md, contracts/api.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each change's golden case lands first and fails before its code.

**Organization**:
- Setup: the baseline of the pre-spec repository, the in-place migration tool, the results file, the
  quick area.
- Foundational: the change table and E0214 with its five kinds of fix (R1), since every library story
  uses them.
- US1 (P1) clashes; US2 (P1) Option lookups; US3 (P1) builtins not shadowed; US4 (P1) order; US5 (P2)
  conventions; US6 (P2) migration proof; US7 (P3) language corners and messages.
- Polish: reference and cost, catalog, changelog, version 0.2.0, audit file, three architectures,
  `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Behavior**: contracts/api.md is the source of every name, fix and code; data-model.md of every rule
  (change kinds, ordered types, limits, units).
- **Golden cases**: `tests/cases/AREA/NAME.vo` with `.out`/`.err`/`.code` (and `.cmd` when not the
  area's default), as the existing areas. Write the case, see it fail, then write the code. New area
  `tests/cases/library/` (default command `check --json {}`) holds one case per change row: the old form
  and its E0214.
- **Migrating the repository**: after a library change lands, its users in `std/`, `examples/`,
  `packages/`, `tests/cases/`, `tests/*.py` (inline programs) and `site/docs/` are migrated with the
  in-place tool of T002 (fixes only, no hand edits). Golden outputs that change only by the migration are
  re-accepted. A hand edit is allowed only where a fix cannot apply (a program inside a Python string),
  and is listed in results "Deviations".
- **std**: every exported item keeps its doc comment and example (doc conformance).
- **C**: zero warnings; no static buffer over 64 KiB.
- **Between edits**: `make quick AREA=...`; `make test` once at the end; never two suites at once.

## Phase 1: Setup

- [X] T001 Record the baseline in `tests/migrate_056/baseline.json`, from commit 8c8db3e:
  - for every `examples/*/main.vo` and `examples/*/*/main.vo` (the glob of `make test`'s examples step)
    and `packages/postgres`: `vo test` results (names, statuses), and `vo run` stdout and exit status
    where a run needs no network or input;
  - written by `tests/migrate_056.py --record` (a new script, finished in T028), run with the toolchain
    built at that commit (`git worktree add`, `make`, then removed).
- [X] T002 Create `tools/vo_fix.py`. Given paths (files or directories), it runs `vo check --json` on
  each entry program and applies the first fix of every diagnostic once, in place, repeating up to 3
  rounds while fixes remain. When E0504 (a changed effects record, which has no fix) is all that is
  left, it runs `vo effects --update` for that entry. It prints the codes it fixed and the diagnostics
  left. It reuses `tests/apply_fixes.py`'s offset logic (import it).
- [X] T003 Create `specs/056-consistency-pass/results.md` with headings SC-001 to SC-007, "Reference
  cost" and "Deviations". In `tools/quick.sh`, add area `consistency`: cases `library check generic run
  fmt lex effects doc`, scripts `std_names.py`, `message_style.py`, `apply_fixes.py` and `catalog.py`.
  Map `src/libchange.c` and `tests/std_names.py` to it. In `tests/run.sh`, add `library` to the areas
  whose default command is `check --json` (`lex|parse|check)`).

## Phase 2: Foundational

- [X] T004 Cases first, in `tests/cases/library/`:
  - `rename_import.vo`: imports `std.lists.contains` and uses it twice; E0214 at the import and at each
    use, with fixes `has_item`;
  - `rename_value.vo`: a renamed function passed as a value;
  - `remove_call.vo`: `std.web.clock_ms()` and `clock_ms` as a value, fixed to `now_ms()` and
    `fn() -> int { return now_ms(); }`;
  - `swap.vo`: `is_match(re, s)`, fixed to `is_match(s, re)`;
  - `option_match.vo`: `match index_of(xs, 3) { case ok(i) ... case err(_) ... }`, wrapped by the fix;
  - `unit_literal.vo`: `fixed_zone(330)`, fixed to `fixed_zone(19800000)`;
  - `unit_expr.vo`: `fixed_zone(m)`, fixed to `fixed_zone(minutes(m))`;
  - `own_name.vo`: a program defining its own `fn contains`, with no E0214.

  Each ends checking clean after `tests/apply_fixes.py`. These fail until T005 and T006.
- [X] T005 `src/libchange.c` (new) and `src/vo.h`: the change table of data-model.md ("module, old,
  kind (`rename`, `remove`, `swap`, `option`, `unit`), new, wrap, factor, since") with every row of
  contracts/api.md. It also has:
  - `libchange_find(module, name)`;
  - the E0214 message builder: "`std.M.OLD` was renamed `NEW` in vo 0.2.0." / "... was removed in vo
    0.2.0: write `now_ms()`." / "... takes the text first since vo 0.2.0." / "... returns `Option[T]`
    since vo 0.2.0." / "... takes milliseconds since vo 0.2.0.";
  - the fix builders for each kind.
- [X] T006 Hook E0214 into the checker:
  - In `src/loader.c`, where E0205 "has no item" would be reported for a std file, a table row reports
    E0214 at the import with its fix, and binds the import to the new item (removed: to its replacement
    builtin, so uses resolve).
  - In `src/check_expr.c`, each use of an old local name (call, value, case name, field name in a record
    literal or access) reports E0214 with the use's fix.
  - For kind `option`: where a call of a retyped item meets a `result` context, E0214 replaces the type
    error, with the wrap fix (`to_result(CALL, "none")` or `or_else(CALL, -1)`), adding the import in
    one edit from the imports to the call when it is missing. A `result` context is a `match` with
    `ok`/`err` arms, `try`, an expected `result` type, or a comparison with `ok(...)`/`err(...)`.

  Then make T004 pass.
- [X] T007 `tests/libchange_rows.py` (new): for every row of the change table (read from
  `src/libchange.c`), generate a minimal program using the old form. Require E0214 with the row's fix
  (or none where the row has none), and require the fixed program to check clean (SC-003). Add it to
  the area `consistency` and to `make test`. It fails for each row until that row's std change lands
  (it skips rows whose std item is not changed yet, listing them).
- [X] T008 `docs/errors.md`: entry `## E0214 library_changed` (Fixable: sometimes; description of the five
  kinds, Example with `std.lists.contains`, Fixed). Also `specs/056-consistency-pass/contracts/codes.md`
  (as spec 055's) and `"056-consistency-pass"` in `tests/catalog.py`'s spec list; then
  `python3 tests/catalog.py`.

**Checkpoint**: an old name is reported and fixed. The library itself is unchanged so far: the table's
rows take effect as each story changes std.

## Phase 3: User Story 1 - Any two library items can be imported together (P1) 🎯 MVP

**Goal**: no exported name is used by two std modules.

**Independent Test**: `python3 tests/std_names.py` passes, and `tests/cases/library/rename_*` pass.

- [X] T009 [P] [US1] `tests/std_names.py` (new):
  - lists every exported item of every `std/*.vo` (from `vo doc --json`);
  - fails on a name used by two modules;
  - generates a program importing one item of each name and checks it clean (no E0202).

  Fails now (7 clashes).
- [X] T010 [US1] Rename in std (contracts/api.md "Renamed"):
  - `std/lists.vo`: `contains` → `has_item`;
  - `std/json.vo`: `field` → `member`;
  - `std/tcp.vo`: `close` → `close_conn`, `send` → `write`, `describe` → `tcp_error_text`;
  - `std/metrics.vo`: `add` → `add_to`;
  - `std/sqlite.vo`: `get` → `column`, `get_int` → `int_column`, `get_float` → `float_column`,
    `get_str` → `text_column`, `get_bytes` → `bytes_column`.

  Update the doc comments and examples, then the std files that use them (`std/web.vo` uses metrics'
  `add`; `std/security.vo` and others: run T002's tool on `std/` through a program importing everything).
- [X] T011 [US1] Migrate the repository's users with `tools/vo_fix.py`: `examples/`,
  `packages/postgres/`, `tests/cases/`, the inline programs in `tests/*.py` (by hand, listed), and the
  vo blocks of `site/docs/`. Re-accept golden outputs that change only by the renames; then
  `make quick AREA=consistency` and the areas of the changed modules (`tcp`, `sqlite`, `web`, `ops`).

**Checkpoint**: SC-001 holds.

## Phase 4: User Story 2 - One answer for "nothing there" (P1)

**Goal**: every lookup of FR-005 returns `Option`.

**Independent Test**: `tests/cases/library/lookups.vo` and the `option_*` cases pass, and doc conformance
reports no `err("none")`.

- [X] T012 [P] [US2] Cases:
  - `tests/cases/library/lookups.vo`: `response_header` and `request_header` with and without the
    header; `key_after` on an empty and a full `ordered_map`; `first` of an empty `ordered_set`;
    `index_of` found and missing; `first_match` with and without a match; each prints `none` or
    `some(...)`;
  - `tests/cases/library/option_try.vo`: `try first_key(m)` in old code, wrapped by the fix.
- [X] T013 [US2] In std, the lookups return `Option[T]`:
  - `std/http.vo`: `header` renamed `response_header` (FR-013) and `request_header`;
  - `std/collections.vo`: `first_key`, `last_key`, `key_at_or_above`, `key_at_or_below`, `key_after`,
    `key_before`, `first`, `last`, `at_or_above`, `at_or_below`, `after`, `before`, `peek`, `front`,
    `back`;
  - `std/lists.vo`: `index_of`;
  - `std/regex.vo`: `first_match`.

  Each imports `std.option.Option`. Doc examples show `some`/`none`. "Taking an element (`pop`,
  `pop_front`) stays a `result` (`err("empty")`)."
- [X] T014 [US2] Add to `tests/doc_conformance.py` a check that no std function's body or example
  returns `err("none")`. Migrate the repository's users (T002's tool, as T011). The wrap fix keeps
  behavior; where a wrapped call reads oddly in an example or std module, replace it by hand with the
  natural `match ... { case some(v) ... case none ... }` (listed in Deviations). Then make T012 pass.

## Phase 5: User Story 3 - A builtin name is never hidden (P1)

**Goal**: E0203 for locals, parameters and bindings named like builtins.

**Independent Test**: `tests/cases/check/shadow_builtin.vo` and `shadow_builtin_fix.vo` pass.

- [X] T015 [P] [US3] Cases:
  - `tests/cases/check/shadow_builtin.vo`: a parameter `keys`, a local `len`, a `for` variable `min`, a
    case binding `count` (`case some(count)`), each E0203 "`len` is a builtin: give the local another
    name.";
  - `shadow_builtin_fix.vo`: the fix renames to `len_value` there and at its uses (`len_value2` when
    `len_value` is taken), and the result checks clean.
- [X] T016 [US3] `src/check.c` `new_local`: E0203 when `builtin_id(name)` (the builtins of
  `check_expr.c`'s table, not `str`/`int`/`float`, which are keywords), with the message above and a
  fix renaming the declaration. Each use gets E0203's "rename it here too" fix, as E0207 does for uses.
  Make T015 pass.
- [X] T017 [US3] Migrate the repository (T002's tool on `std/` and everything T011 covers), then
  `python3 tests/doc_conformance.py`; re-accept golden outputs
  changed only by the renames, and record the count of renamed locals in results.

## Phase 6: User Story 4 - Every key type sorts (P1)

**Goal**: `bool` and variants are ordered; `key` implies `ordered`.

**Independent Test**: `tests/cases/run/order_bool_variant.vo` gives the documented order on three
architectures.

- [X] T018 [P] [US4] Cases:
  - `tests/cases/run/order_bool_variant.vo`:
    - `sort([true, false, true])`;
    - `variant Level { low; mid; high; }`, sorted and compared with `<`;
    - `variant Shape { dot; circle(float); square(float); }` sorted (`dot < circle(1.0) < circle(2.0) <
      square(0.5)`);
    - `min`/`max` of two `Level`s;
    - an `ordered_set[Level]`;
    - `fn sorted_keys[K: key](m: map[K, int]) -> list[K]` on a `map[bool, int]` and a `map[str, int]`;
  - `tests/cases/check/order_unordered_payload.vo`: a variant with a `map` payload sorted is E0313;
  - `tests/cases/generic/order_bounded.vo` (from group 1) now checks clean: re-accept with its comment
    updated.
- [X] T019 [US4] Implement:
  - `src/check_generic.c` `ty_ordered_d`: `TY_BOOL`; `TY_VARIANT` when every case payload is ordered
    (a seen-set for recursive variants); `TY_PARAM` with `TPC_KEY`;
  - `ck_param_order`: `key` passes;
  - `src/rt_coll.c` `value_cmp`: `TY_VARIANT` by case index, then the payload's `value_cmp`;
  - `src/check_expr.c`: ordering operators on bool and variant operands use the general comparison
    instruction (as records do).

  Remove group 1's "takes one bound" E0311 message. Make T018 pass.

## Phase 7: User Story 5 - One convention per concept (P2)

**Goal**: FR-008 to FR-017.

**Independent Test**: the cases below, doc conformance, and `tests/cases/library/` for each changed
signature.

- [X] T020 [P] [US5] Cases:
  - `tests/cases/library/timeout_zero.vo`: tcp `read` with 0 is accepted, and 700000 is
    `err("invalid: the time limit must be 0 to 600000 ms")`;
  - `tests/cases/library/worker_timed_out.vo`: `case timeout` in old code gets E0214 with the fix
    `timed_out`.
- [X] T021 [US5] Time limits:
  - `src/tcp.c` (`MS_MAX` check: 0 means 600000; range 0 to 600000);
  - `std/http.vo` (`send`, `send_bytes`: 0 means 600000, range checked, docs);
  - `std/workers.vo` (`with_deadline` range; `WorkerError.timeout` → `timed_out`, through the table);
  - `src/workers.c`.

  Then the docs of every `timeout_ms`.
- [X] T022 [P] [US5] Case `tests/cases/library/regex_order.vo`: old `replace_all(re, s, r)` and
  `split_on(re, s)` with their fixes. In `std/regex.vo`, the text comes first in `is_match`,
  `first_match`, `all_matches`, `split_on` and `replace_matches` (renamed from `replace_all`), with docs
  and examples. Migrate the users.
- [X] T023 [P] [US5] Units:
  - case `tests/cases/library/units.vo`: `fixed_zone(330)`, `new Cookie { ... max_age_s: 3600 }` and a
    security way with `lifetime_s: s`, each fixed;
  - in std:
    - `std/time.vo` `fixed_zone` takes ms;
    - the tz records' duration fields get `_s`;
    - `std/web.vo` `Cookie.max_age_s` → `max_age_ms`, divided by 1000 for `Max-Age`;
    - `std/security.vo` `lifetime_s` → `lifetime_ms`, `hsts_seconds` → `hsts_ms`.

  Migrate the users.
- [X] T024 [P] [US5] Typed reads and sqlite:
  - `float_column` converts an INTEGER, and the missing-column error reads ``no column `NAME` ``
    (`std/sqlite.vo`);
  - sqlite messages lose `error: ` (`src/sqlite_*.c` or `std/sqlite.vo`, wherever the prefix is added);
  - case `tests/cases/sqlite/float_column_int.vo` and an updated open-failure case (`try open` in `main`
    prints `error: unable to open database file` once).
- [X] T025 [P] [US5] Pairs, spellings and duplicates:
  - `std/http.vo` `serve_bytes_workers` → `serve_workers_bytes`;
  - `std/random.vo` `shuffle` → `shuffled`;
  - `std/web.vo` removes `clock_ms`, and `log_requests`'s doc points at `request_logs` and `now_ms`;
  - `std/time.vo` removes `now`.

  All through the table. Migrate the users.
- [X] T026 [P] [US5] Resources and error texts:
  - `std/tcp.vo` `close_conn` and `close_listener` without effects;
  - `tcp_error_text` (renamed in T010);
  - `worker_error_text` in `std/workers.vo` and `local_error_text` in `std/time.vo`: one sentence per
    case, ending with a period, with doc examples;
  - case `tests/cases/library/error_texts.vo`.
- [X] T027 [US5] Parse errors (FR-011, research R9):
  - values: `invalid KIND: INPUT[; DETAIL]` (int, float: `src/rt_str.c`; decimal; time; `bits.parse_hex`;
    `bytes.from_hex` and `from_base64` sharing the hex detail);
  - documents: `invalid KIND at POSITION: DETAIL` (json `line L, column C`, regex `byte N`, sql).

  Update the golden outputs and doc examples that show these texts, and list every changed text in
  `CHANGELOG.md` (T039).

## Phase 8: User Story 6 - Upgrading is a mechanical step (P2)

**Goal**: SC-002, proven on the pre-spec copies.

**Independent Test**: `python3 tests/migrate_056.py` and `python3 tests/migrate.py` pass.

- [X] T028 [US6] Finish `tests/migrate_056.py`:
  - for every program in the T001 baseline: copy it as of 8c8db3e (`git show`), apply fixes with
    `tools/vo_fix.py` (at most 3 rounds), require a clean check, and compare `vo test` results and
    `vo run` output with the baseline;
  - report the E0214 rows each program needed;
  - hold an allowance table of intended output changes (as `tests/migrate.py`'s `CHANGED`): each entry is
    the program, the expected stdout and exit status now, and the task that changed them (T021
    `http.send`'s 0, T024 sqlite's prefix, T027 parse texts). Anything else must equal the baseline;
  - add it to `make test` in `Makefile` and to the area `consistency`.
- [X] T029 [US6] `tests/migrate.py`: up to 3 rounds of fixes; E0214 and E0203 (builtin shadowing) in
  `MIGRATION`; then 61 of 61.

## Phase 9: User Story 7 - The language's corners behave alike (P3)

**Goal**: FR-018 to FR-022.

**Independent Test**: one golden case per corner, and `tests/message_style.py`.

- [X] T030 [P] [US7] `[]`:
  - case `tests/cases/run/empty_list_context.vo`: `f([])` with `f(xs: list[int])`,
    `let xs: list[str] = [];`, `return [];` from `-> list[int]`, a record field `items: []`;
  - case `tests/cases/check/empty_list_no_type.vo`: `let xs = [];` is still E0306;
  - `src/check_expr.c` `check_list`: with an expected list type, `[]` has that type and no error.
- [X] T031 [P] [US7] `Shape.empty()`:
  - case `tests/cases/check/case_parens.vo`: E0411 with the fix removing `()`;
  - `src/parse.c` (parse the empty argument list after a variant case) and the variant-case check in
    `src/check_expr.c` (E0411 when the case has no payload and `()` was written).
- [X] T032 [P] [US7] Literals:
  - case `tests/cases/lex/number_spellings.vo` (`check --json`): `1E9`, `1e+9`, `00012`, `1e-400`,
    `-0x8000000000000000`, each E0105, with the fixes of contracts/api.md (none for the last two);
  - `src/lex.c`; for the negation, the checker's constant folding of unary minus on a literal.

  Run `tests/grammars.py` (the corpus must still parse).
- [X] T033 [P] [US7] Effect arguments:
  - case `tests/cases/fmt/effect_arg_uses.vo`: `Db[D]` and `Db[Storage]` both formatted `Db[uses ...]`;
  - case `tests/cases/effect_sets/named_set_arg.vo`: `Db[Storage]` checks;
  - `src/fmt.c`; `src/check_generic.c` (a named set accepted as an effect argument, as an effect
    parameter is).

  Migrate the repository's files with `vo fmt` (`tests/fmt_corpus.py`).
- [X] T034 [US7] Message style (FR-022, research R13):
  - `tests/message_style.py` (new) checks the message templates in `src/*.c`: "got", unbackticked code
    examples, ranges with `-`, and tests or packages named another way;
  - fix every message it reports;
  - repeated annotations: E0119 for `@size`, `@allow` and every annotation (`src/check_annot.c`), with
    a fix removing the repeat;
  - `src/build.c`: an unreadable `--base` is T0002;
  - re-accept the golden outputs whose messages changed (codes, spans and fixes unchanged; compare with
    `git diff` that only message texts moved).

## Phase 10: Polish & Cross-Cutting Concerns

- [X] T035 `docs/reference.md` (R14):
  - the ordered-types line (`bool`, variants);
  - the changed std names in the library table and bullets;
  - `[]` from context;
  - the `from_`/`parse_` rule;
  - time limits (one line in the `std.tcp`/`std.http` bullets);
  - E0214 in the code ranges if needed.

  Run the token check after writing, then move detail the site already has until the total is at or
  under 15,000 with at least 10 tokens of headroom. Record before and after in results "Reference cost".
- [X] T036 [P] `docs/errors.md`: update the E0203, E0105, E0119, E0306, E0329 and E0411 entries for
  their new cases. Update E0311's entry (`key` is ordered). `python3 tests/catalog.py`,
  `python3 tests/apply_fixes.py` (fix coverage at least 80%).
- [X] T037 [P] `site/docs/guide/standard-library.md`: a short "Names" note on why some names stay apart
  (FR-023: no overloading, so per-type names in `std.collections`, `decimal.total`/`absolute`;
  `with_cookie` validates, `with_header` does not). Then the `site/docs/guide/` pages naming changed
  items (search the old names): `standard-library.md`,
  `apis.md`, `operating.md`, `collections.md`, `concurrency.md`, `workers.md`, `postgres.md`. Also
  `examples/*/README.md`. `make site-check`.
- [X] T038 [P] `docs/stability.md`: the version moves to 0.2.0 with the migration note. `src/` version
  string 0.2.0 (`vo version`); re-accept the version cases.
- [X] T039 `CHANGELOG.md` 0.2.0 section: every table row with its migration, the language changes,
  every changed error text (T027), `http.send`'s 0. `docs/roadmap.md`: spec 056 row and section.
  `docs/review-2026-11/inconsistencies.md`: each section 3 item marked fixed, declined or deferred, with
  its reason (SC-006).
- [X] T040 Three architectures: `tests/cases/run/order_bool_variant.vo`, `tests/cases/library/` and
  `tests/migrate_056.py --quick` on the ARM64, RISC-V and instrumented builds (`make ARCH=aarch64`,
  `make ARCH=riscv64`, `make instrumented`); identical outputs (SC-005).
- [X] T041 `make test` once. Fill `specs/056-consistency-pass/results.md` (each SC with its measurement,
  "Reference cost", "Deviations"). Mark the spec's Status implemented and every task `[X]`.

## Dependencies & Execution Order

- **Setup (T001–T003)** → **Foundational (T004–T008)** → stories. T001 must run before any std change
  (it records the pre-spec behavior).
- **Within each story**: the std change and the repository migration go together (a std change breaks
  its users until migrated). Run `make quick` after each story.
- **US1 before US2** for `http.header` (renamed in US2, but the table row lands with US1's mechanism) and
  `tcp.describe`/`tcp_error_text` (T010 before T026).
- **US3** touches only the checker and the migration; it can go at any point after Foundational, but its
  migration renames many locals, so it is best done before US5's large migrations to avoid conflicts.
- **US4** is independent (checker and runtime).
- **US5** tasks are independent of each other except T021 and T025, which both touch `std/http.vo`.
- **US6** after US1 to US5: it proves the sum.
- **US7** is independent; T034 (messages) last before Polish, since it re-accepts many outputs.
- **Polish** last; T035 after every name change.

## Parallel Opportunities

- The case tasks marked [P] are separate files.
- US4 (checker and runtime) beside US1 to US3 (library and migration).
- In US5: T022, T023, T024 and T026 touch different modules.
- In US7: T030 to T033 touch different files.
- T036 to T038 are separate documents.

Example (US7): T030 (check_expr), T031 (parse), T032 (lex), T033 (fmt) together, then T034.

## Implementation Strategy

- **MVP**: Setup, Foundational, US1: the change mechanism and the clashes, migrated.
- Then US2 and US3 (the other user decisions), US4 (order), US5 (conventions), US6 (the proof), US7
  (corners), Polish.
- `make quick` after each task; `make test` once at T041.

---

description: "Task list for vo-lang spec 058: Module Imports"
---

# Tasks: Module Imports

**Input**: Design documents from `specs/058-module-imports/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R14), data-model.md, contracts/syntax.md,
contracts/diagnostics.md, contracts/tools.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's golden cases land first and fail before its code.

**Organization**:
- Setup: test wiring, the results file, the codes table.
- Foundational: qualified names parsed (R1); imports bound as modules, the 0.3 form still binding (R2, R3).
- US1 (P1) one import per module; US2 (P1) existing programs migrate by fixes alone; US3 (P2) tools see
  qualified names.
- Polish: documentation, version, three platforms, the gate, the experiment kits.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: single-file cases `tests/cases/imports/<name>.vo` with `.cmd` (`check --json {}` for
  diagnostics; none to run), `.out`, `.code`; multi-file cases in a folder with `main.vo` and `main.cmd`,
  as `tests/cases/multi/missing_import/`. Accept with `tests/run.sh --accept PATH` only after reading the
  diff.
- **Catalog**: each new or widened code gets its entry or extra example in `docs/errors.md` (a broken and
  a fixed example), a row in `specs/058-module-imports/contracts/codes.md`, and its place in
  `tests/catalog.py`'s spec list, in the task that introduces it. Messages: `contracts/diagnostics.md`.
- **Fixes**: every fix's result must check clean: `python3 tests/apply_fixes.py` after each diagnostic task.
- **Old form keeps working until US2 is done**: per-name imports still bind their item (research R3), so
  `std/`, packages, examples and the existing 690 golden sources pass unchanged through Phases 2 and 3,
  except for the E0215 diagnostic, which is introduced in T020 and is the first task allowed to change
  existing expected outputs.
- **Reference budget**: `python3 tests/ref_examples.py` after every reference change; at most 15,000
  tokens (14,987 at the start).
- **Between edits**: `make quick AREA=imports` (T001); `make test` once per change; never two suites at
  once.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: a new area `imports` (cases `imports imports_v03 multi packages library check
  parse fmt query edit doc effect_sets`; scripts `apply_fixes.py`, `catalog.py`, `ref_examples.py`,
  `fmt_corpus.py`, `migrate_058.py --quick`), listed among the known areas in the "unknown area" message;
  `src/loader.c` maps to `imports`.
- [X] T002 [P] `specs/058-module-imports/results.md`: SC-001–SC-006 (pending), Verification, Deviations;
  `specs/058-module-imports/contracts/codes.md` in the format of spec 056's: E0215 and W0203 new; E0201,
  E0202, E0203, E0204, E0205, E0214 widened, with the messages of `contracts/diagnostics.md`.
- [X] T003 [P] Record the baseline for SC-001 and SC-006 in `results.md`: `grep -c '^import'` over
  `examples/*/*.vo` and `std/*.vo` (total and maximum per file; 48 today), and `vo check` times (best of
  5) of `examples/notes-api/main.vo`, `examples/secure-notes/main.vo` and the 8,000-item program of
  `tests/perf.sh`.

---

## Phase 2: Foundational (blocking)

- [X] T004 Golden cases first, `tests/cases/imports/parse_qualified/` (folder; `geometry.vo` exports
  `record Point`, `variant Shape { circle(float); empty; }`, `fn dist2`, `const data_dir = "data/";`,
  `effects Area = file.read("data/");`; `main.vo` uses each qualified in every position of
  contracts/syntax.md: a call, a function value passed to `map`, a constant in an expression and in a
  scope `file.read(geometry.unit_dir)`, a type in a parameter, in `list[...]`, in `result[..., ...]` and
  as a type argument, `new geometry.Point { ... }`, `geometry.Shape.circle(1.0)` and `geometry.Shape.empty`,
  `uses geometry.Area`); `vo run` prints the expected values. Fails today (E0205 for `import geometry;`).
- [X] T005 `src/parse.c`, `src/vo.h`: after the import lines of a file, collect the set of module names
  (last segment of each import whose last segment starts lowercase); in `parse_primary`, `parse_type`,
  the `new` record path (`parse_record_literal`), the `T_TYPE_IDENT` case path and the effect parser, fold
  `module_name "." (ident | TypeIdent)` into one name node whose `a.s` is `m.n`, span the whole text, flag
  `NF_QUAL` (new, documented in `vo.h`); `json.Json.number(...)` becomes an `N_VCASE` with variant
  `json.Json`; in `uses`, a lowercase second segment stays an effect (research R4). A name not in the set
  parses as today (field access).
- [X] T006 `src/loader.c`, `src/packages.c`: import path meaning of research R3: `import a.b.c;` loads
  `a/b/c.vo` (std: `std/c.vo`; package `a`: `DIR/b/c.vo`), one segment loads `NAME.vo` beside the importer;
  record on the `N_IMPORT` whether it is the module form or the 0.3 item form (`a/b/c.vo` missing, `a/b.vo`
  declaring `c`, or a capitalized last segment); `import std;` and a package name alone are E0205 "An
  import names a file: `import std.json;`." (contracts/diagnostics.md).
- [X] T007 `src/loader.c` (`bind_imports`), `src/check.c`, `src/vo.h`: `SYM_MODULE` (new kind) named `m`
  pointing to the file; for a module import, one symbol per top-level item of the target named `m.n`,
  private ones marked so a use reports E0209 (`check_exported`) instead of E0201; the 0.3 form binds the
  bare item exactly as today (E0214 path unchanged). Accept T004.
- [X] T008 [P] `tests/fuzz.py`: noise for imports and qualified names (`import ;`, `import a..b;`,
  `import a.b`, `m.`, `m.1`, `new m. {`, `m.Type.`, `uses m.`, `m.T[`), well formed and broken;
  `python3 tests/fuzz.py --iterations 200 --seed 1` passes.
- [X] T009 `make test`: no existing case changes (old form still binds); `tests/perf.sh` within its limits.

**Checkpoint**: the module form parses, binds and runs; nothing reports mistakes in it yet; old programs
are untouched.

---

## Phase 3: User Story 1 - One import per module (P1) 🎯 MVP

**Goal**: the module form works end to end with every mistake reported at its place, with a fix where
one correction exists.

**Independent Test**: quickstart steps 1 and 3; `examples/notes-api` rewritten by hand in a scratch copy
checks, runs and passes its tests.

- [X] T010 [P] [US1] Golden cases first, each `check --json`, in `tests/cases/imports/`:
  `bare_use/` (`import std.json;` then `parse_json(...)` and `Json` bare: E0201 "`parse_json` is in
  `std.json`: write `json.parse_json`." with the qualify fix, for a call, a value, a type and a case),
  `no_item/` (`json.nope`: E0201 "`std.json` has no item `nope`."; `json.parse_jsn`: the fix
  `json.parse_json`), `module_value/` (`print(json)`: E0201 "`json` is a module: use one of its items,
  `json.NAME`.", no fix), `builtin_qualified/` (`lists.len(xs)`: E0201 with the fix `len(xs)`),
  `private_item/` (`notes.helper`: E0209 with the fix adding `export`; `json.<private>` in std: E0209,
  no fix).
- [X] T011 [US1] `src/check_expr.c`, `src/check.c`: the E0201 cases of T010: on an undefined bare name,
  look for exactly one imported module of the file exporting it (fix: insert `m.` before the name); on an
  undefined `m.n` with `m` a module, the message naming the module path and the existing similar-name
  fix over that module's exported items; `SYM_MODULE` used as a value or type; a builtin written
  qualified. Catalog: second example under E0201. Accept T010.
- [X] T012 [P] [US1] Golden cases first: `tests/cases/imports/clash_modules/` (`import a.util;` and
  `import b.util;`: E0202 "`util` is already a module of this file (`a/util.vo`).", no fix),
  `clash_item/` (`fn json()` and `import std.json;`: E0202), `repeat/` (the same import twice: E0202 with
  the fix removing the second line), `shadow_local/` (with `import std.text;`: a `let text`, a parameter
  `text: str`, a `for text in`, a case binding `text` and a type parameter named like a module: E0203
  "`text` is the module `std.text` in this file: name the local differently." each, the fix renaming to
  `text_value`, and `text_value2` when `text_value` is taken, in the whole function as one edit),
  `bad_path/` (`import std;`, `import postgres;`: E0205).
- [X] T013 [US1] `src/check.c`, `src/loader.c`: E0202 for module/module and module/item clashes with the
  message above and the removal fix for an identical repeat; E0203 for `SYM_MODULE` through the existing
  shadowing check, with the one-edit rename fix: replacement from the declaration to the end of the
  enclosing function, every reference to that local renamed, found through the reference log
  (`q_note_sym`), never by text (research R4, R6). Catalog: second examples under E0202 and E0203. Accept
  T012.
- [X] T014 [P] [US1] Golden cases first: `tests/cases/imports/missing_module/` (`web.on_get` with no
  import: E0204 "`web` is not imported." with the fix `import std.web;` at `import_insert_at`; a sibling
  `notes.vo` used as `notes.add`: fix `import notes;`; a name both a std module and a sibling file: no
  fix), `missing_bare/` (a bare name declared in one sibling file: E0204 with the fix `import notes;`;
  after applying it, E0201 qualifies it), `unused/` (W0203 "`std.json` is imported but not used." with
  the fix removing the line; none with `@allow(W0203)`; none for a module used only in a type, a scope or
  `uses`).
- [X] T015 [US1] `src/loader.c` (`find_import`, `import_text_for`, `import_insert_at`), `src/check.c`:
  E0204 suggests module imports (`import_text_for` returns `import std.web;` / `import notes;`); the
  object of an `N_FIELD` that is an undefined identifier naming exactly one findable module reports E0204
  instead of E0201; W0203 (new) after the file is checked, for each module import with no use in the
  reference log. Catalog: W0203 entry, second example under E0204. Accept T014.
- [X] T016 [P] [US1] Golden cases first: `tests/cases/imports/type_names/` (a file importing `std.json`
  with mistakes whose message or fix prints a type of another module: E0301 `Expected json.Json, found
  str.`, E0306's fix `new list[json.Json]`, E0307's annotation fix, an effect-argument fix with
  `sqlite.Database[uses D]`; and a type reached only through another module's signature printed as
  `std.json.Json`); every fix checks clean.
- [X] T017 [US1] `src/check.c`, `src/vo.h`, and the fix builders that print types (`grep -n ty_name
  src/check*.c src/libchange.c`): `ty_name_in(Type *t, long file)` qualifying a record, variant or effect
  set declared in another file by the module name the file binds for it, else by its import path
  (research R7); messages and fixes in checked files use it; `vo query type-at` and hover too. Accept
  T016; `make quick AREA=imports,front`: no other case changes.
- [X] T018 [P] [US1] Golden cases first: `tests/cases/imports/packages/` (a package with two files
  importing each other as `import pkg.link;` then `link.deliver(...)`, used from `main.vo` as
  `import pkg.client;`; its functions in `vo.effects` as `@pkg/link.vo:deliver`, unchanged) and
  `tests/cases/imports/effect_sets/` (`uses console, api.Storage`, `file.read(api.data_dir)`,
  `effects Local = api.Storage, console;`, E0504's note naming the qualified set).
- [X] T019 [US1] `src/packages.c`, `src/check_effects.c`: whatever T018 shows missing (package module
  paths of T006 in `pkg_import_path`; effect-set lookup by qualified name in `fx_build_set`). Accept
  T018. Checkpoint: quickstart steps 1 and 3 pass by hand.

**Checkpoint**: US1 works alone: new programs use module imports; old ones still check unchanged.

---

## Phase 4: User Story 2 - Existing programs migrate by fixes alone (P1)

**Goal**: E0215 and its one-edit fix; the whole repository migrated by it.

**Independent Test**: quickstart step 2; `python3 tests/migrate_058.py`; `tests/migrate.py` and
`tests/migrate_056.py` pass.

- [X] T020 [P] [US2] Golden cases first, `tests/cases/imports_v03/` (old form kept on purpose):
  `single.vo` (`import std.json.parse_json;` + `import std.json.Json;` and their uses: one E0215 at the
  first import, message "Imports name a module, not an item: `import std.json;`, then `json.parse_json`.",
  the fix one edit from the first import to the last changed use), `mixed/` (sibling file, package and
  std per-name imports together, one already in module form), `renamed_056.vo` (an import of a 0.2 name
  that E0214 renames: the fix writes the new name qualified), `shadow.vo` (a parameter `text` with
  `import std.text.trim;`: the fix also renames the parameter to `text_value`), `text_output.cmd`
  (`check {}` without `--json`: `fix: rewrite lines 1 to N (module imports)`), plus 15 copies of
  representative existing cases from `tests/cases/{multi,library,web,sqlite,json}` in their 0.3 form.
- [X] T021 [US2] `src/loader.c`, `src/check.c`: E0215 (new code, catalog entry): one per file with 0.3
  imports, at the first; its fix built after the file is checked from the reference log: imports of each
  module replaced by one module import in `vo fmt`'s order, each use of an item bound by a 0.3 import
  qualified (types, cases, constants, effect sets included), each E0214 row that is a pure rename applied,
  each local named like a new module renamed (T013's rule); the edit spans from the first import to the
  end of the last changed use (research R6, data-model "Migration region").
- [X] T022 [US2] `src/libchange.c`, `src/check_expr.c`: E0214 at a qualified use (`text.old_name` with
  `import std.text;`): lookup of `m.n` failing for a std module consults `libchange_find(module, n)`; the
  fix replaces the item part. Golden case `tests/cases/library/qualified_change.vo`.
- [X] T023 [US2] `src/diag.c`: text output of a fix whose replacement has more than 3 lines: `fix: rewrite
  lines A to B (module imports)` (the parenthesis from a short label set by the diagnostic; generic
  diagnostics write `(N lines)`); JSON unchanged. Accept T020.
- [X] T024 [US2] `tools/vo_fix.vo`: 4 rounds instead of 3 (header comment too); then migrate the tool
  itself with it (`bin/vo run --timeout 0 tools/vo_fix.vo -- tools/vo_fix.vo`) and check it runs.
- [X] T025 [US2] Migrate `std/*.vo` with `tools/vo_fix.vo --only E0215,E0203,E0201` then `vo fmt`; doc
  comments with examples migrated by hand where the tool cannot reach them; `python3
  tests/doc_conformance.py` and `make quick AREA=docs,library` pass.
- [X] T026 [P] [US2] Migrate `packages/postgres/*.vo` the same way; re-pin it in every `vo.packages`
  that lists it (`vo package ENTRY postgres DIR`); `python3 tests/postgres.py` passes.
- [X] T027 [P] [US2] Migrate `examples/*` the same way; each example's tests and README commands give
  unchanged output; record per-file import counts in `results.md` (SC-001: at most 12 per file, at least
  70% fewer in total).
- [X] T028 [US2] `tests/migrate_058.py` (new): for every golden case under `tests/cases/` except
  `imports_v03/`: copy the source tree, apply fixes with `tools/vo_fix.vo` (4 rounds), run its `.cmd`,
  and compare with the old expected output after normalizing line and column numbers and replacing
  qualified names by their bare item names; any other difference is a problem. `--quick`: 60 cases. Then
  migrate the case sources in place, accept their new expected outputs, and run it again against the
  accepted outputs from git (`HEAD`) as the baseline.
- [X] T029 [US2] `tests/migrate.py`, `tests/migrate_056.py`: pass with the 4-round tool; frozen corpora
  untouched.
- [X] T030 [US2] `make test`. Checkpoint: the repository has no 0.3 import outside `tests/cases/imports_v03/`
  (`grep -rn '^import [a-z_.]*\.[A-Za-z_]*\.[A-Za-z_]*;' --include=*.vo` lists only module imports of
  nested paths, checked by eye).

**Checkpoint**: US2 works: every program in the repository is in the module form, reached by fixes alone.

---

## Phase 5: User Story 3 - Reviewers and tools see where a name comes from (P2)

**Goal**: query, edit, doc, lsp and the editor grammars understand qualified names.

**Independent Test**: quickstart step 5; the existing query/edit/doc/lsp cases give the same answers
after migration (SC-004).

- [X] T031 [P] [US3] Golden cases first: `tests/cases/query/qualified/` (`vo query refs main.vo
  notes.add_note`: declaration, the `import notes;` line with role `import`, each qualified call with role
  `call`, a value use with role `value`; `vo query callers`; `vo query refs main.vo notes.Note.text`;
  `vo query refs main.vo json.Json.number`; an ambiguous module name: T0005), `tests/cases/edit/
  rename_qualified/` (`vo edit rename main.vo notes.add_note add`: declaration and every `notes.add_note`
  → `notes.add`; a clash refused with T0006; all files or none).
- [X] T032 [US3] `src/query.c`: TARGET parsing of `module.name`, `module.Record.field`,
  `module.Variant.case` (lowercase first segment = module, matched against the modules the program's
  files bind); references of a module import counted as role `import` for each item used through it.
- [X] T033 [US3] `src/edit.c`: rename replaces only the item part of an `NF_QUAL` node (from
  `start + len(module) + 1`); `add-field` on a qualified record. Accept T031.
- [X] T034 [P] [US3] Golden cases first: `tests/cases/doc/use_line.cmd` (`doc std.json.parse_json`:
  the declaration plus `use: import std.json; then json.parse_json`; `--json` with `"use"`), and LSP
  step cases in `tests/lsp.py` (hover on `web` of `web.on_get`: `std.web` and its first comment
  paragraph; definition: `std/web.vo` line 1 via the embedded-file URI the server uses today; references
  on the module part; rename on the module part refused with "rename the file instead").
- [X] T035 [US3] `src/doc.c`: the `use:` line and the `use` JSON field (schema 1: a field added; record
  it in the reference's machine-readable section). `src/lsp.c`: module-part hover, definition,
  references and rename refusal, by position against the `NF_QUAL` node's module part. Accept T034.
- [X] T036 [P] [US3] `editors/vscode`, `editors/tree-sitter-vo`, `editors/vim`: qualified types
  (`ident.TypeIdent`), qualified calls and values, `new m.T {`, `m.V.case`, `uses m.Set`; regenerate the
  tree-sitter parser; `node editors/check.js` passes over every `.vo` file of the migrated repository.
- [X] T037 [P] [US3] `src/fmt.c`: confirm qualified names print from `a.s` and imports sort by text;
  `tests/cases/fmt/qualified.vo` with `qualified.expected.vo` (unsorted module imports, long qualified
  calls wrapped at 100 columns); `python3 tests/fmt_corpus.py` passes.

**Checkpoint**: all three stories work.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T038 `docs/reference.md`: "Programs in several files" and "Standard library and packages" describe
  the module form (contracts/syntax.md's table in one sentence each), drop "Write one `import` per name",
  add the module/local rule (E0203) in one sentence; the import examples migrated (7 lines become 5);
  `python3 tests/ref_examples.py` ≤ 15,000 tokens; if over, move the E0204-fix sentence and the E0209 detail
  to `site/docs/guide/` (research R12). Record before/after in `results.md` (SC-005).
- [X] T039 [P] `site/docs/` (17 pages with imports), `README.md` (its example and any import text),
  `docs/roadmap.md` (058 row), `CONTRIBUTING.md` if it shows imports: migrated and described;
  `python3 tests/site_examples.py` passes.
- [X] T040 [P] `docs/stability.md` (migrations "done four times", 0.4.0's module imports, `migrate_058.py`
  among the kept tests), `CHANGELOG.md` (0.4.0: the change, E0215 and W0203, the migration command),
  version 0.4.0 where `vo version` takes it.
- [X] T041 `python3 tests/catalog.py`, `python3 tests/apply_fixes.py` (fix coverage not lower than
  before), `python3 tests/fuzz.py --iterations 1000 --seed 7`, the determinism check: all pass.
- [X] T042 Measure SC-006: `vo check` times of T003's programs (best of 5), within 5%; record in
  `results.md`.
- [X] T043 `make test` on x86-64, then `make test` for ARM64 and RISC-V through `bin/vo` with qemu (as
  for spec 057); `make gate`; fill `results.md` (every SC with its evidence, Verification, Deviations).
- [ ] T044 With the user: migrate the voledger and vositegen kits (`~/dev/priv/voledger`,
  `~/dev/priv/vositegen`, their `imports/` too) with `tools/vo_fix.vo` the day `build/vo` changes, and
  note it in `results.md`.

---

## Dependencies & Execution Order

- **Setup (T001–T003)**: first; T002 and T003 in parallel.
- **Foundational (T004–T009)**: T004 → T005 → T006 → T007 → T009; T008 in parallel after T005. Blocks every
  story.
- **US1 (T010–T019)**: after Phase 2. Pairs (case → code) in order: T010→T011, T012→T013, T014→T015,
  T016→T017, T018→T019; the case tasks marked [P] can be written together first; T017 before T021 (the
  migration fix prints qualified types).
- **US2 (T020–T030)**: after T011, T013 and T017 (its fix qualifies uses, renames locals, prints types).
  T020 → T021 → T022 → T023 → T024 → T025 → (T026, T027 in parallel) → T028 → T029 → T030.
- **US3 (T031–T037)**: after Phase 2; T032–T035 independent of US2, but their "unchanged answers" check
  (SC-004) runs after T028. T036 after T030 (the grammar check runs over migrated files).
- **Polish (T038–T044)**: after the stories; T039 and T040 in parallel; T043 after everything else;
  T044 last.

## Parallel Example: User Story 1

```text
T010 bare_use/no_item/... cases   T012 clash/shadow cases   T014 missing/unused cases
T016 type_names case              T018 packages/effect_sets cases
→ then T011, T013, T015, T017, T019 one at a time (they share src/check.c and src/loader.c)
```

## Implementation Strategy

- **MVP**: Phases 1–3. New programs can use module imports with every mistake reported; nothing old breaks.
  Stop and validate with quickstart steps 1 and 3.
- **Then US2**: the breaking half. Land T021–T024 before migrating anything, migrate std and packages
  before examples and golden cases, and keep `build/vo` consistent for the experiment kits (rebuild only
  with a complete change).
- **Then US3 and Polish**, finishing with the three platforms, the gate and `results.md`.

---

description: "Task list for vo-lang spec 031: Constants"
---

# Tasks: Constants

**Input**: Design documents from `specs/031-constants/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R13), data-model.md, contracts/syntax.md,
contracts/diagnostics.md, contracts/tools.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's golden cases land first and fail before its code.

**Organization**:
- Setup: test wiring and the results file.
- Foundational: the integer helpers shared with the VM, the `const` item parsed and registered as a symbol.
- US1 (P1) values worked out and uses folded; US2 (P1) mistakes; US3 (P1) files; US4 (P2) literal
  positions; US5 (P2) review and tools; US6 (P3) canonical form, documentation, examples.
- Polish: equivalence and record checks, three platforms, the gate, the agent run (with the user).

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: `tests/cases/const/<name>.vo` (`.cmd` `check --json {}` for diagnostics, run
  otherwise; multi-file cases in a folder with `main.vo`), `tests/cases/fmt/<name>.vo` with
  `<name>.expected.vo`, `tests/cases/query/<name>`; accept with `tests/run.sh --accept PATH` after reading
  the diff.
- **Catalog**: each new code gets its entry in `docs/errors.md` (a broken and a fixed example), a row in
  `specs/031-constants/contracts/codes.md` and its place in `tests/catalog.py`'s spec list, in the task
  that introduces it. Message texts: `contracts/diagnostics.md`; reworded messages keep the sentence shape
  the code has today (for example "The template of `format` must be a string literal or a constant, so
  its placeholders can be checked.").
- **Reference budget**: `python3 tests/ref_examples.py` after every reference change; at most +450 tokens
  in all (from about 14,017).
- **Existing programs**: no golden case outside `tests/cases/const/` may change, except `doc/all*` (the
  builtin and item listings) and the cases this list names (SC-002).
- **Between edits**: `make quick AREA=const` (T001); `make test` once per change; never two suites at once.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: a new area `const` (cases `const`, `check`, `effects`, `effect_sets`, `fmt`,
  `query`, `annotations`, `format`, `regex`, `sqlite`, `web`; scripts `apply_fixes.py`, `catalog.py`,
  `fmt_corpus.py`, `ref_examples.py`) and the mapping `src/check_const.c` → `const`, listed among the known
  areas.
- [X] T002 [P] `specs/031-constants/results.md`: SC-001–SC-008 (pending), Verification, Deviations;
  `specs/031-constants/contracts/codes.md` in the format of spec 030's (E0122, E0212, E0213, E0332, E0333,
  W0202 new; E0208, E0315, E0321, E0324, E0330, E0506 changed wording).

---

## Phase 2: Foundational (blocking)

- [X] T003 `src/vo.h`, `src/vm.c`: move the integer rules of the VM into inline helpers in `vo.h`
  (`int_add`, `int_sub`, `int_mul`, `int_neg` returning whether they overflow; `int_div`, `int_rem` with
  the `MIN / -1` and zero rules of `div_rv`/`rem_rv`) and use them in `op_add_int` … `op_neg_int` and the
  `_k` variants; no behaviour change: `make test` and `python3 tests/diff_engines.py --count 1000 --seed 4`
  unchanged, `tests/perf.sh` within its limits (research R3).
- [X] T004 Golden case first, `tests/cases/const/declare.vo`: `const max_items = 100;`, `const rate: float
  = 0.25;`, `export const out_dir = "out/";`, a constant declared after the function that will use it, and
  a function with a local variable and a call named `const` elsewhere in the file (`const` is a word only
  where an item starts, research R1); `vo check` clean, with W0202 for the private ones (accepted again
  in T012 when uses exist).
- [X] T005 `src/parse.c`, `src/vo.h`: the item `["export"] "const" name [":" type] "=" expr ";"` as
  `N_CONST` (A name node, B type node or 0, C expression; `NF_EXPORT`), recognized by the text `const`
  only at the start of an item, as `effects` is; add `N_CONST` to the node-kind comments in `vo.h`.
- [X] T006 `src/check.c`, `src/vo.h`: `SYM_CONST`; `define_item` registers a constant like other items
  (E0202 for a clash with an item, an import or a builtin; E0207 with the `snake_case` name as the fix),
  and the items loop that resolves fields and cases skips `N_CONST` as it skips `N_EFFECTS`.
- [X] T007 [P] `tests/fuzz.py`: noise for the item (`const a = `, `const a: int = 1;`, `export const`,
  `const A = 1 +;`, `const a = a;`), well formed and broken; `python3 tests/fuzz.py --iterations 200
  --seed 1` passes (no crash, every exit documented).

**Checkpoint**: a file with constants parses and its names are symbols; nothing can use them yet.

---

## Phase 3: User Story 1 - A value used in several places is named once (P1) 🎯 MVP

**Goal**: constants have values and can be used in every expression.

**Independent Test**: the program of contracts/syntax.md prints `100 0.25 out/report.txt true`.

- [X] T008 [P] [US1] Golden cases first, `tests/cases/const/use.vo` (the contracts/syntax.md example
  without the scope: every constant used in two functions and a test, a constant declared after its use,
  constants in `requires`, in an anonymous function and in a `prop`), `use_ops.vo` (each operator of the
  constexpr grammar, `7 / 2`, `-7 % 3`, float `1.0 / 0.0` printing `inf`, `str` joined with `+`, `str`
  compared with `<`, `and`/`or`/`not`, nested parentheses, a chain of ten constants), `use_typed.vo`
  (`const rate: float = 0.25;` and E0301 for `const n: int = 0.5;` with `expected`/`found`), `use_pure.vo`
  (a function without `uses` that reads constants checks clean; `vo effects` shows it pure).
- [X] T009 [US1] `src/check_const.c` (new), `src/vo.h`: `const_fold(node)`: folds a subtree whose leaves
  are literals into one literal node in place and returns whether it did; `int` through T003's helpers
  (overflow or division by zero: no fold, returns a code the caller may report), `float` with C `double`
  operations, `bool`, `str` concatenation allocated in the permanent arena, comparisons with `str_cmp`
  and the numeric operators; the node keeps its span and its `type`.
- [X] T010 [US1] `src/check_const.c`, `src/check.c`: `const_eval(decl)` with the three states "not
  started, in progress, done" on the `N_CONST` (data-model.md): checks C with the expression checker and
  the stated type with `expect_type` (E0301), folds it, and records the value's type on the symbol;
  `check_items` evaluates every constant of the file after `define_item` and before `fx_expand_sets`
  (research R4).
- [X] T011 [US1] `src/check_expr.c`, `src/vo.h`: `check_ident_as` on a `SYM_CONST`: evaluate it on demand,
  log the reference for `vo query` (`q_note_*`) and mark the symbol used, then rewrite the identifier node
  in place into the value's literal node with its own span, `NF_CONST` set and the declaration in a field
  the literal kinds do not use (research R2); the compiler then emits `OPC_LOADK` with no change in
  `src/compile.c`.
- [X] T012 [US1] Accept T004 and T008; `tests/cases/const/unchanged.vo` plus `python3 tests/gc/baseline.py`
  (731 of 731) and `make test`: no existing case changes (SC-002).

**Checkpoint**: US1 works alone: constants in expressions, one file.

---

## Phase 4: User Story 2 - A mistake in a constant is reported where it is written (P1)

**Goal**: every invalid constant is a check error at the constant, in one pass, without cascades.

**Independent Test**: quickstart step 2.

- [X] T013 [P] [US2] Golden cases first, each `check --json`: `tests/cases/const/err_call.vo` (E0212 for a
  user function, a builtin `len`, `str(1)`, `todo("x")`, `new`, a list literal, an index, a field, `try`,
  an anonymous function; and for a name that is a function, with the three wordings of
  contracts/diagnostics.md), `err_cycle.vo` (E0213: two constants, three constants, a constant defined by
  itself; the path in the message and related notes; their uses report nothing), `err_type.vo` (E0332 for
  a value of type `list[int]` via a stated type and for a stated type `list[int]`), `err_arith.vo` (E0333:
  `9223372036854775807 + 1`, `-9223372036854775807 - 2`, `10 / 0`, `10 % 0`, the smallest int divided by
  `-1`, unary minus of the smallest int), `err_set.vo` (E0208 "`max_items` is a constant and cannot be
  changed.", no fix), `err_names.vo` (E0207 for `MaxItems` and `MAX_ITEMS` with fixes at the declaration
  and each use; E0202 for a constant named like a function and like `len`; E0203 for a local, a parameter,
  a `for` variable and a case binding named like a constant), `err_unused.vo` (W0202 with its fix; none
  for an exported constant or one used only in another constant), `err_statement.vo` (E0122 for `const x
  = 1;` in a function body with the fix `let`; the rest of the body still checked; a call statement
  `const(1);` of a user function named `const` is not an error), `err_all.vo` (one of each in one file:
  all reported in one pass, SC-004).
- [X] T014 [US2] `src/check_const.c`: the constant mode of the expression check: E0212 at a call, a
  builtin, a hole, `new`, a list, an index, a field, `try`, an anonymous function, or a name that is not a
  constant; E0332 when the stated or resulting type is not `int`, `float`, `bool` or `str`; E0333 from
  `const_fold`'s overflow and division codes at the operator's expression; a constant in error gets the
  error type so its uses are silent (FR-010).
- [X] T015 [US2] `src/check_const.c`: E0213 when `const_eval` reaches a constant that is in progress:
  reported once at the first constant of the cycle in file order, with the path and a related note at
  each other member; every member becomes an error value.
- [X] T016 [US2] `src/check.c`: `check_set` gives E0208 with the constant wording and no fix when the
  place names a `SYM_CONST`; `new_local` and the other binders already give E0203 through `scope_find`
  (add the related note at the constant).
- [X] T017 [US2] `src/parse.c`: a statement that starts with the identifier `const` followed by an
  identifier is E0122 at the word, with the fix replacing it by `let`; the statement is then parsed as a
  `let` (research R7).
- [X] T018 [US2] `src/check.c`, `src/diag.c`: W0202 for a constant that is not exported and has no
  reference in the program after the bodies are checked, with a fix that removes the declaration and the
  comment lines directly above it (as W0505 does).
- [X] T019 [US2] `docs/errors.md`, `tests/catalog.py`, `specs/031-constants/contracts/codes.md`: entries
  for E0122, E0212, E0213, E0332, E0333, W0202 (each a broken and a fixed example that the catalog check
  runs) and the E0208 entry's third case; `python3 tests/catalog.py` and `python3 tests/apply_fixes.py`
  pass with fix coverage not below 92% of fixable codes (SC-004).

**Checkpoint**: US1 and US2: the feature is safe to use in one file.

---

## Phase 5: User Story 3 - Constants cross files like other items (P1)

**Goal**: `export const` and `import file.name;`.

**Independent Test**: quickstart step 3.

- [X] T020 [P] [US3] Golden cases first: `tests/cases/const/files/` (`config.vo` exporting two constants
  and keeping one private, one of them defined from the other; `main.vo` importing and using them),
  `files_private/` (E0209 with the fix adding `export`), `files_missing/` (E0204 with the fix adding the
  `import` line; E0205 for a name the file does not have), `files_dup/` (two files exporting the same name
  imported into a third: E0202 at the second import), `files_chain/` (a constant defined from an imported
  constant defined from another file's), and a package case under `tests/cases/packages/` exporting a
  constant.
- [X] T021 [US3] `src/loader.c`: `bind_imports` carries a `SYM_CONST` like other kinds (its value is its
  declaration's folded expression: no copy needed); `file_defines` recognizes `const NAME` at brace depth
  0 so E0204 offers the import; `check_exported` and its E0209 fix apply unchanged (research R8).
- [X] T022 [US3] `src/doc.c`: `vo doc ENTRY`, `vo doc ENTRY NAME` and `vo doc NAME.module` list exported
  constants after effect sets as `const name: type = value` with their comment; `--json` records with
  `"item":"const"` (contracts/tools.md); golden cases `tests/cases/doc/const*` first.

**Checkpoint**: all P1 stories: constants work across a program.

---

## Phase 6: User Story 4 - A constant can stand where the checker reads a literal (P2)

**Goal**: one rule, "a literal or a constant expression", at every literal position.

**Independent Test**: quickstart step 4.

- [X] T023 [P] [US4] Golden cases first, scopes: `tests/cases/const/scope.vo` (`uses file.write(out_dir)`
  with `write_file(out_dir + "report.txt", ...)` runs; `vo effects` shows `file.write("out/")`),
  `scope_set.vo` (`effects Storage = file.read(data_dir), file.write(data_dir);`), `scope_expr.vo` (a
  scope written `base + "out/"`), `scope_bad.vo` (E0506 for a constant whose value is `""`, `"/abs"` and
  `"a/../b"`, ending "(`NAME` is `"VALUE"`)"; E0506 "A scope is a string literal or a constant, found
  `x`." for a parameter; E0301 for an `int` constant and for an `int` expression written in a scope), `scope_target.vo` (E0503 for
  `write_file(tmp_dir + "x", ...)` under `uses file.write(out_dir)`, and none when it is inside), and a
  step case `tests/cases/const/scope_runtime/` (R0010 at run time still names the scope's value).
- [X] T024 [US4] `src/parse.c`: `parse_effect` parses an expression for each scope where it required a
  string token (a string literal still gives the same `N_STR` node, so existing programs parse
  identically).
- [X] T025 [US4] `src/check_effects.c`: `fx_build_set` checks and folds each scope with `const_fold`
  before reading it (not a `str` literal afterwards: E0506 with the new wording, or E0301 for another
  type); the "(`NAME` is ...)" suffix when the node has `NF_CONST` or was folded; `fx_literal_at` folds
  the target argument before its `N_STR` test, so E0503 covers constants and constant expressions.
- [X] T026 [P] [US4] Golden cases first, annotations: `tests/cases/const/annot.vo` (`@size(1,
  max_name)`, `@range(0, max_items)`, `@range(0.0, top_rate)`, `@pattern(code_pattern)`,
  `@default(default_port)`, `@header(trace_header)`; `from_json` accepts and rejects at the constants'
  values) and `annot_bad.vo` (E0329 for a constant of the wrong type; E0212-style E0329 for a parameter
  name); `@spec` and `@allow` with a constant stay the errors they are today.
- [X] T027 [US4] `src/parse.c`, `src/check_annot.c`: field annotations (the `T_AT` branch on the line
  above a record field) parse their arguments as expressions; `check_annot.c` folds each before reading
  its kind; item annotations (`@spec`, `@allow`) keep the string-token loop.
- [X] T028 [P] [US4] Golden cases first, templates: `tests/cases/const/template.vo` (`format(row, a, b)`,
  `regex(code_pattern)`, `sql(insert_note)`, `on_get(note_path, show)` with `str` constants behave as
  with the literals) and `template_bad.vo` (E0316, E0317, E0319, E0323, E0330 inside a constant's value:
  reported on the whole use, the value quoted, no fix; a written literal keeps its exact span and fix;
  E0315, E0321, E0324, E0330 for a parameter, with the new wording).
- [X] T029 [US4] `src/check_format.c`, `src/check_regex.c`, `src/check_sqlite.c`, `src/check_web.c`: fold
  the argument with `const_fold` before the `N_STR` test; when the node has `NF_CONST` or was folded,
  report problems inside the text on the whole node with the value in the message and without fixes
  (research R5); reword the four "must be a string literal" messages to "a string literal or a constant".
- [X] T030 [US4] `tests/const_equiv.py` (new): 1,000 generated programs with literals in expressions,
  scopes, annotations, templates and E0503 targets, and the same programs with each literal replaced by a
  constant or a two-part constant expression of the same value; `vo check --json`, `vo run` and `vo
  effects` compared byte for byte, spans of diagnostics inside template text excepted as R5 states;
  wired into `make test` and the `const` quick area (SC-005).

**Checkpoint**: a directory or a limit is written once and used as scope, path and annotation.

---

## Phase 7: User Story 5 - Reviewing, querying and editing know constants (P2)

**Goal**: values in every effects report; `vo query`, `vo edit`, `vo impact`, test reports and mutation
testing handle constants.

**Independent Test**: quickstart steps 5 and 6.

- [X] T031 [P] [US5] Golden cases first: `tests/cases/const/record/` (a step case: `vo effects --update`,
  then the literal `"out/"` replaced by a constant: record byte-identical and `vo check` clean; then the
  constant changed to `"./"`: E0504 for each function using it with a related note "`out_dir` is `"./"`
  here." at the declaration; then to `"out/reports/"`: W0502).
- [X] T032 [US5] `src/check_effects.c`, `src/effects_record.c`: the related note on E0504 and W0502 when
  the changed scope's node has `NF_CONST` (research R10); no change to the record or the comparison.
- [X] T033 [P] [US5] Golden cases first, `tests/cases/query/const_refs`, `const_type_at`,
  `const_type_at_json`, `const_callers`, `const_rename` (dry run and a step case that writes two files),
  `const_rename_clash` (T0006), `const_impact` (a diff that changes a constant used by two functions, one
  of them through another constant): the outputs of contracts/tools.md.
- [X] T034 [US5] `src/query.c`: `N_CONST` in `QK_ITEMS`; references to constants in expressions, scopes,
  annotations and imports listed with role `use` and their enclosing function, record or set; `type-at`
  on a use or a declaration prints `name: type = value (declared at ...)` and its documentation, with the
  optional `"value"` field in JSON; `callers` on a constant keeps the existing "is not a function" error.
- [X] T035 [US5] `src/edit.c`: `vo edit rename` on a constant rewrites the declaration, imports and every
  logged reference, checks the result and writes all files or none (the path functions take).
- [X] T036 [US5] `src/query.c`: `vo impact`: a changed line inside an `N_CONST` marks every function, test
  and property that references the constant, directly or through constants defined from it, with the
  reason `uses NAME`.
- [X] T037 [P] [US5] Golden cases first: `tests/cases/test/const_operands` (a failing `expect(len(xs) <=
  max_items)` lists `max_items: 100`; `expect_eq(total, max_items)` unchanged in shape),
  `tests/cases/mutate/const` (no mutant inside a constant's declaration or at a use), a W0603 case for a
  check made only of constants.
- [X] T038 [US5] `src/rt_test.c`, `src/compile.c`, `src/mutate.c`, `src/check_structure.c`: an operand
  with `NF_CONST` is listed with its source text and value where written literals are left out; `mutate.c`
  skips nodes with `NF_CONST` and `N_CONST` items; W0603 treats `NF_CONST` literals as literals.

---

## Phase 8: User Story 6 - One canonical form, documented where agents read (P3)

**Goal**: `vo fmt`, the reference, the guide and the examples.

**Independent Test**: quickstart step 7; `ref_examples.py` and `site_examples.py` pass.

- [X] T039 [P] [US6] Golden cases first, `tests/cases/fmt/const.vo` with `const.expected.vo`: odd spacing,
  `export  const`, a typed constant, a comment above kept as documentation, consecutive constants with
  and without a blank line between them, a constant between two functions, an expression over 100
  characters, scopes `file.write(out_dir, "a/")` reordered to `file.write("a/", out_dir)`; formats to
  itself.
- [X] T040 [US6] `src/fmt.c`: the `N_CONST` item (`[export ]const name[: type] = expr;`, the statement
  rule over 100 characters), the blank-line rule with `N_IMPORT` and `N_EFFECTS` (consecutive constants
  keep at most one blank line), scopes printed from their expressions and sorted by source text;
  `python3 tests/fmt_corpus.py` unchanged for the existing corpus.
- [X] T041 [US6] `docs/reference.md`: a "Constants" subsection under Declarations (where they are
  declared, what a value may be, that they are folded at check time, one runnable example with a scope
  and a path), one sentence each in Effects (scopes), Records (annotations) and the `format` paragraph,
  "no global variables" for "no global variables or constants", `const` in the item list of
  Declarations; at most +450 tokens, checked with `python3 tests/ref_examples.py` (SC-007).
- [X] T042 [P] [US6] `site/docs/guide/`: a section on constants in the chapter that introduces
  declarations (when to use one and when a function, FR-021) and a paragraph in the effects chapter
  (a scope and a path from one constant; how a change shows in `vo.effects`), every example run by
  `python3 tests/site_examples.py`; `site/pygments_vo/pygments_vo.py` highlights `const` at the start of
  an item.
- [X] T043 [P] [US6] `examples/expenses`, `examples/notes-api`, `examples/helpdesk`,
  `examples/release-check`, `examples/status-board`: each directory that is both an effect scope and a
  path becomes a constant; each literal with one meaning repeated three or more times becomes a constant;
  `vo fmt` clean; READMEs mention it where they explain the effects.
- [X] T044 [US6] `tests/const_records.py` (new): for every example, output, exit status and `vo.effects`
  are byte-identical between the commit before T043 and after (SC-001, SC-003), and changing one
  example's directory constant makes `vo check` report E0504 for the functions it reaches; wired into
  `make test`.
- [X] T045 [P] [US6] `docs/roadmap.md`, `README.md`: spec 031 in the status table and the "Next specs"
  notes (what was left out on purpose: calls in values, lists and records, constants inside functions);
  the README's feature list and status paragraph; `docs/review-2026-11/README.md`'s table.

---

## Phase 9: Polish & cross-cutting

- [ ] T046 `tests/perf.sh`, `tests/structure_speed.py`: check time of the 5,000-line program within 5%
  of the figure before T005, and a loop reading a constant no slower than the literal (SC-006); figures
  in `specs/031-constants/results.md`.
- [ ] T047 `make test`, then `make ARCH=aarch64` and `make ARCH=riscv64` with `tests/run.sh
  tests/cases/const tests/cases/fmt tests/cases/query` under each (byte-identical outputs), then `make
  gate`.
- [X] T048 `specs/031-constants/results.md`: SC-001–SC-007 with their measurements, Verification (the
  quickstart's nine steps), Deviations; `specs/031-constants/spec.md` status; SC-008's agent run
  scheduled with the user (benchmark v2's isolated protocol), recorded as pending.

---

## Dependencies & Execution Order

- Setup (T001–T002) → Foundational (T003–T007) → US1 (T008–T012) → US2 (T013–T019).
- US3 (T020–T022) needs US1; it does not need US2.
- US4 (T023–T030) needs US1 and US2 (E0333 and the error value in literal positions); its three parts
  (scopes T023–T025, annotations T026–T027, templates T028–T029) are independent of each other; T030
  needs all three.
- US5 needs US1; T031–T032 also need US4's scopes; T033–T036 are independent of T037–T038.
- US6: T039–T040 need only the Foundational phase; T041–T045 need US1–US4.
- Polish needs everything.

## Parallel opportunities

- T002 with T001; T007 with T005–T006.
- The golden-case tasks marked [P] (T008, T013, T020, T023, T026, T028, T031, T033, T037, T039) touch
  only new files and can be written ahead of their code.
- Within US4, the scope, annotation and template pairs touch different source files.
- T042, T043 and T045 are documentation and examples in different directories.

## Implementation Strategy

- **MVP**: Phases 1–3 (T001–T012): constants in expressions in one file. Useful alone, and no existing
  program changes.
- **Safe to release**: add US2 and US3 (T013–T022): every mistake reported, and constants across files.
- **The differentiator**: US4 (T023–T030): one definition for a scope and its path.
- **Then** US5 and US6, each independently testable, and the Polish phase before `results.md`.
- Commit after each task or small group; `make quick AREA=const` between edits, `make test` once per
  change, `make gate` in T047.

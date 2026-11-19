---

description: "Task list for vo-lang spec 023: Documentation on hover"
---

# Tasks: Documentation on hover

**Input**: Design documents from `specs/023-hover-docs/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R7), data-model.md, contracts/cli.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Cases land first and fail before each piece.

**Organization**:
- Setup: a fixture program.
- Foundational: the shared record builder in `src/doc.c`, which every story needs.
- One phase per user story: US1 is P1, US2 and US3 are P2, US4 is P3.
- Docs and verification.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases** go in `tests/cases/query/` (`hover_*`, each a `.cmd` such as
  `query type-at --json {dir}/hover/main.vo {dir}/hover/main.vo:L:C`) and `tests/cases/doc/`
  (`program_*`). The fixture program is in `tests/cases/query/hover/`, as `shop/` and `amb/` are.
- **Between edits**: `make quick AREA=query,docs`. `make test` once at the end (T014).
- **Regression rule**: existing `query` and `doc` cases keep their output except for added `doc` fields;
  any other change is a bug.

---

## Phase 1: Setup

- [X] T001 [P] Create `specs/023-hover-docs/results.md` (SC-001 to SC-004, Verification, Deviations).
- [X] T002 [P] Fixture program `tests/cases/query/hover/`:
  - `geo.vo`: a documented `export record Point` (a comment above the field `x`, none above `y`), a
    documented `export fn dist2(a: Point, b: Point) -> float` with a two-line description and an example
    line, an undocumented `export fn origin() -> Point`, a generic documented
    `export fn first[T](xs: list[T]) -> result[T]`, and a documented `export variant Shape` with a
    comment above the case `circle(float)`;
  - `main.vo`: imports them and `std.math.sqrt`, and calls each (`dist2`, `origin`, `first`, `sqrt`,
    `len`, `p.x`, `Shape.circle(1.0)`), with a local and a parameter.

  It must check clean (`vo check main.vo`).

---

## Phase 2: Foundational (blocks every story)

- [X] T003 Split `vo doc`'s printing in `src/doc.c` (research R2):
  - `doc_item_record(SB *sb, Str *module, Node *item, Str *desc, Str *example)` writes the fields of the
    record (`"module":…,"name":…,"item":…,"signature":…,` shape, `description` when non-empty,
    `example`) without the `{"v":1,"kind":"doc",` head and the closing brace;
  - `doc_print_item` uses it, so `vo doc --json` output is byte-identical (the existing `doc` cases
    check this);
  - the same split for builtins: `doc_builtin_record(SB *sb, Str *names, Str *signature, Str *notes)`
    from `doc_print_builtin`.
- [X] T004 `doc_comments` (research R3): the description is every description line of the block
  directly above, without `# `, joined by single spaces; the example stays the topmost `#     ` line. No
  `std` item has more than one description line, so the `doc` cases must not change; run them.
- [X] T005 `int doc_record_for(SB *sb, long key_a, long key_b)` in `src/doc.c`, declared in `src/vo.h`,
  returning 1 when it wrote a record (research R1):
  - an item (`key_a` an `N_FN`/`N_RECORD`/`N_VARIANT` node, `key_b == -1`): find the item's index in its
    file's item list, read its comments, and write `doc_item_record` with the module name of its file.
    The module name is the import path relative to the entry's directory with `.vo` removed and `/` as
    `.` (`geo`, `main`), `std.x` for `std/x.vo`, or the package module name `vo doc` uses;
  - the record literal key (`key_b == -2`): as the record itself;
  - a member (`key_a` an `N_RECORD`/`N_VARIANT` node, `key_b >= 0`): the `key_b`-th member of
    `key_a->b.v`, skipping `N_COMMENT` nodes, with its comments read over that list. Write
    `"item":"field"` or `"case"`, `"owner"`, `"name"`, `"signature"` (`x: float`, `circle(float)`, as
    `doc_signature` prints members) and `"description"` when commented;
  - a builtin (`key_a == 0`, `key_b` a builtin id): find the builtin's row in the reference's table by
    `builtin_names[key_b]` and write `doc_builtin_record`; ids without a row (`__` primitives) write
    nothing;
  - anything else: return 0.

**Checkpoint**: `tests/run.sh tests/cases/doc` unchanged; it builds clean.

---

## Phase 3: User Story 1 — Hover a call to see what it does (P1) 🎯 MVP

**Goal**: `type-at` returns the `doc` record for a function, from the program, `std` or the builtins.

**Independent Test**: the `hover_*` cases.

### Tests (first, failing)

- [X] T006 [P] [US1] Golden cases in `tests/cases/query/` (JSON), on the T002 fixture, at:
  - `hover_call`: the call of `dist2` (with the joined two-line description and the example);
  - `hover_decl`: the name `dist2` in its declaration in `geo.vo`;
  - `hover_import`: `dist2` in `import geo.dist2;`;
  - `hover_std`: `sqrt` (its `doc` must equal `vo doc --json std.math.sqrt` without the head; the case
    comment says so);
  - `hover_builtin`: `len`;
  - `hover_undocumented`: `origin` (a `doc` without `description`);
  - `hover_generic`: `first` (the declared signature with `[T]`).

### Implementation

- [X] T007 [US1] `q_type_at` in `src/query.c`: when the name wins and `doc_record_for(name->key_a.i,
  name->key_b.i)` writes a record, append `,"doc":{` + record + `}` to the JSON, after `declaration`.
  The human output is done in T011.

**Checkpoint**: T006 passes; every existing `query` case keeps its output except for the added `doc` fields.

---

## Phase 4: User Story 2 — Hover a type, a field or a case (P2)

- [X] T008 [P] [US2] Golden cases (JSON):
  - `hover_record`: `Point` in `new Point { ... }` and in a type;
  - `hover_field`: `p.x` (described) and `p.y` (no description);
  - `hover_variant`: `Shape` in `Shape.circle(1.0)`;
  - `hover_case`: `circle` there.
- [X] T009 [US2] Make T008 pass through T005's member branch; add a description to the field or case
  JSON only when a comment is directly above it.

---

## Phase 5: User Story 3 — The same in the terminal (P2)

- [X] T010 [P] [US3] Golden cases:
  - `query/hover_human`: `type-at` without `--json` at `dist2`, `sqrt` and a local;
  - `doc/program_item`: `doc {dir}/../query/hover/main.vo dist2` (text) and `program_item_json` (`--json`);
  - `doc/program_private`: a non-exported function of the fixture;
  - `doc/program_unknown`: `doc .../main.vo nosuch`, T0010 with a suggestion.
- [X] T011 [US3] Human output in `q_type_at`: after today's line, `    SIGNATURE` and `    DESCRIPTION`
  (when present), read from the same record (`doc_record_for` also fills a small struct, or a second
  function returns the signature and description strings).
- [X] T012 [US3] `vo doc ENTRY NAME` in `src/doc.c`'s `cmd_doc` (research R5):
  - when the first argument ends in `.vo` and names a file, load and check the program as `vo query`
    does;
  - find NAME among the items of all its files and print it with `doc_print_item` (text) or the record
    (`--json`);
  - an unknown NAME is T0010 through `doc_not_found`, with the nearest name among the program's items.

---

## Phase 6: User Story 4 — Locals and parameters stay as they are (P3)

- [X] T013 [P] [US4] Golden cases `hover_local` and `hover_param` (JSON and human): exactly today's
  answer, no `doc`. All existing `type-at` cases pass unchanged apart from added `doc` fields (list any
  that gained one in `results.md`).

---

## Phase 7: Polish and verification

- [X] T014 [P] Reference (`docs/reference.md`, research R7):
  - two sentences in "Querying and editing programs" (doc comments for program items, and what
    `type-at` and `vo doc ENTRY NAME` return);
  - the `vo doc` line in "Command line" gains `ENTRY NAME`.

  Record the token growth (limit +80) in `results.md`.
- [X] T015 [P] `tests/hover_speed.py` (SC-003): generate a 2,000-line two-file program with documented
  functions; the median of 5 `type-at --json` runs at a call is under 100 ms. Added to `make gate`.
- [X] T016 [P] Docs:
  - `site/docs/` (the tools page for `vo query` and `vo doc`): hover documentation and `vo doc ENTRY NAME`;
  - `docs/roadmap.md`: spec 023 **Done**, with "Not done" (completion, a standing language server);
  - `README.md`: the spec list.
- [X] T017 `make test` on x86-64, then `make ARCH=aarch64 test` and `make ARCH=riscv64 test`, one at a
  time.
- [X] T018 Fill in `results.md`; set the spec status to Implemented; mark every task [X].

---

## Dependencies & Execution Order

- **Phase 1**: T001 and T002 in parallel.
- **Phase 2**: T003 → T004 → T005. Blocks every story.
- **US1** (T006–T007): after Phase 2.
- **US2** (T008–T009): after US1's T007 (the `doc` field in `type-at`).
- **US3** (T010–T012): T011 after T007; T012 after Phase 2.
- **US4** (T013): after T007.
- **Polish**: T014–T016 in parallel; T017 → T018 last.

## Parallel Examples

- After Phase 2: write T006, T008, T010 and T013 together, then run them one suite at a time.
- Polish: T014, T015 and T016 together.

## Implementation Strategy

- **MVP**: Phases 1–3. Hover on any call shows the signature and documentation, so the IntelliJ plugin can
  use it at once.
- **Then**: US2 (types, fields, cases), US3 (terminal, `vo doc ENTRY NAME`), US4 (regression).
- **Last**: reference, speed check, docs, then the three platforms once.

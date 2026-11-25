# Feature Specification: Module Imports

**Feature Branch**: `058-module-imports`

**Created**: 2026-11-25

**Status**: Draft

**Input**: User description: "Module imports: `import std.web;` then qualified use `web.on_get(...)`,
replacing one import line per name."

Decided with the user on 2026-11-25: the separator is `.`; a local named like an imported module is
E0203, and the migration fix renames it (`text` becomes `text_value`).

Background: `import` names one item, and the reference says "Write one `import` per name." A service
file therefore opens with a wall of imports: `examples/notes-api/api.vo` and
`examples/secure-notes/notes.vo` each start with 48 import lines, more than some of their functions.
A reviewer scrolls past them; an agent adds or deletes one line per name it starts or stops using, and
each missing one is a round trip through E0204. The November review (`docs/review-2026-11/language.md`,
item 12) recommended module imports with qualified use as the option that "ages better as programs
grow". In a call, the module name also tells the reader where a name comes from (`sqlite.query` against
`web.request`) without scrolling back to the imports.

What this spec must keep:
- **Principle I, one form:** module imports replace per-name imports; the two do not coexist.
- **No `import ... as`** (decided in spec 056, decision 1): clashes are resolved by names, not aliases.
- **No shadowing:** the rule that no local hides an item or a builtin also covers module names.
- **Stability (`docs/stability.md`):** the change is breaking, so every old import comes with a stable
  diagnostic and a mechanical fix; applying `vo check`'s fixes migrates a program.
- **Principle V:** the reference stays within 15,000 tokens; the spec's results state the change in size.

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents writing vo, the people who review their changes, and the owners of
existing vo programs who upgrade the toolchain.

### User Story 1 - One import per module (Priority: P1)

An agent writes a service that uses `std.web`, `std.sqlite` and `std.log`. It writes three imports and
uses `web.on_get`, `sqlite.query`, `log.log_info`, `web.App` and `sqlite.Database[uses D]`. Starting to
use another item of an imported module needs no new line.

**Why this priority**: it removes the longest repeated block of every multi-module file and the most
common avoidable round trip (a missing per-name import).

**Independent Test**: `examples/notes-api` rewritten with module imports checks, runs and passes its
tests with byte-identical output, and `api.vo` opens with at most 10 import lines instead of 48.

**Acceptance Scenarios**:

1. **Given** `import std.json;`, **When** a function calls `json.parse_json(text)` and names the type
   `json.Json`, **Then** the program checks and runs as it did with the per-name imports.
2. **Given** `import std.json;`, **When** a function calls `parse_json(text)` unqualified, **Then** the
   check reports a diagnostic whose fix writes `json.parse_json`.
3. **Given** a file `notes.vo` beside `api.vo`, **When** `api.vo` has `import notes;` and uses
   `notes.Note` and `notes.add_note(...)`, **Then** both resolve; a private item used this way is E0209
   with its fix adding `export`, as today.
4. **Given** a module used without its import (`web.on_get` and no `import std.web;`), **When** the file
   is checked, **Then** the diagnostic's fix adds the import, as E0204's does today.

### User Story 2 - Existing programs migrate by fixes alone (Priority: P1)

The owner of a program written for vo 0.3 upgrades. `vo check --json` reports each per-name import with
a fix that replaces the imports of a module by one module import and qualifies every use of those names
in the file. Applying the fixes migrates the program with no hand edits.

**Why this priority**: the change is breaking; without a mechanical migration every program, example
and package breaks.

**Independent Test**: every example, the std library, the `postgres` package and the frozen migration
corpora (`tests/migrate.py`, `tests/migrate_056.py`) migrate by applying fixes alone and then check clean
with unchanged test results and output.

**Acceptance Scenarios**:

1. **Given** a file with `import std.json.parse_json;` and `import std.json.Json;`, **When** the fixes
   are applied, **Then** the file has one `import std.json;`, every use reads `json.parse_json` /
   `json.Json`, and nothing else in the file changed.
2. **Given** a migrated file in which a local or parameter has the module's name, **When** it is checked,
   **Then** the clash is reported as described in FR-006, with a fix where one exists.
3. **Given** the fixes have been applied once, **When** the file is checked again, **Then** no
   import diagnostic remains (migration converges in one round for imports).

### User Story 3 - Reviewers and tools see where a name comes from (Priority: P2)

A reviewer reading `try sqlite.exec(db, ...)` knows the call reaches the database module without
looking up the imports. `vo query refs`, `vo query callers`, `vo edit rename`, `vo doc`, hover and
go-to-definition accept and report qualified names.

**Why this priority**: the readability gain for reviewers is the point of the change; the toolchain must
not lose any query it answers today.

**Independent Test**: the existing query, edit, doc and LSP golden cases, migrated to module imports,
give the same answers, and `vo edit rename` of an exported function updates every qualified use in other
files.

**Acceptance Scenarios**:

1. **Given** `vo query refs main.vo notes.add_note`, **When** it runs, **Then** it lists the declaration
   and every qualified use, with role `call` or `value`, as it lists unqualified uses today.
2. **Given** `vo edit rename main.vo notes.add_note add`, **When** it runs, **Then** the declaration and
   every `notes.add_note` become `add` / `notes.add`, the result checks, and all files are written or none.
3. **Given** hover on `web` in `web.on_get`, **When** the language server answers, **Then** it names the
   module (`std.web`) and its documentation summary.

### Edge Cases

- Two imports whose last segments are equal (`import a.util;` and `import b.util;`, or a package module
  named like a std module): an error naming both; no fix (rename a file), since there is no alias.
- A module name equal to an item declared in the importing file (a function `json` and
  `import std.json;`): an error, as two items with one name are today (E0202).
- A local or parameter named like an imported module (`let text = ...;` with `import std.text;`):
  covered by FR-006. The examples and std hold 33 locals and parameters named `text`, so this case is
  common.
- Builtins (`len`, `map`, `print`, `ok`, ...) stay unqualified; they belong to no module.
- Variant cases of an imported variant: built as `json.Json.number(1.0)`; inside `match`, cases are
  still written without the variant name (`case number(n)`).
- Effect sets and constants of another file used in `uses` and scopes: `uses console, api.Storage`,
  `file.read(api.data_dir)`.
- Generic types with arguments: `sqlite.Database[uses D]`, `web.App[Notes[uses D]]`.
- Importing a module and using nothing from it: a new warning (an unused import is not reported today)
  with a fix removing the import, so the import list stays an accurate summary of what a file reaches.
- Importing the same module twice: E0202-style duplicate with a fix removing the second line.
- A qualified name of a non-exported item (`notes.helper`): E0209 with the fix adding `export`.
- `x.y` where `x` is a module and `y` is not one of its items: E0201 "no item `y` in module `x`"
  with a fix when exactly one exported item of that module is a close spelling.

## Requirements *(mandatory)*

### Functional Requirements

**The form**
- **FR-001**: `import a.b.c;` names a file (`a/b/c.vo` relative to the importing file, `std.c`, or
  `PKG.path` for a package, resolved as today) and binds the module name `c`, its last segment, in the
  importing file only.
- **FR-002**: An exported item `n` of an imported module `c` is used as `c.n` everywhere a name may
  appear: calls, function values, types (including type arguments and effect arguments), record
  construction (`new c.Rec { ... }`), variant cases (`c.Var.case`), constants, effect sets in `uses` and
  scopes, and annotations that take constants.
- **FR-003**: Per-name imports (`import std.json.parse_json;`) are no longer accepted. The reference
  states one form: one `import` per module.
- **FR-004**: Items declared in the same file and builtins are used unqualified, as today. A qualified
  use of an item of the same file is an error with a fix removing the qualifier.
- **FR-005**: A module name and an item name are separated by `.` (`web.on_get`), the form agents and
  reviewers know from Go and Python. Because the dot is shared with field access, a name before `.`
  must mean one thing in a file: FR-006 keeps locals from taking module names. (Decided with the user on
  2026-11-25, over `::`.)
- **FR-006**: A local, parameter or type parameter whose name equals an imported module name in the same
  file is E0203 (shadowed name), as shadowing a builtin is. The migration fix of FR-008 renames such a
  local in the same edit, by one fixed rule stated in the catalog (the module's name with `_value`
  appended, then a number if that is taken), so migration still needs no hand edit.
- **FR-007**: Two imports binding the same module name in one file are an error naming both paths; no
  fix. There is no `import ... as`.

**Diagnostics and migration**
- **FR-008**: A per-name import is a stable diagnostic (a new code in the imports range) with one fix
  per file that (a) replaces all per-name imports of the same module by one module import, kept in the
  sorted order `vo fmt` uses, and (b) qualifies every use of those names in the file, including types,
  cases, effect sets and constants. Its catalog entry has a broken and a fixed example.
- **FR-009**: An unqualified use of an item of an imported module is a diagnostic with a fix adding
  the qualifier. A qualified use of a module that is not imported is E0204 with a fix adding the import.
- **FR-010**: A qualified name whose module is imported but whose item does not exist, or is not
  exported, reports the existing codes (E0201, E0209) with their fixes.
- **FR-011**: Retired behavior keeps its code: no code is reused with a new meaning; codes that are
  no longer produced are marked retired in the catalog.

**Tools**
- **FR-012**: `vo fmt` orders imports as today and leaves qualified names as written.
- **FR-013**: `vo query` and `vo edit` TARGETs accept `module.name` (and `module.Record.field`,
  `module.Variant.case`); answers list qualified uses with the same roles as today. `vo edit rename`
  rewrites qualified uses in every importing file.
- **FR-014**: `vo doc`, hover, go-to-definition, references and rename in `vo lsp` work on both parts of
  a qualified name: the module part shows the module, the item part the item.
- **FR-015**: `vo impact`, coverage, mutation testing, traces, runtime error reports and `vo.effects`
  are unchanged in content: they already name functions by file and declaration, not by import.
- **FR-016**: The editor grammars (`editors/`) highlight qualified names and pass the grammar check over
  every `.vo` file in the repository.

**Library, examples and documentation**
- **FR-017**: The std library, the `postgres` package, every example, every reference and site example
  and every catalog example are migrated to module imports.
- **FR-018**: The reference describes the new form and drops "Write one `import` per name"; its size is
  stated before and after, and stays within 15,000 tokens.
- **FR-019**: The changelog lists the change with its migration; the toolchain version becomes 0.4.0.

### Key Entities

- **Module**: one `.vo` file reached by an import path; its exported items are its interface.
- **Module name**: the last segment of an import path, bound in the importing file only; unique within
  that file among module names, items, locals and parameters.
- **Qualified name**: a module name and an exported item name, which together name exactly one item.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The number of import lines across `examples/` and `std/` drops by at least 70%, and no file
  has more than 12 import lines (today: up to 48).
- **SC-002**: Every example, std module, the `postgres` package and every migration corpus migrate by
  fixes alone in one round for imports, then check clean with identical test results and output on
  x86-64, ARM64 and RISC-V.
- **SC-003**: Every new or changed diagnostic has a golden case and a catalog entry with a broken and a
  fixed example; every fix's result checks clean.
- **SC-004**: Every existing `vo query`, `vo edit`, `vo doc` and `vo lsp` golden case gives the same
  answer after migration (same locations and roles).
- **SC-005**: The reference stays at or under 15,000 tokens (about 13 to spare today; the import
  examples get shorter), with the size before and after stated.
- **SC-006**: Check time of the largest example and of a generated program with 8,000 items stays within
  5% of today's.

## Assumptions

- Breaking changes are allowed in a minor version before 1.0 when each comes with a diagnostic and a
  fix (`docs/stability.md`); this one ships as 0.4.0.
- Modules stay files: there are no directory modules, re-exports or nested module declarations.
- Import paths, package resolution, `vo.packages` hashing and the `..` and cycle rules are unchanged;
  only what an import binds changes.
- Std module names are unique among themselves (spec 056 made their item names unique; module names
  already are). A package module whose last segment equals a std module name can only be imported
  beside it by renaming the package file.
- The voledger and vositegen experiments use the working `build/vo`; they are migrated by the same fixes
  when the change lands, and the change lands only when complete.

# Feature Specification: Visibility with `export`

**Feature Branch**: `010-export-visibility`

**Created**: 2026-11-15

**Status**: Draft

**Input**: User description: "Visibility with an `export` keyword: top-level items (fn, record, variant) are private to their file unless declared `export fn`, `export record`, `export variant`. Importing a non-exported item from another file is a new check error with a fix that adds `export` to the declaration. Tests and `main` in the same file can use private items. The standard library and packages follow the same rule (documented public items get `export`; undocumented helpers such as std.json's parse_value become private). An exported record or variant exposes its fields and cases as today (opaque types are out of scope). `vo doc` lists exported items; `vo refs`, `vo edit rename`, the formatter and the reference cover the keyword. Existing programs that import items from their own sibling files need a migration path. Also update the volang-intellij-plugin keyword list."

Background: today every top-level item can be imported by any file ("Every top-level item can be
imported", reference, Programs in several files). A file cannot keep a helper to itself, an agent
cannot tell a file's interface from its internals, and even the standard library's internal helpers
(`std.json`'s `parse_value`, `std.http`'s `exchange`) are importable although `vo doc` does not list
them.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–009: AI coding agents that write vo programs from the reference
and fix them from diagnostics, and the people who review and run those programs.

### User Story 1 - A file keeps its helpers to itself (Priority: P1)

An agent splits a program into `main.vo` and `report.vo`. `report.vo` offers `totals` and `render` to
the rest of the program and uses `column_width` and `money` internally. The author marks `totals` and
`render` with `export`; the other two stay private. When a later edit in `main.vo` imports `money`, the
checker refuses it and names the file that keeps it private, so the boundary holds without anyone
remembering it.

**Why this priority**: it is the feature: without it nothing is private.

**Independent Test**: golden multi-file programs that import exported items (checks and runs), import a
private item (the new error, with a fix that adds `export` to the declaration), and use private items
from the same file, including from tests.

**Acceptance Scenarios**:

1. **Given** `report.vo` declares `export fn totals(...)`, **When** `main.vo` imports `report.totals`,
   **Then** the program checks and runs as today.
2. **Given** `report.vo` declares `fn money(...)` without `export`, **When** `main.vo` imports
   `report.money`, **Then** checking fails with a new error that names the item and its file, and whose
   fix adds `export` to the declaration in `report.vo`.
3. **Given** a private function, **When** it is called by `main`, another function or a test in the same
   file, **Then** it checks and runs (privacy is per file, not per item).
4. **Given** an exported record or variant, **When** another file imports it, **Then** it can build values
   of it, read and set its fields, and match its cases, as today.

---

### User Story 2 - The standard library shows only its interface (Priority: P1)

An agent reads `vo doc std.json` and imports what it lists. Today it could also import `parse_value`, an
internal parser step whose signature may change at any time; nothing tells it not to. With this change
every documented library item is exported and every internal helper is private, so the library's
interface is exactly what `vo doc` shows. Packages follow the same rule.

**Why this priority**: the standard library is what every program imports; its boundary is the first one
agents meet, and it lets the library change its internals without breaking programs.

**Independent Test**: import every item `vo doc` lists for each library module (all check); import each
internal helper (each is the new error, without a fix, since a program cannot change the library).

**Acceptance Scenarios**:

1. **Given** any item listed by `vo doc std.MODULE`, **When** a program imports it, **Then** it checks.
2. **Given** an internal library helper such as `std.json.parse_value` or `std.http.exchange`, **When**
   a program imports it, **Then** checking fails with the new error, which names the module and says the
   item is internal to it (no fix offered).
3. **Given** a package (spec 007) whose file declares exported and private items, **When** a program
   imports them, **Then** exported items work and private ones get the same error as a library helper,
   with no fix (the package's files are protected by its hash).
4. **Given** `vo doc` for a library module or a package module, **When** run, **Then** it lists exactly
   the exported items (and the documentation check fails for an exported library item without a doc
   comment).

---

### User Story 3 - Existing programs move to the rule with mechanical fixes (Priority: P2)

A team has multi-file programs written before this change; every file imports items from its siblings
without any `export`. The rule applies at once (clarified): each such import is now the error of User
Story 1, and each error's fix adds `export` to the declaration in the sibling file. Applying the first
fix of every diagnostic, the documented agent loop, migrates a program in one round.

**Why this priority**: the repository alone has about 990 imports of sibling items (benchmark tasks,
golden cases, examples, evaluation programs). They break on purpose, but each must be fixable without
judgment; the migration follows the feature rather than defining it.

**Independent Test**: check every existing multi-file program in the repository (each import of a sibling
item is the new error, one per imported item), apply the fixes, and check and run the result.

**Acceptance Scenarios**:

1. **Given** an existing multi-file program, **When** checked after this change, **Then** every import of
   a sibling item that is not exported is the new error, each with a fix adding `export` in the declaring
   file, all reported in one pass.
2. **Given** the fixes applied, **When** the program is checked and run, **Then** it has no diagnostic
   about visibility and its output is byte-identical to before.
3. **Given** two files importing the same private item, **When** checked, **Then** both imports are
   reported and their fixes are the same edit (applying both leaves one `export`).

---

### User Story 4 - Tools and editors know the keyword (Priority: P3)

`vo fmt` keeps `export` on the declaration line; `vo refs` and `vo edit rename` work on exported and
private items alike and keep the keyword; the reference documents it; the IntelliJ plugin highlights it.

**Why this priority**: each is small and follows from the keyword; none blocks using it.

**Independent Test**: golden cases for formatting, finding references to and renaming exported items; a
lexer test in the plugin.

**Acceptance Scenarios**:

1. **Given** `export   fn f()` or any spacing, **When** formatted, **Then** it is `export fn f()`.
2. **Given** an exported function, **When** renamed with `vo edit rename`, **Then** the declaration keeps
   `export` and every import is renamed.
3. **Given** a `.vo` file open in IntelliJ with the plugin, **When** `export` appears, **Then** it is
   highlighted as a keyword.

---

### Edge Cases

- **`export` where it does not belong**: on `test`, `prop`, `import`, a field, a case, a local or a
  nested position, `export` is a syntax error that names where `export` is allowed. `export export fn` is
  the same error.
- **`main`**: it is never imported (the entry file is run, not imported); `export fn main` is allowed but
  means nothing more. The single-file programs most users write need no `export` at all.
- **A private type in an exported signature**: an exported function whose parameter or result type is a
  private record or variant of the same file (callers could receive a value whose type they cannot name)
  is a check error with a fix that exports the type. An exported record field or variant case of a private
  type is the same error.
- **`export` as a name**: it becomes a keyword; a program that used `export` as a function, parameter or
  local name gets the existing "keyword used as a name" error.
- **Imports re-exporting**: importing an item does not export it again; `export` applies only to items
  declared in the file. A file cannot pass on another file's item.
- **Effects**: visibility does not change effects; `vo.effects` lists private and exported functions as
  today.
- **Generic items**: `export fn f[T](...)` follows the same rule as any function.

## Requirements *(mandatory)*

### Functional Requirements

**The keyword**

- **FR-001**: `export` MUST be a keyword written before `fn`, `record` or `variant` at the top level of a
  file, making that item importable from other files. It MUST be a syntax error anywhere else.
- **FR-002**: A top-level item without `export` MUST be private to its file: usable by every item and test
  of that file, and not importable from any other file (subject to the migration rule in FR-008).

**Checking imports**

- **FR-003**: Importing a private item MUST be a check error naming the item and the file (or library
  module or package) that declares it. When the declaring file belongs to the program, the error MUST carry
  a fix adding `export` to the declaration; for library and package items it MUST carry no fix.
- **FR-004**: An exported function, record field or variant case whose type mentions a private record or
  variant of its file MUST be a check error with a fix that exports that type.

**Library and packages**

- **FR-005**: Every item of the standard library that `vo doc` lists today MUST be exported, and every
  other library item MUST be private. Library behavior MUST NOT otherwise change.
- **FR-006**: `vo doc` MUST list exactly the exported items of a library or package module; an exported
  library item without a doc comment MUST fail the existing documentation check.
- **FR-007**: Packages MUST follow the same rule as program files.

**Migration**

- **FR-008**: The rule MUST apply at once to existing programs (no warning period and no per-file
  opt-in). Every import of a private item of a program file MUST carry the fix of FR-003, so that applying
  the first fix of every diagnostic migrates a program in one round; repeated fixes of the same declaration
  MUST be identical edits. The repository's own multi-file programs (benchmark tasks, golden cases,
  examples, evaluation programs) MUST be migrated by applying those fixes.

**Tools and documentation**

- **FR-009**: `vo fmt` MUST print `export` followed by one space before the item keyword. `vo refs`,
  `vo edit rename` and every other query and edit command MUST treat exported and private items alike and
  keep `export` on declarations they rewrite.
- **FR-010**: The reference MUST document `export` (Programs in several files) and the keyword list; every
  new diagnostic MUST have a catalog entry, an explanation with an example and a fix, and a golden case.
- **FR-011**: The IntelliJ plugin MUST highlight `export` as a keyword in `.vo` files.
- **FR-012**: Single-file programs and programs that use no imports of their own files MUST behave
  byte-identically to the previous release, except a program that uses `export` as a name. The toolchain
  MUST keep passing its full quality gate on x86-64, ARM64 and RISC-V.

### Key Entities

- **Item visibility**: exported or private, per top-level function, record and variant; decided by its
  declaration alone.
- **Interface of a file or module**: its exported items; what other files may import and what `vo doc`
  lists.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In 100% of golden cases importing a private item of a program file, checking fails with
  the new error and applying its fix gives a program that checks with no visibility diagnostic and the
  same output.
- **SC-002**: Every item `vo doc` lists for each library module is importable, and 100% of the library's
  internal helpers (about 33 today) are refused when imported.
- **SC-003**: Every existing multi-file program in the repository (benchmark tasks, golden cases,
  examples, evaluation programs) fails to check before migration only with the new error, and after its
  fixes are applied (one round) checks with no visibility diagnostic and gives byte-identical output.
- **SC-004**: An agent given only the reference and a two-file task that requires a private helper writes
  a program that uses `export` correctly, or reaches it in one fix round from the diagnostic.
- **SC-005**: The reference grows by at most 250 tokens and stays under 15,000; every reference example
  and catalog entry passes.
- **SC-006**: The full gate passes on all three platforms, and 10,000 generated programs give 0
  disagreements with the previous release.

## Assumptions

- **Opaque types are out of scope**: an exported record exposes all its fields for reading, setting and
  building, and an exported variant all its cases; a later spec may add fields private to the file.
- **Per-item, per-file only**: there is no package-level or directory-level visibility, no `export` of
  imported items (re-export), and no `export { a, b }` list; one form, on the declaration.
- **Private types in exported signatures are an error** (not allowed silently, as in some languages),
  because an agent receiving a value of a type it cannot import cannot write its type in its own code.
- **The previous release is the oracle** for "behaves exactly as before", as in spec 009.
- **The benchmark tasks (`bench2/`) change once**: they are otherwise fixed data, but the rule applies at
  once (clarified 2026-11-15), so their starter and reference programs gain `export` by applying the fixes
  mechanically, and nothing else in them changes. Comparisons with benchmark runs made before this change
  must account for it; this is recorded in results.
- **Migration (clarified 2026-11-15)**: the user chose the clean rule from day one over a warning period
  or a per-file opt-in; every existing multi-file program is fixed mechanically.
- **Doc comments stay optional for program files**; only library items must be documented when exported.

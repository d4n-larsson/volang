# Research: Module Imports

Decisions for spec 058, from reading `src/parse.c`, `src/loader.c`, `src/check.c`, `src/check_expr.c`,
`src/check_effects.c`, `src/fmt.c`, `src/diag.c`, `src/libchange.c`, `tools/vo_fix.vo` and the tests at
commit `6a6e86e`.

## R1. Qualified names are folded by the parser

**Decision**: the parser turns `m.n` into one name whose text is `m.n` whenever `m` is a module name
bound by this file's imports. It knows them because imports come before every item (E0204's rule today),
so the set is complete before the first item is parsed. The folded name goes where an unqualified name
goes today:

| Source | Node today | After |
|---|---|---|
| `web.on_get(...)` | `N_FIELD` + method call | `N_CALL`, name `web.on_get` |
| `notes.fmt` (a value), `api.data_dir` (a constant) | `N_FIELD` | `N_IDENT` `notes.fmt` |
| `web.App`, `sqlite.Database[uses D]` (types) | not parseable | `TY_NAMED`, name `web.App` |
| `new web.App { ... }` | not parseable | `N_NEW_RECORD`, name `web.App` |
| `json.Json.number(1.0)` | `N_FIELD` chain | `N_VCASE`, variant `json.Json` |
| `uses api.Storage` | E01xx | effect set `api.Storage` |

The node gets a flag (`NF_QUAL`); the module part is the text before the first `.` (module and item
names contain no dots), so no new field is needed.

**Rationale**: the checker looks names up by text in about twenty places (`scope_find` in `check.c`,
`check_expr.c`, `check_effects.c`, `check_generic.c`, `check_const.c`, `check_regex.c`, `libchange.c`).
Folding at parse time lets every one of them work unchanged, and every message that prints a name prints
`web.on_get`, which is what the reader wrote.

**Alternatives considered**:
- Resolve `N_FIELD` in the checker (object is an identifier that names a module): needs the same rewrite
  in every place a name can appear, including types and effect clauses, which the parser cannot read
  today. Rejected: more code, and types still need the parser change.
- A new node kind `N_QUALIFIED`: every switch over name nodes would need a new arm. Rejected.

## R2. Binding: a module symbol and its items under qualified names

**Decision**: `bind_imports` adds, per module import, one `SYM_MODULE` symbol named `m` (target file in
the symbol) and one symbol per **exported** top-level item `n` of the target, named `m.n`. Private items
are bound too, marked, so that `notes.helper` reports E0209 (with its fix adding `export`) instead of
E0201.

**Rationale**: with names folded (R1), a qualified name is found by the existing hash lookup. The
`SYM_MODULE` symbol gives three things for free: the existing duplicate check reports two modules with one
name, or a module and an item with one name (E0202); the existing shadowing check reports a local named
like a module (E0203); `m` used alone (`print(json)`) is reported as "`json` is a module, not a value"
(E0201's wording for non-values).

**Alternatives considered**: a per-file module table consulted on lookup: a second lookup path in every
`scope_find` caller. Rejected.

## R3. What an import path means, and the old form

**Decision**:
- `import a.b.c;` loads `a/b/c.vo` (or `std/c.vo` for `std.c`, or `DIR/b/c.vo` for package `a`) and binds
  `c`. One segment, `import notes;`, loads `notes.vo` beside the importer. `import std;` and a package
  name alone are E0205.
- **Old form detection**: when `a/b/c.vo` does not exist, `a/b.vo` does and declares an item `c`, the
  line is the 0.3 per-name form: the item is bound under its bare name as today (no cascade), and the
  file gets E0215 (R5). A last segment starting with a capital (`import geometry.Point;`) is always the
  old form, since a module name is a lowercase identifier.
- When both files exist, the module meaning wins; the catalog says so.

**Rationale**: old programs keep checking (with E0215 and its fix), so migration needs nothing but fixes.
Resolution, packages, `..` refusal, symbolic links and cycles (E0206) are unchanged: only the binding
differs.

## R4. The separator, effects and shadowing

**Decision**: `.` (decided with the user). Consequences:
- A local, parameter or type parameter named like a module of the file is E0203 (R2). Its fix renames it
  to `NAME_value` (then `NAME_value2`, ...) within its function, as one edit (R6).
- In `uses`, a lowercase second segment is an effect (`file.read`, `net`), a capitalized one an effect set
  of a module (`api.Storage`). Effect names are fixed words; a module named like an effect (`log` for
  `std.log`) is not ambiguous there, because effects are never module items.
- A qualified use of the importing file's own item cannot occur: importing the file itself is a cycle
  (E0206). FR-004's second sentence needs no code.
- Builtins (`len`, `map`, `print`, `ok`, ...) are not module items and stay unqualified. A builtin name
  written qualified (`lists.len`) is E0201 with the fix removing the qualifier.

## R5. Diagnostics

| Code | When | Fix |
|---|---|---|
| **E0215** `single_name_import` (new) | the file has 0.3 per-name imports (R3); one diagnostic per file, at the first such import | rewrites the file (R6) |
| E0201 `undefined_name` (new fix) | `parse_json` used bare while `import std.json;` exports it | qualify: `json.parse_json` |
| E0201 (new message) | `json.nope`: "`std.json` has no item `nope`." | the closest exported name, if one is close (existing similar-name machinery) |
| E0204 `missing_import` (wider) | `web.on_get` with `web` not imported, where exactly one std module, package module or sibling file is named `web`; or a bare name declared in exactly one sibling file | adds `import std.web;` (or `import notes;`); for a bare name a second round qualifies it (E0201) |
| E0202 `duplicate_definition` (wider) | two imports bind one module name; a module name equals an item of the file | none for different paths; removes the line for a repeated import |
| E0203 `shadowed_name` (wider) | a local, parameter or type parameter named like an imported module | renames it (R4, R6) |
| E0209 `not_exported` | `notes.helper`, private | adds `export` (std: none), as today |
| E0214 `library_changed` (moved) | a 0.2 std rename used qualified (`text.old_name`) | the new name, at the use |
| **W0203** `unused_import` (new) | a module imported and never used | removes the line |

E0215's message: "Imports name a module, not an item: `import std.json;`, then `json.parse_json`." No
code is reused with another meaning; E0201/E0202/E0203/E0204 keep theirs and widen to modules.

## R6. A migration in one edit

**Finding**: in schema 1 a fix is one edit (`{file, start, end, replacement}`), and an agent applies "the
first fix" of each diagnostic. A migration that changes 48 imports and 150 uses cannot be 200 fixes on
200 diagnostics: the output is capped at 50 diagnostics, so it would take four or more rounds.

**Decision**: E0215's fix is **one edit** whose span runs from the start of the first import to the end
of the last changed use in the file, and whose replacement is that region rewritten: the per-name imports
of each module replaced by one module import (in `vo fmt`'s order), every use qualified, 0.2 renames
(E0214 rows that are renames) applied, and locals named like a module renamed (R4). The checker builds it
from the reference log it already keeps for `vo query` (`q_note_sym`), so every use is found by
resolution, never by text.
- Text output summarizes a fix whose replacement spans more than 3 lines: `fix: rewrite lines 1 to 212
  (module imports)`, instead of printing it; JSON carries the whole replacement.
- Other fixes in that file overlap the region and are dropped by the existing overlap rule
  (`diag.c`, "drops overlapping fixes"); they come back in the next round.
- `tools/vo_fix.vo` allows 4 rounds instead of 3, since programs that needed 3 before (v1 corpus) now
  need one more.

**Alternatives considered**:
- A fix with several edits (a new `edits` field): schema 1 can add fields, but an agent that does not
  know it applies only the first edit and is left with a broken file. Rejected.
- A `vo migrate` command: a second way to apply fixes, and agents already have the loop. Rejected.

## R7. Types printed in the importing file's terms

**Finding**: messages and fixes print types with `ty_name`, which prints the declaration's name (`Json`).
A fix that inserts a type (E0306's `new list[...]`, E0307's annotation, effect argument fixes) would
insert `Json` into a file that must write `json.Json`.

**Decision**: `ty_name` gains the file it prints for (`ty_name_in(t, file)`), qualifying a record,
variant or effect set declared in another file by the module name that file binds. Messages use it too,
so `Expected json.Json, found str.` matches the source. A type from a file the current file does not
import (reached through another module's signature) prints as `std.json.Json`, the path, since no
shorter name exists there.

## R8. Tools

- **`vo query` / `vo edit` targets**: `module.name`, `module.Record.field`, `module.Variant.case`. A first
  segment in lowercase is a module; it is matched against the module names the program's files bind. Two
  different files bound under one name in different importers is T0005, as for a name declared twice.
- **Rename**: a qualified use is replaced from the last `.` on (`notes.add_note` → `notes.add`); the
  reference log keeps the node, whose item part is the text after the module prefix.
- **Answers** list qualified uses with the existing roles (`call`, `value`, `type`, `use`); the import
  line of a module is role `import` for every item of that module that the file uses.
- **`vo lsp`**: hover on the module part shows the module path and the first comment paragraph of its file;
  definition goes to the file's first line; rename on the module part is refused (rename the file).
- **`vo doc`**: unchanged output for declarations (they are written inside their module), plus a line
  `use: import std.json; then json.parse_json` for library items. Examples in doc comments are migrated.
- `vo impact`, coverage, mutation, traces, runtime errors, `vo.effects`, recordings: unchanged (they name
  functions by file and declaration).

## R9. Formatting

`vo fmt` sorts imports by their dotted text today; that stays. Qualified names print from the folded
text. Nothing else changes.

## R10. Editor grammars

TextMate, tree-sitter and Vim highlight `ident.TypeIdent` in type positions and `ident.ident(` as a call;
`editors/check.js` (the check over every `.vo` file) keeps passing after the migration.

## R11. Migrating the repository

- `std/` (7 files with imports), `packages/postgres` (its files import each other as `postgres.link.x`),
  `examples/`, `tools/vo_fix.vo`, the reference, the guide (`site/docs`, 17 pages), the catalog examples
  and doc comments: by `tools/vo_fix.vo --only E0215,E0203,E0201` and then `vo fmt`.
- **Golden cases**: 690 of the sources under `tests/cases/` import. They are migrated by the same fix; the
  expected outputs are regenerated, and a script (`tests/migrate_058.py`) compares old and new expected
  outputs after normalizing line numbers and qualified names: codes, messages and output must otherwise be
  equal. A copy of 20 representative cases stays in the old form under `tests/cases/imports_v03/` with
  E0215 expected.
- `tests/migrate.py` and `tests/migrate_056.py` keep their frozen corpora and pass with 4 rounds.
- The voledger and vositegen kits are migrated with `vo_fix` the day the change lands (memory note: they
  run `build/vo`).

## R12. Reference budget

`tests/ref_examples.py` measures ~14,987 tokens of 15,000. Changes:
- Programs in several files and Standard library: "`import a.b.name;` ... Write one `import` per name"
  becomes the module rule (about the same length).
- 7 import lines in examples become 5.
- New: one sentence for the module/local name rule (E0203).
If the result is over budget, the sentence on E0204's fix and the E0209 detail move to the guide
(`site/docs/guide/`), where they are already explained at length.

## R13. Performance

Binding every exported item of each imported module (std.web exports about 120) adds one hash insert per
item per importing file; the 8,000-item check stays dominated by checking bodies. Measured in the results
against SC-006 (within 5%).

## R14. Version and stability

0.4.0, with the changelog entry and `docs/stability.md`'s list of migrations ("This has been done three
times") updated to four, and `tests/migrate_058.py` kept as a test of every later toolchain.

# Contract: Diagnostics

New codes get catalog entries (`docs/errors.md`) with a broken and a fixed example; widened codes get a
second example. Every fix's result checks clean (`tests/apply_fixes.py`).

## New

### E0215 single_name_import — Fixable: yes

`import std.json.parse_json;` (the 0.3 form). One diagnostic per file, at the first such import.
Message: "Imports name a module, not an item: `import std.json;`, then `json.parse_json`."
Fix: one edit from the first import to the last changed use (research R6): imports regrouped per module,
uses qualified, 0.2 renames applied, locals named like a module renamed. Text output:
`fix: rewrite lines A to B (module imports)`.

### W0203 unused_import — Fixable: yes

A module imported and not used in the file. Message: "`std.json` is imported but not used." Fix: remove the
line. Respects `@allow(W0203)`.

## Widened (meaning unchanged)

| Code | New case | Message | Fix |
|---|---|---|---|
| E0201 | an item of an imported module used bare | "`parse_json` is in `std.json`: write `json.parse_json`." | qualify |
| E0201 | `json.nope` | "`std.json` has no item `nope`." | closest name, if any |
| E0201 | `json` used as a value | "`json` is a module: use one of its items, `json.NAME`." | none |
| E0201 | a builtin written qualified (`lists.len`) | "`len` is a builtin: write `len`." | remove the qualifier |
| E0202 | two imports bind one name; a module and an item share a name | "`util` is already a module of this file (`a/util.vo`)." | for a repeated identical import: remove it |
| E0203 | a local, parameter or type parameter named like a module | "`text` is the module `std.text` in this file: name the local differently." | rename to `text_value`, at the declaration and (with "rename the local to `text_value` here too") at each use |
| E0204 | `web.x` with no import, exactly one module named `web` exists | "`web` is not imported: add `import std.web;`." | add `import std.web;` (later uses: notes on the first report) |
| E0204 | a bare name declared in one sibling file | as today | add `import notes;` (then E0201 qualifies) |
| E0205 | `import std;` | "An import names a file: `import std.json;`." | none |
| E0214 | a 0.2 std rename used qualified | as today | new name at the use |

## Retired

None. The E0205 message "An import needs a file and a name" is reworded (messages may change).

# Contract: Catalog Codes (spec 058)

New codes, each with a `docs/errors.md` entry (example and fixed version) and golden cases. Messages:
[diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0215 | `single_name_import` | 65 | yes | a 0.3 import of one item (`import std.json.parse_json;`) in a program file; one per file; the fix rewrites the imports and every use as one edit |
| W0203 | `unused_import` | 0 | yes | a module imported and not used in the file; the fix removes the line |

Changed codes (meaning unchanged, new cases):

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0201 | `undefined_name` | 65 | sometimes | also an item of an imported module used bare (fix: qualify), a missing item of a module, a module used as a value, a builtin written qualified (fix: drop the qualifier) |
| E0202 | `duplicate_definition` | 65 | sometimes | also two imports binding one module name, or a module named like an item; an identical repeated import (fix: remove it) |
| E0203 | `shadowed_name` | 65 | sometimes | also a local, parameter or type parameter named like an imported module; the fix renames it to `NAME_value` |
| E0204 | `missing_import` | 65 | yes | also `m.name` with `m` not imported; the fix adds `import std.m;` / `import m;` |
| E0205 | `import_not_found` | 65 | no | also `import std;` or a package name alone |
| E0214 | `library_changed` | 65 | sometimes | also at a qualified use (`text.old_name`) |

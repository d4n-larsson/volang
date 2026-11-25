# Contract: Catalog Codes (spec 056)

New codes, each with a `docs/errors.md` entry (example and fixed version) and golden cases. Details:
[api.md](api.md) "Diagnostics".

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0214 | `library_changed` | 65 | sometimes | an import or use of a std item renamed, removed or changed in vo 0.2.0; the fix gives the new form where one keeps the program's behavior |

Changed codes:

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0306 | `empty_list_literal` | 65 | no | a `[]` with no list type to take from its context (with one, `[]` has that type, so the old fix to `new list[T]` has no case left) |
| E0203 | `shadowed_name` | 65 | sometimes | also a local or parameter named like a builtin; the fix renames it to `NAME_value` |
| E0119 | `repeated_annotation` | 65 | yes | also a repeated `@allow` code or field annotation (E0329 and W0706 before); the fix removes the repeat |

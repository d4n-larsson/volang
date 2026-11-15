# Contract: New Catalog Codes (spec 010)

Each code gets an entry in `docs/errors.md` and at least one golden case. No existing code changes meaning.

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0209 | `not_exported` | 65 | yes | importing an item declared without `export` (no fix for library and package items). |
| E0210 | `private_type_exported` | 65 | yes | an exported function, field or case whose type names a private record or variant of its file. |

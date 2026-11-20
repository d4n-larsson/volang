# Contract: Catalog Codes (spec 029)

New codes, each with a `docs/errors.md` entry (example and fixed version), a `vo explain` text and golden
cases. Messages and triggers: [diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0328 | `not_json` | 65 | sometimes | a type bound to a `json` type parameter has a part with no JSON form (fix: `: json` on a bare type parameter) |
| E0329 | `bad_field_annotation` | 65 | sometimes | a record field's annotation is unknown, repeated, misplaced or does not fit the field |
| E0330 | `bad_route_pattern` | 65 | no | an `on_*` pattern that is not a literal, is malformed, repeats a variable, or names a variable the input record cannot take from a path |
| E0331 | `route_conflict` | 65 | no | two routes of one list literal and one method match the same paths |

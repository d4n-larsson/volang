# Contract: Catalog Codes (spec 055)

New codes, each with a `docs/errors.md` entry (example and fixed version) and golden cases. Details:
[api.md](api.md) "Diagnostics".

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| W0506 | `secret_logged` | 0 | yes | a log field's value is a variable, parameter or field access with a secret-like last name; the fix removes the field |
| E0341 | `bad_metric_name` | 65 | sometimes | a literal metric or label name breaks the text format's rules, or a label is repeated; the fix replaces each disallowed character with `_` (none for a `__` label or a repeated label) |
| R0016 | `metric_conflict` | 70 | — | a metric name used with another kind, help or labels; a wrong number of label values; a counter given a negative amount |

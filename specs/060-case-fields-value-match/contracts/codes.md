# Contract: Catalog Codes (spec 060)

New codes, each with a `docs/errors.md` entry (example and fixed version) and golden cases. Messages:
[diagnostics.md](diagnostics.md). A row is added with the task that implements the code.

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0345 | `match_scrutinee` | 65 | no | `match` on a value that is not a variant, a `result`, an `int` or a `str` (replaces E0301 there) |
| E0414 | `value_match_else` | 65 | yes | a `match` on an `int` or `str` without `else`; the fix inserts an empty `else` |
| E0415 | `match_else_exhaustive` | 65 | yes | `else` on a variant or `result` match; the fix removes it |

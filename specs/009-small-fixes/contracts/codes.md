# Contract: New Catalog Codes (spec 009)

Each code gets an entry in `docs/errors.md` and at least one golden case. No existing code changes meaning.

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0411 | `empty_case_parens` | 65 | yes | `case NAME()` on anything but the success of a `result[void]`. |
| W0402 | `void_ok_pattern` | 0 | yes | `case ok(_)` or `case ok` on a `result[void]`. |

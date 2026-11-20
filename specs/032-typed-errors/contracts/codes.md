# Contract: Catalog Codes (spec 032)

New codes, each with a `docs/errors.md` entry (example and fixed version), a `vo explain` text and golden
cases in `tests/cases/typed_errors/`. Messages: [diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0334 | `bad_error_type` | 65 | no | the error type of a `result` is `void`, a function type or contains one |
| E0412 | `try_error_mismatch` | 65 | no | `try` on a result whose error type differs from the enclosing function's |

Changed wording, same meaning:

| Code | Change |
|---|---|
| E0409 `invalid_main_signature` | a `main` returning `result[void, E]`: the message says `main` returns `result[void]` |

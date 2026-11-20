# Contract: Diagnostics (spec 032)

Each new code has a `docs/errors.md` entry (broken and fixed example), a `vo explain` text and golden
cases in `tests/cases/typed_errors/`.

| Code | Name | Exit | Fixable | Trigger | Message |
|---|---|---|---|---|---|
| E0334 | `bad_error_type` | 65 | no | the error type of a `result` is `void`, a function type or contains one | "The error of a `result` is a value the caller can read and print: `$1` cannot be one." |
| E0412 | `try_error_mismatch` | 65 | no | `try` on a result whose error type differs from the enclosing function's | "`try` passes the error on, but this `result` fails with `$1` and `$2` fails with `$3`: convert it with `map_err`." |

Reused:

| Code | When |
|---|---|
| E0301 | `err(x)` with `x` not of the error type; a function given to `map_err` that does not take the error type |
| E0307 | `ok`/`err` whose result type is not known from its place |
| E0401 | a `match` on an error variant that leaves out a case (fix: the case) |
| E0406 | `try` outside a function returning a result |
| E0409 | `main` returning `result[void, E]` with `E` not `str`; the message says main returns `result[void]` |

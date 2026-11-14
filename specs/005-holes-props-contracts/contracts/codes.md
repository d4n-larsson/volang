# Contract: New Catalog Codes

Each code gets an entry in `docs/errors.md`: name, one-line summary, whether it is fixable, explanation, example and fix. Each also gets at least one golden case (`tests/catalog.py` enforces this). No existing code changes meaning.

| Code | Name | Severity / exit | Fixable | Trigger |
|---|---|---|---|---|
| W0601 | `hole` | warning | no | Every `todo(...)` in a checked program. Carries `expected` and `scope`. |
| E0601 | `hole_type_unknown` | error, 65 | no | `todo(...)` where the context gives no type: `let x = todo("a");`, `todo("a") == 1`, `str(todo("a"))`. |
| E0602 | `ungeneratable_type` | error, 65 | no | A property parameter whose type contains a function type or has no finite value. |
| E0603 | `impure_contract` | error, 65 | no | A `requires`/`ensures` condition that needs an effect, such as one that calls `print` or an effectful function. |
| E0604 | `assume_outside_prop` | error, 65 | no | `assume(...)` in a function, lambda or test. |
| R0011 | `hole_reached` | runtime, 70 | — | Running into a hole. |
| R0012 | `precondition_failed` | runtime, 70 | — | A `requires` condition is false at a call. |
| R0013 | `postcondition_failed` | runtime, 70 | — | An `ensures` condition is false at a return. |

Existing codes reused without a change in meaning:

| Code | Meaning (unchanged) | New situations |
|---|---|---|
| E0202 | `duplicate_definition` | A property named like a test or another property. |
| E0203 | shadowing | A parameter named `ret` on a function with `ensures`. |
| E0301 | `type_mismatch` | A non-`str` hole description; a non-`bool` condition. |
| E0406 | `try` outside a result function | `try` in a property. |
| E0410 | `expect_outside_test` | Unchanged. Properties count as tests for `expect`/`expect_eq`, and the entry's text gains "or `prop`". |
| W0401 | `unreachable_code` | Statements after a `todo(...);` statement. |

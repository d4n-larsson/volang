# Contract: Catalog Codes (spec 059)

New codes (a row is added with the task that implements the code: E0342 in T019, E0413 in T029), each with a `docs/errors.md` entry (example and fixed version) and golden cases. Messages:
[diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0343 | `lambda_type_unknown` | 65 | no | an anonymous function with no expected type, or whose parameter type is an open type variable of the call |

Codes in new contexts (meaning unchanged):

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0301 | `type_mismatch` | 65 | sometimes | also a lambda body of another type than the expected return, a lambda with another number of parameters than expected (no fix), two `return`s of different types where the return type is inferred |
| E0502 | `function_value_effects` | 65 | sometimes | also an untyped lambda whose body's effects exceed the expected function type's |
| E0310 | `cannot_infer_type_argument` | 65 | sometimes | also a type variable still open after every anonymous function of a call |

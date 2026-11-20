# Contract: Catalog Codes (spec 031)

New codes, each with a `docs/errors.md` entry (example and fixed version), a `vo explain` text and golden
cases in `tests/cases/const/`. Messages and fixes: [diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0122 | `const_in_function` | 65 | yes | `const NAME = ...;` as a statement inside a function; the fix replaces `const` with `let` |
| E0212 | `const_not_constant` | 65 | no | a constant's value (or a constant expression at a literal position) calls something, names something that is not a constant, or uses a construct other than literals, constants and operators |
| E0213 | `const_cycle` | 65 | no | a constant defined by itself, directly or through other constants |
| E0332 | `const_type` | 65 | no | a constant whose type is not `int`, `float`, `bool` or `str` |
| E0333 | `const_arithmetic` | 65 | no | a constant's value overflows `int` or divides by zero |
| W0202 | `unused_constant` | 0 | yes | a private constant nothing refers to; the fix removes its declaration |

Changed wording, same meaning:

| Code | Change |
|---|---|
| E0208 `assign_to_immutable` | a third case: "`NAME` is a constant and cannot be changed." (no fix) |
| E0315, E0321, E0324, E0330 | "a string literal" becomes "a string literal or a constant" |
| E0506 `invalid_scope` | a scope that is a name but not a constant; a scope from a constant names it and its value |

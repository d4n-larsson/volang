# Contract: Diagnostics

New codes get catalog entries (`docs/errors.md`) with a broken and a fixed example. Every fix's result
checks clean (`tests/apply_fixes.py`). One edit per fix (schema 1).

## New

### E0342 lambda_types_written — Fixable: sometimes

A parameter type or `-> T` on an anonymous function. At the first written type.
Message: "An anonymous function takes its types from where it is used: write `fn(p)`."

| Situation | Fix |
|---|---|
| expected type exists and equals the written types | replace the header `(p: Person) -> int` with `(p)` |
| no expected type; the lambda is the whole initializer of an untyped `let`/`var` | one edit from the end of the binding's name to the end of the header: `f: fn(int) -> int = fn(x)` (the type with the lambda's effects, in the file's terms) |
| expected type differs from the written types | not reported this round: today's E0301 or E0502 (with its fix) is reported instead (FR-005) |
| otherwise (no expected type, not a binding's initializer) | none; E0343 follows once the types are removed |

### E0343 lambda_type_unknown — Fixable: no

An anonymous function with no expected type, or whose parameter type is an open type variable of the
call. At the `fn` keyword.
Messages:
- "Cannot tell the parameter types of this anonymous function: give `f` a function type, `let f: fn(...) -> ... = fn(...)`."
- "`I` of `on_get` is known only from this function's parameters: pass a named function, or give a binding its type."

The body is still checked, with error-typed parameters (no cascades).

### E0413 lambda_single_return — Fixable: yes

An anonymous function whose block body is a single `return expr;` (and holds no comment). At the block.
Message: "A body that only returns a value is written `=> expr`."
Fix: replace `{ return expr; }` with `=> expr`. `vo fmt` applies the same rewrite.

## Unchanged codes, new contexts

| Code | New case | Message |
|---|---|---|
| E0301 | a lambda's body gives a type other than the expected return type | at the expression, naming the expected type |
| E0301 | a lambda with another number of parameters than the expected function type | "Expected a function of 1 parameter (`fn(Person) -> int`), found one of 2." No fix. |
| E0301 | block body returns two different types where the return type is inferred | at the second `return` |
| E0502 | the body's effects exceed the expected function type's | as today |
| E0310 | a type variable still open after every lambda | as today |

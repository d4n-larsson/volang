# Contract: Codes Added in v2

These are added to the stable catalog (`docs/errors.md`). Fixability uses the v1 three-valued rule.

## Effects: E05xx and W05xx

| Code | Name | Fix? | Notes |
|---|---|---|---|
| E0501 | missing_effect | add the missing effects to the `uses` clause | lists the full missing set, transitively, so migration converges in one round |
| W0501 | unused_effect | remove it | a declared effect or scope that nothing needs |
| E0502 | function_value_effects | add the effects to the expected function type when it is written in this file | a function value whose effects exceed the expected function type; names the extra effects |
| E0503 | scope_too_narrow | widen the scope to cover the callee | `related` = the callee's declaration |
| E0504 | effects_grew | — (approve with `vo effects --update`) | `related` = the call paths that introduced the growth |
| W0502 | effects_reduced | — | |
| W0503 | function_renamed | — | effects unchanged across a rename |
| W0504 | no_effects_record | — | a hint to create the record |
| E0505 | captures_var | copy the value into a `let` before the anonymous function | |
| E0506 | invalid_scope | — | a `..` component, an absolute path or an empty scope |
| E0507 | uses_on_anonymous_fn | remove the clause | anonymous functions infer their effects |
| E0509 | unknown_effect | replace with the closest effect name when one is within edit distance 2 | e.g. `uses files`, `uses file.writes` |
| E0510 | invalid_effects_record | — | a line of `vo.effects` that does not parse; names the record line |

## Generics: E031x

| Code | Name | Fix? | Notes |
|---|---|---|---|
| E0310 | cannot_infer_type_argument | add an annotation when the statement is a `let` | names the type parameter |
| E0311 | missing_ordered | add `: ordered` to the type parameter | |
| E0312 | wrong_type_argument_count | — | `Stack[int, str]` |
| E0313 | not_ordered | — | a non-ordered type where an ordered one is required: an `ordered` type argument or a `sort_by` key |

## Runtime: R00xx

| Code | Name | Notes |
|---|---|---|
| R0010 | capability_denied | path or variable outside the innermost declaration or the grant; names the path and the allowed scopes |

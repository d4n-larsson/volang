# Contract: Initial Error Code Catalog

These codes are stable: once a code is published, its meaning never changes (FR-024). The full explanation, example, and fixed version for each code live in `docs/errors.md`, and `vo explain` prints them. The "Fix?" column marks codes that come with a mechanical fix. A fix that depends on the situation ("when …") makes the code `Fixable: sometimes` in the catalog; otherwise it is `Fixable: yes`. SC-003 measures fix coverage over the `yes` codes.

## Syntax: E01xx

| Code | Name | Fix? | Notes |
|---|---|---|---|
| E0101 | invalid_character | remove | includes a `#` placed after code on the same line |
| E0102 | unterminated_string | insert `"` | |
| E0103 | invalid_escape | — | |
| E0104 | invalid_utf8 | — | points to the first bad byte |
| E0105 | number_out_of_range | — | int > 2^63−1, or a float with more than 15 significant digits |
| E0110 | unexpected_token | when exactly one token fits | has `expected`/`found` |
| E0111 | unclosed_delimiter | insert closer | `related` = the opening delimiter |
| E0112 | missing_semicolon | insert `;` | |
| E0113 | trailing_comma | remove `,` | |
| E0114 | statement_not_call | insert `set ` when the statement is an assignment | e.g. `x + 1;` or `x = 1;` |
| E0115 | nesting_too_deep | — | |

## Names: E02xx

| Code | Name | Fix? | Notes |
|---|---|---|---|
| E0201 | undefined_name | a similar name, when one exists | Levenshtein distance ≤ 2 |
| E0202 | duplicate_definition | — | `related` = the first definition |
| E0203 | shadowed_name | — | `related` = the outer definition |
| E0204 | missing_import | insert `import …;` | the name was found in another file under this file's directory |
| E0205 | import_not_found | — | |
| E0206 | import_cycle | — | `related` = the chain |
| E0207 | naming_convention | renamed identifier | |
| E0208 | assign_to_immutable | `let` → `var` when declared with `let` | |

## Types: E03xx

| Code | Name | Fix? | Notes |
|---|---|---|---|
| E0301 | type_mismatch | `str(…)`, `float(…)` or `int(…)` when a conversion fits | has `expected`/`found` |
| E0302 | wrong_argument_count | — | |
| E0303 | not_callable | — | |
| E0304 | unknown_field | a similar field, when one exists | |
| E0305 | missing_field | — | lists the missing fields |
| E0306 | empty_list_literal | `new list[T]` when the type is known | |
| E0307 | cannot_infer_type | — | add a type annotation |
| E0308 | invalid_operands | — | |
| E0309 | invalid_map_key | — | the key must be `int` or `str` |

## Control flow: E04xx

| Code | Name | Fix? | Notes |
|---|---|---|---|
| E0401 | non_exhaustive_match | insert missing `case` arms | lists the missing cases |
| E0402 | duplicate_case | remove arm | |
| E0403 | missing_return | — | |
| E0404 | unhandled_result | prefix `try ` (when the enclosing function returns a result) | |
| E0405 | unused_value | prefix `let _ = ` | |
| E0406 | try_outside_result_fn | — | |
| E0407 | break_outside_loop | — | also covers `continue` |
| E0408 | missing_main | — | only for `run` |
| E0409 | invalid_main_signature | — | |
| E0410 | expect_outside_test | — | |

## Warnings: Wxxxx

| Code | Name | Fix? |
|---|---|---|
| W0201 | unused_variable | — (removing the declaration could drop side effects) |
| W0401 | unreachable_code | — |

## Runtime: R00xx

| Code | Name |
|---|---|
| R0001 | integer_overflow |
| R0002 | division_by_zero |
| R0003 | index_out_of_bounds |
| R0004 | key_not_found |
| R0005 | stack_exhausted |
| R0006 | out_of_memory |
| R0007 | invalid_argument |
| R0008 | exit_code_out_of_range |
| R0009 | timeout |

## Toolchain: T00xx

| Code | Name |
|---|---|
| T0001 | usage_error |
| T0002 | cannot_read_input |
| T0003 | internal_error |

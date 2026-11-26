# Contract: Diagnostics

New codes get catalog entries (`docs/errors.md`) with a broken and a fixed example; every fix's result
checks clean (`tests/apply_fixes.py`). One edit per fix (schema 1).

| Code | Name | Fixable | Trigger | Message | Fix |
|---|---|---|---|---|---|
| E0216 | `one_field_case` | yes | a case declared with one field | "A case with one value is written `missing(str)`." | rewrite the declaration |
| E0344 | `case_field_set` | no | `set b.f = ...` where `b` is a case's fields | "The fields of a case are read-only: build a new value, `V.case { ... }`." | — |
| E0345 | `match_scrutinee` | no | `match` on another type | "`match` takes a variant, a `result`, an `int` or a `str`, found `T`: use `if`." | — |
| E0414 | `value_match_else` | yes | a value match without `else` | "A `match` on an `int` or `str` ends with `else { }`: values not listed go there." | insert `else {\n}` before the match's `}` |
| E0415 | `match_else_exhaustive` | yes | `else` on a variant or `result` match | "A `match` on a variant lists every case: no `else`." | remove the arm |

Codes in new contexts: E0402 (a repeated value, or a constant equal to a literal arm; today's removal
fix), E0301 (an arm of another type), E0304/E0305 (unknown or missing field in a field-case literal),
E0201 (an arm name that is no constant of the scrutinee's type; `new` on a case's fields type).

Dropped (research R8): the warning of US3.

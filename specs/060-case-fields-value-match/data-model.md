# Data Model: Variant Cases With Fields, and `match` on Values

No run-time representation changes: a variant value stays a tag and one payload word; a case with fields
carries a record, as a case with a record payload does today.

## Case declaration (`N_CASE_DECL`)

| Field | Today | After |
|---|---|---|
| A | name | name |
| B | payload type node, or 0 | payload type node, or 0; for a field case, a type node naming the hidden record |
| C | unused | the hidden record (`N_RECORD`) of a field case, or 0 |

## Hidden record (a synthetic `N_RECORD`)

| Property | Value |
|---|---|
| name (messages, hover, `type-at`) | `Variant.case`; generic: `Variant[args].case` |
| fields | the case's fields, with their annotations |
| type parameters | the variant's (own copies; `ty_subst` works by owner) |
| exported | when the variant is |
| file item | no: reached through its case only |

Rules: two or more fields (one is E0216); fields read-only through the binding (E0344); not built with
`new`; equal, ordered, a key and JSON exactly when a record with those fields would be.

## Value match

| Node | Fields |
|---|---|
| `N_MATCH` | A scrutinee, B arms vector (as today) |
| `N_ARM` (value arm) | A the arm's value: an int or string literal node, a constant name, or a module constant; C body |
| `N_ELSE_ARM` (new) | C body; last in the arms vector |

Rules: scrutinee `int` or `str` (else E0345 unless a variant or `result`); arm values of the scrutinee's
type (E0301), distinct (E0402); exactly one `else`, last (E0414 when missing); no `else` on a variant or
`result` match (E0415).

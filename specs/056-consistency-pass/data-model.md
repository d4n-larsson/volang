# Data Model: Consistency Pass

## Library change (one row per change; the single source for E0214, its fix, the catalog and the changelog)

| Field | Meaning |
|---|---|
| `module` | `std.http`, `std.tcp`, ... |
| `old` | the old item name, or `Variant.case` for a case |
| `kind` | `rename`, `remove`, `swap` (the first two arguments), `option` (the result became `Option`), `unit` (a duration's unit or name) |
| `new` | the new name (rename, swap with a rename, unit); for `remove`, the replacement call text |
| `wrap` | for `option`: `to_result(CALL, "none")` or, for `index_of`, `or_else(CALL, -1)`; the import the fix adds |
| `factor` | for `unit`: the factor a literal is multiplied by, and the function that wraps any other expression (`minutes`, `seconds`) |
| `since` | `0.2.0` |

Rules:
- An import of `old` reports E0214 at the import. The import's fix gives `new`, and the import binds the
  new item under the old local name, so the file keeps checking.
- Each use of `old` in the file reports E0214 with the fix of its kind. A fix that cannot keep the
  program's behavior is not offered: the message says what to write.
- Rows apply only to std items. A program's own items are never matched.

## Ordered type (FR-007)

`int`, `float`, `str`, `bytes`, `bool`, a record whose fields are all ordered, a variant whose case
payloads are all ordered, and a type parameter bounded `ordered` or `key`.

| Order | Rule |
|---|---|
| `bool` | `false < true` |
| variant | by case index (declaration order); the same case by its payload |
| record | field by field in declaration order (unchanged) |

`key` types (`int`, `str`, `bool`, `bytes`, records and variants of keys) are all ordered.

## Time limit (FR-008)

`timeout_ms`: 0 to 600,000. 0 means 600,000. Other values are `invalid: the time limit must be 0 to
600000 ms`. An expired limit is the case `timed_out`.

## Duration names (FR-009)

| Where | Unit | Suffix |
|---|---|---|
| function parameters and results | milliseconds | `_ms` when named |
| record fields a program sets or reads | milliseconds | `_ms` |
| tz records (`Zone`, `ZoneRule`, `RuleDay`) | seconds (the tz database) | `_s` |

## Error text form (FR-016)

For each exported error variant `NameError` of a module: `name_error_text(e: NameError) -> str`, one
sentence per case, ending with a period.

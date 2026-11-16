# Data Model: Spec Annotations on Tests

## Annotation (on an item's side record)

| Field | Note |
|---|---|
| name | as written (`spec`, or an unknown name the checker reports) |
| ids | the string arguments, as written |
| span | from `@` to `)`, for diagnostics and fixes |

An item may carry several annotations (only one `@spec` is valid). Annotations change no checking or
running of the item.

## Spec ID

`PART(/PART)*`, PART = one or more of `A-Z a-z 0-9 _ -`. Opaque: no meaning is given to prefixes.

## Reports

- test record: `spec: [ID...]` (sorted) when annotated.
- `spec_result` record (end of `vo test` with annotated tests): `id`, `tests`, `passed`.
- `spec` record (`vo query specs --json`): `id`, `tests: [{name, kind ("test"|"prop"), file, start}]`.

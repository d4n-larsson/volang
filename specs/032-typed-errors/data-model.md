# Data Model: Typed Errors (spec 032)

## Result type

| Field | Meaning |
|---|---|
| ok type `T` | any type, `void` for "no value" |
| error type `E` | any type but `void`, a function type or a type containing one (E0334); `str` when not written |

Identity: `result[T]` and `result[T, str]` are one type. Two result types are equal when both their
`ok` and error types are.

Values: `ok(v)` / `ok()` and `err(e)`, `e` of type `E`; one variant object each, as today.

## Error type

Usually a variant, one case per way to fail, each carrying what the caller needs (a path, a line
number); may be a record, `int`, `str` or any other allowed type. Rendered, compared, generated and
transferred between tasks as any value of its type.

## Conversion

`map_err(r: result[T, E1], f: fn(E1) -> E2) -> result[T, E2]`: `ok` unchanged, `err(e)` becomes
`err(f(e))`; `f`'s effects are the caller's.

## Rules between results

| Where | Rule |
|---|---|
| `try r` in a function returning `result[U, F]` | `r`'s error type must be `F` (E0412) |
| `main`, tests | `result[void]`: a `str` error |
| `err(x)` | `x` must have the expected error type (E0301) |

## std.web

| Item | Meaning |
|---|---|
| `Problem { status: int; detail: str; }` | the answer to one error: a status and the problem's detail |
| `typed(op, answer)` | an operation that fails with `X`, made into an ordinary operation answering `answer(x)` for `err(x)` |

## std.option

`Option[T] { some(T); none; }` with `or_else`, `is_some`, `to_result`.

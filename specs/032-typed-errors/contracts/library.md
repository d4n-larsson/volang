# Contract: Library (spec 032)

## std.web

```text
export record Problem {
    status: int;
    detail: str;
}

export fn typed[C, I: json, X, E: effects](
    op: fn(C, I) -> result[Response, X] uses E,
    answer: fn(X) -> Problem
) -> fn(C, I) -> result[Response] uses E
```

`on_get("/notes/{id}", typed(show, note_problem))`: `ok(response)` is answered as today; `err(x)` is
answered with `answer(x).status` and an RFC 9457 problem body whose `detail` is `answer(x).detail` (the
same body the prefix form gives). The mapping is an ordinary function with a `match`:

```text
fn note_problem(e: NoteError) -> Problem {
    match e {
        case not_found(id) {
            return new Problem { status: 404, detail: "no note " + str(id) };
        }
        case duplicate(title) {
            return new Problem { status: 409, detail: "a note is already called " + title };
        }
    }
}
```

`err("kind: detail")` and `App.errors` keep working for operations that return `result[Response]`.

## std.option

```text
export variant Option[T] {
    some(T);
    none;
}
export fn or_else[T](o: Option[T], default: T) -> T
export fn is_some[T](o: Option[T]) -> bool
export fn to_result[T, E](o: Option[T], e: E) -> result[T, E]
```

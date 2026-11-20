# Contract: header layout (spec 030)

At most 100 characters: unchanged.

```vo
fn handle(db: Database[uses file.read, file.write], r: Request) -> Response
    uses file.read("data/"), file.write("data/") {
    ...
}
```

Still too long without the clause:

```vo
fn import_rows(
    db: Database[uses file.read, file.write],
    rows: list[map[str, str]],
    on_error: fn(int, str) -> result[void] uses console
) -> result[int] uses console, file.read("data/"), file.write("data/") {
    ...
}
```

The clause stays on the `) -> Type` line when that line fits; otherwise it goes on its own line as in
the first rule. Function types are never broken. An
anonymous function's parameters follow the second rule. (Comments between parameters are a syntax error, E0110, so there are none to keep.)

# Contract: `std.sqlite`

Module header: "std.sqlite: SQLite databases in files (file.read, file.write) or in memory (no effect),
with checked statements and ? parameters." Import items one by one: `import std.sqlite.NAME;`.

## Types

```vo
# An open database: from open, open_readonly or open_memory; close it with close. Its type names the
# effects it was opened with: Database[uses file.read, file.write], Database[uses file.read], or
# Database (in memory: none). Using it needs those effects.
export record Database[E: effects] {
    id: int;
}

# One SQL statement with ? parameters: sql("...") (checked by vo check) or parse_sql(s).
export record Statement {
    text: str;
}

# A parameter or column value.
export variant Value {
    integer(int);
    real(float);
    text(str);
    blob(bytes);
    null;
}

# One result row: its column names and values, in the statement's column order.
export record Row {
    columns: list[str];
    values: list[Value];
}

# What a changing statement did: the rows it changed and the row id of the last row it inserted (0 if it inserted none).
export record Changes {
    changed: int;
    last_id: int;
}
```

## Functions

| Function | Signature | Effects | Notes |
|---|---|---|---|
| `open` | `(path: str) -> result[Database[uses file.read, file.write]]` | `file.read`, `file.write` | read-write; creates the file if missing |
| `open_readonly` | `(path: str) -> result[Database[uses file.read]]` | `file.read` | changing statements return `err("readonly: ...")`; a missing file is an `err` |
| `open_memory` | `() -> Database` | none | a new, empty, private in-memory database |
| `close` | `[E: effects](db: Database[E])` | none | ends the connection; using it afterwards is R0007 |
| `sql` | `(text: str) -> Statement` | none | `text` must be a string literal: checked by `vo check` (E0323–E0325) |
| `parse_sql` | `(text: str) -> result[Statement]` | none | for SQL built at run time: `err` on a syntax error or more than one statement |
| `exec` | `[E: effects](db: Database[E], st: Statement, params: list[Value]) -> result[Changes]` | `E` | for statements that change data or the schema |
| `query` | `[E: effects](db: Database[E], st: Statement, params: list[Value]) -> result[list[Row]]` | `E` | every row, in order |
| `transaction` | `[T, E: effects, F: effects](db: Database[E], f: fn() -> result[T] uses F) -> result[T] uses E, F` | `E`, `F` | `BEGIN`; `f`; `COMMIT` on `ok`, `ROLLBACK` on `err` (returned as is); nested: `err("error: a transaction is already open")` |
| `get` | `(row: Row, column: str) -> result[Value]` | none | `err("no column NAME")`; `err("ambiguous column NAME: name each column with AS")` when two columns have it |
| `get_int` | `(row: Row, column: str) -> result[int]` | none | `err` unless the value is `int` |
| `get_float` | `(row: Row, column: str) -> result[float]` | none | `err` unless `float` (an `int` is not converted) |
| `get_str` | `(row: Row, column: str) -> result[str]` | none | `err` unless `text`; never for invalid UTF-8 (read it with `get_bytes`) |
| `get_bytes` | `(row: Row, column: str) -> result[bytes]` | none | `err` unless `bytes` or `text` |
| `is_null` | `(row: Row, column: str) -> result[bool]` | none | |

`exec`, `query` and `transaction` need the effects in the database's type (`E`): a function that uses a
file database declares its file effects, one that uses an in-memory database declares none (spec FR-015).
At run time every statement checks the connection's effects and path against the running grant
(R0010), so a hand-built `Database` cannot bypass the check (FR-016); the files a statement itself opens
(attached databases, `VACUUM INTO`) are checked the same way, since their paths are inside SQL.

## Errors (`err` texts)

`KIND: MESSAGE`, KIND one of `constraint`, `busy`, `readonly`, `corrupt`, `nomem`, `error` (anything
else), MESSAGE SQLite's message. Refused pragmas (`temp_store_directory`, `data_store_directory`,
`journal_mode = wal`) give `error: PRAGMA NAME is not available in vo` (and E0323 for literals). Examples: `constraint: UNIQUE constraint failed: items.name`,
`error: no such table: itemz`, `busy: database is locked`.

Run-time errors (program stops): R0007 for a closed, forged or other-process `Database`; R0010 for a
file outside the grant; R0009 at the deadline; R0006 when rows exceed `--max-memory`.

## `vo doc` examples (each checked by `tests/doc_conformance.py`)

```text
open_memory:   len(try query(open_memory(), sql("select 1 as x"), [])) is 1
sql:           sql("select ?").text is "select ?"
parse_sql:     parse_sql("selec 1") == err("near \"selec\": syntax error") is true
exec:          (try exec(db, sql("insert into t values (?)"), [Value.integer(1)])).changed is 1   (with a db made in the example's setup)
get_int:       get_int(new Row { columns: ["x"], values: [Value.integer(7)] }, "x") == ok(7) is true
get_float:     get_float(new Row { columns: ["x"], values: [Value.integer(7)] }, "x") == err("x is integer, not real") is true
```

(Exact texts are fixed in `std/sqlite.vo`; examples needing a database use `open_memory()`.)

## Reference text (budget)

Standard-library table row:

> | std.sqlite | `open` → `Database[uses file.read, file.write]`, `open_readonly` → `Database[uses file.read]`, `open_memory` → `Database`; `exec`, `query` (need the database's effects) with `sql("...")` (checked) and `?` parameters (`Value`); `transaction`; `get_int`, `get_str`, ... |

Generics section (language, FR-014), one bullet:

> - Records and variants can take an effect parameter, `record Database[E: effects] { ... }`. In a type,
>   effects come last, after `uses`: `Database[uses file.read, file.write]`; without them the type has no
>   effects (`Database`). A function using such a value declares `uses E`, as for a function value.

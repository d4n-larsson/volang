# Contract: SQL diagnostics

Spans are byte ranges in the source; inside a SQL literal they cover the source bytes of the SQL
concerned (from `sqlite3_error_offset`, mapped with `lit_decode`).

## E0323 `sql_invalid`

- **When**: the literal of `sql(...)` has a syntax error, an incomplete statement, an unrecognized
  token, more than one statement, or calls a function that reads the clock or randomness.
- **Messages**: SQLite's own for syntax (`near "selec": syntax error`, `incomplete input`,
  `unrecognized token: "#"`); "One statement per `sql(...)`: split it into one call per statement.";
  "`random()` reads randomness: compute the value in vo (std.random) and pass it as a parameter.";
  "`CURRENT_TIMESTAMP` reads the clock: compute the time in vo (std.time) and pass it as a parameter."
- **Span**: the offending token (syntax), the second statement (more than one), the function call.
- **Fix**: for more than one statement, when the `sql(...)` call is the statement argument of an
  `exec` call that is itself a whole statement whose result is `try`d or discarded with `let _`: one
  such `exec` per statement. Otherwise none.

## E0324 `sql_not_literal`

- **When**: `sql(x)` where `x` is not a string literal.
- **Span**: the argument.
- **Message**: "Statements are written as literals, so values can only be parameters (no SQL
  injection); use `?` for values, or `parse_sql(s)` for SQL that must be built at run time."
- **Fix**: none.

## E0325 `sql_parameters`

- **When**: a named or numbered parameter (`:name`, `@name`, `$name`, `?1`), or a parameter list written
  in an `exec`/`query` call whose length differs from the statement's `?` count.
- **Messages**: "Parameters are `?`, taken from the list in order; `$1` is a second spelling."; "The
  statement has $1 parameter(s) and the list has $2."
- **Fix**: named or numbered parameters each used once (and, for numbered, in order 1, 2, ...): each
  becomes `?`. None otherwise.

## E0326 `sqlite_memory_path`

- **When**: `open(":memory:")` or `open_readonly(":memory:")`.
- **Message**: "An in-memory database needs no file and no effect: use `open_memory()`."
- **Fix**: the call → `open_memory()` (for `open`, which returns a `result`: `ok(open_memory())`).

## E0327 `effect_argument_syntax`

- **When**: effects written as type arguments without `uses` (`Database[file.read]`,
  `Conn[console, net]`), or `uses` not last in the brackets, or effect arguments given to a type that
  has no effect parameter.
- **Message**: "Effects in a type are written last, after `uses`: `$1`." / "`$1` has no effect
  parameter."
- **Fix**: the rewritten type (`Database[uses file.read]`) for the first case; none otherwise.

Missing effects through a handle are the existing E0501 (no new code); a handle type that does not
match is the existing E0301.

## Run time

| Situation | Report |
|---|---|
| `parse_sql` of bad SQL | `err` with the E0323 message |
| a database failure | `err("KIND: MESSAGE")` (contracts/library.md) |
| a closed or forged `Database`, or one opened in another task | R0007 at the user's call |
| a statement on a connection whose effects or path the running grant does not cover (a hand-built `Database`) | R0010 at the user's call |
| SQL naming a file outside the grant | R0010 at the user's call |
| `--timeout` during a statement | R0009 at the call |

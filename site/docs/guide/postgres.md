# PostgreSQL

`std.sqlite` keeps a program's data in a file on its own machine. A program that runs on more than
one machine, or beside other programs that share its data, needs a database server. The `postgres`
package is a client for PostgreSQL, written in vo on `std.tcp` and `std.crypto`. It has the names and
shapes of `std.sqlite`, so what you know from one works on the other, and a program moves between them
by changing its imports, the code that opens the database and the SQL the two spell differently.

## Adding the package

The package is the directory `packages/postgres` of the vo repository. Pin it with
[`vo package`](../tools/package.md), from a release archive or from a copy on disk:

```text
vo package main.vo postgres https://github.com/d4n-larsson/volang/archive/refs/tags/TAG.tar.gz#packages/postgres
vo package main.vo postgres path/to/volang/packages/postgres
```

The output lists what the package can do: `net` (it connects to the server) and `entropy` (it draws
the random part of signing in). Import its items one by one from `postgres.client`.

## Connecting

```text
import postgres.client.Options;
import postgres.client.connect;

fn main() -> result[void] uses console, net("db.internal:5432"), entropy {
    let db = try connect(new Options {
        host: "db.internal",
        port: 5432,
        database: "app",
        user: "app",
        password: "...",
        plain_text: false,
        connect_ms: 10000,
        statement_ms: 30000
    });
    ...
}
```

- **The grant names the server.** `connect` declares `net` without scopes, because it is given its
  host; the program's `net("db.internal:5432")` applies to it, and any other host stops the program
  with [R0010](../errors/R0010.md) (see [Effects](effects.md#functions-that-are-given-their-target)).
- **TLS by default.** With `plain_text: false` the connection is encrypted before anything is sent,
  and the server's certificate must verify for `host`, from the machine's trust store or `--ca-file`.
  A server without TLS is an error, not a fallback. `plain_text: true` is for a server on the same
  machine or in a test, and it is visible in the program's source.
- **Signing in** uses SCRAM-SHA-256: the password is never sent, only a proof made from it, and the
  server must prove that it knows the password too. A server that asks for an MD5 or a clear-text
  password is refused.
- **Time limits.** Every wait while connecting has `connect_ms`, every wait of a statement
  `statement_ms`.

A database's type names `net`, and using it needs `net`: write code for any database generically, as
with `std.sqlite`: `fn save[D: effects](db: Database[D], ...) uses D`.

## Statements and values

`sql`, `parse_sql`, `exec`, `query`, `transaction`, `close` and the readers `value_column`, `int_column`,
`float_column`, `text_column`, `bytes_column`, `is_null` are those of [`std.sqlite`](standard-library.md), with
the same parameters, results and error texts. Parameters are `?` in order, as in SQLite; the package
numbers them for the server. `??` is a literal `?` (PostgreSQL's `jsonb` operators), and `$1` is
refused. Values travel only as parameters: no function puts a value into the SQL text.

| PostgreSQL column | Value | Read with |
|---|---|---|
| `smallint`, `integer`, `bigint` | `integer` | `int_column` |
| `boolean` | `integer` 0 or 1 (as SQLite stores it) | `int_column` |
| `real`, `double precision` | `real`; `NaN` and the infinities are `text` | `float_column` |
| `bytea` | `blob` | `bytes_column` |
| `text`, `varchar`, `char` | `text` | `text_column` |
| `numeric` | `text`, never a float | `text_column`, then `std.decimal.parse_decimal` |
| `timestamptz`, `timestamp`, `date` | `text`, in UTC | `time_column` (milliseconds since 1970 UTC), or `text_column` |
| anything else (`uuid`, `jsonb`, arrays, ...) | `text`, the server's text form | `text_column` |
| `NULL` | `null` | `is_null` |

Every session is set to UTF-8, UTC and ISO dates when it connects, so no result depends on the
server's configuration.

## Errors

Every failure is an `err` whose first word says what kind it is. The first four are those of
`std.sqlite`, with the same meaning:

| Name | Meaning | The connection afterwards |
|---|---|---|
| `constraint` | a constraint failed (`unique`, `not null`, a foreign key) | usable |
| `readonly` | the database or transaction does not allow changes | usable |
| `busy` | a lock, a deadlock or a serialization failure: trying again can succeed | usable |
| `error` | any other failure of a statement | usable |
| `auth` | signing in failed | closed |
| `tls` | the server offers no TLS, or its certificate failed | closed |
| `timeout` | a time limit passed | closed |
| `connection` | refused, not found, lost, or already closed | closed |
| `protocol` | the other end does not speak PostgreSQL's protocol | closed |

A failure the server reports carries its SQLSTATE and message, and the position in your statement when
the server gives one: `error: 42703 column "nope" does not exist (at position 8)`.

## Moving between SQLite and PostgreSQL

A program written against `std.sqlite` moves to PostgreSQL by changing:

1. the imports, from `std.sqlite.NAME` to `postgres.client.NAME`;
2. the code that opens the database: `connect(options)` in place of `open(path)`, and a test server in
   place of `open_memory()`;
3. the effects: `net("host:port"), entropy` in place of `file.read`, `file.write`;
4. the SQL that the two databases spell differently.

Keep a program's statements in one file and the move is a small, reviewable change. The parts of SQL
that differ:

| What | SQLite | PostgreSQL |
|---|---|---|
| a generated id | `id integer primary key` | `id serial primary key`, or `id integer generated always as identity` |
| the id of a new row | `last_id` of `exec`, or `returning id` | `insert ... returning id`, run with `query` (`last_id` is always 0); use `returning` on both |
| insert or update | `insert ... on conflict (k) do update set ...` | the same |
| booleans | `integer` 0 or 1 | `boolean`, read as 0 or 1 |
| dates and times | text you write (`std.time.format_time`) | `timestamptz`, read with `time_column` |
| the clock and randomness in SQL | refused: pass the value as a parameter | `now()`, `random()` run on the server (and are recorded) |
| text comparison | `like` ignores ASCII case | `like` is exact; `ilike` ignores case |
| column types | any value in any column | the declared type is enforced |
| transactions | serializable | read committed; a conflict is `busy` |

## Testing

PostgreSQL has no in-memory database, so tests of database code need a server: a container on the
test machine is the usual one. A test's clause is its grant:

```text
test "notes are kept" uses net("127.0.0.1:5432"), entropy {
    ...
}
```

`vo run --record` keeps everything the server sent, and [`vo replay`](../tools/replay.md) runs the
program again on that recording with no server. The recording never holds the password.

## What `std.sqlite` checks that this package cannot

`vo check` knows `std.sqlite`'s `sql` and checks its SQL before the program runs: the syntax, one
statement, and the number of `?`. For PostgreSQL the same rules are checked when the statement runs,
before anything is sent, and the syntax is the server's to report.

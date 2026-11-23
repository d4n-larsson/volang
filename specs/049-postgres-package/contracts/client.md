# Contract: `postgres.client`

```text
vo package main.vo postgres URL-or-DIRECTORY
import postgres.client.connect;   # one item per import
```

## Items shared with `std.sqlite` (same signature, same meaning)

| Item | Signature |
|---|---|
| `sql` | `(text: str) -> Statement` |
| `parse_sql` | `(text: str) -> result[Statement]` |
| `exec` | `[E: effects](db: Database[E], st: Statement, params: list[Value]) -> result[Changes] uses E` |
| `query` | `[E: effects](db: Database[E], st: Statement, params: list[Value]) -> result[list[Row]] uses E` |
| `transaction` | `[T, E: effects, F: effects](db: Database[E], f: fn() -> result[T] uses F) -> result[T] uses E, F` |
| `close` | `[E: effects](db: Database[E]) uses E` |
| `get` | `(row: Row, column: str) -> result[Value]` |
| `get_int`, `get_float`, `get_str`, `get_bytes` | `(row: Row, column: str) -> result[int \| float \| str \| bytes]` |
| `is_null` | `(row: Row, column: str) -> result[bool]` |

Differences in behaviour, all in the guide:

| Item | `std.sqlite` | `postgres.client` |
|---|---|---|
| `sql` | must be a literal; checked by `vo check` | any text; checked when run (one statement, `?` only, parameter count) |
| `parse_sql` | full syntax check | one statement, `?` only; syntax is the server's to report |
| `close` | no effect needed | `uses E` (it tells the server) |
| `Changes.last_id` | the inserted row id | 0; use `returning` |
| getter `err` texts | `x is text, not integer` | the same texts |

## Items of this package only

| Item | Signature | Notes |
|---|---|---|
| `Options` | record, see [data-model.md](../data-model.md) | |
| `connect` | `(o: Options) -> result[Database[uses net]] uses net, entropy` | TLS unless `o.plain_text`; SCRAM-SHA-256 or no password asked. Depends on research R1 for scoped grants |
| `get_time` | `(row: Row, column: str) -> result[int]` | milliseconds since 1970 UTC from a `timestamptz`, `timestamp` (as UTC) or `date` |

## Parameters

`?` in order. `??` is a literal `?`. `$1` is refused. One statement per text. At most 65,535 values.

## Failure names (the first word of every `err` from this module, except the getters)

| Name | Meaning | Connection afterwards |
|---|---|---|
| `constraint` | a constraint failed (SQLSTATE class 23) | usable |
| `readonly` | changes are not allowed (25006) | usable |
| `busy` | lock not available, deadlock, serialization failure: trying again can succeed | usable |
| `error` | any other failure of a statement, or a statement refused before sending | usable |
| `auth` | signing in failed, or the server asked for an unsupported method | closed |
| `tls` | the server offers no TLS, or its certificate failed (the reason as `std.tcp` words it) | closed |
| `timeout` | a time limit passed | closed |
| `connection` | refused, not found, unreachable, lost, or `closed` | closed |
| `protocol` | the peer sent something that is not the protocol | closed |

## Column types

| PostgreSQL | read as | getter |
|---|---|---|
| `smallint`, `integer`, `bigint`, `oid` | `integer` | `get_int` |
| `boolean` | `integer` 0 or 1 | `get_int` |
| `real`, `double precision` (finite) | `real` | `get_float` |
| `bytea` | `blob` | `get_bytes` |
| `text`, `varchar`, `char`, `name` | `text` | `get_str` |
| `numeric` | `text`, decimal form | `get_str`, then `std.decimal.parse_decimal` |
| `timestamptz`, `timestamp`, `date` | `text` | `get_time`, or `get_str` |
| everything else, and `NaN` / infinite floats | `text`, the server's text form | `get_str` |
| `NULL` | `null` | `is_null` |

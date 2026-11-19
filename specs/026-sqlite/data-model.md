# Data Model: SQLite Databases

## vo types (`std.sqlite`, contracts/library.md)

| Type | Fields / cases | Rules |
|---|---|---|
| `Database[E: effects]` | `id: int` | index into the C connection table; valid only while open and in the process that opened it (else R0007); `E` is the effect set it was opened with: `uses file.read, file.write` (`open`), `uses file.read` (`open_readonly`), none (`open_memory`) |
| `Statement` | `text: str` | exactly one statement; `?` parameters only; from `sql` (literal, checked) or `parse_sql` |
| `Value` | `integer(int)`, `real(float)`, `text(str)`, `blob(bytes)`, `null` (SQLite's storage classes; `int`, `float` and `bytes` are vo keywords) | case order fixed (the C side builds them by index) |
| `Row` | `columns: list[str]`, `values: list[Value]` | same length; column names as SQLite reports them (`AS` aliases); a getter on a name two columns share is an `err` |
| `Changes` | `changed: int`, `last_id: int` | `sqlite3_changes64`; `last_id`: `sqlite3_set_last_insert_rowid(db, 0)` before the statement, `sqlite3_last_insert_rowid` after, so it is the row id this statement inserted or 0 |

Value mapping (SQLite storage class → `Value`): INTEGER → `int`, REAL → `float`, TEXT → `text` when valid
UTF-8 (else `bytes`, so `get_str` is an `err` and `get_bytes` works), BLOB → `bytes`, NULL → `null`.
Parameters bind the other way: `int` → `sqlite3_bind_int64`, `float` → `_double`, `text` → `_text`
(UTF-8, length given), `bytes` → `_blob`, `null` → `_null`.

## C connection table (`src/sqlite_rt.c`)

| Field | Meaning |
|---|---|
| `db` | `sqlite3 *`, or 0 when the slot is free |
| `pid` | the process that opened it (tasks are processes) |
| `readonly` | opened with `open_readonly` |
| `effects`, `path` | the effect bits it was opened with (`file.read`, `file.write`, or none) and the path; every statement checks them against the running grant (`rt_check_path`), R0010 otherwise (FR-016) |
| `in_tx` | a `transaction` is running (nesting is an `err`) |
| `stmts` | up to 64 prepared statements keyed by text (round-robin) |

64 slots; opening a 65th connection is `err("error: too many open databases (64)")`. Every connection is
closed at program end (an open transaction rolls back) and, in `vo test`, at the end of the test that
opened it.

## Primitives (std-only)

| Id | Name | Arguments | Result |
|---|---|---|---|
| 87 | `__sql_open` | `path: str`, `mode: int` (0 read-write create, 1 read-only, 2 memory) | `result[int]` (the id) |
| 88 | `__sql_close` | `id: int` | — |
| 89 | `__sql_exec` | `id: int`, `text: str`, `params: list[Value]` | `result[list[int]]`: `[changed, last_id]` |
| 90 | `__sql_query` | `id: int`, `text: str`, `params: list[Value]` | `result[list[list[Value]]]`: first list the column names as `text` values, then the rows |
| 91 | `__sql_tx` | `id: int`, `op: int` (1 begin, 2 commit, 3 rollback) | `result[void]` |
| 92 | `__sql_prepare` | `text: str` | `result[str]`: `ok("")`, or `err` with the E0323/E0325 message (for `parse_sql`) |

`__sql_exec` and `__sql_query` take a `Value` list, a std type: typed loosely in the checker (as
`__coll`, spec 014), with the std wrappers declaring the public types.

## Limits

| Limit | Value |
|---|---|
| SQLite heap (memsys5) | 64 MiB per program (`nomem` when full) |
| open connections | 64 |
| cached statements per connection | 64 |
| busy wait | 5 s, never past `--timeout` |
| SQLite's own limits | defaults (`SQLITE_MAX_LENGTH`, `SQLITE_MAX_SQL_LENGTH`, ...) |

## Files on disk

The database file and, during a write transaction, `NAME-journal` next to it (the same scope). No WAL
files (compiled out), no temporary files (memory).

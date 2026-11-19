# Contract: the `sql` recording event

One JSON record per `std.sqlite` primitive call in `vo run --record FILE` (schema 1 adds a kind; fields
are only added). Fields:

| Field | Type | Meaning |
|---|---|---|
| `kind` | `"sql"` | |
| `op` | `"open"`, `"open_readonly"`, `"open_memory"`, `"exec"`, `"query"`, `"begin"`, `"commit"`, `"rollback"`, `"close"` | |
| `db` | int | the connection number the program saw |
| `path` | str | `open` / `open_readonly` only |
| `sql` | str | `exec` / `query` only: the statement text |
| `params` | list | `exec` / `query` only: each parameter as `{"int":5}`, `{"float":"3ff8000000000000"}` (the IEEE-754 bits, 16 hex digits), `{"text":"..."}`, `{"bytes":"..."}` (bytes >= 0x80 and control bytes as `\u00XX`, like every byte string in a recording) or `{"null":true}` |
| `result` | `"ok"`, `"err"` or `"denied"` | `denied`: SQLite was refused a file (R0010 ended the run) |
| `effect`, `target` | str | on `denied`: `"file.read"` or `"file.write"`, and the refused path |
| `error` | str | on `err`: the `err` text |
| `changed`, `last_id` | int | `exec` on `ok` |
| `columns` | list of str | `query` on `ok` |
| `rows` | list of lists | `query` on `ok`: values encoded as `params` |

The fields are written in this order, without spaces. Replay reads the events in order: each primitive
call must match the next `sql` event's `op`, `db`, `path`, `sql` and `params` (the line up to `result`,
byte for byte; else R0014, which says when only the connection or parameters differ) and gets its
outcome without opening any file or running SQLite; a `denied` outcome is the same R0010 again.
`parse_sql` and `vo check` use no outside input and are not recorded. A recording of a program that wrote to a database holds what each statement
returned, never the database file itself.

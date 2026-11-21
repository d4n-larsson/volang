# Data Model: Streaming

## Piece and answer

| Stream | Piece | Limits |
|---|---|---|
| `each_line` | `str`: one line without `\n` or `\r\n` | 16 MiB per line; UTF-8 |
| `each_chunk` | `bytes`: exactly `size` bytes, the last one fewer (never empty) | `size` 1 to 16 MiB |
| `send_stream` | `bytes`: 1 to 64 KiB of the body, with the response head | no total limit; head at most 64 KiB |
| `run_lines` | `str`: one line of standard output | 16 MiB per line; UTF-8 |

Answer of the passed function: `ok(true)` go on; `ok(false)` stop, the stream succeeds; `err(m)` stop,
the stream returns `err(m)`.

## Followed child (one of 16 in `src/process.c`)

pid; owner pid; pipe ends for input, output, error (-1 when closed); the input not yet written; a line
buffer (mapped, grows to 16 MiB); a standard error buffer (the existing output limit); the exit
status once reaped. States: free → running → ended (status kept) → free on reuse.

## Recording events (added)

| Kind | Fields |
|---|---|
| `read_file` (existing) with `offset` | `path`, `offset` (present for block reads only, absent for `read_file` and `read_bytes`), `result`, `value` the block's bytes or the error |
| `proc_line` | `program`, `result`, `value` the line or the error (`eof` at the end) |
| `proc_end` | `program`, `status`, `value` standard error |

A streamed HTTP request is recorded as `tcp_open`, `tcp_recv` and `tcp_send` events (spec 039).

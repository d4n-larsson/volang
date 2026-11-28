# Data Model: Events (spec 063)

## `std.sse`

| Type | Fields | Rules |
|---|---|---|
| `StreamEvent` (record) | `name: str`, `data: str`, `id: str` | received: `name` is `"message"` when the stream gave none; `id` is the last id the stream set (`""` before any). Sent: `name` and `id` hold no CR, LF or NUL; `data` may hold line breaks (one `data:` line each) |
| `StreamEnd` (record) | `why: EndReason`, `last_id: str`, `retry_ms: int` | `last_id`: of the last event handed over, or the `last_id` given; `retry_ms`: the server's last `retry:` value, -1 when none |
| `EndReason` (variant) | `ended`, `broken`, `timed_out`, `stopped`, `not_a_stream(int)`, `too_large`, `failed(str)` | exactly one per reading |
| `StreamOptions` (record) | `headers: list[http.Header]`, `max_open: int` | `headers` go into the stream's head; `max_open` 0 to 63 (0: half the workers) |
| `EventStream[E: effects]` (record) | `conn: tcp.Conn[uses E]`, `sent: list[StreamEvent]`, `scripted: bool`, `state: int` (0 open, 1 client_gone, 2 not_reading, 3 stopping) | `EventStream[uses serve]` from `serve_events` (connected) or `EventStream` from `scripted_stream` (collects in `sent`) |
| `StreamError` (variant) | `client_gone`, `not_reading`, `stopping`, `invalid(str)` | after any but `invalid` every later send answers the same |

Parser state while reading (internal): the bytes of an unfinished line, the fields of the event being
built (data lines, name), the last id, the retry value, the size so far.

## `std.websocket`

| Type | Fields | Rules |
|---|---|---|
| `Message` (variant) | `text(str)`, `binary(bytes)` | whole; a text is valid UTF-8 |
| `ConnectOptions` (record) | `headers: list[http.Header]`, `protocols: list[str]`, `max_message: int` | `headers` may not set `Host`, `Upgrade`, `Connection` or any `Sec-WebSocket-*`; `max_message` 0 (1 MiB) or 1 to 16777216 |
| `AcceptOptions` (record) | `origins: list[str]`, `protocols: list[str]`, `max_message: int`, `max_open: int` | `origins`: exact `scheme://host[:port]` texts; `[]` only requests without `Origin`; `["*"]` any |
| `Socket[E: effects]` (record) | `conn`, `protocol`, `client`, `state` (0 open, 1 closed, 2 broken, 3 stopping), `code`, `reason`, `pong_count`, `limit`, `parts`, `kind`, `parts_len`, `scripted`, `sent`, `incoming` | records are references, so the fields are set in place; `protocol` is the chosen sub-protocol or `""` |
| `WsError` (variant) | `nothing_in_time`, `closed { code: int; reason: str }`, `broken`, `protocol(str)`, `not_text`, `too_large`, `stopping`, `refused(int)`, `connect(tcp.TcpError)`, `invalid(str)` | see the contract |

### Socket states

```text
open ──receive: close frame──▶ closed(code, reason)   (the close is echoed, the connection closed)
open ──close_socket──────────▶ closed(code, reason)   (after the peer's echo or 5 s)
open ──violation by the peer─▶ closed                 (close 1002 / 1007 / 1009 sent; the call returned the error)
open ──connection lost───────▶ broken
open ──server stopping───────▶ closed(1001, "")       (close sent; the call returned stopping)
```

In every state but `open`, sends and receives return the state's error without touching the network.

## The server process (C)

| State | Where | Rules |
|---|---|---|
| a worker's state | `Worker.state` | a fourth value beside starting, ready and busy: talking (busy with a taken connection). Counts as busy for stopping and for replacement when it dies |
| talking count | one integer | workers in state talking; the gauge `http_conversations_open`; a take is granted below `min(max_open, workers - 1)` |
| hand-over channel | one socket pair per worker | carries the descriptor or a refusal down, and the stop byte |
| taken connection | the calling process's `std.tcp` entry, plus its `kind` | closed when the handler returns; a WebSocket gets a 1011 close when the handler fails |

## Recording

One new event kind, `serve_take`: `name` the request's connection label, `result` ok or the refusal
text. Everything else is spec 039's and spec 035's events.

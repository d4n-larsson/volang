# Contract: `std.sse` and `std.websocket` (spec 063)

```text
import std.sse.read_events;        # one item per import, as for every module
import std.websocket.ws_connect;
```

Time limits are milliseconds, 0 to 600000. Types are in [data-model.md](../data-model.md).

**Handlers that can be tested**: the function given to `serve_events` or `ws_accept` is written over
any effect, `fn chat[E: effects](s: Socket[uses E]) -> result[void] uses E`, so that the same function
runs on a `Socket[uses serve]` in a server and on a `Socket` from `scripted_socket` in a test, where
it needs no effect. The same holds for `EventStream`.

## `std.sse`

| Item | Signature | Notes |
|---|---|---|
| `read_events` | `[E: effects](method: str, url: str, headers: list[http.Header], body: str, last_id: str, timeout_ms: int, f: fn(StreamEvent) -> result[bool] uses E) -> StreamEnd uses net, E` | calls `f` per event as it arrives; `f` answers `ok(true)`, `ok(false)` (stop) or an error. `last_id` other than `""` is sent as `Last-Event-ID`. `timeout_ms` bounds each wait (0: 600000). Never reconnects |
| `parse_events` | `(text: str) -> list[StreamEvent]` | the format's rules on text; an unfinished last event is dropped |
| `serve_events` | `[E: effects](r: http.Request, options: StreamOptions, f: fn(EventStream[uses serve]) -> result[void] uses E) -> http.Response uses serve, E` | in a handler: takes the connection, sends the head, runs `f`, ends the stream. The response is for the steps and the log, not the client |
| `send_event` | `[E: effects](s: EventStream[uses E], e: StreamEvent) -> result[void, StreamError] uses E` | on its way when it returns; `e.name` `""` or `"message"` sends no `event:` line, `e.id` `""` no `id:` line |
| `keep_alive` | `[E: effects](s: EventStream[uses E]) -> result[void, StreamError] uses E` | a comment line; the client is handed nothing |
| `scripted_stream` | `() -> EventStream` | for tests: not connected; sending to it needs no effect |
| `sent_events` | `[E: effects](s: EventStream[uses E]) -> list[StreamEvent]` | what was sent to a scripted stream |
| `stream_error_text` | `(e: StreamError) -> str` | a sentence for a person |

`StreamEnd.why`:

| Case | When |
|---|---|
| `ended` | the server ended the response properly |
| `broken` | the connection was lost, or the body ended early |
| `timed_out` | nothing arrived within `timeout_ms` |
| `stopped` | `f` answered `ok(false)` |
| `not_a_stream(status)` | the status is not 200 or the content type is not `text/event-stream` |
| `too_large` | one event is larger than 1 MiB |
| `failed(text)` | the URL, the connect, TLS, an error `f` returned, or a response with a `Content-Encoding` (no `Accept-Encoding` is sent; `std.http` does not decompress) |

`StreamError`: `client_gone`, `not_reading` (30 s without the client taking bytes), `stopping` (the
server is stopping), `invalid(what)` (a line break or NUL in `name` or `id`).

## `std.websocket`

| Item | Signature | Notes |
|---|---|---|
| `ws_connect` | `(url: str, options: ConnectOptions, timeout_ms: int) -> result[Socket[uses net], WsError] uses net` | `ws://` (port 80) or `wss://` (443, certificate checked as `https://`); the scope is the URL's host and port |
| `ws_accept` | `[E: effects](r: http.Request, options: AcceptOptions, f: fn(Socket[uses serve]) -> result[void] uses E) -> http.Response uses serve, E` | in a handler: checks the request and its origin, takes the connection, answers the handshake, runs `f`, closes with 1000 (or 1011 when `f` returns an error) |
| `send_text` | `[E: effects](s: Socket[uses E], text: str) -> result[void, WsError] uses E` | one message |
| `send_binary` | `[E: effects](s: Socket[uses E], data: bytes) -> result[void, WsError] uses E` | one message |
| `receive_message` | `[E: effects](s: Socket[uses E], timeout_ms: int) -> result[Message, WsError] uses E` | the next whole message; 0: no limit. Pings are answered here |
| `ping` | `[E: effects](s: Socket[uses E]) -> result[void, WsError] uses E` | sends a ping |
| `pongs` | `[E: effects](s: Socket[uses E]) -> int` | answers to pings seen so far |
| `close_socket` | `[E: effects](s: Socket[uses E], code: int, reason: str) -> result[void, WsError] uses E` | sends the close, waits up to 5 s for the answer, closes. Harmless to repeat (`ok()`). `code` 1000 to 1003, 1007 to 1011 or 3000 to 4999, `reason` at most 123 bytes: otherwise `invalid` and nothing is sent |
| `scripted_socket` | `(incoming: list[Message]) -> Socket` | for tests: receives give `incoming`, then `closed(1000, "")` |
| `sent_messages` | `[E: effects](s: Socket[uses E]) -> list[Message]` | what was sent to a scripted socket |
| `ws_error_text` | `(e: WsError) -> str` | a sentence for a person |

`WsError`:

| Case | When | The library closes with |
|---|---|---|
| `nothing_in_time` | the receive's limit passed; the socket stays usable | |
| `closed { code, reason }` | the other side closed (or this side did); every later call says the same | the echo of the peer's close |
| `broken` | the connection was lost without a close, or a frame stayed half sent for 30 s | |
| `protocol(what)` | a frame breaks RFC 6455 (reserved bits, unknown opcode, wrong masking, a bad close, a control frame over 125 bytes or in fragments) | 1002 |
| `not_text` | a text message is not UTF-8 | 1007 |
| `too_large` | a message is larger than `max_message` | 1009 |
| `stopping` | the server is stopping (server side only) | 1001 |
| `refused(status)` | connect: the server did not agree to a WebSocket (also: a wrong accept value, an extension or sub-protocol not asked for, status 0) | |
| `connect(tcp.TcpError)` | connect: the error of `std.tcp`'s connect or TLS handshake (`refused`, `timed_out`, `not_found`, `unreachable`, `tls(why)`, ...) | |
| `invalid(what)` | an argument is out of range | |

## What `ws_accept` and `serve_events` answer without a conversation

| Status | When |
|---|---|
| 400 | `ws_accept`: not a GET, or `Sec-WebSocket-Key` is not 16 bytes in base64; either: bytes after the request (`serve_events` takes any method) |
| 403 | an `Origin` not in `origins` (`[]`: only requests without `Origin`; `["*"]`: any) |
| 426 | `Sec-WebSocket-Version` is not 13 (with `Sec-WebSocket-Version: 13`), or no `Upgrade: websocket` |
| 503 | the bound is reached (`Retry-After: 5`); with one worker always |
| 500 | called outside a serving handler (a test through `handle_request`) |

## Limits

| Limit | Value |
|---|---|
| one event | 1 MiB |
| one message | `max_message`: default 1 MiB, 1 to 16 MiB |
| open conversations per server | `max_open` per route (0: half the workers), and never the last free worker |
| a server-side send | 30 s |
| the rest of a frame once begun | 30 s |
| the answer to a close | 5 s |
| handshake response head | 64 KiB |

## On the wire

- **Stream head**: `HTTP/1.1 200 OK`, `Content-Type: text/event-stream`, `Cache-Control: no-store`,
  `X-Accel-Buffering: no`, `Connection: close`, then `StreamOptions.headers`; no `Content-Length`; the
  body ends when the connection closes.
- **StreamEvent**: `event: NAME` (when not the default), `id: ID` (when not empty), one `data: LINE` per line
  of the data, an empty line. Keep-alive: `:` and a line end.
- **WebSocket**: RFC 6455, version 13, no extensions. Client frames masked with 4 fresh key bytes;
  server frames unmasked. Messages are sent in one frame. Sub-protocol: the first of the client's
  list that the server's list holds.

## Internal builtins

| Builtin | Meaning |
|---|---|
| `__serve_take(kind, max_open) -> result[int]` | the connection of the request being handled, as a `std.tcp` id; `kind` 1 stream, 2 WebSocket; errors `busy`, `no connection`, `bytes after the request` |
| `__ws(op, a, b) -> bytes` | `1 + 8 * n`: n key bytes, 1 to 16 (recorded, no effect); 2: `a` XOR the 4-byte key `b`; 3: SHA-1 of `a` |
| `__tcp_io(6, id, data)` | send unless the peer has closed |

## Recording

| StreamEvent | In | Holds |
|---|---|---|
| `tcp_*` (spec 039) | the program, or the request's block | everything received, failed sends, time limits; all sends under `vo test` |
| `serve_take` (new) | the request's block | granted, or the refusal |
| `entropy` (spec 035) | where drawn | key bytes |

## Logs and metrics (spec 055)

- `request_logs` writes its one record when the conversation ends: `status` 200 or 101, `ms` its
  duration, and `end` from the response's `Vo-End` header (`done`, `client_gone`, `not_reading`,
  `stopping`, `closed CODE`, `failed: TEXT`).
- Gauge `http_conversations_open`, set by the server process.

## `std.web` (spec 063)

| Item | Signature | Notes |
|---|---|---|
| `on_request` | `[C, E: effects](method: str, pattern: str, op: fn(C, http.Request, map[str, str]) -> result[http.Response] uses E) -> Route[C, uses E]` | the operation gets the request and the path variables; its pattern is checked when serving starts (`check_routes`), not by `vo check` |

## As built

- `Event` is `StreamEvent` (std.security exports an `Event`); `closed` is a case with fields.
- A taken conversation is not limited by `--timeout`, which limits answering the request.
- On a taken connection every send's outcome is recorded (`tcp_send` without the bytes), so a replay
  of a handler that only sends hands each failure to the right send.
- In recorded tests a `std.tcp` send that is an HTTP request head (a WebSocket handshake) is kept with
  `Authorization`, `Cookie` and `--redact` headers as `[redacted]`.
- A client that ends its side of the connection after its request counts as gone (`client_gone`).

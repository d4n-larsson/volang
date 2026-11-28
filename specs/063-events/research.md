# Research: Events (spec 063)

What was read: `std/http.vo` (`send_stream`, `serve`, `serve_workers`), `std/tcp.vo`, `std/web.vo`
(routes, steps, `request_logs`, `request_metrics`), `src/serve.c`, `src/serve_workers.c`, `src/tcp.c`
(sending, replay), `src/replay.c` (entropy), specs 039, 040, 047, 054, 055.

## R1. How a handler keeps a connection: it takes it

**Found**: the server process owns up to 64 connections, reads each request whole, gives its text to a
free worker over a pipe and writes back the one response text the worker returns. A handler has no
connection, and workers are forked before any connection exists.

**Decision**: a builtin `__serve_take(kind, max_open) -> result[int]`, valid while a handler runs,
moves the connection of the request being answered into the calling process's `std.tcp` table and
returns its id; the library wraps it in a `Conn[uses serve]`.

- Under `serve` (one process) the descriptor moves from `sv_conns` to a `std.tcp` entry.
- Under `serve_workers` the worker sends `TW_TAKE` on its control pipe; the server process answers on
  a socket pair made per worker at spawn, with the descriptor (`sendmsg` with `SCM_RIGHTS`) or a
  refusal, frees its slot and marks the worker as talking. The worker's later `TW_RESPONSE` is
  recorded and logged as always but written to nobody.
- Bytes the client sent after its request (before any answer) make the take fail with 400: both
  protocols forbid them, so nothing has to be carried over.
- `kind` (1 stream, 2 WebSocket) tells the C side how to end a connection the handler left open: when
  the handler returns it is closed; when it stops with a runtime error a WebSocket first gets the
  four bytes of a close frame with code 1011 (FR-010, FR-020).

**Rationale**: after the take, server conversations are ordinary `std.tcp` connections: reads with
time limits, typed errors, recording as `tcp_*` events inside the request's block, replay without a
network and the existing sweep all apply unchanged, and `std.websocket` has one framing code for both
sides.

**Alternatives considered**:
- *Relay through the server process* (the worker sends pieces up, the server forwards bytes down): no
  descriptor passing, but the server needs per-conversation output buffers, back-pressure toward the
  worker and a second framing of every message on the pipes; about three times the C, and the client
  side could share none of it.
- *New serve functions with handlers of another type*: four more exported functions (text, bytes,
  with and without workers) and a second way to write a server.
- *Workers accept connections themselves*: undoes spec 028's limits and replay order.

## R2. The handler's shape: a call that returns a Response

**Decision**: `serve_events(r, options, f)` and `ws_accept(r, options, f)` are called in a handler and
return an `http.Response`. They check the request, take the connection, write the head (or the
handshake), run `f` with the stream or socket, end the conversation, and return a response that is
not sent: status 200 or 101, and a header `Vo-End` with why it ended (`done`, `client_gone`,
`not_reading`, `stopping`, `failed: TEXT`, or for a WebSocket `closed CODE`). When the request is
refused before the take (not a WebSocket request, origin not named, the bound reached) the response
is an ordinary 400, 403, 426 or 503 and is sent.

**Rationale**: `Request -> Response` stays the only handler type, so `serve`, `serve_workers`,
`std.web` routes, steps and `handle_request` need no variants. Steps run before the handler as for
any request (FR-012). `request_logs` already measures the time around the handler and reads a `Vo-`
header (`Vo-Error`); it gains one field, `end`, and FR-015's log record exists.

**Consequence, documented**: a step that changes the response *after* the handler (CORS adds
`Access-Control-Allow-Origin`) cannot change a head already sent. `StreamOptions.headers` carries
headers the stream's head must have; the guide shows the CORS case. A WebSocket handshake needs none
(its origin check is `AcceptOptions.origins`).

`std.web` gets `on_request(method, pattern, op: fn(C, http.Request) -> result[http.Response])`, a
route whose function sees the request itself: today a route's function gets only the bound input, and
both calls need the request's headers.

## R3. The bound on open conversations

**Decision**: the server process counts workers that hold a taken connection. A take with `max_open`
N is granted when the count is below N and below `workers - 1`; `max_open` 0 means half the workers,
rounded down. Otherwise the take is refused and the library answers 503 with `Retry-After: 5`. Under
`serve` (one process) the take is always granted and the server reads nothing else meanwhile, which
the documentation states with the advice to use `serve_workers`.

**Rationale**: the bound is written at the route, where a reviewer reads it, with no global setting
and no change to `serve_workers`' parameters. Keeping one worker free whatever the routes say is what
makes SC-006 hold. With one worker every take is refused; the 503's body says that conversations need
at least two workers.

**Alternatives considered**: a `--conversations N` option (not in the program, so not reviewed); a
setter called before serving (hidden state); a fifth parameter of `serve_workers` (breaks every
caller).

## R4. Masking, the handshake key and SHA-1: one internal builtin

**Decision**: `__ws(op, a, b)`:
1. `n` key bytes from the system's random source, drawn through the path of `__entropy` (so a
   recording holds them and `vo test` gives the same bytes on every run) but without the `entropy`
   effect;
2. `a` XORed with the 4-byte key `b` (masking and unmasking are the same);
3. the SHA-1 of `a` (for `Sec-WebSocket-Accept`; `src/sha1.c` exists, `std.crypto` deliberately does
   not export SHA-1).

**Rationale**: a mask per frame is required by RFC 6455 section 5.3 and must be unpredictable; in vo
code a 1 MiB payload would cost a million bytecode iterations. The bytes never reach the program, do
not influence what it receives, and replay to the same frames, so they are not nondeterminism the
program can observe: the same position TLS's handshake randomness has. `docs/security.md` gets a
sentence saying so. Listed under Complexity Tracking.

## R5. Stopping, a client that is gone, a client that does not read

- **Stop** (FR-014): while a process holds a taken connection, `net_wait` also polls a wake
  descriptor (the socket pair's end in a worker; the signal descriptor under `serve`). The server
  process writes one byte to it at the first stop signal, with or without `--drain`, since a
  conversation does not end by itself. The interrupted `std.tcp` call returns `other("the server is
  stopping")` once and the wake is switched off, so the library can still send: a WebSocket closes
  with 1001, a stream's send returns `stopping`. A handler that has not returned when the server's
  usual stopping time ends is killed with its worker, as a slow request is today.
- **Gone** (SC-005): a stream's client sends nothing after its request, so any readable byte or end
  of file on the connection means it left. `__tcp_io` gets op 6, "send unless the peer has closed":
  a non-blocking look, then the ordinary send; its result is recorded as the send's. The first send
  after the client leaves reports it.
- **Not reading** (story 5, scenario 3): a server-side send waits at most 30 s, then returns
  `not_reading`; nothing is buffered beyond the kernel's socket buffer.
- **Half a frame**: the caller's time limit applies to the wait for a frame's first bytes; once a
  frame has begun the rest must come within 30 s or the connection counts as broken. A time limit
  that passes between the fragments of a message leaves the fragments in the socket for the next
  receive.

## R6. Reading event streams on `send_stream`

**Decision**: `read_events` calls `http.send_stream` with a function that checks the head once
(status 200 and `Content-Type: text/event-stream`, else it stops with `not_a_stream(status)`), feeds
the pieces to the format's parser and calls the program's function per event. The end's reason comes
from where it stopped: the parser's state for `stopped`, `not_a_stream` and `too_large`; the error
text of `send_stream` for the rest (`tcp_error_text(TcpError.timed_out)` is `timed_out`, "the
response ended early" is `broken`, any other error is `failed(text)`); a clean return is `ended`.

**Rationale**: chunked bodies, TLS, scopes, recording and the per-wait time limit are `send_stream`'s
and are not written twice. The comparison of error texts is inside the standard library and pinned by
golden cases for each reason. `timeout_ms` is `send_stream`'s: 0 means 600 s, the most. A stream silent
for ten minutes ends with `timed_out` and the program reads again from `last_id`; servers that mean to
stay silent send comments, which count as arrivals.

**Compression**: `std.http` sends no `Accept-Encoding` and does not decompress, so a response with a
`Content-Encoding` ends the reading with `failed` naming the encoding.

**The parser** follows the HTML standard's "event stream interpretation" exactly (BOM, CR / LF /
CRLF, comments, `data` joined with LF and its last LF removed, `event`, `id` ignored when it holds a
NUL, `retry` digits only, an event without data not dispatched, an unfinished last event dropped). It
is one exported pure function over text, `parse_events`, plus the incremental form used by
`read_events`. An event (all its lines) over 1 MiB ends the reading with `too_large`.

## R7. Recording and replay

- **Clients**: nothing new. Connect, each read, each failed send and each time limit are `tcp_*`
  events; `vo replay` opens no connection. In recorded tests (spec 054) every send is compared with
  the recording; under `vo replay` sends are not compared (spec 039). The spec's FR-017 is worded to
  match.
- **Servers**: the take is a new event, `serve_take`, with its result (granted, or refused and why),
  written in the request's block; the conversation's `tcp_*` events follow it there. Replay runs the
  handler in the replaying process as for every request, and the take returns an entry without a
  descriptor, as a replayed `connect` does.
- **A long conversation while recording**: its block is complete only when it ends. Later blocks wait
  in the ring of 4,096 and are written out of order beyond that (spec 028's existing shortcut); the
  block itself grows with what the conversation received. Stated in the guide: record conversations
  of bounded length.
- **Key bytes** are `entropy` events (R4).

## R8. Testing handlers without a port

**Decision**: `scripted_stream()` and `scripted_socket(incoming)` build an `EventStream` and a
`Socket` that are not connected: sends append to a list the test reads with `sent_events` and
`sent_messages`; receives take from `incoming` and then answer `closed(1000, "")`. They need no
effect. A test calls the function it would pass to `serve_events` or `ws_accept`.

**Rationale**: about 40 lines of vo, no C, no socket, no timing. `handle_request` on a conversation
route outside a server gets a 500 from the failed take; the guide says to test the function, not the
route.

**Alternative considered**: a socket pair in one process (the test writes its messages first, then
runs the handler): limited by the kernel's buffer and blocked forever by a handler that waits for
more.

## R9. Conformance and independent peers (SC-004, SC-007)

- **WebSocket**: the Autobahn test suite (`crossbario/autobahn-testsuite` in Docker, which is on the
  development machine) against both roles, sections 1 to 7, 9 and 10; 12 and 13 are compression and
  are excluded. `tests/autobahn.sh` runs it; it is not in `make test` (it needs the image). The
  maximum message is set to 16 MiB for section 9.
- **In `make test`**: `tests/ws_peer.py`, a client and server written from the RFC with the Python
  standard library (no WebSocket package is installed), with a table of at least 200 hostile
  sequences; `tests/events.py --fuzz` sends 100,000 mutated sequences per role in the slow run and
  2,000 in `make test`.
- **StreamEvent streams**: the web-platform-tests `eventsource/format-*` cases, ported as text vectors for
  `parse_events`; `curl -N` as an independent client of `serve_events`. No independent event *server*
  is installed, so for the reading role the ported cases stand in for a peer; the results document
  says so against SC-004.

## R10. Names, modules, the reference

- **Two modules**, `std.sse` and `std.websocket`. Exported names are unique across the standard
  library (`tests/std_names.py`), hence `receive_message` (not `receive`), `close_socket`,
  `ws_connect`, `ws_accept`, `ws_error_text`.
- **Time limits** are `timeout_ms: int` as in `std.tcp`; for `receive_message`, 0 means no limit (a
  loop over the 600 s maximum), because waiting for hours is the ordinary case.
- **Closing codes**: `close_socket` accepts the codes RFC 6455 lets an endpoint send (1000 to 1003,
  1007 to 1011, 3000 to 4999); another code is `invalid`, not replaced.
- **Pings**: `ping(s)` sends one; `pongs(s)` is how many answers have arrived so far. No queue and no
  third message kind.
- **The reference** is at 15,000 tokens. Two table rows cost about 90; the same amount is cut from
  the `std.web` and `std.http` rows, stated in the change.
- **Version**: a minor release (new modules, nothing changed).

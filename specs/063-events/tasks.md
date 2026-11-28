---

description: "Task list for vo-lang spec 063: Events"
---

# Tasks: Events (server-sent events and WebSockets, as client and as server)

**Input**: Design documents from `specs/063-events/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R10), data-model.md, contracts/events.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases and its group in `tests/events.py`
land first and fail before its code.

**Organization**:
- Setup: the two modules registered, the builtins' shells, the test scripts.
- US1 (P1) reading event streams; US2 (P1) the WebSocket client. Neither needs new server code, and
  the two are independent of each other.
- The take (Phase 5): a handler keeping its connection. Not a story, but US3, US4 and US5 need it;
  US1 and US2 do not.
- US3 (P2) serving a stream; US4 (P2) serving a WebSocket (needs US2's framing); US5 (P2) the bound,
  stopping, logs and the gauge.
- US6 (P1) recordings, scopes and tests: placed after the four roles exist, because its test replays
  all four. Its client half (T014, T022) is done inside US1 and US2.
- US7 (P2) example and documentation. Polish: conformance, three architectures, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Types, signatures, errors, statuses, limits, wire format**: contracts/events.md and
  data-model.md. Limits, verbatim: "one event: 1 MiB"; "one message: `max_message`: default 1 MiB, 1
  to 16 MiB"; "a server-side send: 30 s"; "the rest of a frame once begun: 30 s"; "the answer to a
  close: 5 s"; "handshake response head: 64 KiB"; `max_open` "0 to 63 (0: half the workers)".
  Decisions: research.md R1 to R10.
- **Library code is vo**: `std/sse.vo` on `std.http.send_stream`, `std/websocket.vo` on `std.tcp`.
  Models: `send_stream` and its helpers in `std/http.vo`; the error mapping (`fail`) in `std/tcp.vo`.
  Exported names must be unique across the standard library (`tests/std_names.py`).
- **C code**: the take and the wake in `src/serve.c`, `src/serve_workers.c`, `src/tcp.c`, `src/net.c`;
  `src/ws.c` is new. Every heap pointer held across an allocation is rooted; after each C task run
  `make STRESS=1` and `tests/run.sh --stress 1` on the cases the task touches.
- **Golden cases** live in `tests/cases/sse/` and `tests/cases/websocket/` and need no network: they
  use `parse_events`, `scripted_stream`, `scripted_socket` and the argument checks. A case needs a
  `.code` file; accept with `--accept` and review the diff. Everything that needs a peer is in
  `tests/events.py`.
- **No existing golden case may change**, except `tests/cases/doc/all.out` and `all_json.out` (the
  module listing). `serve`, `serve_workers`, `tcp` and `stream` cases must pass unchanged after every
  phase: `make quick AREA=serve_workers` and `AREA=tcp`.
- **Every wait is bounded in tests**: scripts run programs with `--timeout` and a subprocess timeout.
- **Doc comments**: every exported item has one with examples in the form `#     EXPR is VALUE`;
  private functions have none. No spec numbers on the site.
- **Shortcuts are marked** (constitution V): each deliberate limit gets a `ponytail:` comment naming
  its ceiling and the way up, where it is built: one serving process per conversation and at most 63
  per server (T019); a conversation under `serve` blocks every other request (T018); one wait for a
  stream is at most 600 s and a compressed stream is refused (T008); a conversation's recording block
  grows until it ends (T020); messages are whole, never fragments, up to 16 MiB (T015).
- **Handlers are written over any effect** (`fn f[E: effects](s: Socket[uses E]) ... uses E`), so the
  same function runs in a server and on a scripted socket or stream in a test (contracts/events.md).
- `make quick AREA=events` between edits; `make test` once, in T046; the gate is not run. Never two
  suites at once.

## Phase 1: Setup

- [X] T001 Create `std/sse.vo` and `std/websocket.vo` with their header comments and the types of
  data-model.md (`Event`, `StreamEnd`, `EndReason`, `StreamOptions`, `EventStream[E: effects]`, `StreamError`;
  `Message`, `ConnectOptions`, `AcceptOptions`, `Socket`, `WsError`), each with a doc comment and a
  comment per field or case, and `stream_error_text` and `ws_error_text`; add both files to
  `STD_FILES` in `Makefile` and `"sse"`, `"websocket"` to `doc_modules` in `src/doc.c`; accept the
  module listing in `tests/cases/doc/all.out` and `all_json.out`; `make` builds and
  `python3 tests/std_names.py` passes.
- [X] T002 The internal builtins `__ws(op, a, b)` and `__serve_take(kind, max_open)`: names and ids in
  `src/check_expr.c` and `src/vo.h`, no effect for `__ws` and `serve` for `__serve_take` in
  `src/check_effects.c`, `BU_` entries in `src/rt_call.c` dispatching to `rt_ws` in a new `src/ws.c`
  and `rt_serve_take` in `src/serve.c`, both stopping with T0003 "not implemented" for now; `src/ws.c`
  added to the `Makefile`'s sources.
- [X] T003 [P] Create `tests/events.py` (groups selected with `--only NAME`, `--fuzz`, `--quick`;
  helpers to write a vo program to a temporary directory, run it with `--timeout`, record and replay
  it) and a small event-stream server in it (Python standard library: sends a scripted list of raw
  chunks with delays, plain or chunked, then closes or resets); add `"events.py --quick"` to the test
  list in `Makefile`; add area `events` to `tools/quick.sh` (golden areas `sse websocket web
  serve_workers_steps tcp doc`; scripts `doc_conformance.py std_names.py "events.py --quick"`; file
  mapping for `std/sse.vo`, `std/websocket.vo`, `src/ws.c`, `tests/events.py`, `tests/ws_peer.py`).
- [X] T004 [P] Create `tests/ws_peer.py`: a WebSocket server and client written from RFC 6455 with the
  Python standard library (handshake, frames, masking, fragments, ping/pong, close), importable by
  `tests/events.py`, with modes `echo`, `script` (send a given list of raw frames, record what
  arrives) and a table `HOSTILE` of at least 200 named frame sequences with the close code or error
  each must cause (reserved bits, unknown opcodes, masked server frames, unmasked client frames,
  control frames over 125 bytes or fragmented, a continuation without a start, a new message inside
  one, invalid UTF-8 whole and split across fragments, lengths not minimally encoded, 64-bit lengths
  with the top bit set, close payloads of 1 byte, with codes 0, 999, 1005, 1006, 1015, 5000 and with
  a non-UTF-8 reason, messages one byte over the limit, 10,000 pings).

## Phase 2: User Story 1 - Reading a stream of server-sent events (P1)

**Goal**: `read_events` hands over each event as it arrives and says why the stream ended.

**Independent test**: `python3 tests/events.py --only read`: 1,000 events with delays, multi-line
data, names, ids and comments, printed in order as they arrive; the recording replays with the server
gone.

- [X] T005 [P] [US1] Port the web-platform-tests `eventsource/format-*` cases as golden cases over
  `parse_events` in `tests/cases/sse/` (BOM, CR / LF / CRLF and a CRLF split across pieces, comments,
  `data` joined with LF, a leading space after the colon removed once, a field without a colon,
  unknown fields, `id` with NUL ignored, `retry` with non-digits ignored, an event without data not
  handed over, an unfinished last event dropped, the default name `message`); they fail.
- [X] T006 [P] [US1] Group `read` in `tests/events.py`: order and content of 1,000 events; each event
  printed within 50 ms of its last byte in 100 measurements, also after a silent minute (shortened
  with `--quick`); headers and a POST body sent; each `EndReason` (`ended`, `broken` by reset and by
  a chunked body cut short, `timed_out`, `stopped`, `not_a_stream(404)`, `not_a_stream(200)` with
  another content type, `too_large` with a 2 MiB line under `--max-memory 64m`, `failed` for a
  refused connect and for a response with `Content-Encoding: gzip`, naming the encoding); `last_id` and `retry_ms` in the end; a second call with `last_id` sends
  `Last-Event-ID`; R0010 under `net("other:1")`; 100,000 events of 100 bytes with flat peak memory.
- [X] T007 [US1] The parser in `std/sse.vo`: an internal state (unfinished line, data lines, name,
  last id, retry, size so far) with a function that takes one piece of bytes and returns the events
  completed by it, following research R6; `parse_events(text)` on it; `too_large` when an event's
  lines pass 1 MiB. T005's cases pass.
- [X] T008 [US1] `read_events` in `std/sse.vo` on `http.send_stream` per research R6: `Last-Event-ID`
  when `last_id` is not `""`; the head checked on the first piece; the program's function called per
  event; a `Content-Encoding` header ends with `failed`; the `EndReason` from the parser's state or from
  `send_stream`'s error text
  (`tcp.tcp_error_text(tcp.TcpError.timed_out)` is `timed_out`, "the response ended early" and the
  texts of `closed` are `broken`, anything else `failed(text)`); `StreamEnd.last_id` and `retry_ms`
  (-1 when none). Doc comments with runnable examples. T006 passes.
- [X] T009 [US1] Reading in a worker (FR-021): a golden-free check in group `read` that a worker
  running `read_events` and emitting each `Event` delivers them to `receive`, that `receive_first`
  takes them beside a second worker, and that `stop_worker` closes the connection within 200 ms (the
  server sees it); fix what it finds in `std/sse.vo`.

## Phase 3: User Story 2 - A WebSocket client (P1)

**Goal**: `ws_connect`, send, receive, ping and close over `ws://` and `wss://`, with typed errors.

**Independent test**: `python3 tests/events.py --only client`: 10,000 echoed messages of 0 bytes to
1 MiB unchanged; every `HOSTILE` sequence ends with its error value and close code.

- [X] T010 [US2] `rt_ws` in `src/ws.c`: op 1 returns `a` key bytes (1 to 16) through the function
  behind `__entropy` in `src/replay.c`, so they are recorded and replayed and fixed under `vo test`;
  op 2 returns `a` XORed with the 4-byte key `b` (any length, 8 bytes at a time); op 3 returns the 20
  bytes of SHA-1 of `a` with `src/sha1.c`. Golden cases in `tests/cases/websocket/` through a private
  test in the module: the RFC's accept value for key `dGhlIHNhbXBsZSBub25jZQ==` is
  `s3pPLMBiTxaQ9kYGzzhZRbK+xOo=`; masking twice gives the input back.
- [X] T011 [P] [US2] Golden cases in `tests/cases/websocket/` on `scripted_socket`: receives give the
  incoming messages then `closed(1000, "")`; sends appear in `sent_messages`; after `close_socket`
  every call answers `closed`; `pongs` is 0; argument checks (`max_message` 16777217, a header named
  `Sec-WebSocket-Key`, a URL that is not `ws://` or `wss://`, a close reason of 124 bytes, close
  codes 999, 1005, 1006 and 5000 are `invalid` and 1008 is sent); one function written over any
  effect checks both as the argument of `ws_accept` and called on a `scripted_socket` in a test
  that declares no effect; they fail.
- [X] T012 [P] [US2] Group `client` in `tests/events.py` against `tests/ws_peer.py`: the echo run;
  fragments joined, a ping between fragments answered; `nothing_in_time` leaves the socket usable,
  also between two fragments; a frame half sent for 30 s is `broken` (measured outside `--quick` only; no
  setting shortens the limit); the peer's
  close gives `closed(code, reason)` and the peer sees the echo; `close_socket` waits for the echo at
  most 5 s; a reset is `broken`; both sides closing at once end `closed` on both; `connect(TcpError.refused)`,
  `connect(TcpError.timed_out)`, `connect(TcpError.not_found)` and `connect(TcpError.tls(...))` are
  matched as cases; `refused(404)`, `refused(101)` with a wrong accept value, with an
  extension, with a sub-protocol not asked for; the chosen sub-protocol in `Socket.protocol`; request
  headers arrive; `wss://` against `tests/https_server.py`'s certificate with and without
  `--ca-file`; R0010 under `net("other:1")`; every row of `HOSTILE` for a client; a client conversation inside a
  `concurrent_map` task works; time limits under `--clock-ms` behave as `std.tcp`'s; a socket left
  open by a program is closed at its end (the peer sees it) and one left open by a test is closed
  when that test ends: if `vo test` does not do that for `std.tcp` connections today, add a sweep in
  `src/tcp.c` called from `src/testrun.c` beside `proc_sweep()` (FR-020).
- [X] T013 [US2] Frames in `std/websocket.vo`: encoding one message as one frame (masked with
  `__ws(1, 4)` and `__ws(2, ...)` for a client, unmasked for a server; lengths 0 to 125, 16-bit,
  64-bit); decoding a frame head as a pure function over its bytes returning the opcode, flags,
  length and what to read next, or `protocol(what)`; `scripted_socket`, `sent_messages`; the socket's
  state cells of data-model.md. T011 passes.
- [X] T014 [US2] `ws_connect` in `std/websocket.vo`: the URL (scheme, host, port 80 or 443, target),
  `tcp.connect` or `tcp.connect_tls`, the request with `Sec-WebSocket-Key` from `__ws(1, 16)`,
  `Sec-WebSocket-Version: 13`, the options' headers and protocols; the response head (at most
  64 KiB) checked: status 101, `Upgrade`, `Connection`, the accept value, no extension, a
  sub-protocol only from the list; errors as `connect(tcp.TcpError)`, `refused(status)`, `invalid(what)`.
- [X] T015 [US2] `receive_message`, `send_text`, `send_binary`, `ping`, `pongs`, `close_socket` (a result: `invalid` for a code
  outside 1000 to 1003, 1007 to 1011, 3000 to 4999) in `std/websocket.vo` per contracts/events.md and the state diagram of data-model.md: the caller's
  limit on a frame's first two bytes (0: a loop over 600000), 30 s for the rest; fragments kept in
  `parts` across a `nothing_in_time`; pings answered, pongs counted; text checked with
  `__text_of_bytes` when the message is complete; the size checked against `max_message` from the
  frame heads, before reading the payload; each violation closes with the code in the contract's
  table and returns its error; a `closed` state answers every later call without the network. Doc
  comments with runnable examples. T012 passes.
- [X] T016 [US2] A WebSocket held by a worker (FR-021): in group `client`, a worker that owns the
  socket, sends what it is told and emits what arrives; the program waits on it with
  `receive_first` beside a second worker; `stop_worker` closes the connection; fix what it finds.

## Phase 4: The take (prerequisite of US3, US4, US5)

**Goal**: a handler can take the connection of the request it is answering as a `Conn[uses serve]`.

- [X] T017 Step cases in `tests/cases/serve_workers_steps/` (or the directory the existing serve step
  cases use) with a handler that calls a private wrapper of `__serve_take(1, 0)`, writes
  `HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nhello` with `tcp.write_all` and returns: under `serve`
  and under `serve_workers` with 2 workers the client reads `hello`; with 1 worker it gets the
  handler's ordinary response because the take returned `err("busy")`; a second call of the take in
  one handler and a call outside a handler return `err("no connection")`; they fail.
- [X] T018 `rt_serve_take` under `serve` in `src/serve.c` and `src/tcp.c` (research R1): the
  descriptor of `sv_conns[sv_conn]` moves to a new `std.tcp` entry (effect `serve`, the listener's
  label) remembered with its `kind`; the slot is freed without closing; `rt_serve_reply` writes
  nothing when the request's connection was taken; bytes after the request make it
  `err("bytes after the request")`; when the handler returns the entry is closed if still open; when
  it stops with a runtime error a `kind` 2 entry first gets the bytes `88 02 03 F3`.
- [X] T019 The hand-over under `serve_workers` in `src/serve_workers.c`, `src/sys.c` and the platform
  headers: `socketpair`, `sendmsg`, `recvmsg` for the three architectures; a socket pair per worker
  made in `sw_spawn` (closed in the other workers and in tasks, as the pipes are); `TW_TAKE` with
  `max_open` from the worker; the parent answers with the descriptor (`SCM_RIGHTS`) or one refusal
  byte, frees the connection's slot, sets the worker's state to talking and counts it; granted only
  when the count is below `max_open` (0: half the workers, rounded down) and below `workers - 1`;
  `TW_RESPONSE` from a talking worker writes nothing and ends the talking state; a talking worker
  that dies is replaced and uncounted. T017 passes; `make quick AREA=serve_workers` unchanged.
- [X] T020 Recording and replay of the take in `src/replay.c` and `src/vo.h`: event kind
  `serve_take` with the connection's label and the result, written in the request's block; under
  replay `rt_serve_take` reads it and returns an entry without a descriptor, as a replayed connect
  does; the kind is documented in this spec's contracts/events.md and data-model.md (the `tcp_*` kinds
  are documented the same way, in `specs/039-std-tcp/data-model.md`). A step case records T017's program and replays it to the same output.

## Phase 5: User Story 3 - Serving an event stream (P2)

**Goal**: a handler answers with a stream and learns when its client has gone.

**Independent test**: `python3 tests/events.py --only stream`: five events a second apart read by
`curl -N`; a client that leaves after two makes the handler stop within a second.

- [X] T021 [P] [US3] Golden cases in `tests/cases/sse/` on `scripted_stream`: `send_event` and
  `keep_alive` collect in `sent_events`; `invalid` for a name or id with CR, LF or NUL; data with
  line breaks kept; `parse_events` of the text a private formatting function gives for an event
  returns the event (a round trip, as a `prop` over generated events); one function written over any
  effect checks both as the argument of `serve_events` and called on a `scripted_stream` in a test
  that declares no effect; they fail.
- [X] T022 [P] [US3] Group `stream` in `tests/events.py`: the head of contracts/events.md arrives at
  once, with `StreamOptions.headers`; events arrive as sent, read by `curl -N` and by `read_events`; a reconnect as a browser's
  `EventSource` makes it, scripted with `curl` and `Last-Event-ID`, goes on after the last id;
  `Last-Event-ID` readable in the handler; a keep-alive hands the client nothing; `client_gone`
  within 1 s of the next send after the client closes; `not_reading` after 30 s to a client that
  stops reading (outside `--quick`), with the server's memory flat; a handler that divides by zero
  after two events ends the stream, logs R-code as any handler and the server answers the next
  request; a non-GET gets 400; called through `handle_request` in a test it answers 500.
- [X] T023 [US3] `__tcp_io` op 6 in `src/tcp.c` (research R5): a non-blocking look at the connection
  (`recv` with `MSG_PEEK | MSG_DONTWAIT`); end of file or any byte returns `closed` without sending;
  otherwise the ordinary send; recorded and replayed as a send.
- [X] T024 [US3] `serve_events`, `send_event`, `keep_alive`, `scripted_stream`, `sent_events` in
  `std/sse.vo`: the method check, `__serve_take(1, options.max_open)` (`busy` answers 503 with
  `Retry-After: 5` and a body naming the two-worker minimum when that is why; `no connection` 500;
  bytes after the request 400), the head, `f` run with the stream, the connection closed, and the
  returned `Response` with status 200 and `Vo-End` (`done`, `client_gone`, `not_reading`, `stopping`,
  `failed: TEXT`); sends through op 6 with a 30 s limit, their errors mapped to `StreamError`. Doc
  comments with runnable examples. T021 and T022 pass.
- [X] T025 [US3] `on_request(method, pattern, op: fn(C, http.Request) -> result[http.Response] uses E)`
  in `std/web.vo`, a route whose function gets the request; `check_routes` and `vo check`'s route
  table checks treat it as any route; golden cases in `tests/cases/web/` (matched, 405 for another
  method, a step before it refuses with 401 and the function does not run).

## Phase 6: User Story 4 - Serving a WebSocket (P2)

**Goal**: a route accepts a WebSocket and its handler holds the conversation with US2's operations.

**Independent test**: `python3 tests/events.py --only accept`: 1,000 echoed messages with
`tests/ws_peer.py`'s client and with `ws_connect`; an origin not named gets 403 before the handler.

- [X] T026 [P] [US4] Group `accept` in `tests/events.py`: the echo runs; every refusal of the
  contract's status table (400, 403 for an origin not in `origins`, a request without `Origin`
  accepted by `[]`, any origin by `["*"]`, 426 with `Sec-WebSocket-Version: 13`, 503 at the bound);
  the first of the client's sub-protocols that the route names is chosen and reaches the handler;
  the handler returning closes with 1000, returning an error with 1011, dividing by zero with 1011
  and the server goes on; a sign-in step refusing means no handshake; every row of `HOSTILE` for a
  server.
- [X] T027 [US4] `ws_accept` in `std/websocket.vo`: the request checked (GET, `Upgrade: websocket`,
  `Connection` holding `upgrade`, version 13, a key that is 16 bytes of base64), the origin against
  `options.origins`, the sub-protocol chosen; `__serve_take(2, options.max_open)` with the refusals
  as in T024; the 101 head with the accept value from `__ws(3, ...)`; a server-role `Socket`
  (unmasked sends, masked frames required from the client, `max_message`); `f` run; `close_socket`
  with 1000 or 1011; the returned `Response` with status 101 and `Vo-End` (`done`, `closed CODE`,
  `stopping`, `failed: TEXT`). Doc comments with runnable examples. T026 passes.

## Phase 7: User Story 5 - Open conversations do not take the server down (P2)

**Goal**: a bound that keeps ordinary requests answered, a clean stop, one log record and a gauge.

**Independent test**: `python3 tests/events.py --only bound` and `--only stop`.

- [X] T028 [P] [US5] Groups `bound` and `stop` in `tests/events.py`: 8 workers, `max_open` 4, 50
  clients that open a stream and never read: 4 are served, 46 get 503, and `/health/live`'s response
  time stays within twice its idle time throughout; two routes with `max_open` 2 and 6 share the
  count; `max_open` 63 with 8 workers stops at 7; 64 workers with 60 open conversations and SIGTERM:
  every WebSocket client reads a close with 1001, every stream ends, the server exits within its
  usual time in 20 of 20 runs, with and without `--drain 5`; a handler that ignores `stopping` is
  killed at the limit; under `serve` (one process) the same stop with one conversation.
- [X] T029 [US5] The wake in `src/net.c`, `src/serve.c` and `src/serve_workers.c` (research R5): a
  global wake descriptor polled by `net_wait` beside the connection while the process holds a taken
  connection (the worker's end of its socket pair; the signal descriptor under `serve`); the parent
  writes one byte to every talking worker at the first stop signal; the interrupted `std.tcp` call
  returns `other("the server is stopping")` once and the wake is switched off for that connection;
  talking workers are waited for and then killed exactly as busy ones.
- [X] T030 [US5] `stopping` in `std/sse.vo` and `std/websocket.vo`: the text of T029 mapped to
  `StreamError.stopping` and `WsError.stopping`; a socket sends a close with 1001 before returning
  it. Group `stop` passes.
- [X] T031 [US5] Logs and the gauge: `request_logs` in `std/web.vo` writes the field `end` from the
  response's `Vo-End` header, and `without_vo_error` (or its place in `serve_app`) removes the
  header; the server process sets the gauge `http_conversations_open` through the registry of
  `src/rt_metrics.c` whenever its talking count changes (1 and 0 under `serve`); golden or step cases
  in the directories of spec 055's cases: one record per conversation with `status`, `ms`, `end` and
  the request id; `/metrics` shows the gauge at 2 with two open streams and at 0 after they end, also
  when a talking worker was killed. Group `bound` passes.
- [X] T032 [US5] Group `hostile` in `tests/events.py`: one conversation fed each `HOSTILE` row while a
  second one echoes undisturbed; a flood of 10,000 pings is answered without growth; with `--fuzz`,
  100,000 mutated frame sequences per WebSocket role and 100,000 mutated event streams for the
  reader (2,000 each with `--quick`): only error values, no runtime error, no hang, peak memory under
  the limits. Fix what it finds.

## Phase 8: User Story 6 - Tests, recordings and review (P1)

**Goal**: all four roles record and replay; scopes are enforced; handlers and clients are testable.

**Independent test**: `python3 tests/events.py --only replay`: 100 recordings per role against peers
with random delays, each replaying to its own output and exit status with the network off.

- [X] T033 [P] [US6] Group `replay` in `tests/events.py`: the four programs of quickstart.md recorded
  100 times each against peers with random delays (seeded) and replayed with no peer running; a
  program whose reads have no time limit prints the same bytes in every run; `--trace` of a run shows
  the conversation's events; a recording made with secret redaction holds no handshake
  `Authorization` value; a recording of 5,000 requests answered while one conversation stays open
  (blocks written out of order past the ring of 4,096) replays to the same output. Fix what it finds in `src/replay.c`, `src/tcp.c` or the modules.
- [X] T034 [P] [US6] Golden cases in `tests/cases/effects/` (or the directory of the existing scope
  cases): a function calling `ws_connect` or `read_events` without `net` is the existing effect
  error; with `net("a.example:443")` a `wss://b.example/` address stops with R0010 naming the
  function and its scope, checked before any connect; a handler calling `serve_events` or
  `ws_accept` without `serve` is the effect error; `vo.effects` of a small program lists `net` with
  its scope and `serve` as today.
- [X] T035 [US6] Recorded tests (spec 054) in `tests/recorded_tests.py` or its cases: a test that
  reads a stream and one that exchanges WebSocket messages are recorded against local peers and
  replay without them; changing a sent message makes the replay report the difference at that send.
- [X] T036 [US6] `docs/security.md`: conversations are covered by `net` and `serve` with host and port
  scopes; mask and handshake key bytes are drawn without the `entropy` effect and why (research R4);
  WebSocket routes name their origins; the bound and what it protects.

## Phase 9: User Story 7 - Documented with an example, within budget (P2)

- [X] T037 [P] [US7] `examples/job-queue/`: a route `GET /jobs/{id}/events` that streams a job's
  progress with `serve_events` (`max_open` set, a keep-alive while idle, resuming from
  `Last-Event-ID`), a command that follows it with `read_events` and starts again from `last_id`
  after `broken`, and tests on `scripted_stream` and a recorded test that run without the network;
  `examples/README.md` updated; `python3 tests/build_examples.py` passes.
- [X] T038 [P] [US7] A guide page in `site/docs/guide/` (reading a stream, reading one in a worker, a
  WebSocket client, a route that streams, a route that accepts a WebSocket with origins, the bound
  and the two-worker minimum, CORS headers through `StreamOptions.headers`, starting again from the
  last id, testing with scripted streams and sockets, what replay does, recording only conversations
  of bounded length, what is left out); linked from the standard-library page; no development
  material; the first two examples are under 15 and under 20 lines without import and blank lines
  (SC-001); `python3 tests/site_examples.py` passes.
- [X] T039 [US7] `docs/reference.md`: one row each for `std.sse` and `std.websocket` in the module
  table and `ws_connect`, `read_events` beside `net`, `serve_events`, `ws_accept` beside `serve` in
  the effects table; cut the same number of tokens from the `std.web` and `std.http` rows and state
  the cut in the change; `python3 tests/ref_examples.py` reports at most 15,000.
- [X] T040 [US7] `docs/roadmap.md`: spec 063 in the table of specs and a section as the others have
  (what it does, "Not done (on purpose)": broadcast, automatic reconnection, compression, fragments,
  HTTP/2); the "Events" row removed from the open gaps; "Workers in tasks and servers" notes that
  broadcast waits for it. `CHANGELOG.md`: the new modules, `web.on_request`, the `end` log field, the
  gauge, the `serve_take` recording event. The version raised by a minor step in
  `src/build.c` (`vo_version_text`) and `src/main.c` (the JSON version record), with the cases that
  print it.

## Phase 10: Polish

- [X] T041 [P] `tests/autobahn.sh`: runs the Autobahn test suite from the `crossbario/autobahn-testsuite`
  Docker image against a vo echo server (`fuzzingclient`) and a vo client program (`fuzzingserver`),
  sections 1 to 7, 9 and 10 with `max_message` 16 MiB, and prints failed cases; not part of
  `make test`. Fix what it finds.
- [X] T042 Stress and instrumented builds: `make STRESS=1`, `tests/run.sh --stress 1 tests/cases/sse
  tests/cases/websocket` and the touched serve step cases with `VO_GC_STRESS=1`; the instrumented
  build over the same; add `.stress-every` only to a case that merely times out.
- [X] T043 Group `take` in `tests/events.py`: malformed and
  early-closing requests on conversation routes (a client that closes during the hand-over, sends
  bytes after its request, or sends half a handshake) never stop the server or leak a talking count.
- [X] T044 [P] ARM64 and RISC-V: `make ARCH=aarch64 quick AREA=events` and `ARCH=riscv64` (the script's
  `--quick` sizes under qemu); a recording made on x86-64 replays on both (`tests/cross_replay.py`).
- [X] T045 Measure SC-002, SC-003, SC-005 and SC-006 with `tests/events.py` outside `--quick` and
  `tests/serve_workers_speed.py` unchanged within its bounds.
- [X] T046 `make test` once, alone; then `specs/063-events/results.md`: each success criterion with
  its measurement, the Autobahn summary, the note that ported format cases stand in for an
  independent event server (SC-004), what was found and fixed on the way; the spec's status set to
  Implemented; the roadmap row linked to the results.

## Dependencies

```text
Setup (T001–T004)
 ├─ US1 (T005–T009)                       independent
 ├─ US2 (T010–T016)                       independent of US1
 └─ The take (T017–T020)
     ├─ US3 (T021–T025)    needs US1's parser for its round trip (T007)
     ├─ US4 (T026–T027)    needs US2 (T013–T015)
     └─ US5 (T028–T032)    needs US3 and US4
US6 (T033–T036)            needs US1–US4; T034 can start after US2
US7 (T037–T040)            needs US3 and US1; T039, T040 last
Polish (T041–T046)         T041 after US4; T046 last
```

## Parallel opportunities

- T003 and T004 (two new test files).
- US1 and US2 as whole phases; within them the test tasks marked [P] (T005, T006; T011, T012).
- The take (Phase 4) beside US1 and US2: it touches only C and step cases.
- T021 and T022; T033 and T034; T037 and T038; T041 and T044.
- Never two test suites running at once on this machine: parallel means writing, not running.

## Implementation strategy

- **MVP**: Setup and US1. A service can read a streaming API with scopes, a recording and a test.
- **Second step**: US2. Both client roles, no change to the server.
- **Third step**: the take, then US3, US4, US5 in order; a server with conversations is not shipped
  before US5's bound and stop exist.
- US6's replay group runs after each role is added, and completes at Phase 8.

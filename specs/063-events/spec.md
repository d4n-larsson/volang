# Feature Specification: Events (server-sent events and WebSockets, as client and as server)

**Feature Branch**: `063-events`

**Created**: 2026-11-28

**Status**: Implemented (2026-11-28; not released)

**Input**: User description: "Events: clients and servers for server-sent events (SSE) and WebSockets in
the standard library. This is the first open gap in docs/roadmap.md ("Events" row): the model exists
since spec 047 (a worker reads the connection and sends each event on), but the protocols do not. A vo
program should be able to (1) serve an SSE stream and a WebSocket endpoint from a std.http/std.web
server, (2) connect as an SSE client and as a WebSocket client (ws and wss), receiving events through the
spec 047 worker model (next_message / emit). As with everything that reaches the outside world: each gets
an effect with scopes, and record and replay. Judge scope by the roadmap's question: does it make an
agent-written network service easier to write, review or operate?"

Background: a vo service can ask and answer. `std.http` sends a request and gets a response, and a
handler takes a request and returns a response. What a service cannot do with the library today is take
part in a conversation that stays open:

- **Reading a stream of events** from another system (a model's answer token by token, a build's
  progress, a feed of changes). `send_stream` (spec 040) hands over the bytes as they arrive, but every
  program must write the event format's parser itself, and its rules for lines, multi-line data and ids
  are easy to get almost right.
- **Talking over a WebSocket** to a system that offers nothing else (chat platforms, market data,
  subscriptions). `std.tcp` gives the connection; the handshake, framing, masking, fragments, pings and
  the closing exchange are several hundred lines an agent would write again in every program, with no
  test of the hostile cases.
- **Sending events to a frontend**, or holding a WebSocket with it. A handler returns one whole
  response; it cannot answer a piece at a time, and it cannot keep the connection.

Spec 047 gave the model for the waiting: a worker holds the connection and passes each event to the
program as a message, so the program can wait on several sources, give a wait a time limit and stop a
source. It left the protocols as separate work. This spec is that work.

What this spec must keep:

- **Effects (`docs/security.md`):** reaching another system needs `net` with its scopes
  (`net("api.example.com:443")`); accepting connections needs `serve`. A reviewer reads in a function's
  declaration, and in `vo.effects`, which hosts a program may hold conversations with.
- **Principle III:** timing enters only through named operations (a time limit, the first of several
  workers); a recording replays to the output and exit status of its run, with no network.
- **Principle V:** the reference stays under 15,000 tokens; nothing is built that no requirement names.
- **Principle VI:** rendering what a browser shows stays out; the protocols a frontend speaks to a
  service are in.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Reading a stream of server-sent events (Priority: P1)

A developer names a URL and gets each event the server sends, as it is sent: its data, its event name
and its id, already put together by the format's rules. When the stream ends, the program knows why, and
knows the last id so that it can ask to go on from there.

**Why this priority**: it is the most common of the four in a backend service (every streaming model
API, most progress and change feeds), and the smallest.

**Independent Test**: a local server sends 1,000 events with fixed delays, some with multi-line data,
names, ids and comment lines; the program prints each as it arrives, in order, and its recording replays
to the same output with the server gone.

**Acceptance Scenarios**:

1. **Given** a server that answers with an event stream, **When** the program reads it, **Then** it is
   handed each event once, in order, within the time of SC-002 after the event's last byte arrived, not
   when the connection closes or a buffer fills.
2. **Given** events whose data spans several lines, that carry a name or an id, or that are separated
   by any of the three line endings the format allows, **When** read, **Then** the program gets the data
   joined as the format says, the name (or the default name), and the id; comment lines and unknown
   fields are not handed over.
3. **Given** a request that needs headers or a body (a token, a JSON question sent with POST), **When**
   the program reads the stream, **Then** it can send them, as with any other request.
4. **Given** a server that answers with an error status or with something that is not an event stream,
   **When** read, **Then** the program gets an error value that tells the two apart and carries the
   status; no event is handed over.
5. **Given** a stream that ends (the server closes, the connection breaks, or nothing arrives within
   the program's time limit for one wait), **When** it ends, **Then** the program learns which of these
   happened, the id of the last event it was handed, and the waiting time the server asked for, and can
   start again from that id with one more call. The library does not reconnect by itself.
6. **Given** a program that has seen enough, **When** it says so, **Then** the connection is closed and
   reading returns.
7. **Given** the reading done in a worker that passes each event on, **When** the program waits on that
   worker and others, **Then** events reach it as messages of an ordinary record type, and stopping the
   worker closes the connection.

---

### User Story 2 - A WebSocket client (Priority: P1)

A developer connects to a `ws://` or `wss://` address, sends and receives text and binary messages, and
closes. The program sees whole messages and a typed reason when the conversation ends; the handshake,
frames, masking, pings and the closing exchange are not its business.

**Why this priority**: there is no way around the protocol when the other system speaks nothing else,
and it is the largest piece of code an agent would otherwise write and get subtly wrong.

**Independent Test**: against a local echo server, the program sends 10,000 text and binary messages
from 0 bytes to 1 MiB and receives each back unchanged; against a script of hostile frames it ends with
the right error value each time and never stops with a runtime error.

**Acceptance Scenarios**:

1. **Given** a WebSocket address, **When** the program connects with a time limit, **Then** it gets a
   connection or a typed error (refused, timed out, the name not found, the certificate not accepted,
   the server did not agree to a WebSocket and answered with status N).
2. **Given** a connection, **When** the program sends a text or a binary message, **Then** the other
   side receives exactly that message; **When** the other side sends one, in one piece or in many,
   **Then** the program receives it whole, with its kind.
3. **Given** a connection on which nothing arrives, **When** the program receives with a time limit,
   **Then** it gets "nothing in time" as an error value and the connection stays usable.
4. **Given** a ping from the other side, **When** the program is receiving, **Then** it is answered
   without the program seeing it. The program can send a ping of its own and learn that it was answered.
5. **Given** either side closing, **When** the program next receives, **Then** it gets "closed" with
   the code and reason the other side gave, and so does every later operation. A connection that just
   breaks is told apart from one that was closed properly.
6. **Given** a handshake that needs request headers (a token, an origin) or one of several
   sub-protocols, **When** the program connects, **Then** it can send them and learns which sub-protocol
   the server chose.
7. **Given** a `wss://` address, **When** connected, **Then** the server's certificate is checked as for
   `https://`, with the same errors.
8. **Given** the connection held by a worker, **When** the program tells the worker what to send and
   receives what arrived, **Then** it can wait on this conversation and other sources at once (spec 047).

---

### User Story 3 - Serving an event stream (Priority: P2)

A developer writes a route that answers with a stream: the handler sends events one at a time, for as
long as it has something to say, and learns when the client has gone.

**Why this priority**: it is how a service shows a frontend progress, or passes on what it reads from
another stream; simpler than a WebSocket and enough for most one-way cases.

**Independent Test**: a route sends one event a second for five seconds; a client sees five events about
a second apart, then the end of the stream; a second client that disconnects after two events makes the
handler stop within a second.

**Acceptance Scenarios**:

1. **Given** a route that answers with a stream, **When** a client asks, **Then** it receives the
   headers that mark an event stream at once, and each event as the handler sends it.
2. **Given** an event with a name, an id and data of several lines, **When** sent, **Then** a client
   reading by the format's rules gets exactly those values; data that cannot be sent as given (a name or
   id containing a line break) is an error value at the send, never a corrupted stream.
3. **Given** a client that has gone, **When** the handler next sends, **Then** it gets an error value
   that says so, within the time of SC-005, and can return.
4. **Given** a client that reconnects with the id of the last event it saw, **When** the handler runs,
   **Then** it can read that id from the request.
5. **Given** a handler with nothing to say for a while, **When** it sends a keep-alive, **Then** the
   connection stays open through a proxy and the client is handed nothing.
6. **Given** the route in a `std.web` application, **When** its steps run (sign-in, request ids,
   logging), **Then** they apply to the request that opens the stream as to any other, and a refused
   request gets an ordinary response. Headers a step would add to the response afterwards (CORS) are
   given to the stream when it is opened, since its head is sent before the handler returns.
7. **Given** a handler that stops with a runtime error after events were sent, **When** it fails,
   **Then** the stream ends, the failure is logged as for any handler, and the server goes on.

---

### User Story 4 - Serving a WebSocket (Priority: P2)

A developer writes a route that accepts a WebSocket: after the handshake the handler holds the
conversation with the same operations as the client of story 2, until it returns.

**Why this priority**: needed when the frontend must speak as well as listen; it reuses the framing of
story 2 and the serving of story 3.

**Independent Test**: a route echoes messages; a browser-like client and the client of story 2 each
exchange 1,000 messages with it; a request from an origin the program did not name is refused before
the handler runs.

**Acceptance Scenarios**:

1. **Given** a route that accepts a WebSocket, **When** a client asks correctly, **Then** the handshake
   is answered and the handler runs with a connection; **When** the request is not a correct WebSocket
   request, **Then** it gets an ordinary error response and the handler does not run.
2. **Given** a browser's request, **When** its origin is not among those the program named for the
   route, **Then** it is refused; a route that names none accepts only requests without an origin (other
   programs). Nothing is open to every site unless the program says so in words a reviewer can find.
3. **Given** the handler returns, **When** it does, **Then** the conversation is closed properly with a
   normal code; **When** it stops with a runtime error, **Then** it is closed with the code for a server
   error, the failure is logged, and the server goes on.
4. **Given** the steps of a `std.web` application, **When** a WebSocket request arrives, **Then** they
   run before the handshake is answered, so that an unsigned request never gets a connection.
5. **Given** sub-protocols asked for by the client, **When** the route names the ones it speaks,
   **Then** the first of the client's that the route speaks is chosen and the handler learns which.

---

### User Story 5 - Open conversations do not take the server down (Priority: P2)

An operator runs a service with streams and WebSockets beside its ordinary routes. Conversations that
stay open must not use up what ordinary requests need, must end when the server is told to stop, and
must be visible.

**Why this priority**: a stream holds on to its share of the server for minutes or hours, where a
request holds it for milliseconds. Without a bound, a handful of idle clients stops the API; that is a
defect an agent will not notice in its tests.

**Independent Test**: a server with a bound of 4 open conversations gets 50 clients that each open a
stream and never read; the 5th and later get a refusal, and an ordinary route answers in its usual time
throughout.

**Acceptance Scenarios**:

1. **Given** a server, **When** it runs, **Then** the number of conversations it holds open at
   once has a bound the program sets at each route (with a documented default); one more is refused with the status
   for "try later", and ordinary requests are answered as if no conversation were open.
2. **Given** open conversations, **When** the server is told to stop (SIGTERM, with or without
   `--drain`), **Then** each WebSocket is closed with the code for "going away" and each stream ends,
   and the server finishes within its usual time.
3. **Given** a client that stops reading, **When** the handler keeps sending, **Then** the send returns
   an error value after a documented time limit; the server's memory does not grow with what was not
   read.
4. **Given** request logging and metrics (spec 055), **When** a conversation opens and ends, **Then**
   it is logged once with its request id, how long it lasted and why it ended, and the number of open
   conversations can be read as a metric.
5. **Given** a peer that sends frames that break the protocol, messages beyond the size limit, text that
   is not UTF-8, or a flood of pings, **When** received, **Then** the conversation is closed with the
   matching code and the handler gets an error value; no other conversation is affected.

---

### User Story 6 - Tests, recordings and review (Priority: P1)

Programs that read streams or hold WebSockets can be tested without a network, recorded and replayed,
and a reviewer sees what they may talk to.

**Why this priority**: it is what vo promises for everything that reaches outside; a protocol that
escaped it would undo the promise for every service that uses one.

**Independent Test**: each example of stories 1 to 4 is run with `--record` 100 times with random
delays in the peer; every recording replays to the output and exit status of its own run with the
network off.

**Acceptance Scenarios**:

1. **Given** `vo run --record`, **When** a program reads a stream or holds a WebSocket, as client or as
   server, **Then** the recording holds what it received (each event or message, each "nothing in time",
   each end and its reason, in order); **When** replayed, **Then** output and exit status are those of
   the run. What the program sent is compared with the recording where `std.tcp` compares it today: in
   recorded tests (spec 054).
2. **Given** a function that connects to a host, **When** checked, **Then** it must declare `net`; with
   a scope, a connection to another host or port stops with R0010 naming the function and its scope, for
   `ws://` and `wss://` addresses and stream URLs alike. A function that accepts conversations declares
   `serve`. `vo.effects` shows both as it does today.
3. **Given** a handler for a stream or a WebSocket, **When** a test calls it, **Then** the test can give
   it the messages a client would send and read what it sent, in the test's own process, with no
   listening port.
4. **Given** code that reads events or messages, **When** a test runs it, **Then** recorded tests (spec
   054) replay the conversation, and the format's parsing can be given text directly.
5. **Given** a redacted recording (secrets by header name), **When** a handshake carried a token,
   **Then** the token is redacted as for other requests.

---

### User Story 7 - Documented with an example, within budget (Priority: P2)

**Acceptance Scenarios**:

1. **Given** the guide, **When** read, **Then** it shows reading a stream, reading one in a worker
   beside other work, a WebSocket client, a route that streams and a route that accepts a WebSocket,
   the bound on open conversations, starting again from the last id, and what replay does with each.
2. **Given** the examples, **When** listed, **Then** one of them uses events for something it could not
   do before (job-queue or status-board showing progress as a stream; a client reading it), with tests
   that run without the network.
3. **Given** the reference, **When** counted, **Then** it is under 15,000 tokens.
4. **Given** the roadmap, **When** read, **Then** the "Events" row is gone from the open gaps and what
   was left out is named.

### Edge Cases

- An event stream that starts with a byte-order mark; a last event not followed by an empty line (not
  handed over, as the format says); an event with no data line (not handed over); an id containing a
  zero byte (ignored); a waiting time that is not a number (ignored).
- One line, or one event, larger than the limit: the reading ends with an error value that names the
  limit; memory stays bounded.
- A stream answered with a redirect: not followed, as for `send_stream`; the error carries the status.
- A stream sent in chunks: read as the HTTP client reads any body. A compressed stream (never asked
  for): the reading ends with an error value that names the encoding.
- A WebSocket message of 0 bytes; one of exactly the size limit and one byte more; a message in a
  thousand fragments; a ping between the fragments of a message; a close frame with no code, with a
  code and no reason, with a code that may not be sent, and with a reason that is not UTF-8.
- A server that sends masked frames, or a client that sends unmasked ones: closed as a protocol error.
- A server that agrees to an extension or a sub-protocol the client did not ask for: the connect fails.
- Both sides closing at once; a close that is never answered (given up after a documented time).
- Sending on a connection after "closed": an error value, never a stop and never a wait.
- A send while the other side sends faster than the program reads: neither side waits forever (the
  rule of spec 047, story 3, scenario 6 applies to the worker; the connection's own sends have a time
  limit).
- A connection or stream still open when the program, a test or a handler ends: closed by the run
  time; nothing is left behind (at a program's end by the system; at a test's end by a sweep like
  spec 040's for child programs).
- A WebSocket or stream handler under `serve` with one process: one conversation at a time is all it
  can hold, and the documentation says to use the concurrent server.
- A WebSocket request to a route that is not one, and an ordinary request to a route that is: ordinary
  error responses (the statuses are in the plan's contract).
- A conversation opened inside a `concurrent_map` task or a serving worker as a client: allowed, since
  it is an ordinary connection; starting a worker there is still not (spec 047).
- Time limits under `vo test` and `--clock-ms`: as for `std.tcp`.
- The same program and recording on x86-64, ARM64 and RISC-V: the same bytes sent, apart from what a
  named source of randomness decides (the masking of a client's frames), which never shows in what the
  program sees or prints.

## Requirements *(mandatory)*

### Functional Requirements

**Reading event streams**

- **FR-001**: The standard library MUST let a program read a server-sent event stream from a URL with
  a method, headers and a body of its choosing, and hand it each event (data, name, id) as it arrives,
  parsed by the rules of the HTML standard's event stream format.
- **FR-002**: The end of a stream MUST be an answer that distinguishes: the server ended it, the
  connection broke, nothing arrived within the time limit of one wait, the program stopped it, the
  answer was not an event stream (with its status), and a limit was exceeded. It MUST carry the last id
  handed over and the waiting time the server asked for. The library MUST NOT reconnect by itself; a
  program MUST be able to ask to go on from an id.
- **FR-003**: Lines and events MUST have documented size limits; exceeding one ends the reading with an
  error value and bounded memory.
- **FR-004**: The format's parsing MUST be usable on text a program already has, without a connection.

**WebSocket conversations**

- **FR-005**: The standard library MUST let a program open a WebSocket to a `ws://` or `wss://` address
  with a time limit, request headers and a list of sub-protocols, following RFC 6455, and MUST report
  the chosen sub-protocol. No extension is offered or accepted.
- **FR-006**: On a connection a program MUST be able to send a text message, send a binary message,
  receive the next whole message with its kind and a time limit, send a ping and learn of its answer,
  and close with a code and reason. Pings from the other side MUST be answered without the program's
  help. Fragments MUST be put together before the program sees a message.
- **FR-007**: Every failure MUST be a typed error value: nothing in time (the connection stays usable),
  closed by the other side (code and reason, once), broken, a protocol error, a message over the size
  limit, text that is not UTF-8, the connect errors of scenario 2.1. The library MUST close with the
  code RFC 6455 gives for each violation. No input from a peer may stop the program with a runtime
  error or make its memory grow beyond the documented limits.
- **FR-008**: The size limit of a message MUST be documented and settable by the program.

**Serving**

- **FR-009**: A route of the HTTP server, and of a `std.web` application, MUST be able to answer with
  an event stream: the handler sends events (data, optional name and id) and keep-alives one at a time
  and each is on its way to the client when the send returns. A send to a client that has gone or does
  not read MUST return an error value within a documented time.
- **FR-010**: A route MUST be able to accept a WebSocket: the handshake is checked and answered by the
  library, and the handler gets a connection with the operations of FR-006 to FR-008. When the handler
  returns the conversation MUST be closed properly; when it fails, closed with the server-error code.
- **FR-011**: A WebSocket route MUST name the origins it accepts. A request with an origin not named
  MUST be refused before the handler runs; accepting every origin MUST take an explicit word in the
  program.
- **FR-012**: The steps of a `std.web` application (sign-in, CORS, request ids, logging) MUST run on the
  request that opens a conversation, before it is opened.
- **FR-013**: The number of conversations a server holds open at once MUST be bounded by a number the
  program sets, with a documented default. A request beyond it MUST be refused with a "try later"
  status. Under the concurrent server, ordinary requests MUST be answered within their usual time
  while the bound is reached; the single-process server holds one conversation at a time and answers
  nothing else meanwhile, which its documentation MUST say.
- **FR-014**: When the server is told to stop, open conversations MUST be ended (WebSockets with the
  "going away" code) within the server's usual stopping time, `--drain` included.
- **FR-015**: Each conversation MUST be logged once at its end with its request id, duration and
  reason, by the request logging of spec 055, and the number open MUST be available as a metric.

**Effects, determinism, recording, tests**

- **FR-016**: Connecting MUST need `net` and obey its scopes by host and port, for stream URLs and
  WebSocket addresses; accepting MUST need `serve`. No new effect is added. A handler's own effects
  are declared as for any handler.
- **FR-017**: `vo run --record` and `vo replay` MUST work for all four roles: a replay reproduces the
  run's output and exit status without the network; recorded tests (spec 054) report a difference
  when the program sends something other than what was recorded. Redaction of secrets MUST cover
  handshake headers.
- **FR-018**: A program whose reads of one conversation never run into a time limit MUST give the
  same output on every run and architecture. Timing enters only through the time limits (a WebSocket
  receive may have none; one wait for a stream is at most 600 seconds) and through the worker
  operations spec 047 names.
- **FR-019**: A test MUST be able to run a stream handler or a WebSocket handler in its own process
  with the client's messages given as values, and read what the handler sent. Recorded tests (spec 054)
  MUST work for the client roles.
- **FR-020**: Every conversation left open when a program, a test or a handler ends MUST be closed by
  the run time.
- **FR-021**: Reading a stream and holding a WebSocket MUST work inside a worker of spec 047, with
  events and messages passed on as values of ordinary record types, and stopping the worker MUST close
  the connection.

**Documents, platforms**

- **FR-022**: Every exported item MUST have a doc comment with examples that run. The guide documents
  the feature; one example uses it; the site mentions no development material; the reference MUST stay
  under 15,000 tokens.
- **FR-023**: The feature MUST work on x86-64, ARM64 and RISC-V. Existing servers and clients MUST
  keep their behaviour: no existing golden case changes except the listing of modules.

### Key Entities

- **Event**: what a stream carries: data, a name (with the format's default), and an optional id.
- **Stream end**: why reading stopped, the last id handed over, and the waiting time the server asked
  for.
- **Conversation**: an open WebSocket, on either side: where messages are sent and received.
- **Message**: text or binary, whole.
- **Closing**: a code and a reason, given by the side that closes.
- **Event error / WebSocket error**: the typed failures of FR-002 and FR-007.
- **Open-conversation bound**: how many streams and WebSockets a server holds at once.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A program that prints each event of a stream needs no parsing code of its own: the
  example for story 1 is under 15 lines, and the one for story 2 (connect, send, receive, close) under
  20, not counting import lines and blank lines.
- **SC-002**: An event or message is handed to the program within 50 milliseconds of its last byte
  arriving on an idle machine, in 100 of 100 measurements, also when it is the only one for a minute.
- **SC-003**: Reading 100,000 events of 100 bytes, and exchanging 10,000 WebSocket messages of 0 bytes
  to 1 MiB with an echo server: none lost, repeated, changed or out of order, and peak memory does not
  grow with the number of events or messages.
- **SC-004**: Both WebSocket roles pass an independent conformance suite for RFC 6455 without
  extensions (the plan names the suite and the sections), and each of the four roles exchanges events
  or messages correctly with at least one implementation not written for this project.
- **SC-005**: A handler learns that its client has gone within 1 second of its next send, and a server
  told to stop with 60 open conversations finishes within its usual stopping time, in 20 of 20 runs.
- **SC-006**: With the bound on open conversations reached and 50 more clients trying, an ordinary
  route's response time stays within twice its time on an idle server.
- **SC-007**: A corpus of hostile streams and frames (at least 200 cases, and 100,000 fuzzed inputs per
  role) produces only error values: no runtime error, no hang, no memory beyond the limits.
- **SC-008**: 100 recordings of each of the four roles, made against peers with random delays, each
  replay to the output and exit status of their own run with the network off.
- **SC-009**: The example has tests that run without the network; the reference is under 15,000 tokens;
  every doc example passes; the full suite passes on x86-64 and the feature's cases on ARM64 and RISC-V.

## Assumptions

These were decided without asking and are listed for review; the first three shape the scope.

- **A server holds each conversation in one of its serving processes** for as long as it lasts, and
  the bound of FR-013 is what keeps processes free for ordinary requests. With the 64 serving
  processes a server may have, that is at most 63 conversations. A service for thousands of
  idle browser connections is not what this builds; a service with tens of streams (progress, relayed
  model output, an operator's console) is.
- **No broadcast.** One handler talks to one client. A message for every connected client (a chat
  room) needs serving processes that exchange messages, which is the roadmap's separate row "Workers
  in tasks and servers". Until then each handler can poll what the others wrote (a table, a file).
- **The library does not reconnect by itself.** The program gets the end, the last id and the server's
  waiting time, and loops; a retry policy hidden in the library would be control flow a reviewer cannot
  see (Principle I).
- **No new effect.** A WebSocket to a host is a connection to that host, as in spec 039; a reviewer
  who must know which conversations stay open reads the calls. Scopes are by host and port, not by
  path.
- **No WebSocket extensions** (no per-message compression): servers must work without them, and
  compression of attacker-chosen text beside secrets has a known weakness.
- **Whole messages only.** A program does not send or receive fragments; a message larger than the
  limit is an error. Streaming a large binary transfer over a WebSocket is not served.
- **Pings are answered by the library; sending them on a schedule is the program's choice** (a worker
  that sleeps is a timer). There is no automatic keep-alive.
- **A client's frame masking is not an effect** (plan, R4): the key bytes are recorded, never reach
  the program, and change nothing it sees, prints or replays, as with TLS's own randomness.
- **HTTP/1.1 only**, as the rest of `std.http`: no WebSockets or streams over HTTP/2.
- **Names and module layout are chosen in the plan**: `std.sse` and `std.websocket`.
- Out of scope: broadcast and rooms, automatic reconnection, per-message compression, fragments in the
  program's hands, a browser-side script, serving over HTTPS itself (its own roadmap row; behind a
  proxy `wss://` and streams work as they are), rate limiting of conversations (its own row), WebSocket
  over HTTP/2, UDP-based protocols.

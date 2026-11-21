# Feature Specification: TCP Connections (`std.tcp`)

**Feature Branch**: `039-std-tcp`

**Created**: 2026-11-21

**Status**: Implemented (gate not run)

**Input**: User description: "`std.tcp`: TCP connections for vo programs, so that protocols other than
HTTP (Postgres, Redis, SMTP, message queues, a custom line protocol) can be written as vo packages.
Source: docs/review-2026-11/ecosystem.md, Standard library, 'TCP and UDP' (order step 3), and the
roadmap ('scoped like `net` and `serve`'). Connect with a timeout, write bytes, read bytes (up to N,
exactly N, up to a delimiter, each with a time limit), close; upgrade to TLS at connect time or later
(STARTTLS) with the TLS client and trust store of `std.http`; listen and accept. Connecting is under
the existing `net` effect and its host scopes, listening under `serve` and its address scopes; no new
effect. A connection is a library value carrying its effect like `Database[E]`. Errors are typed.
Everything read is part of record and replay. Must keep: determinism under replay, no libc, three
architectures, scopes enforced at run time, `vo.effects` showing every host, the reference under 15,000
tokens (about 14,843 now), no large static buffers, a size cap per read. Decide: UDP, ports in `net`
scopes, IPv6, rebuilding `std.http`. Out of scope: Postgres/Redis/SMTP packages, serving TLS, pools,
streaming types, Unix sockets, raw sockets, other socket options."

Background: the only way a vo program reaches the network is one HTTP request and one HTTP response.
Every other protocol a deployed program needs (a server database, a cache, mail, a queue) is a
conversation over a TCP connection, so none of them can be written in vo today, however small. The
pieces exist inside the toolchain: the HTTP client already opens sockets, resolves names, enforces
host scopes and speaks TLS with the machine's trust store. What is missing is a library that hands a
program the connection itself, under the effects it already declares.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Talk to a server over a connection (Priority: P1)

A developer writes a client for a line protocol (Redis `PING`, an SMTP greeting, a custom service):
connect to a host and port, write a request, read the answer, write the next, close. Every step that
can wait has a time limit, and every failure says which kind it is.

**Why this priority**: it is the feature; every package named in the review is this loop.

**Independent Test**: against a local test server that speaks a small line protocol, a vo program
connects, exchanges several messages on one connection, and closes; the bytes on the wire and the
values the program sees match exactly, for text and for binary data with every byte value.

**Acceptance Scenarios**:

1. **Given** a server listening on a host and port, **When** a program connects with a time limit,
   **Then** it gets a connection; with nothing listening it gets the error "refused"; with a host that
   does not answer in time, "timed out"; with a name that does not exist, "not found".
2. **Given** an open connection, **When** the program sends bytes, **Then** all of them are sent, or
   the call returns an error; nothing is sent in part silently.
3. **Given** an open connection, **When** the program reads "up to N bytes", **Then** it gets between 1
   and N bytes as soon as any are available; "exactly N" returns N bytes or an error; "up to a
   delimiter" returns the bytes through the first delimiter. Bytes that arrived after what a read
   returned are kept for the next read, in order.
4. **Given** a peer that sends nothing, **When** a read's time limit passes, **Then** the read returns
   "timed out" and the connection remains usable.
5. **Given** a peer that closes the connection, **When** the program reads, **Then** it gets the bytes
   that had arrived, and then "closed"; a write to it returns "closed". Neither ends the program.
6. **Given** a connection the program closed, **When** it is used again, **Then** every operation
   returns "closed"; closing twice is harmless.
7. **Given** a read whose result would exceed the size limit for one read (a delimiter that never
   comes), **When** the limit is reached, **Then** the read returns "too large" and the bytes stay
   readable in smaller pieces.

---

### User Story 2 - Only the hosts the program declares (Priority: P1)

A reviewer reads `vo.effects` and sees every host, and optionally every port, the program can open a
connection to. A program cannot connect anywhere else, whatever its input says.

**Why this priority**: a raw connection is the widest thing vo has offered a program; without the
scope it would undo the reason for effects.

**Independent Test**: a program declaring `net("127.0.0.1:6379")` connects there, and is stopped with
R0010 when it tries another host or another port; the same without `uses net` is a check error;
`vo.effects` lists the scope.

**Acceptance Scenarios**:

1. **Given** a function that connects, **When** it does not declare `net`, **Then** `vo check` reports
   E0501 with the fix that adds it, as for `std.http`.
2. **Given** `uses net("db.internal")`, **When** the program connects to `db.internal` on any port,
   **Then** it is allowed; to any other host it stops with R0010 before any packet is sent.
3. **Given** `uses net("db.internal:5432")`, **When** the program connects to that host on another
   port, **Then** it stops with R0010. `std.http` requests obey the same scope with the URL's port.
4. **Given** a caller whose scope is `net("db.internal:5432")`, **When** it calls a function declaring
   `net("db.internal")`, **Then** that is E0503 (the callee is wider); the reverse is allowed.
5. **Given** a function that listens, **When** it declares `serve("9000")`, **Then** it can listen on
   port 9000 of this machine only, and `serve("0.0.0.0:9000")` on the network, exactly as
   `std.http.serve`; anything else is R0010.
6. **Given** a function that takes a connection and reads or writes it, **When** checked, **Then** it
   must declare the connection's effect (`uses E` for `Conn[E]`), so a pure function cannot use one.

---

### User Story 3 - Encrypted connections (Priority: P2)

A developer connects to a service that requires TLS, either from the first byte (Redis with TLS,
SMTPS) or after a plain-text exchange on the same connection (Postgres, SMTP `STARTTLS`). The server
is verified as for `https://`.

**Why this priority**: almost no hosted service accepts plain connections; without this the P1 story
works only on a private network.

**Independent Test**: against a local TLS test server with a test authority given by `--ca-file`, a
program connects with TLS and exchanges data; another connects in plain text, exchanges a line, then
upgrades and exchanges more; a certificate for another name, an expired one and an unknown authority
each give the error "tls" with the reason.

**Acceptance Scenarios**:

1. **Given** a TLS server with a valid certificate for the host, **When** the program connects with
   TLS, **Then** reads and writes work as on a plain connection.
2. **Given** an open plain connection, **When** the program upgrades it, naming the host to verify,
   **Then** later reads and writes are encrypted; bytes read before the upgrade were plain.
3. **Given** a certificate that does not verify (wrong name, expired, unknown authority, or a protocol
   older than TLS 1.2), **When** connecting or upgrading, **Then** the result is the error "tls" with
   the same reasons `std.http` gives; nothing turns verification off.
4. **Given** bytes already received but not yet read when an upgrade is asked, **When** upgrading,
   **Then** it is refused with an error (they would be plain text treated as encrypted).

---

### User Story 4 - Accept connections (Priority: P2)

A developer writes a small server for a protocol that is not HTTP, or a test that plays the server
side of a protocol for a client under test: listen on an address, accept a connection, converse,
close, accept the next.

**Why this priority**: needed to test clients inside vo, and for simple services; the client side is
what the review's packages need first.

**Independent Test**: a vo program listens on a loopback port and echoes lines; an outside client
connects twice in turn and gets its lines back; a vo test opens a listener and then runs the client
and the accepting side as two tasks.

**Acceptance Scenarios**:

1. **Given** a free port, **When** the program listens on it, **Then** it gets a listener; a port in
   use gives the error "in use".
2. **Given** a listener, **When** a client connects, **Then** accept returns a connection with the
   client's address, usable with every read and write of User Story 1; with no client before the time
   limit, "timed out".
3. **Given** a listener with port `0`, **When** it is opened, **Then** the system picks a free port
   and the program can ask the listener which (so tests do not collide).
4. **Given** a listener opened before tasks start, **When** one task accepts and another connects to
   it, **Then** they converse; neither waits on the other except through the connection.
5. **Given** a closed listener, **When** accept is called, **Then** it returns "closed".

---

### User Story 5 - Recorded and replayed (Priority: P2)

A developer records a run that talked to a database and replays it on another machine with no
network, getting the same output, as for HTTP and files.

**Why this priority**: determinism under replay is a property of every effect in vo; a connection that
escaped it would make every program using one unreplayable.

**Independent Test**: record a run of the User Story 1 client; stop the server; `vo replay` gives the
same output and exit status and opens no socket. The same for a run with a listener.

**Acceptance Scenarios**:

1. **Given** a recording, **When** replayed, **Then** each connect, read, accept and each error gives
   what it gave when recorded, in the same order per connection; writes succeed and send nothing.
2. **Given** a changed program that connects to another host or port than the recording has next,
   **When** replayed, **Then** it stops with R0014, as for a different HTTP request.
3. **Given** a recording of a TLS connection, **When** inspected, **Then** it holds the plain bytes
   the program read, and no key.
4. **Given** a timed-out read in the recording, **When** replayed, **Then** it returns "timed out"
   without waiting.

---

### User Story 6 - Documented where the rest is (Priority: P3)

`vo doc std.tcp` lists every item with signature, effects and an example; the effect table in the
reference and the site name the module; the scope syntax with a port is described where scopes are.

**Independent Test**: `vo doc std.tcp` and each item print; examples in the documentation run in the
suite; the reference stays under 15,000 tokens.

**Acceptance Scenarios**:

1. **Given** the reference, **When** read, **Then** `net` and `serve` rows name `std.tcp`, the scope
   paragraph gives `host:port`, and the library table has one `std.tcp` row.
2. **Given** `vo doc std.tcp.read_until`, **When** run, **Then** it shows the signature, the error
   type and a runnable example.

### Edge Cases

- Port outside 1 to 65535 (0 allowed only for listening), an empty host, a host with a scheme or path:
  an "invalid" error result naming the argument, before anything is opened.
- A time limit of zero or below, or above 10 minutes; a size of zero or below, or above the read
  limit; an empty delimiter: "invalid".
- A host given as an IPv4 literal: no name lookup; scopes match it as text, as for `std.http`.
- An IPv6 literal or a name with only IPv6 addresses: "not found" (IPv6 is not in this version).
- A name with several addresses: the first is used, as the HTTP client does.
- The peer resets the connection in the middle of a write: "closed", never a signal that ends the
  program.
- A write the peer does not read: it waits up to the connection's write time limit, then "timed out".
- More open connections and listeners than the limit: "too many", and the program can close some.
- Connections and listeners still open when the program ends, or when a task ends: closed by the
  system; no warning.
- A connection passed into a task (`concurrent_map`): the task works on its copy of the value, which
  refers to the same connection; two tasks reading one connection each get some of the bytes, which is
  the program's mistake and is documented.
- `--timeout` passes while a read waits: the run stops as for any other wait.
- `vo test`: a test using a connection declares `net` or `serve` with scopes like any function.
- `vo build`: a built program connects and listens under the same grant.
- Replay of a program that now reads fewer or more bytes per call than when recorded: what was
  received is replayed as a stream, so the replay works while the program waits for the peer at the
  same points; waiting where the recording has something else is R0014.
- Bytes are `bytes`; the library does not decode text (the caller uses the existing conversions).

## Requirements *(mandatory)*

### Functional Requirements

**Connecting, reading, writing**

- **FR-001**: The standard library MUST provide a module `std.tcp` that opens a TCP connection to a
  host name or IPv4 address and a port within a caller-given time limit.
- **FR-002**: It MUST provide three reads, each with a caller-given time limit: up to N bytes (returns
  as soon as at least one is available), exactly N bytes, and through a caller-given delimiter of one
  or more bytes. Bytes received beyond what a read returns MUST be kept for later reads.
- **FR-003**: It MUST provide a write that sends all the given bytes or returns an error, and a close
  that is harmless to repeat.
- **FR-004**: One read MUST NOT return more than 16 MiB; a read that would is the error "too large"
  and loses no bytes.
- **FR-005**: Every operation on a closed connection or listener MUST return the error "closed". No
  operation of the module ends the program except a scope violation (FR-010) and the general limits
  (`--timeout`, `--max-memory`).
- **FR-006**: Errors MUST be values of one exported variant, with at least these kinds: refused, timed
  out, closed, not found, unreachable, tls (with the reason), in use, too large, too many, invalid
  (with what), and other (with the system's reason). Results use it as their error type (spec 032).
- **FR-007**: At most 256 connections and listeners MUST be open at a time in one program; opening
  more is "too many".

**Effects and scopes**

- **FR-008**: Connecting MUST require the `net` effect and listening the `serve` effect; no new effect
  is added. A connection's type carries its effect (`Conn[uses net]` from connecting, `Conn[uses
  serve]` from accepting), and every function that reads, writes, upgrades or closes one MUST declare
  that effect.
- **FR-009**: A `net` scope MUST be either a host (any port, as today) or `host:port` (that port
  only). A scope with a port covers less than the same host without one, for E0503 and for the
  effects record (a gained port-less scope over a ported one is a widening, E0504). A malformed port
  is E0506. `std.http` MUST obey ported scopes with the port of the URL.
- **FR-010**: A connection to a host or port outside the grant, and a listener on an address outside
  it, MUST stop the program with R0010 before any network activity, as `std.http` does.
- **FR-011**: `vo.effects`, `vo effects FILE` and `vo build`'s `--vo-effects` MUST show the scopes
  with their ports.

**TLS**

- **FR-012**: The module MUST open a connection that is TLS from the first byte, and MUST upgrade an
  open plain connection to TLS, in both cases verifying the server for a given host name with the
  trust store, `--ca-file`, protocol versions and failure reasons of `std.http`. Verification cannot
  be turned off.
- **FR-013**: An upgrade MUST be refused when received bytes are waiting unread, and when the
  connection is already TLS or was accepted by a listener.

**Listening**

- **FR-014**: The module MUST listen on an address written as `serve` scopes are (`"9000"`: this
  machine only; `"0.0.0.0:9000"`: the network), accept one connection per call within a time limit,
  report the peer's address, and report the listener's port. Port `0` asks the system for a free
  port and is covered by a scope that names port `0` on the same address.
- **FR-015**: A listener opened before `concurrent_map` MUST be usable from its tasks, so that a
  program can run both ends of a connection.

**Record and replay**

- **FR-016**: `--record` MUST save, per connection, the result of its connect or accept, the bytes
  read and each error returned, and for listeners the chosen port. `vo replay` MUST return them in
  order with no network activity and no waiting; writes succeed. A connect to another host or port
  than the next recorded one, or a read past the recorded end, is R0014.
- **FR-017**: A recording MUST NOT contain TLS keys, and holds the decrypted bytes read.

**Fit with the rest**

- **FR-018**: The module MUST work on x86-64, ARM64 and RISC-V, without libc, in `vo run`, `vo test`
  and built programs.
- **FR-019**: The toolchain MUST NOT gain a static buffer over 64 KiB for this feature.
- **FR-020**: `std.http` MUST keep its behaviour; it is not rebuilt on `std.tcp` in this change.
- **FR-021**: `vo doc std.tcp` MUST document every exported item with a runnable example; the
  reference MUST stay under 15,000 tokens; the site, roadmap, changelog and review MUST be updated.

### Key Entities

- **Connection**: an open conversation with one peer; has an effect (`net` or `serve`), a state (open,
  closed), whether it is TLS, and received bytes not yet read.
- **Listener**: an address the program accepts connections on; has the port and a state.
- **TCP error**: the kind of a failure, with a reason where the kind has one.
- **`net` scope**: a host, optionally with a port.
- **Connection recording**: per connection, in order: how it opened, each read's bytes, each error.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A vo program holds a 1,000-message conversation on one connection with a local server,
  every message arriving intact, including binary messages that contain every byte value.
- **SC-002**: In the test suite, every attempt to connect or listen outside the declared scopes (other
  host, other port, other address) is stopped, at check time when the scope is a literal mismatch
  between caller and callee and at run time otherwise: 0 connections opened outside the grant.
- **SC-003**: A recorded run of each test program replays with the same output and exit status with
  the network unavailable.
- **SC-004**: Each of the three bad-certificate cases is refused with its reason; a valid one works,
  both from the first byte and after an upgrade.
- **SC-005**: A round trip of a 100-byte message on the loopback address takes under 1 ms on the
  development machine, and 16 MiB is transferred in under 1 second.
- **SC-006**: A 60-line vo program implements a working client for one real protocol command (Redis
  `PING`/`SET`/`GET` in the documentation's example) against a test server that speaks it.
- **SC-007**: The whole suite passes on the three architectures and the reference stays under 15,000
  tokens.

## Assumptions

- **No UDP in this version.** The review names both; every package it lists (Postgres, Redis, SMTP,
  queues) is TCP. UDP is added when a program needs it.
- **Ports in `net` scopes are optional**: `net("host")` keeps meaning any port, so no existing program
  or `vo.effects` file changes.
- **IPv4 only**, as the HTTP client is today. IPv6 is a separate change for client and server
  together.
- **`std.http` is not rebuilt on `std.tcp`**; they share the toolchain's socket, name lookup and TLS
  code underneath.
- **A server accepts one connection per call and the program serves it before the next** (or hands
  work to tasks itself). A worker pool like `serve_workers` for TCP is a later change.
- **Scope violations stop the program (R0010)**, as everywhere else; "not permitted" is therefore not
  an error kind. A refusal by the operating system is "other".
- Time limits are required arguments in milliseconds, 1 to 600,000; there is no "wait for ever".
- Limits: 16 MiB per read, 256 open connections and listeners.
- Listening sockets allow quick reuse of a port after a restart; connections send small writes
  without delay. No other socket option is exposed.
- Replay does not compare what the program writes with what it wrote when recorded; only where it
  connects.
- No real editor, database or mail server is contacted by the suite: test servers are local scripts.
- Out of scope: Postgres, Redis and SMTP packages; serving TLS; connection pools; streaming types;
  Unix domain sockets; raw sockets; half-close; keep-alive settings.

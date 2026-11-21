# Research: TCP Connections (`std.tcp`)

## R1. Where a connection lives

**Decision**: a static table of 256 entries in `src/tcp.c`. An entry holds the socket, a kind
(connection or listener), the effect it was opened under, a generation number, the peer's host and
port, the TLS session if any, and a mapped read buffer (start, read position, length, capacity, and how
far a delimiter search has looked). The vo value is `Conn { id }` with `id = slot + 256 × generation`,
so a value kept after `close` never reaches the connection that later reuses the slot: it gets
"closed".

**Rationale**: the same shape as `std.sqlite`'s databases. Nothing of a connection is in the collected
heap, so a collection never moves or scans it, and a fork (a task) copies the table as it copies
everything. 256 × about 80 bytes is 20 KiB of static data, under the 64 KiB rule; the read buffers are
mapped on first use (64 KiB, doubling up to 16 MiB + 64 KiB) and unmapped at close.

**Alternatives considered**: a heap object with a finalizer (the collector has none); the file
descriptor itself as the value (reused numbers reach the wrong peer, and a program could guess one).

**Forged values**: a program can write `new Conn { id: 0 }` with no effect in its type, as it can for
`Database`. The table's "opened under" effect is checked against the running grant on every use
(`rt_check_effect`), so a function without `net` (or `serve`) that reads a connection stops with R0010
whatever its type says.

## R2. Sockets, names and waiting

**Decision**: reuse `net_open` (IPv4 literal, `/etc/hosts`, one DNS query, non-blocking connect) and
`net_wait` (bounded by `--timeout`, which is R0009, and by the call's own limit `net_cap_ms`, which is
an error value). `net_open` gets an out-parameter for the kind of failure (not found, refused,
unreachable, timed out, other) next to its message, which `std.http` ignores. Sockets are non-blocking;
sends use `MSG_NOSIGNAL`; connections set `TCP_NODELAY`; listeners set `SO_REUSEADDR`. The listening
socket code of `src/serve.c` (address parsing, bind, listen) is called by both.

**Several addresses** (spec edge case): today's lookup returns the first A record only. This stays;
the edge case is narrowed in the contract to "the first address". Trying each is a change to the
resolver that HTTP would want too, and is left with IPv6.

**Time limits**: each call sets `net_cap_ms = now + timeout_ms` around its waits, as `std.http.send`
does, so one limit covers lookup, connect and handshake together. 1 to 600,000 ms, else "invalid".

## R3. TLS

**Decision**: split `tls_exchange` into `tls_open(fd, host, port, alpn, &err)` (configuration,
handshake, verification: returns a session or the existing error results), `tls_read`, `tls_write`,
`tls_close`. `tls_exchange` becomes open, write, read to the end, close, with ALPN `http/1.1`;
`std.tcp` passes no ALPN. A session (the library's context and configuration, about 40 KiB with its two
16 KiB record buffers) is allocated from the TLS arena, which grows from 4 MiB to 8 MiB; when the
arena is full, opening gives "too many". The handshake's failure texts are the ones `std.http` gives;
the kind is always `tls`, except a time limit ("timed out") and a refused or lost socket.

`start_tls` is `tls_open` on the entry's existing socket. It is refused ("invalid") when the read
buffer holds unread bytes, when the entry is already TLS, and when the entry was accepted.

**Tasks**: a forked task has a copy of the session; two processes writing through copies of one
session corrupt it for the peer, exactly as two tasks reading one plain connection split its bytes.
Documented with that edge case: a connection belongs to one task at a time.

**Alternatives considered**: a second TLS configuration for `std.tcp` (two places to keep the cipher
and version policy: rejected).

## R4. `net` scopes with a port

**Decision**: a `net` scope is `HOST` or `HOST:PORT` (decimal 1 to 65535, no leading zero); anything
else after a colon is E0506. One function, `es_pair_covered`, decides coverage for `net`: equal texts,
or the declared scope is the host of the required `HOST:PORT`. Everything goes through it: E0503
(caller covers callee), E0504 and W0502 against `vo.effects`, and the run-time check. At run time the
required pair is built from the call's host and port (`rt_check_net(host, port)`), used by `std.tcp`
and by `io_exchange` for `std.http`, replacing `rt_check_name(EF_NET, host)` there. R0010 names
`host:port`.

`serve` scopes accept port `0` (`"0"`, `"0.0.0.0:0"`) for a system-chosen port. `std.http.serve`
keeps refusing it with its error result, since a server nobody can find is a mistake there.

**Rationale**: a bare host keeps its meaning, so no program or record changes. IPv6 literals contain
colons; they are not valid hosts today, so the syntax is free.

## R5. The primitives and typed errors

**Decision**: two builtins, usable only from the standard library like the other `__` names:

- `__tcp(op, a, b, text) -> result[int]`: connect (`text` host, `a` port, `b` ms: the id), connect
  with TLS, listen (`text` address: the id; the port is asked with another op), accept (`a` listener,
  `b` ms), start TLS (`a` id, `b` ms, `text` host), close, port of a listener.
- `__tcp_io(op, id, data, n, ms) -> result[bytes]`: read up to `n`, read exactly `n`, read through
  `data`, write `data`, peer address as text.

The effect of `__tcp` depends on `op` (connect: `net`; listen and accept: `serve`), set by the checker
as for `__sql_open`; `__tcp_io` has none of its own (the entry's effect is checked at run time, R1) and
the library functions declare `uses E` from `Conn[E]`.

An error comes back as text `KIND: reason`, with `KIND` one of eleven fixed words written only in
`src/tcp.c`, and `std/tcp.vo` turns it into the `TcpError` variant in one private function.

**Rationale**: the builtin signature table has no code for a variant result, and adding one for one
module is more machinery than one function that splits at the first colon. The words are not an
interface: programs see only the variant.

**Alternatives considered**: one builtin per operation (twelve ids and table rows for no gain); error
numbers with a second call for the reason (state between calls).

## R6. Record and replay

**Decision**: events are recorded where the program waits for the outside, in program order like every
other event:

| Event | Recorded | On replay |
|---|---|---|
| open | host, port, TLS or not; ok or the error text | must be the next event with the same host, port and TLS flag, else R0014 |
| start TLS | ok or the error text | as recorded |
| receive | the bytes one wait brought in, or the error (timed out, closed, other) | fills the entry's read buffer, without waiting |
| send failed | the error | a write returns it when it is the next event; otherwise writes succeed and send nothing |
| listen | address; the port; ok or error | no socket; the recorded port |
| accept | the peer's address, or the error | an entry with no socket |

The three reads run the same code in both modes over the entry's buffer, so their results follow from
the recorded receives. A program changed to read in other sizes replays as long as it waits for the
peer at the same points; if it now waits where the recording has another event, that is R0014 (the
spec's edge case is worded to match). Tasks relay their events to the parent as they do today.
Recordings hold what was decrypted, never session keys (they never leave the TLS library).

**Alternatives considered**: recording each read call's result (replay breaks on any change of read
sizes, and three reads need three formats).

## R7. Reads

**Decision**: `read(c, max, ms)` returns the buffered bytes if there are any (up to `max`), otherwise
waits for one receive. `read_exact(c, n, ms)` and `read_until(c, delimiter, ms)` receive until
satisfied, under one time limit for the call. A timeout or "too large" leaves the buffer as it is, so
the bytes are still readable. After the peer closes, buffered bytes are still returned; then "closed"
(`read_exact` and `read_until` return "closed" when the rest can never come, leaving the bytes for
`read`). `max` and `n` are 1 to 16 MiB; the buffer never holds more than 16 MiB + 64 KiB, so
`read_until` gives "too large" when 16 MiB hold no delimiter. The delimiter search remembers how far it
looked (minus the delimiter's length), so it is linear overall.

## R8. Listening and tasks

**Decision**: `listen(address)` under `serve`, the address checked against the grant as
`std.http.serve`'s is; `accept(listener, ms)` waits with `net_wait` and returns `Conn[uses serve]`
with the peer's `a.b.c.d:port`. A task is a fork, so a listener (or connection) opened before
`concurrent_map` is open in each task: a test opens the listener, then maps over two roles, one
accepting and one connecting. Entries opened inside a task end with the task.

## R9. Documentation budget

**Decision**: the reference changes in three places: the `net` and `serve` rows of the effect table
name `std.tcp`; the scope sentence gains "hosts (optionally `host:port`) for `net`"; the library table
gains one row. About 60 tokens. The site gets `library/tcp.md` with the Redis example of SC-006, which
runs in the suite against the test peer.

## R10. Tests

**Decision**:

- `tests/tcp_server.py`: local peers started by the test: a line echo, a tiny Redis subset (`PING`,
  `SET`, `GET` in RESP), a silent one, one that closes mid-message, one that resets, a STARTTLS
  greeter, and TLS variants with the certificates `tests/https_server.py` already makes (valid, wrong
  name, expired, unknown authority).
- `tests/tcp.py` (in `make test`, `VO_BIN` for the other architectures): groups client, limits,
  scopes, tls, server, tasks, replay (each client program recorded, the peer stopped, replayed, same
  output), docs example, timing (`--time`, SC-005, not part of the pass).
- Golden cases: `tests/cases/effects/` for E0506 on a bad port, E0503 between ported and bare scopes,
  E0504 on a widened record; `tests/cases/tcp/` for argument errors and use after close, which need no
  peer.
- Regression: the HTTP and HTTPS tests unchanged; `run/oom_small` and `map_keys/many` under
  `--stress` once.

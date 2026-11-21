# Contract: `std.tcp`

```text
import std.tcp.connect;   # one item per import, as for every module
```

## Items

| Item | Signature | Notes |
|---|---|---|
| `connect` | `(host: str, port: int, timeout_ms: int) -> result[Conn[uses net], TcpError] uses net` | name lookup, then connect, within the limit; the first address of a name |
| `connect_tls` | same | then a verified TLS handshake for `host`, within the same limit |
| `start_tls` | `(c: Conn[uses net], host: str, timeout_ms: int) -> result[void, TcpError] uses net` | `invalid` with unread bytes waiting or on a TLS connection (an accepted connection is refused by its type) |
| `read` | `[E: effects](c: Conn[E], max: int, timeout_ms: int) -> result[bytes, TcpError] uses E` | 1 to `max` bytes as soon as any are there |
| `read_exact` | `[E: effects](c: Conn[E], n: int, timeout_ms: int) -> result[bytes, TcpError] uses E` | exactly `n` |
| `read_until` | `[E: effects](c: Conn[E], delimiter: bytes, timeout_ms: int) -> result[bytes, TcpError] uses E` | through the first `delimiter`, which is included |
| `send` | `[E: effects](c: Conn[E], data: bytes, timeout_ms: int) -> result[void, TcpError] uses E` | all of `data`, or an error |
| `close` | `[E: effects](c: Conn[E]) uses E` | harmless to repeat |
| `listen` | `(address: str) -> result[Listener, TcpError] uses serve` | `"PORT"`: this machine; `"0.0.0.0:PORT"`: the network; port `0`: a free one, in `Listener.port` |
| `accept` | `(l: Listener, timeout_ms: int) -> result[Conn[uses serve], TcpError] uses serve` | `Conn.peer` is `a.b.c.d:port` |
| `close_listener` | `(l: Listener) uses serve` | |
| `describe` | `(e: TcpError) -> str` | a sentence for a person |

## Errors

| Case | When |
|---|---|
| `refused` | nothing listens there |
| `timed_out` | the call's time limit passed; the connection stays usable (a connect leaves nothing open) |
| `closed` | the peer closed or reset; the program closed it; the value is from a closed connection |
| `not_found` | the name has no IPv4 address, or no name server answered |
| `unreachable` | no route to the host or network |
| `tls(reason)` | the handshake or verification failed; `reason` as `std.http` words it |
| `in_use` | `listen`: the port is taken |
| `too_large` | 16 MiB arrived without what the read asked for; the bytes stay readable |
| `too_many` | 256 connections and listeners are open |
| `invalid(what)` | an argument out of range: port, time limit, size, empty host or delimiter, a bad address; `start_tls` refused |
| `other(reason)` | anything else the system reports |

Stops the program: a host, port or address outside the grant (R0010, before any network activity);
`--timeout` (R0009); `--max-memory` (R0006).

## Scopes

```text
uses net("db.internal")          # any port of that host
uses net("db.internal:5432")     # that port only
uses serve("9000")               # listen on this machine's port 9000
uses serve("0")                  # listen on a free port of this machine
```

`net("h")` covers `net("h:1")`; not the reverse (E0503, E0504). A bad port is E0506. `std.http` is
checked against the same scopes with the URL's port.

## Recording

`--record` saves how each connection opened and every piece received, in order; `vo replay` opens no
socket and never waits. A replayed program must connect to the same hosts and ports in the same order
and wait for the peer at the same points (R0014 otherwise). What it writes is not compared.

## Tasks

A connection or listener open before `concurrent_map` is open in every task. Use each connection from
one task at a time. Those opened inside a task are closed when it ends.

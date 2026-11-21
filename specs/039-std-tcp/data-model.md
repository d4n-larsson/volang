# Data Model: TCP Connections (`std.tcp`)

## Entry (one of 256 in `src/tcp.c`)

| Field | Meaning |
|---|---|
| kind | free, connection, listener |
| generation | incremented at close; part of the id |
| fd | the socket; -1 in replay |
| effect | `net` or `serve`: what it was opened under, checked on every use |
| accepted | 1 for a connection from `accept` (no `start_tls`) |
| host, port | the peer (connection) or the bound port (listener) |
| tls | the TLS session, or none |
| eof | the peer closed; buffered bytes may remain |
| buf, pos, len, cap, scanned | the mapped read buffer: unread bytes are `buf[pos..len)`; `scanned` is how far a delimiter search has looked |

Id: `slot + 256 × generation`. States: free → open → free (close, or the end of the process). An id
whose slot is free or whose generation differs is "closed".

## vo types (`std/tcp.vo`)

```text
variant TcpError { refused; timed_out; closed; not_found; unreachable; tls(str); in_use;
                   too_large; too_many; invalid(str); other(str); }
record Conn[E: effects] { id: int; peer: str; }
record Listener { id: int; port: int; }
```

## `net` scope

`HOST` or `HOST:PORT`, PORT 1 to 65535 without a leading zero. `HOST` covers `HOST` and every
`HOST:PORT`; `HOST:PORT` covers only itself. `serve` scope: `PORT` or `0.0.0.0:PORT`, PORT 0 to 65535.

## Recording events (schema of `--record`, added kinds)

| Kind | Fields |
|---|---|
| `tcp_open` | `name` host, `port`, `scheme` (`tls` when TLS), `result`, error text |
| `tcp_tls` | `result`, error text |
| `tcp_recv` | `result`; `value` the bytes, or the error text |
| `tcp_send` | error text (only failed writes) |
| `tcp_listen` | `name` address, `result`, `value` the port or the error text |
| `tcp_accept` | `result`, `value` the peer address or the error text |

Events are in program order. A replayed entry has no socket; its buffer is filled from `tcp_recv`
events.

## Limits

| What | Limit | Beyond it |
|---|---|---|
| open entries | 256 | `too_many` |
| one read | 16 MiB | `too_large` (`invalid` when asked for more) |
| time limit of a call | 1 to 600,000 ms | `invalid` |
| port | 1 to 65535 (0 for `listen`) | `invalid` |
| TLS sessions | what the 8 MiB TLS arena holds (over 100) | `too_many` |

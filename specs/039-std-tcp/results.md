# Results: TCP Connections (`std.tcp`)

Measured on the development machine (x86-64), 2026-11-21. The gate was not run.

## Success criteria

| Criterion | Result |
|---|---|
| SC-001: 1,000 messages on one connection, every byte value | passes (`client/thousand messages and every byte value`): 1,000 text messages and one of all 256 byte values, each read back equal |
| SC-002: nothing opened outside the grant | passes: another port, another host, `std.http` to another port and a listener on another address each stop with R0010; a watching socket saw no connection |
| SC-003: every test program replays with no network | passes: client, timeouts, peer closing, failed writes, TLS, upgrade, a failed handshake, refused, a server with two clients, both ends in tasks: same output and exit status with every peer stopped |
| SC-004: bad certificates refused, valid ones work | passes: wrong name, expired and unknown authority, each on `connect_tls` and on `start_tls`, with `std.http`'s wording; the valid certificate works both ways |
| SC-005: round trip under 1 ms, 16 MiB under 1 s | 0.036 ms per 100-byte round trip (2,000 in 72 ms); 16 MiB received in 31 ms |
| SC-006: a Redis client in at most 60 lines | passes: the example in `site/docs/guide/standard-library.md` is 58 lines and prints `PONG`, `OK`, `tea` against the test peer (`docs` group) |
| SC-007: three architectures, reference budget | `tests/tcp.py`: 8 groups, 58 cases, 0 failures on x86-64, ARM64 and RISC-V (the last two under qemu); reference about 14,898 of 15,000 tokens |

`make test` passed once on the final code (`tcp: 8 groups, 58 cases, 0 failures`; `mcp`, `lsp`,
`grammars`, `https_interop` and the golden cases with 0 failures; the log ends at the last step).
`make site-check` passes. Stress run (`tests/run.sh --stress`) of `run/oom_small`, `map_keys/many`,
`tcp/arguments` and `effects/serve_port_zero`: 4 passed. No new static object: the connection table and
the read buffers are mapped on first use.

## Deviations from the plan

- **`write` is called `send`.** `write` is a builtin name and cannot be redefined by a module.
- **The primitives take three arguments**, the most a builtin can: the time limit is set with the
  existing `__http_timeout` just before the call, and the size of a sized read rides with the operation
  number. (The plan had four and five arguments.)
- **No shared `net_listen` helper** (task T005): `std.tcp` has its own ten lines of bind and listen, and
  `src/serve.c` is untouched. `net_open` reports the kind of failure through a variable (`net_kind`),
  not a new parameter.
- **`start_tls` on an accepted connection cannot be written**: the types refuse it (`Conn[uses serve]`
  where `Conn[uses net]` is needed), so the run-time refusal has no test.
- **No separate site page**: the module has a section in the standard-library guide, as the others do,
  and its generated `vo doc` page.
- **One existing golden case changed on purpose**: `effects/serve_scope_bad` no longer reports
  `serve("0")`, which is now valid. `doc/all` and `doc/all_json` gain the module's line.
- **A union of `net` scopes drops ports a bare host covers**, so the E0503 fix says `net("db")`, not
  `net("db", "db:5432")`.
- **R0010 names `host:port` only when the grant has a scope with a port**; otherwise the host alone,
  so no existing message changed.
- **Tests and code were written together**, story by story, not tests first.

## Found on the way

- **Strings have no `\r` escape.** Every line protocol (Redis, SMTP, HTTP) ends lines with a carriage
  return and a line feed, so a client writes `chr(13) + "\n"`, as `std/http.vo` does internally. Adding
  the escape is a language change (lexer, reference, the three editor grammars) and was left out.

## Not verified

- No real Redis, PostgreSQL or SMTP server: the peers are Python scripts.
- The unknown-name case accepts "not found" or "timed out", because the answer depends on the machine's
  name server.
- Names with several addresses, a full TLS arena (`too_many` for sessions) and `unreachable` have no
  test.
- The gate (`make gate`), including the collector stress suite over every case and the fault builds.

# Results: Events (spec 063)

Status: implemented. `make test` passed (see the last section). The gate was not run.

Two modules written in vo (`std/sse.vo`, `std/websocket.vo`), one small runtime file (`src/ws.c`), two
internal builtins (`__ws`, `__serve_take`), one `std.tcp` operation (send unless the peer has closed),
one recording event kind (`serve_take`), `web.on_request`, the request-log field `end` and the gauge
`http_conversations_open`. No syntax, no effect and no diagnostic code was added.

## After the analysis

The eighteen findings of `/speckit-analyze` were applied before the code was written: typed connect
errors, `EventStream` generic over its effect, `close_socket` returning a result, the `ponytail:`
convention in the tasks, and the rest as recommended.

## Changed while building

- **`Event` is `StreamEvent`**: `std.security` already exports an `Event` (`tests/std_names.py`).
- **`closed { code, reason }`**: a variant case carries one value or named fields.
- **`--timeout` stops at the take.** `vo run` limits each handler call to `--timeout` seconds (10 by
  default), so every conversation died after 10 s. Found by the full-size run, not the quick one. A
  handler that has taken its connection is no longer under it; its own time limits, its client, the
  route's bound and the server's stop limit it. Case: `stream`, a 2 s stream under `--timeout 1`.
- **Every send on a taken connection is recorded** (`tcp_send`, without the bytes). Outside tests
  `std.tcp` records only failed sends and a replay gives a failure to the next send; a stream handler
  only sends, so the failure at event 8 went to the head and the handler never ran.
- **A client that ends its side after the request counts as gone.** The test tool's `request` step does
  that; browsers, `EventSource` and curl do not. The take's step cases use `request-open`.
- **`serve_events` takes any method** (model APIs stream answers to POST); `ws_accept` requires GET.
- **`web.on_request` routes are checked when serving starts** (`check_routes`), not by `vo check`,
  whose route checks read two-argument `on_get`-style calls.
- **The version was not raised**: the change is under "Unreleased" in the changelog, beside spec 062.
- **No independent event-stream server was installed**: for the reading role the format cases
  (`tests/cases/sse/format.vo`) were written from the HTML standard's rules, case by case, not copied
  from web-platform-tests.

## Found and fixed on the way

- `std.http.send_stream` opened a plain connection before every `https://` one and left it open
  (since spec 040): every `https://` stream used two entries of 256.
- `vo test` kept the `std.tcp` connections a test left open until the whole run ended.
- A missing collector root in the new send path made workers crash under the stress build.
- A message in many fragments took quadratic time (65,536 fragments of 64 bytes: over 100 s);
  Autobahn's 9.3.1 found it. Now a running size and a pairwise join.

## SC-001: no parsing code

The guide's examples: reading a stream is 7 lines without imports and blank lines (bound 15); connect,
send, receive and close is 17 (bound 20). `examples/live-status/follow/main.vo` reads a stream and
reads again after a lost connection in 14 lines of `main`.

## SC-002: delivery delay

`events.py --only read`: 100 events 50 ms apart after 60 s of silence, each printed by the program
within 50 ms of its last byte leaving the server (20 events and 3 s in `--quick`). Under qemu the bound
is 20 times larger.

## SC-003: volume

- 100,000 events of 100 bytes read in 0.36 s under `--max-memory 32`.
- 10,000 WebSocket messages of 0 bytes to 1 MiB (2.6 GB echoed) came back unchanged in 56.6 s under
  `--max-memory 128`; 1,000 more against vo's own server, from `ws_connect` and from the Python peer.

## SC-004: conformance

`tests/autobahn.sh` (Autobahn test suite in Docker, sections 1 to 7, 9 and 10; not part of
`make test`): no failed case in either role.

| Role | OK | Non-strict | Informational | Failed |
|---|---|---|---|---|
| server (`ws_accept`) | 294 | 4 | 3 | 0 |
| client (`ws_connect`) | 294 | 4 | 3 | 0 |

The non-strict cases are 6.4.1 to 6.4.4: a text message whose UTF-8 breaks in a middle fragment is
refused when the message is complete, not at that fragment. The informational ones are 7.1.6, 7.13.1
and 7.13.2. Independent peers besides Autobahn: `tests/ws_peer.py` (written from RFC 6455 with the
Python standard library) in both roles, and curl as a client of `serve_events`.

## SC-005: a client that left, and stopping

- A handler learns that its client left within 1.15 s of the next send (sends every 100 ms).
- A client that stops reading: `not_reading` after 30 to 44 s (the full run).
- 64 workers holding 30 WebSockets and 30 streams, SIGTERM, with and without `--drain 5`: every
  WebSocket got 1001, every stream ended, the server exited within 5 s. Run twice per test run (the
  spec asks for 20 of 20; that count was not run). A handler that ignores the stop is ended at the
  drain limit; under `serve` the one conversation is closed with 1001.

## SC-006: the bound

8 workers, a route with `max_open` 4, 50 clients that open a stream and never read: 4 served, 46
refused with 503, and `/health` answered within twice its idle time throughout. Routes with 2 and 6
share the count; `max_open` 63 with 8 workers stops at 7; the gauge shows 4, then 0.

## SC-007: hostile input

- 140 named hostile frame sequences per direction (`tests/ws_peer.py`): each gives its error value and
  the right close code, in both roles; 24 sequences that keep the rules give their messages.
- `events.py --fuzz`: 100,000 mutated sequences against `ws_connect` (63 s), 100,000 against
  `ws_accept` (27 s) and 100,000 mutated event streams against `read_events` (80 s): error values only,
  no runtime error, no hang, under `--max-memory 64`. 2,000 of each in `make test`.
- A second conversation echoes undisturbed while the first is fed every hostile sequence; 10,000 pings
  are answered by both roles.

## SC-008: recordings

100 recordings of each role against peers with random delays each replay to their own run. For the
two server roles the lines are compared as a multiset: a replay of `serve_workers` runs requests one
at a time in arrival order (spec 028), so the output of conversations that overlapped in time can come
in another order. A program reading without a time limit printed the same bytes in 20 of 20 runs. A
recorded test holds no handshake token and replays offline; a changed message is R0014 at its send.

## SC-009: example, budget, suite

- `examples/live-status`: a service streaming a job's progress and a follower, tests without a network,
  recordings of both replayed by `make test` and `tests/build_examples.py`.
- The reference: 14,998 tokens of 15,000. Two rows and the effect entries cost 34 words; 36 were cut
  (the `std.http`, `std.tcp`, `std.json`, `std.decimal` and `std.math` rows and one sentence on
  `serve`).
- `make test` passed on x86-64; `make quick AREA=events` passed on ARM64 and RISC-V under qemu (697
  cases), and `tests/cross_replay.py` is identical for all six pairs. The stress build (a collection
  every allocation for the cases, every 64th for `events.py`) and the instrumented build pass the new
  cases and `events.py --quick`.

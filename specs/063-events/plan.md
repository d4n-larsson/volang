# Implementation Plan: Events (server-sent events and WebSockets, as client and as server)

**Branch**: `063-events` (work on `main`, as for specs 001–062) | **Date**: 2026-11-28 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/063-events/spec.md`

## Summary

Both protocols are written in vo on what exists: `std.sse` on `std.http.send_stream`, `std.websocket`
on `std.tcp`. Because every byte goes through `std.tcp`, time limits, scopes, recording, replay,
recorded tests and the sweep of open connections are the ones spec 039 built, for clients with no new
C at all. The C work is the one thing the library cannot do today: a handler keeping its request's
connection.

- **Clients** (stories 1, 2): `sse.read_events` is a parser fed by `send_stream`'s pieces;
  `websocket.ws_connect` dials with `std.tcp`, does the handshake, and frames in vo. One internal
  builtin, `__ws`, supplies what vo code cannot do fast or at all: masking bytes, the SHA-1 of the
  handshake, and key bytes that are recorded but are not the `entropy` effect (R4).
- **A handler takes its connection** (stories 3, 4): `__serve_take` moves the connection of the
  request being answered into the process's `std.tcp` table, as a `Conn[uses serve]`. Under
  `serve_workers` the server process passes the socket to the worker over a socket pair
  (`SCM_RIGHTS`) and stops watching it (R1). From then on server and client run the same vo code on
  the same kind of connection.
- **The handler's shape does not change**: `serve_events(...)` and `ws_accept(...)` are called inside
  an ordinary handler, hold the conversation in a function the handler passes, and return a
  `Response` that is never sent but is what the steps after the handler see (status, `Vo-End`), so
  request logging records the conversation's end and duration with no new code path (R2).
- **The bound** (story 5): the server process counts taken connections and grants a take only below
  the route's `max_open` and never for the last free worker; a refused take answers 503 (R3). On a
  stop signal the server wakes every worker that holds a conversation; its next wait returns
  "stopping" and the library closes with 1001 (R5).
- **Tests**: a scripted stream and a scripted socket, in vo, let a test run a handler's function with
  given messages and read what it sent (R8). Clients use recorded tests (spec 054) unchanged.

| Part | File | What |
|---|---|---|
| Take | `src/serve.c`, `src/serve_workers.c`, `src/tcp.c`, `src/sys.c`, `src/platform*.h` | `__serve_take(kind, max_open)`; socket pair per worker, `sendmsg`/`recvmsg`; `TW_TAKE`; the talking count and its gauge; closing a taken connection when its handler returns or fails |
| Stop | `src/net.c`, `src/serve.c`, `src/serve_workers.c` | a wake descriptor beside every network wait while a conversation is held |
| Framing help | `src/ws.c` (new, about 70 lines), `src/sha1.c` (as is) | `__ws(op, a, b)`: key bytes, mask, accept hash |
| Recording | `src/replay.c`, `src/vo.h` | event `serve_take` (granted or refused); key bytes as `entropy` events |
| Wiring | `src/vo.h`, `src/check_expr.c`, `src/rt_call.c`, `src/check_effects.c`, `src/doc.c`, `Makefile`, `tools/quick.sh` | two builtin ids, two modules |
| Library | `std/sse.vo` (new), `std/websocket.vo` (new), `std/web.vo` (`on_request`; `end` in request logs) | [contracts/events.md](contracts/events.md) |
| Example | `examples/job-queue/` (progress as a stream, a client that reads it) | story 7 |
| Docs | `docs/reference.md` (two rows; about 90 tokens cut elsewhere), `docs/security.md`, `docs/roadmap.md`, `CHANGELOG.md`, `site/docs/guide/` (one page) | R10 |
| Tests | `tests/events.py` (new, in `make test`), `tests/ws_peer.py` (new: a peer written from RFC 6455 with the Python standard library), `tests/cases/sse/`, `tests/cases/websocket/`, `tests/autobahn.sh` (outside `make test`) | R9 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc) and vo.

**Primary Dependencies**: none new. `std.sse` imports `std.http`; `std.websocket` imports `std.tcp`,
`std.http` (its `Request`, `Response`, `Header`) and `std.bytes`. `std.web` imports neither.

**Storage**: none.

**Testing**: golden cases for parsing, framing and every error value (through scripted streams and
sockets, so they need no network); `tests/events.py` runs vo programs against `tests/ws_peer.py` and a
small event server for latency, order, sizes, hostile input, the bound, stopping and 100 replays per
role; `tests/autobahn.sh` runs the Autobahn test suite in Docker for the results document.
`make quick AREA=events` between edits, `make test` once per change, `make STRESS=1` with the
interval-1 runs for the C changes; the gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: an event or message reaches the program within 50 ms of arriving (SC-002: one
read per wait, no buffering stage); 10,000 messages up to 1 MiB without growth (SC-003: masking in C,
one copy per message).

**Constraints**: memory per conversation bounded by one message (default 1 MiB, at most 16 MiB, the
size of one `std.tcp` read); no static buffer over 64 KiB; reference ≤ 15,000 tokens (at the limit
now); 64 workers and 256 `std.tcp` entries as today, so one server holds at most 63 conversations.

**Scale/Scope**: about 300 lines of C (take and hand-over 150, wake 40, `ws.c` 70, recording 40),
900 lines of vo (`sse` 300, `websocket` 550, `web` 50), 1,100 lines of Python tests.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One way per thing: events arrive through a function the program passes (as `send_stream`), a socket is sent to and received from. Handlers keep one shape. No reconnection or keep-alive happens that the program did not write. | Pass |
| II. Machine-readable feedback | Typed error variants with a text function each; R0010 for scopes as today. No new diagnostic code; the recording gains one event kind (an addition, schema version unchanged). | Pass |
| III. Determinism | Everything received is a `std.tcp` event in the recording; key bytes are recorded; timing enters only through time limits and the stop. No input from a peer is a runtime error. | Pass |
| IV. Test-first | Golden cases and `tests/events.py` per story before its code; the hostile corpus before the framing. | Pass |
| V. Single source of truth, minimal surface | Two modules with 19 functions, and one in `std.web`. No new effect, type parameter kind or syntax. The reference rows are paid for by cuts stated in the change. Left out and marked: broadcast, compression, fragments in the program's hands. | Pass |
| VI. Backend network services | Write: an agent no longer writes a frame parser or an event parser per program. Review: `net("host:443")` covers conversations; a route that holds connections says `uses serve` and names its origins. Operate: a bound that keeps the API answering, a clean stop, one log record and a gauge per conversation. | Pass |
| Vendored code | None (SHA-1 is the project's own `src/sha1.c`). | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/063-events/
├── plan.md
├── research.md          # R1–R10
├── data-model.md
├── quickstart.md
├── contracts/
│   └── events.md        # both modules, errors, limits, the wire, the recording
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
std/sse.vo, std/websocket.vo (new); std/web.vo
src/ws.c (new); src/serve.c, src/serve_workers.c, src/tcp.c, src/net.c, src/replay.c, src/sys.c,
    src/vo.h, src/check_expr.c, src/check_effects.c, src/rt_call.c, src/doc.c
tests/events.py, tests/ws_peer.py, tests/autobahn.sh (new); tests/cases/sse/, tests/cases/websocket/
examples/job-queue/; docs/reference.md, docs/security.md, docs/roadmap.md, site/docs/guide/
```

**Structure Decision**: one module per protocol, named as people search for them (`sse`,
`websocket`); the server and client halves of a protocol share its types and code, so they are not
split further.

## Order of work

1. `std.sse` reading: the parser with the ported format cases, `read_events`, the ends; replay.
2. `std.websocket` client: `__ws`, handshake, frames, the hostile corpus, `tests/ws_peer.py`; replay.
3. The take: `serve` first (one process), then `serve_workers` with the hand-over, the bound and 503.
4. `serve_events` and `ws_accept` on the take; scripted streams and sockets; `web.on_request`; the
   log field and the gauge.
5. Stopping (the wake), a failing handler, the sweep; the stress build at interval 1.
6. Example, guide, reference rows and cuts, roadmap; Autobahn; three architectures; results.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Key bytes drawn without the `entropy` effect (`__ws` op 1) | RFC 6455 requires an unpredictable mask per frame; the bytes never reach the program | Declaring `entropy` on every function that sends a WebSocket message tells a reviewer something false (that the function makes secrets); a fixed mask breaks the RFC. TLS already draws its randomness the same way. |
| A second channel per worker (a socket pair) | Pipes cannot carry a descriptor | Relaying every frame through the server process keeps one channel but adds buffering, back-pressure and a second copy of each message there (R1). |

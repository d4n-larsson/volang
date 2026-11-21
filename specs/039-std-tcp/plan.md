# Implementation Plan: TCP Connections (`std.tcp`)

**Branch**: `039-std-tcp` (work on `main`, as for specs 001–038) | **Date**: 2026-11-21 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/039-std-tcp/spec.md`

## Summary

A new library module `std/tcp.vo` over two new primitives in a new file `src/tcp.c`. The C side keeps a
small table of open connections and listeners (R1); a vo value holds only a number into it. Sockets,
name lookup and waiting are the HTTP client's (`src/net.c`), opened up for reuse (R2). The TLS exchange
in `src/tls.c` is split into "open a verified session", "read", "write" and "close", which both the
HTTP client and `std.tcp` call (R3). `net` scopes learn an optional port in one comparison function,
which the checker, the effects record and the run-time check all go through (R4). Recording works one
level below the reads: each wait for the peer is one event, so the three reads are ordinary code over a
recorded stream (R6).

| Part | File | What |
|---|---|---|
| Library | `std/tcp.vo` (new) | `TcpError`, `Conn[E]`, `Listener`, `connect`, `connect_tls`, `start_tls`, `read`, `read_exact`, `read_until`, `send`, `close`, `listen`, `accept`, `close_listener`, `describe` |
| Runtime | `src/tcp.c` (new) | the table, buffers, the two primitives `__tcp` and `__tcp_io`, record and replay of their events |
| Sockets | `src/net.c` | `net_open` gains an error kind for its caller; a listening socket helper shared with `src/serve.c` |
| TLS | `src/tls.c` | `tls_open`, `tls_read`, `tls_write`, `tls_close`; `tls_exchange` rebuilt on them |
| Scopes | `src/effects.c`, `src/rt_effects.c`, `src/check_effects.c`, `src/replay.c` | `host:port` in `net` scopes; port `0` in `serve` scopes; `std.http` checks host and port |
| Wiring | `src/vo.h`, `src/check_expr.c`, `src/rt_call.c`, VM and assembly builtin tables, `src/doc.c`, `Makefile` | two builtin ids, the module in the embedded library |
| Docs | `docs/reference.md`, `site/docs/library/tcp.md` (new), `tools/site.py`, roadmap, README, changelog, review table | R9 |
| Tests | `tests/tcp.py` (new, in `make test`), `tests/tcp_server.py` (new, the local peers), `tests/cases/tcp/`, `tests/cases/effects/` | R10 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc) and vo (the library module).

**Primary Dependencies**: none new. Mbed TLS is already vendored and used the same way.

**Storage**: none.

**Testing**: `tests/tcp.py` drives vo programs against local Python peers (plain, TLS with the test
authority of `tests/https_server.py`, misbehaving); golden cases for the checker's side (scopes with
ports); `make quick AREA=tcp` between edits, `make test` once per change; the gate only when asked.
Because `src/tls.c` and `src/net.c` change under `std.http`, the existing HTTP and HTTPS tests are the
regression check, and because of spec 036's lesson the two collector stress cases are run once.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: a 100-byte round trip on loopback under 1 ms; 16 MiB in under 1 s (SC-005):
reads take what the socket has in 64 KiB pieces, and a delimiter search never rescans.

**Constraints**: no static buffer over 64 KiB (the table is 256 entries of about 80 bytes; read
buffers are mapped per connection and unmapped at close); no libc; reference ≤ 15,000 tokens (14,843
now: about +60 allowed, R9); a connection's memory is outside the collected heap.

**Scale/Scope**: about 550 lines in `src/tcp.c`, 150 changed in `src/tls.c`, 60 in the scope code, 250
of vo in `std/tcp.vo`, 600 of Python tests.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One module, one way to do each thing; every wait has a required time limit, so an agent cannot write a program that hangs for ever. | Pass |
| II. Machine-readable feedback | Failures are a variant, not message text. Scope errors keep their codes (E0501, E0503, E0506, R0010). | Pass |
| III. Determinism | Everything received is recorded; replay opens nothing and never waits. | Pass |
| IV. Test-first | `tests/tcp.py` and the scope cases are written before the code of each story. | Pass |
| V. Single source of truth, minimal surface | No new effect. No second socket or TLS code: the HTTP client's is shared. One scope comparison for check, record and run. No UDP, pools or options. | Pass |
| Vendored code | None added. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/039-std-tcp/
├── plan.md
├── research.md          # R1–R10
├── data-model.md        # connection, listener, scope, events
├── quickstart.md
├── contracts/
│   └── tcp.md           # the module's items, errors, limits, scope syntax, recording
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
std/tcp.vo (new), src/tcp.c (new), src/net.c, src/tls.c, src/serve.c
src/effects.c, src/rt_effects.c, src/check_effects.c, src/replay.c
src/vo.h, src/check_expr.c, src/rt_call.c, src/doc.c, Makefile (+ the builtin tables of the VM)
docs/reference.md, site/docs/library/tcp.md (new), tools/site.py
tests/tcp.py (new), tests/tcp_server.py (new), tests/cases/tcp/, tests/cases/effects/
```

**Structure Decision**: one C file for the table and primitives, one vo file for the typed surface.
Existing files change only where their code becomes shared or where scopes are compared.

## Order of work

1. Scopes: `host:port` and port `0`, with golden cases; `std.http` obeys them. (User Story 2's checker
   half, independent of sockets.)
2. `tests/tcp.py` with the plain client; the table, connect, the three reads, write, close; typed
   errors; run-time scope check. (User Story 1 and the rest of 2.)
3. Record and replay of the client. (User Story 5, first half.)
4. Split `src/tls.c`; `connect_tls` and `start_tls`. (User Story 3.) HTTPS tests still pass.
5. Listen and accept, tasks, their recording. (User Story 4, rest of 5.)
6. Docs and the example; timing; stress cases; `make test`; three architectures; results.

## Complexity Tracking

No constitution violations.

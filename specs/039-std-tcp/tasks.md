---

description: "Task list for vo-lang spec 039: TCP Connections (std.tcp)"
---

# Tasks: TCP Connections (`std.tcp`)

**Input**: Design documents from `specs/039-std-tcp/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R10), data-model.md, contracts/tcp.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's cases land in `tests/tcp.py` or `tests/cases/`
first and fail before its code.

**Organization**:
- Setup: wiring of the module and the two primitives, the test peers, the results file.
- Foundational: the connection table, shared socket code, the error words and their variant.
- US1 (P1) the plain client; US2 (P1) scopes with ports; US3 (P2) TLS; US4 (P2) listen and accept;
  US5 (P2) record and replay; US6 (P3) documentation.
- Polish: timing, stress cases, three architectures, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Everything a program sees** (items, signatures, error cases, limits, scope syntax):
  contracts/tcp.md. Table entry and event fields: data-model.md.
- **The test driver**: `tests/tcp.py` starts peers from `tests/tcp_server.py` on free loopback ports,
  writes small vo programs into a temporary directory with the port in their `net`/`serve` scope, runs
  them with `$VO_BIN` (default `bin/vo`) and compares stdout and exit status. Output: `FAIL <case>:
  ...` per failure and a one-line summary; exit 1 on a failure; `--group NAME`, `--time`.
- **No static array over 64 KiB** in new code (research R1); read buffers are mapped.
- **Unchanged programs**: no existing golden case may change, except lists that print every module or
  builtin, which gain `tcp`.
- **Reference budget**: `python3 tests/ref_examples.py` after the reference change; at most +60 tokens
  (from about 14,843).
- **Between edits**: `make quick AREA=tcp` (T001); `make test` once, through `tools/progress.py run
  test`, reading `build/progress/test.log`; the gate only when asked. Never two suites at once.

## Phase 1: Setup

- [X] T001 Add area `tcp` to `tools/quick.sh` (runs `python3 tests/tcp.py` and `tests/run.sh tcp effects`), add `tcp.py` to the test list in `Makefile`, add `std/tcp.vo` to `STD_FILES` in `Makefile` and `"tcp"` to `doc_modules` in `src/doc.c`
- [X] T002 Add builtin ids `B_TCP` and `B_TCP_IO` in `src/vo.h` (after `B_ENTROPY`; move `B_LAST`), their names `__tcp`, `__tcp_io` and signatures `iiis>n`, `iiyii>Y` in `src/check_expr.c`, the `_Static_assert` in `src/rt_call.c`, and the builtin dispatch of the interpreter, the VM and each platform's assembly table, calling `rt_tcp` and `rt_tcp_io` declared in `src/vo.h`; create `src/tcp.c` with both returning `err("other: not implemented")`
- [X] T003 [P] Create `tests/tcp_server.py` with the peers of research R10 (line echo, Redis subset with `PING`/`SET`/`GET`, silent, closes mid-message, resets, STARTTLS greeter, TLS valid / wrong name / expired / unknown authority using the certificates made by `tests/https_server.py`); each prints its port on stdout when ready
- [X] T004 [P] Create `tests/tcp.py` with the driver of Conventions and an empty group list, and `specs/039-std-tcp/results.md` with headings for SC-001 to SC-007

## Phase 2: Foundational

- [X] T005 In `src/net.c`, give `net_open` an out-parameter for the failure kind (not found, refused, unreachable, timed out, other) without changing its messages; update its callers in `src/net.c` and `src/tls.c`; move the bind-and-listen code of `src/serve.c` into a helper `net_listen(port, all, reuse)` used by `src/serve.c`
- [X] T006 In `src/tcp.c`, the table of data-model.md (256 entries; id = slot + 256 × generation; kinds free, connection, listener; effect; accepted; host, port; tls; eof; mapped buffer with pos, len, cap, scanned), with `entry_of(id)` returning 0 for a free slot or another generation, `entry_new` returning "too_many" when full, `entry_free` unmapping the buffer, and the run-time effect check of research R1 on every use
- [X] T007 Create `std/tcp.vo` with `TcpError`, `Conn[E]`, `Listener` exactly as in data-model.md, the private function that turns `KIND: reason` into a `TcpError` (research R5), `describe`, and doc comments with examples on every exported item

## Phase 3: User Story 1 - Talk to a server over a connection (P1) 🎯 MVP

**Goal**: connect, write, the three reads, close, with time limits and typed errors.

**Independent Test**: `python3 tests/tcp.py --group client` and `--group limits`; `tests/run.sh tcp`.

- [X] T008 [US1] In `tests/tcp.py`, group `client`: several messages on one connection with the echo peer; 1,000 messages including one with all 256 byte values (SC-001); `read` returning what is there, `read_exact`, `read_until` with a multi-byte delimiter and bytes kept for the next read; refused, timed out (silent peer, connection usable after), not found; peer closes (buffered bytes, then closed; write gives closed); peer resets mid-write (closed, exit status 0)
- [X] T009 [P] [US1] In `tests/tcp.py`, group `limits`: `read_until` with no delimiter in 16 MiB gives `too_large` and the bytes are still readable; opening a 257th connection gives `too_many` and works again after a close
- [X] T010 [P] [US1] Golden cases in `tests/cases/tcp/` that need no peer: every `invalid` of contracts/tcp.md (port, time limit 0 and 600001, size 0 and over 16 MiB, empty host, empty delimiter), use after `close` gives `closed`, `close` twice, `describe` of each case
- [X] T011 [US1] In `src/tcp.c`, connect (argument checks, `net_open` under `net_cap_ms`, `TCP_NODELAY`, kinds mapped to the error words) and close; in `std/tcp.vo`, `connect` and `close`
- [X] T012 [US1] In `src/tcp.c`, receive into the entry's buffer (64 KiB pieces, growth to 16 MiB + 64 KiB, eof) and the three reads of research R7 (one time limit per call; timeout and `too_large` leave the buffer; delimiter search resumes at `scanned`); in `std/tcp.vo`, `read`, `read_exact`, `read_until`
- [X] T013 [US1] In `src/tcp.c`, write (all bytes, `MSG_NOSIGNAL`, waits under the call's limit, a reset or closed peer is `closed`); in `std/tcp.vo`, `write`

## Phase 4: User Story 2 - Only the hosts the program declares (P1)

**Goal**: `net("host:port")`, `serve` port `0`, enforced at check time, in the record and at run time.

**Independent Test**: `tests/run.sh effects`; `python3 tests/tcp.py --group scopes`.

- [X] T014 [P] [US2] Golden cases in `tests/cases/effects/`: `net("h:5432")` accepted; E0506 for `net("h:0")`, `net("h:65536")`, `net("h:05")`, `net("h:x")`, `net(":80")`; E0503 when a callee declares `net("h")` and the caller `net("h:1")`, none the other way; E0504 when `vo.effects` has `net("h:1")` and the program `net("h")`, W0502 the other way; `serve("0")` and `serve("0.0.0.0:0")` accepted; E0501 with its fix for a function calling `connect` without `net`; a function reading a `Conn[E]` without `uses E`
- [X] T015 [P] [US2] In `tests/tcp.py`, group `scopes`: with `net("127.0.0.1:P")` a connect to P works, to another port and to another host stops with R0010 naming `host:port` and the peer sees no connection; `std.http.get` to another port of a ported scope stops with R0010; a bare-host scope allows any port; a forged `new Conn { id: ... }` used in a function without `net` stops with R0010
- [X] T016 [US2] In `src/effects.c`, accept `HOST:PORT` (1 to 65535, no leading zero) as a `net` scope and port `0` in `serve` scopes (`serve_address_port` with a flag), and make `es_pair_covered` treat a bare host as covering that host with any port; E0506 message for a bad `net` scope in `src/check_effects.c`
- [X] T017 [US2] In `src/rt_effects.c`, `rt_check_net(host, port)` (the grant has the host, or `host:port`; R0010 names `host:port`); use it in `io_exchange` of `src/replay.c` in place of `rt_check_name(EF_NET, host)` and in connect in `src/tcp.c`; keep `std.http.serve` refusing port `0` in `src/serve.c`; set the effect of `__tcp` by its `op` in `src/check_effects.c` as `fx_builtin_sql_open` does

## Phase 5: User Story 3 - Encrypted connections (P2)

**Goal**: TLS from the first byte and as an upgrade, verified as `https://`.

**Independent Test**: `python3 tests/tcp.py --group tls`; the HTTPS tests of `make test` unchanged.

- [X] T018 [US3] In `tests/tcp.py`, group `tls` (with `--ca-file` of the test authority): `connect_tls` exchange; plain greeting then `start_tls` then exchange; wrong name, expired and unknown authority each give `tls(reason)` with `std.http`'s wording, for both ways; `start_tls` with unread bytes, on a TLS connection and on an accepted one gives `invalid`; a handshake against the silent peer gives `timed_out`
- [X] T019 [US3] In `src/tls.c`, split `tls_exchange` into `tls_open(fd, host, port, alpn, &err)`, `tls_read`, `tls_write`, `tls_close` (research R3), rebuild `tls_exchange` on them with ALPN `http/1.1`, and grow `TLS_ARENA` to 8 MiB; an allocation failure in `tls_open` is reported to the caller as "too many"
- [X] T020 [US3] In `src/tcp.c`, connect with TLS and `start_tls` (the three refusals), and route receive, write and close through the session when the entry has one; in `std/tcp.vo`, `connect_tls` and `start_tls`

## Phase 6: User Story 4 - Accept connections (P2)

**Goal**: listen, accept, port `0`, both ends in one program through tasks.

**Independent Test**: `python3 tests/tcp.py --group server` and `--group tasks`.

- [X] T021 [US4] In `tests/tcp.py`, group `server`: a vo echo server on `serve("0")` printing its port, two outside clients in turn; `Conn.peer` is `127.0.0.1:PORT`; a port in use gives `in_use`; accept with no client gives `timed_out`; accept after `close_listener` gives `closed`; listening outside the declared address stops with R0010. Group `tasks`: a `vo test` that listens on port `0` and runs `concurrent_map` over two roles, one accepting and echoing, one connecting
- [X] T022 [US4] In `src/tcp.c`, listen (`serve` grant check on the address, `net_listen` with `SO_REUSEADDR`, the bound port from `getsockname`), accept (wait under the call's limit, non-blocking accepted socket, peer text, `accepted` set, effect `serve`), close of a listener and the port op; in `std/tcp.vo`, `listen`, `accept`, `close_listener`

## Phase 7: User Story 5 - Recorded and replayed (P2)

**Goal**: every program of the earlier groups replays with no network.

**Independent Test**: `python3 tests/tcp.py --group replay`.

- [X] T023 [US5] In `tests/tcp.py`, group `replay`: record the client, TLS, server and tasks programs, stop every peer, replay each: same stdout and exit status, and no socket opened (the driver runs the replay with the peers' ports closed); a recorded timeout replays at once (under 100 ms); a program changed to connect to another port stops with R0014; a recording of the TLS run contains the plain reply text and no key material; a recorded failed write replays as the same error
- [X] T024 [US5] In `src/vo.h` and `src/replay.c`, the event kinds of data-model.md (`tcp_open`, `tcp_tls`, `tcp_recv`, `tcp_send`, `tcp_listen`, `tcp_accept`) in the writer, the reader and `rp_check_event`
- [X] T025 [US5] In `src/tcp.c`, record each of them where it happens and, in replay, take them instead (research R6): open and accept make entries without a socket, receive fills the buffer from `tcp_recv`, a write returns a `tcp_send` error only when that is the next event, mismatches go through `rp_mismatch`

## Phase 8: User Story 6 - Documented where the rest is (P3)

**Goal**: `vo doc std.tcp`, the reference, the site.

**Independent Test**: `vo doc std.tcp`; `python3 tests/ref_examples.py`; `make site-check`.

- [X] T026 [P] [US6] In `docs/reference.md`: `std.tcp` in the `net` and `serve` rows of the effect table, "hosts (optionally `host:port`)" in the scope sentence, one `std.tcp` row in the library table; run `python3 tests/ref_examples.py` and keep it under 15,000 tokens
- [X] T027 [P] [US6] Create `site/docs/library/tcp.md` (items, errors, scopes, recording, tasks, and the Redis `PING`/`SET`/`GET` client of SC-006 in at most 60 lines) and add it to `tools/site.py`; add group `docs` to `tests/tcp.py` running that example against the Redis peer
- [X] T028 [P] [US6] Update `docs/roadmap.md` (TCP row done, UDP and IPv6 remaining), `README.md`, `CHANGELOG.md` and the "Fixed since the review" table in `docs/review-2026-11/README.md`

## Phase 9: Polish

- [X] T029 `python3 tests/tcp.py --time` for SC-005 (100-byte round trip under 1 ms; 16 MiB under 1 s) and record the figures in `specs/039-std-tcp/results.md`
- [X] T030 Run `tests/run.sh --stress` on `run/oom_small` and `map_keys/many`, and check with `nm` that no new static object in `build/` exceeds 64 KiB; record in `specs/039-std-tcp/results.md`
- [X] T031 Run `python3 tests/tcp.py` with `VO_BIN=bin/vo-aarch64` and `VO_BIN=bin/vo-riscv64`; record in `specs/039-std-tcp/results.md`
- [X] T032 Run `make test` once through `tools/progress.py run test`, read `build/progress/test.log`, fix what it finds, and complete `specs/039-std-tcp/results.md` (SC-001 to SC-007, deviations, what was not verified); set the spec's status

## Dependencies

- Setup → Foundational → US1. US2's T014 and T016 need only Setup; T015 and T017 need US1's connect.
- US3 needs US1. US4 needs US1 and T016 (port `0`). US5 needs US1, and its TLS, server and tasks
  cases need US3 and US4. US6 needs the items to exist (US1, US3, US4). Polish last.

## Parallel opportunities

- T003 and T004; T009 and T010; T014 and T015; T026, T027 and T028. Everything in `src/tcp.c` is
  sequential (one file).

## Implementation strategy

MVP is Setup, Foundational, US1 and US2 together: a plain client that cannot leave its declared hosts.
Then TLS (most real services), then the server side, then replay over all of them, then documentation.

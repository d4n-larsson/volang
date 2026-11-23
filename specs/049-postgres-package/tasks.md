---

description: "Task list for vo-lang spec 049: PostgreSQL Package (postgres)"
---

# Tasks: PostgreSQL Package (`postgres`)

**Input**: Design documents from `specs/049-postgres-package/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R13), data-model.md, contracts/client.md, quickstart.md; spec 050 (implemented)

**Tests**: REQUIRED (constitution IV). Each story's `test` items and `tests/postgres.py` cases are
written first and fail before its code.

**Organization**:
- Setup: the package directory, the test driver and peers, wiring.
- Foundational: messages, the `?` scanner, values, failure names (no connection needed).
- US1 (P1) store and query; US3 (P1) connect safely (before US2: the move needs a working connect);
  US2 (P1) move a program between databases; US4 (P2) transactions; US5 (P2) failures and limits;
  US6 (P2) replay and tests.
- Polish: guide, speed, versions, three architectures, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Everything a program sees**: contracts/client.md. Records, states and message order: data-model.md.
  Decisions: research.md (R4 connecting, R5 statements, R6 `?`, R7 values, R8 failure names, R9
  transactions).
- **No toolchain change**: nothing under `src/`, `std/`, `third_party/`, and not `docs/reference.md`.
- **Package files** are vo in `packages/postgres/`; every exported item has a comment with an example
  that needs no server; `vo fmt --check` clean; helper modules export only what `client.vo` uses.
- **Unit tests** are `test` items in the package files, run with `bin/vo test FILE`.
- **The driver** `tests/postgres.py` writes small programs into a temporary directory with a
  `vo.packages` naming `packages/postgres`, runs them with `bin/vo` (`$VO_BIN`) and compares output.
  Groups: `unit`, `connect`, `auth`, `query`, `types`, `tx`, `failures`, `fuzz`, `replay`, `parity`,
  `move`, `docs`. Groups that need a real server run with `--server` and read
  `VO_PG_TEST=host:port:database:user:password` (default `127.0.0.1:54329:vo:vo:secret`, the
  container of quickstart.md); without `--server` they are skipped with one line.
- **The scripted peer** `tests/postgres_server.py` listens on a free loopback port and plays a named
  script (TLS with `tests/tls/certs`, SCRAM with `hashlib`, canned and hostile answers).
- **Between edits**: `make quick AREA=postgres`; `make test` once per change, through
  `tools/progress.py run test`; the gate only when asked. Never two suites at once.

## Phase 1: Setup

- [X] T001 Create `packages/postgres/` with empty modules `client.vo`, `wire.vo`, `scram.vo`, `statement.vo`, `values.vo` (a header comment each), `tests/postgres.py` with the driver of Conventions and an empty group table, and `specs/049-postgres-package/results.md` with headings for SC-001 to SC-009
- [X] T002 Add area `postgres` to `tools/quick.sh` (files `packages/postgres/*`, `tests/postgres*.py`, `tests/postgres/*`; script `postgres.py`), and `postgres.py` to the script list of the `test` target in `Makefile`

## Phase 2: Foundational

- [X] T003 [P] In `packages/postgres/statement.vo`: `rewrite(text: str) -> result[Rewritten]` (`Rewritten { text: str; count: int; }`) per research R6: `?` to `$1`…, `??` to `?`, skipping `'...'` with `''`, `E'...'` with backslashes, `"..."`, `--` comments, nested `/* */`, `$tag$...$tag$`; `error: write parameters as ?, not $1` for `$` followed by a digit; `error: one statement per sql` for a `;` followed by anything but spaces and comments; with `test` items for each form first
- [X] T004 [P] In `packages/postgres/wire.vo`: big-endian `int16`/`int32` writers and readers over `bytes`, a message builder (type byte, length, body built as `list[int]`), the builders `tls_request`, `startup(user, database)` with the session settings of research R4, `sasl_initial`, `sasl_response`, `parse`, `bind` (text parameters, `null` as length -1), `describe_portal`, `execute`, `sync`, `terminate`, and readers for `ErrorResponse` fields (`C`, `M`, `P`), `RowDescription` (names, type numbers), `DataRow` (values or null), `CommandComplete` (tag), `Authentication` (kind and body), `ReadyForQuery` (status); `test` items with hand-built messages first
- [X] T005 [P] In `packages/postgres/values.vo`: the table of research R7 from a type number and text to a `Value` (integers, `bool` to 0/1, floats with `NaN`/infinities as text, `bytea` hex to `blob`, everything else `text`), a `Value` to parameter text (or null), and `time_of(text: str) -> result[int]` for `YYYY-MM-DD`, `YYYY-MM-DD HH:MM:SS[.ffffff]` and the same with `+00`; `test` items first. `Value` itself is declared in `client.vo` (T007), so this module takes and returns the parts (`kind`, text) that `client.vo` wraps, or `client.vo` holds the mapping if a cycle between the modules cannot be avoided
- [X] T006 [P] In `packages/postgres/scram.vo`: `pbkdf2(password, salt, rounds)` from `hmac_sha256`, `client_first(nonce)`, `client_final(password, client_first_bare, server_first) -> result[Final]` (`Final { message: bytes; server_signature: bytes; }`; checks the server nonce starts with ours, the salt is base64, rounds 1 to 600000) and `verify(server_final, expected)` with `same_bytes`; `test` items with the RFC 7677 example (user `user`, password `pencil`, the nonces, salt and proofs of its section 3) first
- [X] T007 In `packages/postgres/client.vo`: the types of data-model.md (`Options`, `Database[E]`, `Statement`, `Value`, `Row`, `Changes`) with the same case and field names as `std.sqlite`, `sql`, `parse_sql`, and the getters `get`, `get_int`, `get_float`, `get_str`, `get_bytes`, `is_null` with the `err` texts of `std/sqlite.vo`, and `get_time`; `test` items comparing each getter's `err` text with the one `std.sqlite` gives for the same row
- [X] T008 In `packages/postgres/client.vo`: the failure names of research R8 as one function from an `ErrorResponse`'s fields to `NAME: CODE message[ (at position N)]`, and one from a `TcpError` to `connection:`/`timeout:`/`tls:`/`protocol:` text; `test` items for each row of the table

## Phase 3: User Story 1 - Store and query data on a server (P1) 🎯 MVP

**Goal**: connect (no TLS, password or none), `exec`, `query`, the getters, `close`.

**Independent Test**: `python3 tests/postgres.py --server --group query` and `--group types`.

- [X] T009 [US1] In `tests/postgres.py`, groups `query` and `types` (need `--server`): create a table, insert three rows with `?`, read them back by name with each getter; `changed` of a five-row insert; `returning id` with `query`; a wrong-kind getter; a unique violation (`constraint: 23505 ...`) after which the connection still works; every row of the type table of contracts/client.md in and out, including `NULL`, `numeric` as text, `bool` as 0/1, `bytea`, `NaN` as text, a `timestamptz` through `get_time`
- [X] T010 [US1] In `packages/postgres/client.vo`: `connect(o: Options) -> result[Database[uses net]] uses net, entropy` per research R4 (TCP, optional TLS request and `start_tls`, startup, authentication `Ok` or SCRAM through `scram.vo`, read to `ReadyForQuery`), and `close`
- [X] T011 [US1] In `packages/postgres/client.vo`: the statement round trip of research R5 (one send, the read loop to `ReadyForQuery`, large messages read in parts), `exec` (count from the command tag for `INSERT`, `UPDATE`, `DELETE`, `MERGE`) and `query` (rows with names); the parameter count checked before sending; T009 passes

## Phase 4: User Story 3 - Connect safely (P1)

**Goal**: TLS by default with verification, SCRAM with the server's proof checked, scopes and effects, parameters that cannot become SQL.

**Independent Test**: `python3 tests/postgres.py --group connect` and `--group auth` (scripted peer), and `--server --group auth`.

- [X] T012 [P] [US3] Create `tests/postgres_server.py`: a scripted peer with scripts `plain-ok` (no TLS asked: answers `N`), `tls-ok` (answers `S`, TLS with the test certificate for `localhost`), `tls-bad-name`, `scram-ok`, `scram-wrong-password`, `scram-bad-signature` (a forged server signature), `asks-md5`, `asks-cleartext`, and a `--capture FILE` option writing every byte received
- [X] T013 [US3] In `tests/postgres.py`, groups `connect` and `auth`: default options against `tls-ok` with `--ca-file` succeed; against `plain-ok` give `tls: the server does not offer TLS` and nothing more is sent (capture); `tls-bad-name` gives `tls:`; `plain_text: true` works against `plain-ok`; `scram-ok` succeeds; `scram-wrong-password` gives `auth: 28P01`; `scram-bad-signature` gives `auth:` and closes; `asks-md5` and `asks-cleartext` name the method; the password is in no capture and no error text; a host outside `main`'s `net` scope is R0010; a function using a `Database[uses net]` without `net` is E0501; with `--server`: the real server's SCRAM, and 200 hostile strings as parameters come back byte for byte (SC-004)
- [X] T014 [US3] Fix what T013 shows in `packages/postgres/client.vo` and `scram.vo` (the TLS step order, the proof check, error texts)

## Phase 5: User Story 2 - Move a program between databases (P1)

**Goal**: the same program text on both databases apart from imports, opening, effects and SQL.

**Independent Test**: `python3 tests/postgres.py --group parity` and `--server --group move`.

- [X] T015 [P] [US2] In `tests/postgres.py`, group `parity` (SC-003): compare `bin/vo doc std.sqlite` with `bin/vo doc postgres.client` for `Statement`, `Value`, `Row`, `Changes`, `sql`, `parse_sql`, `exec`, `query`, `transaction`, `get`, `get_int`, `get_float`, `get_str`, `get_bytes`, `is_null`: same signature lines apart from the module; `close` differs only by `uses E`
- [X] T016 [US2] Create `tests/postgres/notes-api/` from `examples/notes-api/` (FR-023): imports to `postgres.client`, the two functions that open the database (the service's connects with options from the environment or constants; the tests' connects to the test server and drops the table), `uses` clauses and effect arguments, and the SQL that differs; a `vo.packages` naming `../../../packages/postgres`; all statements in one file
- [X] T017 [US2] In `tests/postgres.py`, group `move` (SC-002, needs `--server`): fail on any line that differs between `examples/notes-api/*.vo` and `tests/postgres/notes-api/*.vo` and is not an import, inside a function that opens the database, a `uses` clause or effect argument, or SQL text; run `vo test` on the PostgreSQL form

## Phase 6: User Story 4 - Transactions (P2)

**Independent Test**: `python3 tests/postgres.py --server --group tx`.

- [X] T018 [US4] In `tests/postgres.py`, group `tx` (needs `--server`): commit seen by a second connection afterwards and not before; `err` rolls back and is returned; a runtime error midway leaves no change; a nested transaction is an `err`; a failed statement followed by `ok` ends rolled back with `error: a statement in the transaction failed`
- [X] T019 [US4] In `packages/postgres/client.vo`: `transaction` per research R9 with the `in_transaction` and `failed` flags

## Phase 7: User Story 5 - Failures are values, and nothing waits forever (P2)

**Independent Test**: `python3 tests/postgres.py --group failures` and `--group fuzz`.

- [X] T020 [P] [US5] In `tests/postgres_server.py`: scripts `refuses` (closed port), `stalls-connect`, `stalls-query`, `closes-in-query`, `closes-in-commit`, `garbage` (random bytes, bad lengths, unknown message types, oversized length, seeded)
- [X] T021 [US5] In `tests/postgres.py`, groups `failures` and `fuzz`: each script gives the `err` name of spec User Story 5 within its time limit; after `timeout`, `connection` or `protocol` later calls give `connection: closed`; a server error shows name, SQLSTATE, message and position; 300 `garbage` seeds (10,000 with `--full`) each end in an `err` within the limit and never a runtime error or hang (SC-006)
- [X] T022 [US5] Fix what T021 shows in `packages/postgres/wire.vo` and `client.vo` (bounds on every length read, the `closed` flag, the commit message)

## Phase 8: User Story 6 - Replay and tests (P2)

**Independent Test**: `python3 tests/postgres.py --group replay`.

- [X] T023 [US6] In `tests/postgres.py`, group `replay`: record a run against `scram-ok` with a query, stop the peer, replay: identical output and status; the recording does not contain the password; a `test` with `uses net("127.0.0.1:PORT"), entropy` passes against the peer and fails with `connection:` in its message when the peer is stopped

## Phase 9: Polish

- [X] T024 [P] Write `site/docs/guide/postgres.md` (adding the package, `Options`, the type table, failure names, the table of differences from SQLite of FR-012 with both spellings, `?` and `??`, testing against a server, what is lost against `std.sqlite`), link it from `site/docs/guide/modules-and-packages.md` and the site navigation; examples that need a server are `text` blocks
- [X] T025 [P] In `tests/postgres.py`, group `docs`: every exported item of `postgres.client` has a comment with an example, and each example evaluates as written
- [X] T026 Speed (SC-007) with `python3 tests/postgres.py --server --time`: connect and sign in, 100,000 rows of five columns read, 10,000 rows inserted in one transaction; record in results.md
- [X] T027 [P] Run `python3 tests/postgres.py` (scripted groups and `unit`) with `VO_BIN` set to `build-aarch64/vo` and `build-riscv64/vo` (SC-001)
- [ ] T028 Versions (SC-008): run `python3 tests/postgres.py --server` against PostgreSQL 15, 16, 17 and 18 (images must be present); record in results.md
- [X] T029 SC-009: `git diff --stat BASE -- src std third_party docs/reference.md` is empty for this feature's commits; record it
- [X] T030 Run `make test` once; fill results.md; update `docs/roadmap.md`, `CHANGELOG.md`, the Postgres row of `docs/review-2026-11/ecosystem.md` and the spec's status line

## Dependencies

- T001, T002 → Foundational (T003–T006 in parallel, then T007, T008) → US1 → US3 → US2; US4, US5, US6
  need US1 and the peer of T012.
- Polish needs every story.

## Parallel examples

- T003, T004, T005, T006 (four files).
- T012 and T015 while `client.vo` is being written.

## Implementation strategy

1. Foundational modules with their unit tests (no server).
2. US1 against the real server on `127.0.0.1:54329`: the MVP.
3. US3 with the scripted peer, then US2 (the move), then US4 to US6.
4. Polish.

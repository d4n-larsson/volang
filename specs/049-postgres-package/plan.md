# Implementation Plan: PostgreSQL Package (`postgres`)

**Branch**: `049-postgres-package` (work on `main`, as for earlier specs) | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/049-postgres-package/spec.md`

## Summary

A vo package in `packages/postgres/`, five files of vo and no C: the public module `client.vo` with
the names of `std.sqlite`, over `wire.vo` (the protocol's messages), `scram.vo` (signing in),
`statement.vo` (the `?` scanner) and `values.vo` (types and getters). One statement is one send and
one read loop (R5). A `Database` is a shared record holding the `std.tcp` connection and three flags
(R3). Tests are `test` items inside the package for everything that needs no server, a scripted Python
peer for connecting, failures and fuzzing, and a real PostgreSQL in a container for the gate (R10).

**Prerequisite met (R1).** Planning found that no function outside `std` could connect to a host it
is given under a scoped grant: `main` would have had to declare plain `net`. Spec 050 (implemented
2026-11-23) fixed that in the checker and the run-time check: `connect(options)` declares `net,
entropy` and is callable under `net("db.internal:5432")`, with the host in a record field.

| Part | File | What |
|---|---|---|
| Public module | `packages/postgres/client.vo` (new) | `Database`, `Options`, `Statement`, `Value`, `Row`, `Changes`, `connect`, `close`, `sql`, `parse_sql`, `exec`, `query`, `transaction`, the getters, `get_time` |
| Messages | `packages/postgres/wire.vo` (new) | building and reading protocol messages, the read loop, error fields |
| Signing in | `packages/postgres/scram.vo` (new) | SCRAM-SHA-256 client, PBKDF2 from `hmac_sha256` |
| Statements | `packages/postgres/statement.vo` (new) | `?` to `$n`, one-statement and parameter-count checks |
| Values | `packages/postgres/values.vo` (new) | column type to `Value`, `Value` to parameter text, timestamps |
| Tests | `tests/postgres.py`, `tests/postgres_server.py` (new), `tests/postgres/notes-api/` (new) | R10 |
| Wiring | `Makefile` (`test`, `gate-step`), `tools/quick.sh` (area `postgres`) | R10 |
| Docs | `site/docs/guide/postgres.md` (new), `site/docs/guide/modules-and-packages.md`, `docs/roadmap.md`, `CHANGELOG.md`, `docs/review-2026-11/ecosystem.md` | R12 |

## Technical Context

**Language/Version**: vo (the package); Python 3 for test drivers only.

**Primary Dependencies**: `std.tcp`, `std.crypto` (`hmac_sha256`, `sha256`, `same_bytes`,
`random_bytes`), `std.bytes`, `std.text`, `std.time`. No new vendored code. The prerequisite spec (R1).

**Storage**: a PostgreSQL server, 15 or later, reached over TCP.

**Testing**: `vo test` on each package file (no server, three architectures); `tests/postgres.py`
against `tests/postgres_server.py` (in `make test`); `tests/postgres.py --server` against a real
PostgreSQL from `VO_PG_TEST` or a container (in the gate). `make quick AREA=postgres` between edits.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64 (the package is vo; the toolchain carries it).

**Project Type**: a library package beside a compiler toolchain.

**Performance Goals**: SC-007. Probed on the native build: PBKDF2 with 4,096 rounds in vo takes
15 ms; splitting 100,000 rows of five columns out of 6.7 MB takes 35 ms. The targets have room.

**Constraints**: no change under `src/`, `std/`, `third_party/`; `docs/reference.md` untouched;
messages are built as `list[int]` and turned to `bytes` once (`bytes` are immutable, `+` copies).

**Scale/Scope**: about 900 lines of vo in five files, 700 of Python tests, one site page.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | The names, parameters and getter errors an agent knows from `std.sqlite`; `?` only; one way to open (`connect`), one to run (`exec`, `query`). | Pass |
| II. Machine-readable feedback | No new diagnostic or code: the package is vo code and its failures are `err` values with a stable leading name and the SQLSTATE. | Pass |
| III. Determinism | Session fixed to UTF-8, UTC and ISO dates; everything received and every random byte is in the recording through `std.tcp` and `std.crypto`; replay opens nothing. No input from a server stops the program (R5, R8). | Pass |
| IV. Test-first | The checker and CLI do not change, so there is no new golden case in `tests/cases/`. The package's behaviour is observable only through programs that use it: its `test` items and the cases of `tests/postgres.py` are written before the code of each story and fail first. | Pass |
| V. Single source of truth, minimal surface | Reference unchanged. No pool, no binary formats, no named statements, no cancel, no second placeholder form. Two known ceilings carry `ponytail:` comments (R4: non-ASCII passwords are not SASLprep-normalised; R5: all rows in memory). | Pass |
| Implementation constraints | No C, no assembly, no vendored code; Python only in tests. | Pass |
| Workflow | A prerequisite spec is named instead of patching the toolchain here (the spec's "must keep"). | Pass, blocked on R1 |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/049-postgres-package/
├── plan.md
├── research.md          # R1–R13
├── data-model.md        # records, connection states, message flow
├── quickstart.md
├── contracts/
│   └── client.md        # the public module, failure names, type table
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
packages/postgres/
├── client.vo            # the only module programs import
├── wire.vo
├── scram.vo
├── statement.vo
└── values.vo

tests/
├── postgres.py          # groups: unit, connect, auth, query, types, tx, failures, fuzz, replay, parity, move, docs; --server
├── postgres_server.py   # scripted peer: TLS with the test authority, SCRAM, canned and hostile answers
└── postgres/
    └── notes-api/       # the notes API on PostgreSQL (FR-023), with vo.packages naming ../../../packages/postgres

site/docs/guide/postgres.md
```

**Structure Decision**: the package is a new top-level directory because `vo package` pins a
directory and hashes every `.vo` file under it; tests and the example therefore live outside it. The
PostgreSQL form of the notes API is under `tests/`, not `examples/`, because `make test` runs
`vo test` in every example directory and these tests need a server.

## Complexity Tracking

No violations.

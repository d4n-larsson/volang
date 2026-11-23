# Results: PostgreSQL Package (`postgres`)

Measured 2026-11-23 on x86-64 (native) against PostgreSQL 17 in a container on 127.0.0.1:54329
(`docker run ... postgres:17`, password authentication `scram-sha-256`), and on ARM64 and RISC-V under
qemu against the same server.

## The package

`packages/postgres/`, eight modules, about 2,100 lines with their tests and comments: `client.vo` (the
only module programs import), `link.vo` (the connection, TLS request, one message at a time),
`signin.vo` (startup and signing in), `scram.vo`, `exchange.vo` (one statement and its answer),
`statement.vo` (`?` to `$n`), `values.vo` (types to kinds, `bytea`, times), `wire.vo` (messages,
failure names). The plan named five; signing in, the transport and the round trip were split out to
keep `client.vo` under the structure rules' 400 code lines and decisions in pure functions (W0701,
W0702). Every module checks with no warning, and its `test` items pass (71 in all).

## SC-001: every scenario a case, on three architectures

`python3 tests/postgres.py --server`: 13 groups, all passing on x86-64. The eight groups without a
server (`unit`, `connect`, `auth`, `failures`, `fuzz`, `replay`, `parity`, `docs`: 60 cases) pass on
`build-aarch64/vo` and `build-riscv64/vo`, with the same fuzz outcomes (262 `protocol`, 38
`connection`). The real-server groups (`query`, `types`, `hostile`, `tx`, `move`) also pass on both, against the
same server, with the same expected output as on x86-64.

## SC-002: the move

`tests/postgres/notes-api/` is `examples/notes-api/` on PostgreSQL. `--group move` compares the two
line by line and fails on a line that is not an import, a comment, a `uses` clause or effect argument,
SQL, or inside a function that opens the database (`open_notes`, and the tests' `memory` and
`fresh`); it passes. Both forms pass the example's 7 tests (the PostgreSQL form against the server).
The non-comment differences are: the imports; `id serial primary key` for `id integer primary key`;
the effect set `Storage`; `open_notes` (connect instead of open); the tests' `memory`/`fresh`; and
`uses` clauses on `call` and on the tests.

The SQLite example changed once for this: `add_note` used `last_id`, which PostgreSQL has no
equivalent of; it now runs `insert ... returning id` with `query`, which SQLite also accepts. Its
recording was made again with the same eight requests and replays.

Found while doing it: a property that opens a server connection per case keeps them all open until
`vo test` ends (std.tcp closes connections when the program ends, not when a test case does), so the
property reached PostgreSQL's limit of 100 connections. The PostgreSQL form's test helper ends the
user's earlier connections on the server. A toolchain change (closing a test case's connections at
its end) would remove the need; noted on the roadmap.

## SC-003: the same signatures as std.sqlite

`--group parity`: `vo doc` of `std.sqlite` and `postgres.client` give the same signature lines for
`Statement`, `Value`, `Row`, `Changes`, `sql`, `parse_sql`, `exec`, `query`, `transaction`, `get`,
`get_int`, `get_float`, `get_str`, `get_bytes`, `is_null`; `close` differs only by `uses E`. The
getters' `err` texts are the same.

## SC-004: the password and hostile parameters

- The scripted peer captured every byte of a SCRAM sign-in: the password is not among them. No error
  text and no recording holds it (`auth`, `replay` groups).
- `--group hostile`: 200 strings built from quotes, comments, `;`, `$$`, `$1`, `?`, `??`, backslashes,
  `drop table`, control characters and non-ASCII text, inserted as parameters and selected back by
  equality: 0 mismatches, 200 rows, no other statement ran.

## SC-005: replay

A run against the scripted SCRAM peer, recorded and replayed with the peer stopped: the same output.

## SC-006: malformed answers

`--group fuzz` (300 seeds in `make test`): every malformed, truncated or oversized answer is an `err`
(`protocol` or `connection`) within the time limit; none is a row, a runtime error or a hang. The
fuzzer found one bug, fixed: a field length below -1 was read as `NULL`. With `--full`, 10,000 seeds: 8,912 `protocol`, 1,067 `connection`, 21 `error` (a malformed error
message read as the server's failure), 0 failures; 11.6 minutes.

## SC-007: speed

`python3 tests/postgres.py --server --time`, median of 3, native x86-64, server on the same machine:

| | Measured | Target |
|---|---|---|
| connect and sign in (SCRAM, 4,096 rounds) | 15 ms | under 1 s |
| 10,000 rows inserted in one transaction (5 columns, one statement each) | 935 ms | under 5 s |
| 100,000 rows of five columns read | 281 ms | under 3 s |

## SC-008: PostgreSQL versions

Not met yet: only PostgreSQL 17 was tested (the image pulled for this work). 15, 16 and 18 need their
images: `docker run -d -e POSTGRES_PASSWORD=secret -e POSTGRES_USER=vo -e POSTGRES_DB=vo -p
127.0.0.1:54329:5432 postgres:N`, then `python3 tests/postgres.py --server`.

## SC-009: the toolchain is untouched

`git diff --stat c4b9948 -- src std third_party docs/reference.md` is empty: no change to the
toolchain, the standard library, vendored code or the reference.

## `make test`

Pass (x86-64, exit 0), with `tests/postgres.py` in it (the eight groups without a server). The
instrumented build and the gate were not run.

## Differences from the plan and tasks

- Eight modules instead of five (above).
- `:name` parameters are left to the server's syntax error (research R6); inside a string, `??` is two
  characters, as every string is left alone.
- `close` sends the goodbye message and so needs the database's effect (`uses E`), where `std.sqlite`'s
  needs none: the one signature difference.
- The site guide is `site/docs/guide/postgres.md`, linked from the packages guide and the navigation.
- Not done: SASLprep of non-ASCII passwords (a `ponytail:` comment in `scram.vo`), cancelling a running
  statement (a timeout closes the connection), and the PostgreSQL versions other than 17.

# Research: PostgreSQL Package

Probes were run with `bin/vo` (0.1.0) on 2026-11-23, in a throwaway two-module package.

## R1. A package cannot connect under a scoped grant (prerequisite)

**Finding**: a function outside `std` that passes a host on to `std.tcp.connect` must declare plain
`net`, and "a caller's scopes must cover its callees'" then makes `main` declare plain `net` too
(E0503, fix `uses net`). Only standard-library callees are exempt (`fx_call` in
`src/check_effects.c`: "a standard-library callee counts like a builtin"). Tried and refused the same
way: the package function generic over two effect parameters with `connect` and `start_tls` passed in
as function values. Accepted under `net("127.0.0.1:1")`: a function generic over `Conn[E]` that only
sends and reads.

**Decision**: a separate spec, implemented first, lets a package function pass a host to `std.tcp`
under its caller's scoped `net`. This plan keeps `connect(options)` as specified.

**Done**: spec 050, implemented 2026-11-23. Its case `tests/cases/packages/scoped_connect` is this
package's `connect(options)` in miniature. `connect` keeps the single options record.

**What that spec had to give this one**:
- `connect(o: Options) -> result[Database[uses net]] uses net, entropy`, in a package, is callable from
  a `main` or a test that declares `net("db.internal:5432")`;
- the host reaches `std.tcp.connect` and `start_tls` from a record field (`o.host`); if the mechanism
  needs the host as a parameter of its own, `connect` becomes `connect(host, port, options)` and the
  contract here changes in that one line;
- a host outside the grant is still R0010 at run time, and `vo.effects` shows the program's hosts.

**Alternatives considered**: the program opens the connection and upgrades it itself, the package
taking a `Conn[E]` (works today, but the package cannot see whether the TLS step was done, so "TLS by
default" would be a habit, not a rule); plain `net` for every program using the package (no host in
`vo.effects`, no R0010).

## R2. Package layout

**Decision**: five modules in `packages/postgres/`; programs import only `postgres.client`. Modules of
the package import each other by the package name (`import postgres.wire.int32;`, probed). Helper
modules must `export` what `client.vo` uses, so those names are reachable as `postgres.wire.NAME`; the
guide says only `postgres.client` is the API. `test` items inside package files run with `vo test
FILE` (probed), and `vo doc postgres.client` lists the module (probed).

**Alternatives**: one file (about 900 lines; the scanner and SCRAM are easier to test apart).

## R3. Connection state

**Decision**: `Database[E]` is a record holding the `Conn[E]`, the statement time limit, and three
flags: `in_transaction`, `failed` (a statement failed in the open transaction), `closed`. Records are
shared, not copied, and fields are assignable (`set db.in_transaction = true;`, probed), so every
holder of the handle sees the state. A `Database` built with `new` holds a `Conn` built by hand, which
`std.tcp` refuses.

`Database`'s fields differ from `std.sqlite`'s (`id: int`); the parity check (SC-003) compares the
functions and the fields of `Statement`, `Value`, `Row` and `Changes`.

**Alternatives**: asking the server for its transaction state before `BEGIN` (an extra round trip per
transaction).

## R4. Connecting and signing in

**Decision**, in order, each step under the connect time limit:
1. `std.tcp.connect`.
2. Unless `plain_text` is set: send the 8-byte TLS request, read one byte. `S`: `start_tls(c, host)`
   (it refuses when unread bytes wait, which closes the injection hole of bytes sent before the
   handshake). `N`: `tls: the server does not offer TLS`. Anything else: `protocol:`.
3. Startup message, protocol 3.0, with `user`, `database`, `client_encoding=UTF8`, `DateStyle=ISO`,
   `TimeZone=UTC`, `IntervalStyle=postgres`, `bytea_output=hex`, `standard_conforming_strings=on`,
   `extra_float_digits=1`, `application_name=vo`.
4. Authentication: `Ok` (the server asks for nothing); `SASL` offering `SCRAM-SHA-256` (the `-PLUS`
   form is ignored); any other request is `auth: the server asked for NAME; only SCRAM-SHA-256 is
   supported`.
5. SCRAM (RFC 5802, RFC 7677): an 18-byte nonce from `random_bytes`; salted password by PBKDF2 built
   from `hmac_sha256`; proof sent; the server's signature compared with `same_bytes`, and a wrong or
   missing one is `auth:` and closes. The iteration count must be 1 to 600,000 (a hostile server
   cannot make the program compute for minutes), the salt valid base64, the server nonce must start
   with ours.
6. Read to `ReadyForQuery`; parameter reports and the cancel key are dropped.

The password is used only as the PBKDF2 key; it is in no message and no error text.

`ponytail:` passwords are sent through PBKDF2 as their UTF-8 bytes, without SASLprep. ASCII passwords
are unaffected; a password with characters SASLprep would change does not sign in. Upgrade: implement
SASLprep (RFC 4013) when someone needs it.

**Alternatives**: `connect_tls` (PostgreSQL's direct TLS needs version 17 and an ALPN name `std.tcp`
does not send).

## R5. Running a statement

**Decision**: the extended protocol with the unnamed statement, one send and one read loop:
`Parse` (text, no parameter types) + `Bind` (parameters in text form, results in text form) +
`Describe` (portal) + `Execute` (all rows) + `Sync`. The loop reads a 5-byte header and then the body
(in parts when over 16 MiB, the `std.tcp` read limit), until `ReadyForQuery`, whose status byte sets
`failed`. It handles `ParseComplete`, `BindComplete`, `RowDescription`, `NoData`, `DataRow`,
`CommandComplete`, `EmptyQueryResponse`, `ErrorResponse`; drops `NoticeResponse`, `ParameterStatus`
and `NotificationResponse`; anything else, or a length under 4, is `protocol:` and closes.

After an `ErrorResponse` the server skips to `Sync` and answers `ReadyForQuery`, so one loop serves
success and failure and the connection stays usable (FR-019).

`exec` returns the count of the command tag for `INSERT`, `UPDATE`, `DELETE` and `MERGE`, else 0, and
drops any rows. `query` returns the rows; no row description means an empty list.

`ponytail:` all rows are held in memory (as `std.sqlite`); upgrade: a row limit on `Execute` and a
streaming read when a program needs it.

**Alternatives**: the simple query protocol (no parameters); describing the statement first to learn
types (a second round trip); named prepared statements (state to keep, no need yet).

## R6. `?` parameters

**Decision**: one scanner in `statement.vo` walks the text once and knows: `'...'` with `''`, `E'...'`
with backslashes, `"..."`, `--` to the end of the line, nested `/* */`, and `$tag$...$tag$`. Outside
those: `?` becomes `$1`, `$2`, ...; `??` becomes `?`; `$` followed by a digit is `error: write
parameters as ?, not $1`; a `;` followed by anything but spaces and comments is `error: one statement
per sql`. It returns the rewritten text and the count, compared with the number of values before
sending. `parse_sql` runs it and returns the `Statement` or the `err`; `sql` returns the `Statement`
and the same `err` comes from `exec` or `query`.

`:name` is not detected: `:` also appears in casts (`::`) and array slices (`a[1:n]`), and the server
reports it as a syntax error with its position. The spec is changed to say numbered parameters only.

## R7. Values

**Decision**: results are asked for in text form and mapped by the column's type number:

| PostgreSQL | `Value` |
|---|---|
| `int2` (21), `int4` (23), `int8` (20), `oid` (26) | `integer` (`parse_int`) |
| `bool` (16) | `integer` 1 for `t`, 0 for `f` |
| `float4` (700), `float8` (701) | `real` (`parse_float` reads the server's exponent form, probed: `1e+20`, `1.5e-07`, `1.7976931348623157e+308`) |
| `bytea` (17) | `blob` (from `\x` hex) |
| a length of -1 | `null` |
| everything else | `text` |

`NaN`, `Infinity` and `-Infinity` are not vo floats: they arrive as `text`, so `get_float` says `x is
text, not real` (added to the spec's edge cases).

Parameters are text: `integer` as `str(i)`, `real` as `str(f)`, `text` as is (a NUL byte is refused
with `error:` since the server cannot store it), `blob` as `\x` hex, `null` as length -1. The server
reads each as the type the statement gives the parameter.

`get_time` parses `YYYY-MM-DD`, `YYYY-MM-DD HH:MM:SS[.ffffff]` and the same with `+00` (the session
is UTC), and returns milliseconds through `std.time`; `BC` dates and `infinity` are an `err`.

**Alternatives**: binary results (a decoder per type, and `numeric` and timestamps would still need
one); booleans as text `t`/`f` (breaks reading the same column on SQLite).

## R8. Failure names

**Decision**:

| From | Name |
|---|---|
| SQLSTATE class `23` | `constraint` |
| `25006` | `readonly` |
| `40001`, `40P01`, `55P03` | `busy` |
| class `28` | `auth` |
| any other SQLSTATE | `error` |
| `TcpError.timed_out` | `timeout` |
| `TcpError.tls(reason)` | `tls` |
| `refused`, `not_found`, `unreachable`, `closed`, `other`, `too_many` | `connection` |
| a message the client cannot read, `too_large`, `invalid` | `protocol` |

A server failure is `NAME: CODE message`, plus ` (at position N)` when the server gives one, with the
position mapped back to the `?` text. `timeout`, `connection` and `protocol` close the connection and
set `closed`; later calls return `connection: closed`.

## R9. Transactions

**Decision**: `transaction` refuses when `in_transaction` is set (`error: a transaction is already
open`), runs `begin`, calls the function, then `commit` on `ok` and `rollback` on `err`. When `failed`
is set at the end it rolls back and returns `error: a statement in the transaction failed` if the
function returned `ok`. A connection failure during `commit` is `connection: ...; whether the
transaction was committed is not known`. A program that stops leaves the server to roll back when
the connection closes.

## R10. Tests

**Decision**:
- **In the package** (`test` items, no effects): the scanner (every quoting form, `??`, `$1`, two
  statements), message encoding and decoding, SCRAM against the RFC 7677 example, the value table,
  `get_time`, the failure-name table, the getters' `err` texts equal to `std.sqlite`'s. Run by
  `tests/postgres.py --group unit` on the three architectures.
- **Scripted peer** (`tests/postgres_server.py`, Python `ssl`, `hashlib.pbkdf2_hmac`): TLS yes/no/bad
  certificate, SCRAM right/wrong/forged server signature, other sign-in requests, canned results,
  refusing, stalling, closing midway, malformed and oversized messages (SC-006: 10,000 in the gate,
  300 in `make test`), a capture of every byte received searched for the password (SC-004), record
  and replay (SC-005).
- **Real server** (`tests/postgres.py --server`): `VO_PG_TEST=host:port:database:user:password`, or a
  container per supported version started with `docker run` on a fixed local port; the corpus of
  types and statements, transactions seen from a second connection, the 200 hostile strings, the
  notes API moved (SC-002), the timings (SC-007). In `gate-step`; `make test` prints a skip line when
  there is neither. The images must be on the machine: the suite does not pull.
- **Parity** (SC-003): a script compares `vo doc std.sqlite` and `vo doc postgres.client` for the
  shared items.
- **Untouched toolchain** (SC-009): `git diff --stat` over `src/ std/ third_party/ docs/reference.md`
  against the commit before the feature is empty.

`tools/quick.sh` gets the area `postgres` (`packages/postgres/*`, `tests/postgres*`).

## R11. The notes API on PostgreSQL

**Decision**: `tests/postgres/notes-api/` is `examples/notes-api/` with the imports, the two functions
that open the database (the service's and the tests'), the `uses` clauses and effect arguments, and
the SQL changed. The tests' opening function connects to the test server and drops the table, since
there is no in-memory database. `tests/postgres.py --group move` fails on any other differing line.
The spec's wording "the opening call" becomes "the code that opens the database".

## R12. Documentation and distribution

**Decision**: every exported item has a comment with an example that needs no server
(`sql("select ?").text is "select ?"`), run by `tests/postgres.py --group docs`. The guide is
`site/docs/guide/postgres.md`: adding the package, connect options, the type table, the failure
names, the table of differences from SQLite (FR-012), testing against a server. The package is added
with `vo package main.vo postgres https://github.com/d4n-larsson/volang/archive/refs/tags/TAG.tar.gz#packages/postgres`;
the repository must be readable without credentials for that URL to work (spec 041 refuses URLs with
credentials), otherwise the local form is used.

## R13. Speed

**Finding**: PBKDF2-HMAC-SHA256 with 4,096 rounds written in vo over `hmac_sha256`: 15 ms. Splitting
100,000 `DataRow` messages of five 8-byte columns (6.7 MB) into lists of `bytes`: 35 ms. No new
`std.crypto` primitive is needed, and SC-007 stays as written.

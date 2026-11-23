# Feature Specification: PostgreSQL Package (`postgres`)

**Feature Branch**: `049-postgres-package`

**Created**: 2026-11-23

**Status**: Draft (planned 2026-11-23; its prerequisite, spec 050, is implemented)

**Input**: User description: "PostgreSQL client as a first-party vo package (not in std), written in vo
on std.tcp and std.crypto: no toolchain changes, no C, nothing added to the language reference. Follow-up
to spec 026 (std.sqlite: 'PostgreSQL over TCP is a later, separate spec') and spec 039 (std.tcp, made so
Postgres can be a package); installed with `vo package` (spec 041). Wire protocol v3, TLS upgrade on the
same connection, SCRAM-SHA-256, extended query protocol with parameters, SQLSTATE in errors, time limits
on every step, the `net` effect and host scopes, a handle that carries its effect, record and replay from
std.tcp, identical results on three architectures. It should be replaceable with other databases (with
modifications if syntax differs for example): the API mirrors std.sqlite name for name wherever the
meaning is the same, so a program swaps databases by changing the imports, the opening call and the SQL
text where dialects differ."

Background: `std.sqlite` (spec 026) gives a vo program a database on one machine. A program deployed on
more than one machine, or beside other programs that share its data, needs a server database, and
PostgreSQL is the usual one. `std.tcp` (spec 039) was added so that such clients can be written in vo as
packages; this is the first.

Why a package and not `std.postgres`: the client needs nothing from the toolchain (the protocol is bytes
over a connection `std.tcp` already gives, with TLS, scopes and replay); the reference is close to
its 15,000-token budget; a client's fixes should not wait for a toolchain release; and it proves that the
package system carries a real library.

Why replaceable: a program often starts on SQLite and moves to a server when it is deployed, or is
tested on one and run on the other, and a team may later choose another server. If each database package
has its own API shape, every such move is a rewrite and agents must learn one API per database. If they
share one shape, a move is a change of imports, of the opening call, and of the SQL that the two
databases really spell differently.

What this spec must keep:
- **No toolchain change**: no file under `src/`, `std/` or `third_party/` changes, and the reference
  does not grow. A gap found in `std.tcp` or `std.crypto` is a separate spec, not a patch here.
- **Effects**: reaching a server is `net` with the host in scope; a database handle's type names the
  effect, and using it needs it (as `Database[E]` and `Conn[E]` do).
- **Principle III**: the same program against the same server state gives the same results on x86-64,
  ARM64 and RISC-V; a recorded run replays without the server.
- **SQL injection cannot be written by accident**: values travel only as parameters.

## User Scenarios & Testing *(mandatory)*

The users are the same as in earlier specs: AI coding agents that write vo programs from the reference,
`vo doc` and a package's own documentation, and the people who review and run those programs.

### User Story 1 - Store and query data on a server (Priority: P1)

An agent writes a service that keeps its notes in a PostgreSQL database: it adds the package with one
command, connects with a host, port, database name, user and password, creates a table, inserts rows
with parameters, and reads them back by column name.

**Why this priority**: it is the feature.

**Independent Test**: cases that connect to a test server, write rows with every value kind, read them
back and print them, byte for byte on the three architectures.

**Acceptance Scenarios**:

1. **Given** a project, **When** `vo package main.vo postgres URL` is run, **Then** the package is
   pinned, the command's output lists `net` and `entropy` as what the package can do, and
   `vo doc postgres.client` documents every item with a checked example.
2. **Given** a program with `net("db.internal:5432")`, **When** it connects, creates a table and
   inserts three rows with `?` parameters, **Then** a second run sees the three rows.
3. **Given** a query, **When** it runs, **Then** each row's columns are read by name with `get_int`,
   `get_float`, `get_str`, `get_bytes` and `is_null`, and a `NULL` is reported as such, never as `0`
   or `""`.
4. **Given** an insert of five rows, **When** it succeeds, **Then** `changed` is 5; **Given** an
   insert written with `returning id` and run with `query`, **Then** the new ids are the rows.
5. **Given** a column read as the wrong kind (`get_int` on text), **When** it is read, **Then** the
   result is an `err` naming the column and the actual kind, as in `std.sqlite`.
6. **Given** a statement that violates a constraint, **When** it runs, **Then** the result is an
   `err` starting with `constraint: 23505` followed by the server's message, and the connection is
   still usable.

---

### User Story 2 - Move a program between databases (Priority: P1)

A developer has the notes API example on `std.sqlite` and moves it to PostgreSQL. They change the
imports from `std.sqlite` to `postgres.client`, replace `open("notes.db")` by the connect call, change
the effects from the file effects to `net` and `entropy`, and edit the statements that the two databases
spell differently (the id column's declaration, for example). No other line changes. Later, moving back
is the same edit in reverse.

**Why this priority**: it is the requirement that shapes the API. Without it the package would have
its own names, value type and error shape, and each move would be a rewrite.

**Independent Test**: the notes API example exists in a SQLite and a PostgreSQL form; a script compares
the two and fails if any differing line is not an import, inside a function that opens the database, a
`uses` clause, a type's effect arguments, or SQL text; both pass the same tests of their HTTP behaviour.

**Acceptance Scenarios**:

1. **Given** code that uses `Statement`, `Value`, `Row`, `Changes`, `sql`, `parse_sql`, `exec`,
   `query`, `transaction`, `close`, `get`, `get_int`, `get_float`, `get_str`, `get_bytes` and `is_null`
   from `std.sqlite`, **When** the imports name `postgres.client` instead, **Then** every call checks
   with the same arguments and the same result types.
2. **Given** a function written `fn save[D: effects](db: Database[D], ...) uses D`, **When** the
   imports are changed, **Then** it checks unchanged and works with a server connection.
3. **Given** statements written with `?` parameters, **When** they run on PostgreSQL, **Then** they
   work as written: the program never writes `$1`.
4. **Given** a statement that only one of the databases accepts (`integer primary key autoincrement`),
   **When** it runs on the other, **Then** the result is an `err` with the server's message and
   position, and the package's documentation has a table of such differences with both spellings.
5. **Given** code that tells errors apart by their leading name (`constraint`, `readonly`, `busy`),
   **When** the database is swapped, **Then** the same names mean the same thing.
6. **Given** a program that keeps all its statements in one file, as the example does, **When** it is
   moved, **Then** only that file and the file that opens the database change.

---

### User Story 3 - Connect safely (Priority: P1)

A reviewer checks that a program reaches only the database host it declares, that the password never
travels to the server or into a recording, that the connection is encrypted and the server verified
unless the program says otherwise in plain sight, and that values a user typed cannot become SQL.

**Why this priority**: a database holds what the program is trusted with; a client that leaks the
password or lets a value become SQL is worse than none.

**Independent Test**: cases against a test server with TLS and password authentication; a capture of
the bytes sent and of a recording searched for the password; a corpus of hostile strings sent as
parameters.

**Acceptance Scenarios**:

1. **Given** a program whose grant does not include the database host, **When** it connects, **Then**
   it stops with the existing scope error (R0010), from `std.tcp`.
2. **Given** a function that takes a `Database[uses net]` and queries it without declaring `net`,
   **When** checked, **Then** the existing missing-effect diagnostic (E0501) is reported.
3. **Given** default options, **When** connecting, **Then** the connection is upgraded to TLS before
   anything else is sent and the server's certificate is verified for the host; a server without TLS,
   or with a certificate that fails, gives an `err` starting with `tls:` and nothing more is sent.
4. **Given** the option that allows an unencrypted connection, **When** it is used, **Then** it is a
   named field set in the program's source, visible to a reviewer.
5. **Given** a server that asks for SCRAM-SHA-256, **When** the program signs in, **Then** it succeeds
   with the right password and gives `auth: 28P01 ...` with a wrong one; the password is in no byte
   sent, in no recording and in no error text, and the server's proof is checked before the
   connection is used.
6. **Given** a server that asks for an MD5 or a clear-text password, **When** connecting, **Then**
   the result is an `err` saying which method the server asked for and that only SCRAM-SHA-256 is
   supported.
7. **Given** 200 hostile strings (quotes, comments, `; drop table`, NUL, invalid escapes) passed as
   parameters, **When** inserted and read back, **Then** each comes back byte for byte and no other
   statement runs.

---

### User Story 4 - Transactions (Priority: P2)

An agent moves stock between two warehouses as a function passed to `transaction`: when the function
returns `ok` the changes are kept, when it returns `err` they are undone, exactly as with `std.sqlite`.

**Why this priority**: correctness of multi-step changes, and part of the shared shape.

**Independent Test**: cases where the function succeeds, returns an `err` midway, and where the
program stops midway; the table is read afterwards from a second connection.

**Acceptance Scenarios**:

1. **Given** a transaction function that runs two updates and returns `ok`, **When** it runs, **Then**
   both changes are visible to another connection afterwards and not before.
2. **Given** one that runs an update and returns `err("out of stock")`, **When** it runs, **Then** no
   change is visible and the caller gets the same `err`.
3. **Given** one that stops with a runtime error after an update, **When** the table is read later,
   **Then** it has no change from it.
4. **Given** a transaction started inside another on the same connection, **When** it runs, **Then**
   it is an `err`, not a silent nesting.
5. **Given** a statement that fails inside a transaction and a function that goes on and returns
   `ok`, **When** it ends, **Then** the transaction is rolled back and the result is an `err` saying
   that an earlier statement failed.

---

### User Story 5 - Failures are values, and nothing waits forever (Priority: P2)

A program's database server is down, slow, restarts in the middle of a query, or answers with something
that is not PostgreSQL. Every call returns within its time limit with an `err` whose first word says
what kind of failure it is; the program never stops and never hangs.

**Why this priority**: a server database fails in ways a file does not, and a service must answer its
own callers when it does.

**Independent Test**: cases against a test server that refuses, stalls, closes midway and sends
malformed messages; each call's result and elapsed time are checked.

**Acceptance Scenarios**:

1. **Given** no server at the address, **When** connecting, **Then** the result is `err` starting
   with `connection:` within the connect time limit.
2. **Given** a statement that runs longer than the statement time limit, **When** the limit passes,
   **Then** the result is `err` starting with `timeout:`, the connection is closed, and later calls
   on it give `connection: closed`.
3. **Given** a server that closes the connection during a query, **When** it happens, **Then** the
   result is `err` starting with `connection:`; during a commit the text says that the outcome is
   not known.
4. **Given** a server that sends a malformed or unexpected message, **When** it is read, **Then** the
   result is `err` starting with `protocol:` and the connection is closed.
5. **Given** a server error, **When** returned, **Then** the text is the name, the five-character
   SQLSTATE and the server's message, with the position in the statement when the server gives one.

---

### User Story 6 - Replay and tests (Priority: P2)

A developer records a run of the service against the real server and replays it later with no server:
same output. An agent tests its database functions with `vo test` against a local test server, each
test's grant naming that host.

**Why this priority**: agents fix code by running it; the recording is how a server-backed program is
run again without its server.

**Independent Test**: a recorded run replayed with the server stopped; a `vo test` suite run 20 times
against a fresh test database.

**Acceptance Scenarios**:

1. **Given** a run recorded with `vo run --record`, **When** replayed with the server stopped, **Then**
   the output and exit status are identical and no connection is opened.
2. **Given** a recording, **When** inspected, **Then** it holds what the server sent and the random
   bytes drawn, and not the password.
3. **Given** a test with `uses net("localhost:5432"), entropy`, **When** `vo test` runs it against a
   test server, **Then** it passes or fails on the data alone; with no server it fails with
   `connection:` in the message, not a crash.

### Edge Cases

- **Placeholders**: parameters are `?` in order, as in `std.sqlite`. A `?` inside a string, a quoted
  name, a comment or a dollar-quoted body is not a parameter. `$1` is an `err` saying to write `?`; `:name` is
  the server's syntax error. PostgreSQL's own `?` operators (on `jsonb`) are written `??`, or with their function form;
  the documentation says so.
- **Parameter count**: a statement run with a different number of values than it has `?` is an `err`
  giving both counts, before anything is sent. (`vo check` cannot report it: see Assumptions.)
- **One statement**: text holding more than one statement is an `err` from `parse_sql` and from
  running it; nothing is sent.
- **Kinds of parameter**: the server decides each parameter's type from the statement. A value the
  server cannot read as that type (text `"abc"` for an integer column) is an `err` with the server's
  message; an ambiguous parameter (`select ?`) needs a cast in the SQL, and the error says so.
- **Booleans**: a boolean column is read as integer 0 or 1 and written from 0 or 1, as SQLite stores
  them, so the same getters work on both.
- **Exact numbers**: `numeric` values are text in decimal form, which `std.decimal.parse_decimal`
  reads; they are never turned into floats. Reading one with `get_float` is an `err`.
- **Times**: `timestamp with time zone` values are text; `get_time` reads one as milliseconds since
  1970 UTC (the microseconds beyond are dropped), and `std.time.format_time` text is accepted as a
  parameter. The session's time zone is UTC, so no result depends on the server's or the machine's
  zone.
- **Other types** (`uuid`, `json`, `jsonb`, dates, intervals, arrays, enums, ranges): text in
  PostgreSQL's own text form, in and out.
- **Floats that vo does not have**: `NaN` and the infinities are text, so `get_float` on one is an
  `err`.
- **Integers**: `smallint`, `integer` and `bigint` are vo's `int`. An integer column read with
  `get_float` is an `err`, and the reverse.
- **`last_id`**: PostgreSQL has no row id of the last insert; `Changes.last_id` is always 0. `insert
  ... returning id` works on both databases and is what the documentation shows.
- **Server clock and randomness** (`now()`, `random()`): they run on the server and are allowed,
  unlike in `std.sqlite`; what they return is part of the recording. The difference is in the table
  of differences.
- **Isolation**: transactions run at the server's default level (read committed), where SQLite's are
  serializable; a serialization failure or deadlock is an `err` starting with `busy:`.
- **Large results**: a query's rows are held in memory and count toward `--max-memory`; a single value
  larger than one `std.tcp` read is read in parts.
- **Limits**: a statement takes at most 65,535 parameters (the protocol's limit); more is an `err`.
- **Notices and warnings** from the server are dropped.
- **Tasks and workers**: a connection is used from one task at a time; each `serve_workers` worker
  opens its own in `setup`.
- **Closing**: `close(db)` says goodbye to the server and closes the connection; a connection open
  when the program ends is closed then, and the server rolls back an open transaction.
- **Built by hand**: a `Database` record made with `new` is refused when used, as `std.tcp` refuses a
  `Conn` built by hand.
- **Compatible servers** (other products that speak the protocol): not tested and not promised.

## Requirements *(mandatory)*

### Functional Requirements

**The package**

- **FR-001**: The client MUST be a vo package named `postgres`, written only in vo, using only `std`
  modules, added with `vo package ENTRY postgres URL` and imported as `import postgres.client.NAME;`.
  No file of the toolchain, the standard library or the vendored code changes, and the reference does
  not grow.
- **FR-002**: `vo doc postgres.client` MUST document every exported item with a checked example, and
  the package MUST carry a guide with the connect options, the table of differences from `std.sqlite`
  (FR-012) and the type table (FR-015).

**The shared shape**

- **FR-003**: The package MUST export, with the same names, parameters and result types as
  `std.sqlite`: `Database[E: effects]`, `Statement`, `Value` (cases `integer`, `real`, `text`, `blob`,
  `null`), `Row` (`columns`, `values`), `Changes` (`changed`, `last_id`), `sql`, `parse_sql`, `exec`,
  `query`, `transaction`, `close`, `get`, `get_int`, `get_float`, `get_str`, `get_bytes`, `is_null`.
  Where the meaning is the same the behaviour MUST be the same, including the `err` texts of the
  getters.
- **FR-004**: The items that differ MUST be only: opening (`connect` with an options record in place
  of `open`, `open_readonly` and `open_memory`), the effects (`net` and `entropy` in place of the file
  effects), `last_id` (always 0), and additions that `std.sqlite` has no use for (`get_time`).
- **FR-005**: Parameters MUST be positional `?`, rewritten by the package to the server's form outside
  strings, quoted names, comments and dollar-quoted bodies; `??` is a literal `?`. Numbered
  parameters (`$1`) MUST be an `err` naming `?`.
- **FR-006**: Failures MUST be `err` text that starts with a stable short name, as in `std.sqlite`,
  with the same meaning for the names both have: `constraint` (a constraint failed), `readonly` (the
  database or transaction does not allow changes), `busy` (locked, deadlock or serialization failure:
  trying again can succeed), `error` (any other failure of a statement). The package adds
  `connection`, `timeout`, `tls`, `auth` and `protocol`. For a failure reported by the server the name
  is followed by the SQLSTATE and the server's message.
- **FR-007**: Statements MUST be values made by `sql` or `parse_sql` and run with a list of values;
  no function of the package takes SQL text together with values to splice into it, and no function
  quotes or escapes a value into text.

**Connecting**

- **FR-008**: `connect` MUST take one options record (host, port, database, user, password, whether
  TLS is required, the connect time limit and the statement time limit; the guide gives the usual values) and return a `Database[uses net]` or an `err`. It needs `net` (the host,
  or host and port, in scope, checked by `std.tcp`) and `entropy`.
- **FR-009**: TLS MUST be required by default: the connection is upgraded before any name, password
  or statement is sent, and the server is verified for the host with the trust store `std.http` uses
  (`--ca-file` included). Nothing turns verification off. An unencrypted connection is possible only
  by setting the options field that says so.
- **FR-010**: Signing in MUST support SCRAM-SHA-256 and servers that ask for nothing. The password
  MUST NOT be sent, recorded or quoted in an error; the server's final proof MUST be verified. Any
  other method the server asks for is an `err` naming it.
- **FR-011**: At connect the session MUST be set so that results do not depend on the server's
  configuration or the machine: UTF-8 text, UTC, ISO date form, and standard string literals.

**Statements and values**

- **FR-012**: The guide MUST list, with both spellings, the differences a move between SQLite and
  PostgreSQL meets: generated ids, `returning`, upsert, column types, booleans, dates and times, text
  comparison and `like`, clock and random functions, isolation and locking, the limits of each.
- **FR-013**: `exec` MUST return the number of rows the statement changed; `query` MUST return every
  row with its column names (`AS` names where given), an empty list for a statement that returns
  none.
- **FR-014**: The text of a statement MUST be checked before it is sent for: exactly one statement,
  only `?` parameters, and as many values as `?`. `parse_sql` reports the first two without a
  connection.
- **FR-015**: Column values MUST map as: `smallint`, `integer`, `bigint` and `boolean` (0 or 1) to
  `integer`; `real` and `double precision` to `real`; `bytea` to `blob`; SQL `NULL` to `null`; every
  other type to `text` in the server's text form. Parameters MUST be sent so that the server reads
  each as the type the statement gives it. No value is converted between kinds by the getters.
- **FR-016**: `get_time` MUST read a `timestamp with time zone`, a `timestamp` (taken as UTC) or a
  `date` (its midnight UTC) as milliseconds since 1970 UTC, and return an `err` for anything else.

**Transactions**

- **FR-017**: `transaction` MUST commit when its function returns `ok` and roll back when it returns
  `err` (returning that `err`); a transaction inside another on the same connection MUST be an `err`;
  a transaction in which a statement failed MUST end rolled back with an `err`, whatever the function
  returns; a program that stops midway leaves no change.

**Failures and limits**

- **FR-018**: Every call that waits MUST be bounded by the connect or the statement time limit. After
  a `timeout`, `connection` or `protocol` failure the connection MUST be closed and later calls on it
  MUST return `connection: closed`. No input from a server stops or hangs the program.
- **FR-019**: A failed statement outside a transaction MUST leave the connection usable for the next
  statement.

**Replay and determinism**

- **FR-020**: A run recorded with `vo run --record` MUST replay with `vo replay` without a server and
  with identical output; the package adds nothing to the recording format.
- **FR-021**: For the same server state and program, values, their text forms, row order and error
  texts MUST be identical on x86-64, ARM64 and RISC-V.

**Fit**

- **FR-022**: The package MUST live in this repository, be served as an archive that `vo package`
  reads, and have its tests run by the project's suite: against a real PostgreSQL server where the
  gate runs, and against recorded conversations and a scripted test server elsewhere, so that the
  quick tests need no server.
- **FR-023**: The notes API example MUST exist in a PostgreSQL form that differs from the SQLite form
  only as User Story 2 allows, with all its statements in one file.
- **FR-024**: The site's package documentation, the roadmap, the changelog and the review's Postgres
  row MUST be updated; `docs/reference.md` is not.

### Key Entities

- **Database**: an open, signed-in connection to one database on a server; its type names `net`.
- **Options**: where and how to connect: host, port, database, user, password, TLS required or not,
  the two time limits.
- **Statement**: the text of one SQL statement with `?` parameters.
- **Value**: integer, real, text, blob or null: a parameter or a column value; the same five cases as
  `std.sqlite`.
- **Row**: the column names and values of one result row.
- **Changes**: the number of rows a statement changed (`last_id` is kept for the shared shape and is
  0).
- **Failure name**: the first word of an `err` text: `constraint`, `readonly`, `busy`, `error`,
  `connection`, `timeout`, `tls`, `auth`, `protocol`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every acceptance scenario is a test case, and every case gives byte-identical output on
  x86-64, ARM64 and RISC-V against the same server.
- **SC-002**: The notes API example moves from SQLite to PostgreSQL and back with changes only to
  imports, the functions that open the database, effect clauses and effect arguments, and SQL text; no
  other line differs
  between the two forms, and both pass the same behaviour tests.
- **SC-003**: Every function and type the two modules share has the same signature apart from the
  module name; a script that compares `vo doc` output of both finds no other difference.
- **SC-004**: The password appears in no byte sent, no recording and no error text across the whole
  test suite; the 200 hostile parameter strings are stored and returned byte for byte.
- **SC-005**: Recorded runs replay with byte-identical output with the server stopped.
- **SC-006**: Against a test server sending 10,000 malformed, truncated and oversized responses, every
  call returns an `err` within its time limit; none stops or hangs the program.
- **SC-007**: Connecting and signing in to a server on the same machine takes under 1 second; reading
  100,000 rows of five columns takes under 3 seconds; inserting 10,000 rows in one transaction takes
  under 5 seconds (median of 3 runs, native x86-64 build, idle machine).
- **SC-008**: The suite passes against every PostgreSQL release its project supports when this spec is
  implemented (15 and later).
- **SC-009**: The toolchain is untouched: no change under `src/`, `std/` or `third_party/`, the `vo`
  binary is byte-identical before and after, and the reference's token count is unchanged.

## Assumptions

- **Prerequisite (found while planning, decided 2026-11-23).** Today a function outside `std` that
  connects to a host it is given must declare plain `net`, and so must `main` (E0503), so the scoped
  grants of User Stories 1, 3 and 6 cannot be written. A separate spec, implemented before this one,
  lets a package function connect under its caller's scoped `net`. That is the one toolchain change
  this feature depends on, and it is not made here. It is spec 050 (scoped effects through package
  functions), implemented 2026-11-23: `connect(options)` declares `net, entropy` and the program
  keeps `net("db.internal:5432")`.

- **The shared shape is a convention, not a type.** vo has no interfaces, and this spec changes no
  language feature, so one function cannot take both a SQLite and a PostgreSQL handle. Replaceable
  means a program is moved by editing imports, the opening call and SQL; it does not mean a program
  chooses its database at run time. A later database package follows the same convention.
- **`?` everywhere.** The package rewrites `?` to the server's numbered form, as JDBC does, so
  placeholders are not one of the things a move has to edit.
- **Errors are text, as in `std.sqlite`**, not a variant (`result[T, E]`, spec 032). A typed error
  would make every `try` in a moved program a type error (E0412) and would not fit `transaction`,
  whose function fails with the program's own text. The SQLSTATE is in the text for programs that
  need it.
- **What is lost against `std.sqlite`**, because `vo check` knows only `std.sqlite`'s `sql` and the
  toolchain is not changed here: `sql` is not required to be a literal, and syntax and parameter
  count are not reported at check time. The count and the one-statement rule are checked when the
  statement runs, before it is sent; syntax is the server's to report. Extending the check-time rules
  to packages is a possible later toolchain spec.
- **No in-memory database and no read-only open**: tests of server code need a server (User Story 6);
  read-only access is a matter of the database user's rights.
- **SQL keeps the statements in one place by convention**: the example and the guide put a program's
  statements in one file; the package has no statement registry or dialect switch.
- **The package lives in this repository** (`packages/postgres/`), fetched with the `#PATH` form of
  spec 041, and is versioned with the repository's tags.
- **If a needed primitive is missing or too slow** (the key derivation of SCRAM is built in vo from
  `hmac_sha256`), SC-007 decides; a new `std.crypto` function would be its own spec.
- **A query returns all its rows at once**, as in `std.sqlite`.
- Out of scope: connection pools, `LISTEN`/`NOTIFY`, `COPY`, replication, MD5 and other old sign-in
  methods, SCRAM channel binding, client certificates, Unix sockets, cancelling a running statement
  (a timeout closes the connection), several statements in one text, an ORM or query builder, schema
  migrations, packages for other databases, choosing the database at run time, and any change to
  `std.sqlite`.

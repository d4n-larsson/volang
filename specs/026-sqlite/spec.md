# Feature Specification: SQLite Databases

**Feature Branch**: `026-sqlite`

**Created**: 2026-11-19

**Status**: Implemented (clarified 2026-11-19: effects travel in the handle type)

**Input**: User description: "SQLite database support: a std.sqlite module with an embedded, vendored
SQLite (public domain, allowed by constitution 1.5.0). A database file is scoped by the existing file
effects (file.read for read-only, file.write to change it); ":memory:" needs no effect, so vo test can
use real SQL deterministically. API shape from an agent's point of view: sql("...") takes a string
literal checked at check time (syntax, ? placeholder count) so SQL is never built from strings (no
injection by accident), with parse_sql for dynamic SQL; parameters and column values are a variant (int,
float, text, bytes, null); rows read with getters returning result; exec and query; transactions as a
function whose result commits or rolls back. Record and replay of query results like std.http. Results
identical on x86-64, ARM64 and RISC-V. PostgreSQL over TCP is a later, separate spec."

Background: vo programs can keep data only in files they read and write whole (`std.fs`, `std.json`). An
agent asked for a to-do service, an inventory, a ledger or a cache of fetched pages writes its own file
format and its own search, or cannot do the task. A local SQL database is what such programs use in
other languages, and SQLite is the one embedded in almost every platform. The constitution (1.5.0) now
allows vendoring public-domain code.

From an agent's point of view:
- **SQL injection comes from building SQL out of strings.** Agents do it when the API allows it. If the
  statement must be a literal and values travel only as parameters, the mistake cannot be written.
- **A literal statement can be checked when the program is checked**: syntax errors and a wrong number
  of parameters become diagnostics pointing into the SQL, before anything runs.
- **SQLite's dynamic typing surprises agents** (a column declared `INTEGER` can hold text). Reading a
  column as the wrong type must be an `err`, never a silent conversion, as everywhere in vo.
- **Tests need a database without a server**: an in-memory database gives real SQL in `vo test`, the
  same on every machine.
- **A handle must not hide effects.** In vo a function without `uses` touches nothing. If a database
  handle could be passed to such a function and used to read or write its file, that promise (and
  security property S1) would be gone. The handle's type therefore names the effects it was opened
  with, and using it needs them, exactly as calling a function value needs the effects in its type.

What this spec must keep:
- **Effects**: a database file is a file. Opening it needs `file.read` for its directory to read, and
  `file.write` to change it; no file outside the grant is ever opened, including the database's journal
  and temporary files and anything SQL itself names.
- **Principle III**: the same program, database and inputs give the same results on every platform; no
  SQL function may read the clock or randomness behind the program's back.
- **Record and replay**: a recorded run replays without the database file.
- **Principle V**: the reference budget (about 12,930 of 15,000 tokens used).

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–025: AI coding agents that write vo programs from the reference
and `vo doc`, and the people who review and run those programs.

### User Story 1 - Store and query data (Priority: P1)

An agent writes an inventory program that keeps items in a database file under `data/`: it creates the
table on first run, inserts items, updates quantities and lists items below a threshold, ordered by name.

**Why this priority**: keeping and searching structured data is the reason programs use a database.

**Independent Test**: golden cases that create a database in a scoped directory, write rows, read them
back with every value type, and print them, byte for byte.

**Acceptance Scenarios**:

1. **Given** a program with `file.write("data/")`, **When** it opens `data/inventory.db` (which does not
   exist yet), creates a table and inserts three rows with parameters, **Then** the file exists and a
   second run of a read-only program sees the three rows.
2. **Given** a query with `?` parameters, **When** it runs, **Then** each row's columns can be read by
   name as `int`, `float`, `str` or `bytes`, and a `NULL` is reported as such, never as `0` or `""`.
3. **Given** an insert, **When** it succeeds, **Then** the program learns how many rows changed and the
   id of the inserted row; **Given** an update after it, **Then** the reported id is 0, not the
   insert's.
4. **Given** a value of the wrong type in a column (text in a column read as `int`), **When** it is read,
   **Then** the getter returns an `err` naming the column and the actual type.
5. **Given** a statement that violates a constraint (a duplicate in a `UNIQUE` column), **When** it runs,
   **Then** it returns an `err` whose text says which constraint failed, and the program goes on.

---

### User Story 2 - SQL mistakes are caught before the program runs (Priority: P1)

An agent writes a statement with a syntax error, the wrong number of parameters, two statements in one
literal, a named parameter (`:id`), or builds the SQL with `+`. `vo check` reports each at its place, with
a fix where the correction is clear.

**Why this priority**: SQL errors otherwise surface only when that line runs, and string-built SQL is the
most damaging mistake an agent can make with a database.

**Independent Test**: golden check cases, one per kind of mistake, with the expected JSON diagnostics and
the fix applier re-checking every fix.

**Acceptance Scenarios**:

1. **Given** `sql("selec * from items")`, **When** checked, **Then** a diagnostic points at `selec` and
   quotes SQLite's message.
2. **Given** `sql("insert into t values (?, ?)")` run with a list of three parameters written in the
   call, **When** checked, **Then** a diagnostic gives both counts.
3. **Given** `sql(text)` where `text` is not a literal (for example built with `+`), **When** checked,
   **Then** the diagnostic says statements are literals and names `parse_sql` for SQL that must be
   built at run time.
4. **Given** `sql("create table a(x); create table b(y)")`, **When** checked, **Then** the diagnostic
   says one statement per `sql`, with the fix splitting it into one call per statement only when that
   is unambiguous (the call is a statement of its own).
5. **Given** `sql("select * from t where id = :id")` (or `$id`, `@id`, `?1`), **When** checked, **Then**
   the diagnostic says parameters are `?` in order, with the fix rewriting them when each name is used
   once.
6. **Given** a statement naming a table that does not exist, **When** checked, **Then** nothing is
   reported (the schema is only known when the program runs); **When** it runs, **Then** it returns an
   `err` with SQLite's message.

---

### User Story 3 - Tests use a real database with no setup (Priority: P1)

An agent tests its inventory functions with `vo test`, each test opening an in-memory database, creating
the schema and checking query results. The tests need no effect, no file and no server, and give the
same results every run on every machine.

**Why this priority**: agents fix code by running tests; database code that cannot be tested cheaply is
code they cannot fix.

**Independent Test**: golden test cases (`vo test`) using in-memory databases, run repeatedly and on all
three platforms.

**Acceptance Scenarios**:

1. **Given** a test that opens an in-memory database, **When** it is checked, **Then** it needs no effect.
2. **Given** two tests each opening an in-memory database, **When** they run, **Then** each starts with an empty
   database (nothing leaks between tests).
3. **Given** the same test suite, **When** it runs 20 times and on all three platforms, **Then** the
   output is byte-identical every time.

---

### User Story 4 - Transactions (Priority: P2)

An agent moves stock between two warehouses: subtract from one, add to the other, and either both
happen or neither. It writes the work as a function; when the function returns `ok`, the changes are
kept, when it returns `err`, they are undone.

**Why this priority**: correctness of multi-step changes; without it agents write partial updates.

**Independent Test**: golden cases where the function succeeds, returns an `err` midway, and where a
runtime error stops the program midway; the database afterwards is checked.

**Acceptance Scenarios**:

1. **Given** a transaction function that runs two updates and returns `ok`, **When** it runs, **Then**
   both changes are visible afterwards.
2. **Given** one that runs one update and then returns `err("out of stock")`, **When** it runs, **Then**
   no change is visible and the caller gets the same `err`.
3. **Given** one that stops with a runtime error after one update, **When** the program is run again,
   **Then** the database has no change from it.
4. **Given** a transaction started inside another, **When** it runs, **Then** it is an error, not a
   silent nesting.

---

### User Story 5 - Safe, scoped and replayable (Priority: P2)

A reviewer checks that a program granted `file.read("data/")` only can read but never change
`data/app.db`, that SQL cannot reach files outside the grant (`ATTACH`, `VACUUM INTO`), and that a
recorded run replays without the database file.

**Why this priority**: SQL is a language of its own that can name files; the effect system must still
bound what a program can touch.

**Independent Test**: golden and step cases for read-only opens, every SQL statement that names a file,
and record/replay.

**Acceptance Scenarios**:

1. **Given** a program granted only `file.read("data/")`, **When** it opens `data/app.db` and runs an
   `insert`, **Then** the insert returns an `err` saying the database is read-only, and the file is
   unchanged.
2. **Given** `attach database '/etc/x.db' as x` or `vacuum into '../copy.db'`, **When** run, **Then** it
   stops with the existing effect error (R0010) naming the path, and no file outside the grant is
   created or read.
3. **Given** a recorded run that read rows from `data/app.db`, **When** it is replayed with the file
   deleted, **Then** the output is identical.
4. **Given** SQL calling `random()`, `randomblob()` or `datetime('now')` (or another function that reads
   the clock or randomness), **When** it runs, **Then** it is an `err` telling the agent to compute the
   value in vo and pass it as a parameter.
5. **Given** a database file that is not a database (or is corrupt), **When** it is opened or queried,
   **Then** the program gets an `err`, never a crash.

---

### User Story 6 - Effects follow the database (Priority: P1)

An agent writes `fn low_stock(db: Database[uses file.read, file.write]) -> result[list[Row]]` without
a `uses` clause. `vo check` reports that querying the database needs `file.read` and `file.write`, as
for any missing effect, with the call path. The same helper written for an in-memory database needs no
effect. A reviewer reading `vo.effects` sees which functions touch the database file.

**Why this priority**: without it a database handle is a way to do file I/O from a function that
declares none: the effect system's central promise would not hold for databases.

**Independent Test**: golden check cases for missing effects through database handles (direct, through
a record field holding a handle, through a function value), cases for user-defined records with an
effect parameter, and a run case where a hand-built handle is refused at run time.

**Acceptance Scenarios**:

1. **Given** a function taking `Database[uses file.read, file.write]` and calling `query` on it without
   declaring effects, **When** checked, **Then** the existing missing-effect diagnostic (E0501) names
   `file.read` and `file.write` and the call.
2. **Given** the same function taking `Database` (no effects: an in-memory database), **When** checked,
   **Then** it needs no effect.
3. **Given** a database from `open_readonly`, **When** it is passed where `Database[uses file.read,
   file.write]` is expected, **Then** it is a type error (E0301) naming both types; where `Database[uses
   file.read]` is expected it is accepted.
4. **Given** a type written `Database[file.read]` (without `uses`), **When** checked, **Then** a
   diagnostic says effects in a type are written after `uses`, with the fix `Database[uses file.read]`.
5. **Given** a user record `record Conn[E: effects] { id: int; }` and a function `fn ping[E:
   effects](c: Conn[E]) uses E`, **When** used with `Conn[uses console]`, **Then** the caller needs
   `console`, as for the library's `Database`.
6. **Given** `new Database { id: 0 }` built by hand to reach a file connection opened elsewhere, **When**
   a statement runs on it from a function without file effects, **Then** it stops with R0010 (the
   connection's effects are checked at run time against the running grant).
7. **Given** `vo effects --update`, **When** a function queries a file database, **Then** `vo.effects`
   lists its file effects like any other.

### Edge Cases

- **Opening**: opening read-write (which creates a missing file) needs `file.read` and `file.write`;
  opening read-only is a separate function that needs only `file.read` (a missing file is then an
  `err`). Which one a program uses is visible in its code and effects, never inferred from the grant.
- **Journal and temporary files**: the database's journal lives next to it (inside the same scope);
  temporary storage for sorts and large results is kept in memory, so no file outside the scope is ever
  created.
- **Text and bytes**: column text must be valid UTF-8 to be read as `str` (an `err` otherwise, pointing to
  reading it as `bytes`); `str` parameters are always valid UTF-8 (vo's `str` guarantee).
- **Integers**: SQLite's 64-bit integers are vo's `int`; reading an integer column as `float` is an `err`
  (no implicit conversion), as is the reverse.
- **Large results**: a query's rows are held in memory and count toward `--max-memory` (R0006 when over).
- **Time limits**: `--timeout` stops a long statement (R0009 at the call); a database locked by another
  process waits up to a bounded time and then returns an `err` ("database is locked").
- **Tasks**: a database connection cannot be used inside `concurrent_map` tasks (a runtime error naming
  the fix); a task opens its own.
- **Closing**: `close(db)` ends a connection; any connection still open when the program ends is closed
  then, and an uncommitted transaction is rolled back.
- **SQL extensions and pragmas**: loading extensions is not available (`load_extension` is not a
  function), and exactly these pragmas are refused with an `err` (in `vo check` too, for literals):
  `temp_store_directory`, `data_store_directory` (they name directories) and `journal_mode = wal`
  (vo databases use the rollback journal). Every other pragma behaves as in SQLite and touches no file
  outside the database and its journal.
- **Same output everywhere**: SQL that turns floats into text (`CAST(x AS TEXT)`, `printf`) gives the
  same text on every platform.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The standard library MUST provide `std.sqlite` with: opening and closing a database (a
  file path or `":memory:"`), a statement type made from a string literal (`sql`) or from text at run
  time (`parse_sql`, returning a `result`), running a statement that changes data (`exec`: rows changed
  and the last inserted id) and one that returns rows (`query`), and transactions run by a function.
- **FR-002**: Parameters MUST be positional `?` only, passed as a list of values of a variant with the
  cases integer, float, text, bytes and null. Rows MUST expose their column names and typed getters by
  column name that return `result` (`err` for an unknown column, a name that more than one column has,
  a type mismatch, or invalid UTF-8 read as text) and a way to tell `NULL` apart.
- **FR-003**: `sql` MUST require a string literal (any other argument is a check error naming
  `parse_sql`); the literal MUST be checked for SQL syntax and for exactly one statement, and its `?`
  count MUST be checked against a parameter list written in the call. Named or numbered parameters MUST
  be check errors with a fix to `?` when each is used once.
- **FR-004**: Table and column names MUST NOT be checked at check time; at run time, unknown names are
  `err` values with SQLite's message.
- **FR-005**: Opening a file read-only MUST need `file.read` and give a `Database[uses file.read]`;
  opening it read-write (and creating it) MUST need `file.read` and `file.write` and give a
  `Database[uses file.read, file.write]`; a read-only connection MUST refuse changes. An in-memory
  database MUST need no effect, has its own opening function and gives a `Database` with no effects
  (`":memory:"` passed as a path is a check error with a fix to it). Effects of literal paths are checked at check time as for other file functions, and every path
  at run time against both grants (as spec 013 requires).
- **FR-006**: Every file the database engine opens (the database, its journal, attached databases,
  `VACUUM INTO` targets) MUST be checked against the grant, through the same scoped file access as the
  rest of vo (no link escapes); temporary storage MUST stay in memory; loading extensions MUST be
  disabled.
- **FR-007**: SQL functions that read the clock or randomness MUST return an error explaining the
  alternative; all other built-in SQL functions behave as in SQLite.
- **FR-008**: A transaction function MUST commit when it returns `ok` and roll back when it returns
  `err` (returning that `err`); a runtime error during it MUST leave no change in the database; a nested
  transaction MUST be an `err`.
- **FR-009**: `vo run --record` MUST record what each statement returned (rows, changes, errors) and
  `vo replay` MUST give them back without opening the database; a program that runs a different
  statement than the recording is R0014, as for other inputs.
- **FR-010**: Results (values, their text forms, ordering of rows with the same `ORDER BY`, error texts)
  MUST be identical on x86-64, ARM64 and RISC-V for the same database and program.
- **FR-011**: Every failure from the database (constraint, busy, corrupt file, read-only, no such table)
  MUST be an `err` value whose text starts with a stable short name (`constraint`, `busy`, `corrupt`,
  `readonly`, `nomem`, `error`) followed by SQLite's message; no database failure crashes the program.
- **FR-012**: Each new diagnostic MUST have a stable code with a catalog entry, an example and a fix
  where one exists; `vo doc std.sqlite` MUST document every item with a checked example; the reference
  grows by one standard-library row.
- **FR-013**: The vendored SQLite (its `VENDOR.md` links SQLite's vulnerability page, and the roadmap
  lists the obligation) MUST follow the constitution's vendoring rules (a pinned release, its
  checksum and dedication in `VENDOR.md`, unmodified files, patches with reasons, built freestanding
  with the project's floating-point rules, passing the instrumented build) and MUST be updated in the
  spec cycle of any SQLite security release.
- **FR-014**: Record and variant types MUST be able to declare an effect parameter (`[E: effects]`, as
  functions already can). In a type, effect arguments MUST be written after `uses` as the last item
  in the brackets (`Database[uses file.read, file.write]`); leaving them out means no effects (a type
  whose only parameter is an effect parameter is then written without brackets: `Database`). Effect
  arguments are inferred like type arguments (from values and expected types); an unbound effect
  parameter means no effects. Effect arguments written without `uses` MUST be a check error with the
  fix. The formatter, `vo doc`, hover and diagnostics MUST print types in this form.
- **FR-015**: Every `std.sqlite` operation on a database MUST need the effects named in the database's
  type (`exec[E: effects](db: Database[E], ...) uses E`), so effects of database use are checked,
  reported (E0501) and recorded in `vo.effects` like all others.
- **FR-016**: Each connection MUST remember the effects and path it was opened with, and every
  statement MUST check them against the running grant at run time (R0010 otherwise), so a handle built
  by hand cannot bypass the static check.

### Key Entities

- **Database**: an open connection to a file (read-only or read-write) or to an in-memory database; its
  type names the effects it was opened with (`Database[uses file.read, file.write]`, `Database[uses
  file.read]`, `Database`).
- **Statement**: SQL text checked once (a literal) or parsed at run time; exactly one statement; `?`
  parameters.
- **Value**: integer, float, text, bytes or null: a parameter or a column value.
- **Row**: the column names and values of one result row.
- **Changes**: the number of rows a statement changed and the row id of the last row that statement
  itself inserted (0 when it inserted none: an `UPDATE` never reports an earlier insert's id).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every acceptance scenario is a golden or step case, and every case gives byte-identical
  output on x86-64, ARM64 and RISC-V; a corpus of at least 500 SQL statements exercising every value
  type and text conversion gives identical results on the three platforms.
- **SC-002**: Inserting 100,000 rows in one transaction takes under 2 seconds, and reading them back
  with a query takes under 1 second (median of 3 runs of the native x86-64 build on an idle machine,
  database file on a local disk).
- **SC-003**: Every kind of SQL mistake in User Story 2 is reported at check time with a stable code;
  every fix, applied, gives a program that checks.
- **SC-004**: No statement in a corpus of file-naming SQL (`ATTACH`, `VACUUM INTO`, journal and temporary
  file settings, extension loading) touches a file outside the program's grant.
- **SC-005**: A test suite using in-memory databases runs 20 times with byte-identical output and needs
  no effect.
- **SC-006**: Recorded runs that read and write a database replay with byte-identical output after the
  database file is deleted.
- **SC-007**: Fuzzing the database API with 10,000 random statements and corrupt database files produces
  no crash (only `ok` and `err` results).
- **SC-008**: The reference grows by at most 250 tokens (the library row and the effect arguments in
  the Generics section); the binary grows by at most 1.5 MB.
- **SC-009**: Every path by which a database handle can reach a function (parameter, record field,
  list element, captured by a function value, returned) is covered by a check case in which a missing
  effect is reported; no case reaches a database file from a function without file effects.

## Assumptions

- Effect parameters on types are a language change made in this spec because `std.sqlite` is the first
  library that needs it; later handles (PostgreSQL and TCP connections, under `net`) will use the same
  form. Effect arguments name effects as `uses` clauses do (scopes allowed: `Database[uses
  file.read("data/")]`), and a type with fewer or different effects is a different type (no subtyping
  between effect sets in this spec).
- The module is `std.sqlite`, imported item by item; names do not clash with builtins or other std
  modules. A later PostgreSQL module (separate spec, after TCP) will reuse the same shapes (statement,
  value, row, changes) so agents learn one API.
- Positional `?` parameters only: one form; named parameters are a second spelling.
- Two SQLite defaults are changed because agents expect the other behavior: declared foreign keys are
  enforced (`REFERENCES` means something), and double-quoted text is always an identifier (`"name"` is
  never silently a string; a misspelled column in double quotes is an error, not a constant).
- One statement per `sql` literal: schema setup is one `exec` per statement; a script runner is out of
  scope.
- A query returns all its rows at once (a list), bounded by `--max-memory`; streaming rows is out of
  scope until a program needs it.
- Clock and random SQL functions are disabled rather than tied to vo's clock and random effects:
  passing values as parameters keeps every effect visible in vo code.
- No schema checking at check time (the database is not available then), no migrations framework, no
  ORM or mapping of rows to records.
- Concurrency between processes uses SQLite's file locking with a bounded wait; vo's own tasks do not
  share connections.
- Full-text search, JSON functions and other optional SQLite extensions are out of scope unless they are
  part of the default build and deterministic; the plan decides which compile options are used.

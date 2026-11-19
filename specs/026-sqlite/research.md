# Research: SQLite Databases

Several decisions below name a SQLite compile option or behavior that depends on the release vendored
at T001. Each is marked **verify**: the first task that builds or runs that part checks it against the
vendored sources and records the result in `results.md`; if the vendored release differs, the stated
fallback is used.

## R1. What is vendored and how it is pinned

**Decision**: the amalgamation (`sqlite3.c`, `sqlite3.h`) of the newest 3.x release on
`sqlite.org/download.html` when T001 runs. `third_party/sqlite/VENDOR.md` records the version, the
download URL, SQLite's published SHA3-256 of the archive and our SHA-256 of each kept file, the
public-domain dedication (copied from the header of `sqlite3.c`), the kept files and the compile options
(R2). Files are unmodified; any change would be a patch in `third_party/sqlite/patches/` (none planned).

**Rationale**: constitution 1.5.0 vendoring rules; the amalgamation is SQLite's supported way to embed
it (one translation unit, also faster).

**Alternatives considered**: the canonical source tree (needs SQLite's build tools); a system library
(vo links none).

## R2. Compile options

| Option | Why |
|---|---|
| `SQLITE_OS_OTHER=1` | no built-in OS layer: ours (R5) is the only way to files |
| `SQLITE_THREADSAFE=0` | vo runs one thread; tasks are processes |
| `SQLITE_OMIT_LOAD_EXTENSION` | no native code loading (security property S3) |
| `SQLITE_OMIT_WAL` | WAL needs shared memory mapping between processes; the rollback journal suffices and keeps the VFS small |
| `SQLITE_TEMP_STORE=3` | temporary tables and sort spills always in memory: no temp files outside the scope |
| `SQLITE_ENABLE_MEMSYS5`, `SQLITE_ZERO_MALLOC` | SQLite's own allocator over one region (R4); no libc `malloc` |
| `SQLITE_DEFAULT_MEMSTATUS=0`, `SQLITE_OMIT_DEPRECATED`, `SQLITE_OMIT_SHARED_CACHE`, `SQLITE_UNTESTABLE` | smaller, no global state that vo does not use |
| `SQLITE_DQS=0` | double-quoted strings are identifiers only: `"name"` meaning a string is a classic agent bug; SQLite then reports it |
| `SQLITE_OMIT_LOCALTIME` | the `localtime` modifier reads the machine's time zone (nondeterministic) |
| `SQLITE_ENABLE_MATH_FUNCTIONS` | `sqrt`, `pow`, ... through the shim's `math.h`, i.e. CORE-MATH: correctly rounded, identical everywhere |
| `SQLITE_DEFAULT_FOREIGN_KEYS=1` | foreign keys enforced by default (what agents expect from a declared `REFERENCES`) |
| double-only floating point | **verify**: recent releases choose between `long double` and a double-double path for text ↔ float conversions (`sqlite3AtoF`, `sqlite3FpDecode`); `long double` is 80-bit on x86-64 and 128-bit on ARM64 and RISC-V, so it must be off. Use the release's compile option or configuration switch for it; fallback: a patch selecting the double-only path, with its reason |
| `-ffp-contract=off`, no `-ffast-math` | constitution |

Everything else stays at SQLite's defaults (limits such as `SQLITE_MAX_LENGTH`, `SQLITE_MAX_SQL_LENGTH`
are SQLite's, which bound hostile input).

## R3. libc for SQLite

**Decision**: compile, see what is undefined, and add those functions to `src/vo_c.c` (declarations in
`third_party/shim/`). Expected: `memcpy`, `memset`, `memmove`, `memcmp`, `strlen`, `strcmp`, `strncmp`,
`strcspn`, `strrchr`, `qsort` (none of these with locale or I/O). **verify** at T003.

## R4. Memory

**Decision**: one region reserved with `mmap` (address space only; pages are touched as used) and given
to SQLite with `sqlite3_config(SQLITE_CONFIG_HEAP, region, 64 MiB, 64)`. When it is full, a statement
fails with `nomem` (an `err`); vo values built from rows count toward `--max-memory` as usual.

**Rationale**: memsys5 is part of SQLite and tested there; no allocator of our own; bounded.
ponytail: a fixed 64 MiB heap, not counted in `--max-memory`; a per-program setting if programs need
more.

## R5. The file layer (VFS)

**Decision**: a VFS named `vo`, registered by `sqlite3_os_init`:

| Method | Behavior |
|---|---|
| `xOpen` | main database and its journal, attached databases, `VACUUM INTO` targets: the path is checked with `rt_check_path(FILE_READ or FILE_WRITE, path)` (writing when SQLite asks for read-write or create) and opened with `rt_open_file` (spec 013: from the scope directory, one component at a time, no links). Outside the grant: R0010 at the user's call, as for any file operation. A file without a name (a temporary file): refused (never happens with `SQLITE_TEMP_STORE=3`) |
| `xDelete` | the journal: `unlinkat` through `rt_open_parent(FILE_WRITE, ...)` (spec 017) |
| `xAccess` | `faccessat` through the same scoped parent |
| `xFullPathname` | the path as given (vo's checks resolve it); no `getcwd` |
| `xRead`, `xWrite`, `xTruncate`, `xSync`, `xFileSize` | `pread64`, `pwrite64`, `ftruncate`, `fsync`, `fstat` |
| `xLock`, `xUnlock`, `xCheckReservedLock` | POSIX advisory locks (`fcntl F_SETLK`) as SQLite's Unix VFS uses them, so other processes (and other vo programs) see the locks |
| `xRandomness` | a fixed byte pattern (SQLite's PRNG, used only for things like temp names, is then deterministic) |
| `xSleep` | `nanosleep`, used by the busy handler (R10) |
| `xCurrentTime`, `xCurrentTimeInt64` | fail (R6) |

The VFS file is small enough to review on its own; it is listed in `docs/security.md`.

## R6. Clock and randomness in SQL

**Decision**: an authorizer (`sqlite3_set_authorizer`) refuses the functions `random` and `randomblob`
at prepare time, so they are errors in `vo check` (E0323) and at run time alike, with the message
"`random()` reads randomness: compute the value in vo (std.random) and pass it as a parameter." For the
current time (`'now'`, `CURRENT_TIMESTAMP`, `CURRENT_DATE`, `CURRENT_TIME`), the VFS time hook fails.
**verify** that SQLite then reports an error (not a `NULL` result); fallback: the authorizer refuses
`current_timestamp`, `current_date` and `current_time`, and the date functions are registered again
as wrappers that refuse the argument `'now'` and call the built-in otherwise.

**Pragmas** (same authorizer, `SQLITE_PRAGMA` action): refuse `temp_store_directory`,
`data_store_directory` and `journal_mode` with the argument `wal` (any case), with the message
"PRAGMA NAME is not available in vo"; allow every other pragma.

## R7. Handles and the vo types

**Decision**:
- `Database` is a record `{ id: int }`: an index into a C table of open connections (at most 64 open at
  once; ponytail). Each entry records the process that opened it; use from another process (a
  `concurrent_map` task) is R0007 "Open the database inside the task." A closed or forged id is R0007.
- `Statement` is a record `{ text: str }`; prepared statements are cached per connection by text
  (`sqlite3_prepare_v3` with `SQLITE_PREPARE_PERSISTENT`, at most 64 per connection, round-robin;
  ponytail).
- `Value` is a std variant `integer(int); real(float); text(str); blob(bytes); null;` in that order. The C
  side reads and builds these variants with the runtime's variant layout (as `make_result` does); the
  case order is fixed and a golden case breaks if it changes.
- Rows come back from `__sql_query` as `list[list[Value]]`, the first list being the column names as
  `text` values; `std/sqlite.vo` turns that into `list[Row]` (`Row { columns: list[str]; values:
  list[Value]; }`).

**Rationale**: no new language types; the same record-over-primitive style as `std.process` and
`std.regex`; values cross once, already typed.

## R8. Effects

**Decision**: `open(path) -> result[Database] uses file.read, file.write` (read-write, creates the file),
`open_readonly(path) -> result[Database] uses file.read`, `open_memory() -> Database` (no effect).
Paths are checked at run time against both grants like every `std.fs` function (statement and
attached-database paths through the VFS, R5). `open(":memory:")` is a check error with the fix
`open_memory()` (E0326), and at run time an `err` (a file named `:memory:` is never created).

**Rationale**: the effect is visible in the signature (explicit read-only versus read-write), instead of
depending on the grant at run time.

## R9. Check-time SQL (`src/check_sqlite.c`)

**Decision**: when the callee is `std.sqlite`'s `sql`:
- the argument must be `N_STR` (else E0324, naming `parse_sql`);
- a small tokenizer (strings `'...'`, identifiers `"..."`, `` `...` `` and `[...]`, comments `--` and
  `/* */`) finds top-level `;` (more than one statement: E0323, with a fix splitting the call only when
  the call is a whole expression statement), `?` parameters, and named or numbered ones (`:a`, `@a`,
  `$a`, `?1`: E0325 with the fix to `?` when each is used once);
- the literal is prepared on a scratch in-memory connection (opened once per check, the same authorizer
  as at run time). Errors that are not about names (syntax, incomplete input, unrecognized token,
  authorizer refusals) are E0323 at the byte `sqlite3_error_offset` gives (mapped to source with
  `lit_decode`); "no such table/column/function" and other schema errors are ignored (FR-004).

For `exec(db, sql("..."), [a, b])` and `query(...)` with a list literal as parameters, the list's length
is compared with the `?` count (E0325).

## R10. Time limits and locks

**Decision**: `sqlite3_progress_handler` every 1,000 virtual-machine steps spends `rt_budget` and calls
`rt_poll`, so `--timeout` stops a long statement (R0009 at the user's call). A busy handler retries a
locked database every 10 ms for up to 5 s, never past the deadline, then the statement returns
`err("busy: database is locked")`.

## R11. Recording and replay

**Decision**: a new event kind `sql` ([contracts/recording.md](contracts/recording.md)) per primitive
call: the operation (`open`, `exec`, `query`, `begin`, `commit`, `rollback`, `close`), the connection
number, the statement text and the parameters, and the outcome (rows, changes, or the `err` text).
Replay answers from the events in order and opens no file; a different operation, statement or
parameters is R0014. In-memory databases are recorded too (they are cheap, and replay then never runs
SQLite at all).

## R12. Verification scripts

- `tests/sqlite_corpus.py`: generates 500+ statements (every value type, casts between them, `printf`
  with float formats, math functions, aggregates, string functions, `ORDER BY` with ties broken),
  runs them through one vo program on each build and requires identical output; also compares the
  non-float results with Python's `sqlite3` module where its SQLite version agrees (a smoke check, not
  the criterion).
- `tests/sqlite_scope.py`: programs with scoped grants running `ATTACH`, `VACUUM INTO`, `PRAGMA
  journal_mode`, `PRAGMA temp_store_directory`, `load_extension` with paths inside and outside the
  grant (absolute, `..`, links swapped in): checks the reports and that no file outside appeared.
- `tests/sqlite_fuzz.py`: random statements through `parse_sql` and `query`, random parameter lists,
  and databases with random bytes flipped: every outcome `ok` or `err`; also under the instrumented
  build.
- `tests/sqlite_speed.py`: SC-002, in `make gate`.

## R13. Effect arguments in types (language)

**Decision**: a type argument list may end with `uses EFFECTS` (the same effect syntax as a `uses`
clause, scopes allowed): `Database[uses file.read, file.write]`, `Pair[int, str]` unchanged,
`Conn[str, uses console]`. It binds the type's single effect parameter (declared `[E: effects]` as
for functions, now accepted on records and variants for use, not only declaration); a type whose
only parameter is an effect parameter and that is written without brackets has the empty set
(`Database`). Effect sets in types compare by equality (same effects, same scopes); there is no
subtyping. Inference: from values (`open_readonly(...)` returns `Database[uses file.read]`), then the
expected type; an unbound effect parameter is the empty set (as `check_generic.c` already does for
functions). A generic function `fn f[E: effects](db: Database[E]) uses E` binds `E` from the argument's
type, and the existing effect-parameter machinery (`effects.c`, ES_EVARS) makes the caller need it, so
missing effects are E0501 with the call path, and `vo.effects` records them, with no new effect logic.

`Database[file.read]` (no `uses`) parses as a type argument named `file.read`, which is not a type: the
checker reports E0327 with the fix inserting `uses ` (instead of today's E0110 "Expected a type").

Printing (formatter, `vo doc`, hover, diagnostics) writes the canonical form: `uses` last, effects in
the order of the effect catalog, no brackets for a lone empty effect argument.

**Rationale**: the form already exists for function types (`fn(T) uses E`), so agents meet nothing new;
the checker already binds effect parameters from function-typed arguments, and a record argument is one
more source. No subtyping keeps type equality as it is (a read-only handle is not accepted where a
read-write one is expected: an explicit choice of handle, FR-005).

**Alternatives considered**: effects on every operation (in-memory tests would need file effects);
two handle types with separate function names (no overloading); effect sets with subtyping (a larger
change to type equality and inference, not needed by any current program).

## R14. Run-time re-check of a connection's effects

**Decision**: each connection slot records its effect bits and its path. Every `exec`, `query` and
transaction step checks, before running, that the running grant (the frame of the std wrapper, which
runs with its caller's grant, as every library function) covers those effects for that path, with
`rt_check_path` (FILE_READ, and FILE_WRITE for read-write connections); otherwise R0010 at the user's
call. In-memory connections have no bits and skip the check. Cost: one path check per statement
(cached per frame if `tests/sqlite_speed.py` shows it matters; ponytail).

**Rationale**: security property S2 (every effectful operation is checked at run time); a `Database`
record can be built by hand (`new Database { id: 3 }` has no effects), and the static check alone
would then not protect the file.

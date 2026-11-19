---

description: "Task list for vo-lang spec 026: SQLite Databases"
---

# Tasks: SQLite Databases

**Input**: Design documents from `specs/026-sqlite/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R12), data-model.md, contracts/library.md,
contracts/diagnostics.md, contracts/recording.md, contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's cases land first and fail before its code.

**Organization**:
- Setup: vendoring SQLite and the tooling around it.
- Foundational: SQLite built into the binary, the VFS, the connection table, the primitives and the
  core of `std/sqlite.vo` (in-memory databases, `exec`, `query`), which every story needs.
- One phase per user story: US1–US3 and US6 are P1 (US6 after US3 in the file), US4 and US5 are P2.
- The language change (effect arguments in types, research R13) is foundational: the library's
  signatures need it.
- Docs and verification.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases** go in `tests/cases/sqlite/` (run cases by default; check cases with `.cmd`
  `check --json {}`; test cases with `.cmd` `test {}`). **Step cases** (several runs, files, other
  processes) go in `tests/cases/sqlite_steps/<name>/steps`, as `tests/cases/sleep_steps/`.
- **Research items marked "verify"** (R2 long double, R3 libc, R6 `'now'`) are checked by the first task
  that builds or runs that part; the outcome goes into `results.md` (Deviations if a fallback was used).
- **SQLite is vendored code**: never edit `third_party/sqlite/*.c`/`*.h`; configuration is compile
  options and the VFS; a change would be a patch in `third_party/sqlite/patches/` with its reason.
- **GC safety**: C code that holds vo objects across allocations roots them (`GC_ROOT`); SQLite's own
  memory is its heap (R4), never the collected heap. Run the collector stress build over
  `tests/cases/sqlite` before marking a C task done (spec 025 found three such bugs this way).
- **Between edits**: `make quick AREA=sqlite`. `make test` once per platform at the end; never two
  suites at once.
- **Shortcuts**: `ponytail:` comments (the 64 MiB heap, 64 connections, 64 cached statements).

---

## Phase 1: Setup

- [X] T001 `tools/vendor_sqlite.sh` (as `tools/vendor_mbedtls.sh`): `tools/vendor_sqlite.sh VERSION` downloads
  the amalgamation zip of that release from sqlite.org, checks SQLite's published SHA3-256 for it
  (`sha3sum -a 256` or Python's `hashlib.sha3_256`), keeps `sqlite3.c` and `sqlite3.h` in
  `third_party/sqlite/`, and writes `third_party/sqlite/VENDOR.md` (version, URL, SHA3-256 of the
  archive, SHA-256 of each kept file, the public-domain dedication copied from the top of `sqlite3.c`,
  the kept files as a `- \`file\`` list, and the compile options of research R2); `--verify` checks the
  files against `VENDOR.md`. Run it with the newest 3.x release listed on `sqlite.org/download.html`.
- [X] T002 [P] Create `specs/026-sqlite/results.md` (SC-001 to SC-008, Verification, Size, Deviations), as
  `specs/025-regular-expressions/results.md`.
- [X] T003 [P] `tools/quick.sh`: area `sqlite` with cases `sqlite sqlite_steps lex doc` and scripts
  `apply_fixes.py`, `catalog.py`, `doc_conformance.py`, `"sqlite_scope.py --quick"`,
  `"sqlite_fuzz.py --count 1000"`; map `src/sqlite_*.c`, `src/check_sqlite.c`, `std/sqlite.vo`,
  `third_party/sqlite/*`, `tests/sqlite_*.py` to it, placed before the `src/check*.c` line.
  `tests/apply_fixes.py`: add `sqlite`. `tests/catalog.py`: add `"026-sqlite"` after
  `"025-regular-expressions"`.

---

## Phase 2: Foundational (blocks every story)

- [X] T004 [P] Golden run case `tests/cases/sqlite/memory_basic`, failing until T016: `open_memory()`, a
  `create table`, three inserts with every `Value` case as parameters, a `select ... order by`, printing
  each row's values through `match` on `Value`, and `Changes` of an insert (`changed` 1, `last_id` 3).
- [X] T005 [P] Golden cases for effect arguments in types (research R13), failing until T013–T014:
  `tests/cases/generic/effect_args.vo` (run: `record Conn[E: effects] { id: int; }`, `fn ping[E:
  effects](c: Conn[E]) uses E`, called with `new Conn[uses console] { id: 1 }` from `main` with
  `console`; a `Conn` with no effects passed to a function without `uses`; a field of type
  `Conn[uses console]` in another record), `tests/cases/generic/effect_args_missing` (check: calling
  `ping` with a `Conn[uses console]` from a function without `console` is E0501 with the call path),
  `tests/cases/generic/effect_args_syntax` (check: `Conn[console]` → E0327 with the fix `Conn[uses
  console]`; `Pair[uses console, int]` (not last) → E0327; `Pair[int, uses console]` for a type without
  an effect parameter → E0327 "has no effect parameter"; `Conn[uses console]` where `Conn[uses net]` is
  expected → E0301 naming both types), `tests/cases/fmt/effect_args` (the formatter prints `uses` last,
  effects in catalog order, no brackets for `Conn` without effects).
- [X] T006 `Makefile`: build `third_party/sqlite/sqlite3.c` (object list from `VENDOR.md` as for Mbed TLS
  and CORE-MATH) with the compile options of research R2 ("`SQLITE_OS_OTHER=1`, `SQLITE_THREADSAFE=0`,
  `SQLITE_OMIT_LOAD_EXTENSION`, `SQLITE_OMIT_WAL`, `SQLITE_TEMP_STORE=3`, `SQLITE_ENABLE_MEMSYS5`,
  `SQLITE_ZERO_MALLOC`, `SQLITE_DEFAULT_MEMSTATUS=0`, `SQLITE_OMIT_DEPRECATED`,
  `SQLITE_OMIT_SHARED_CACHE`, `SQLITE_UNTESTABLE`, `SQLITE_DQS=0`, `SQLITE_OMIT_LOCALTIME`,
  `SQLITE_ENABLE_MATH_FUNCTIONS`, `SQLITE_DEFAULT_FOREIGN_KEYS=1`", plus the double-only floating-point
  switch, `-ffp-contract=off`, the freestanding flags of the other vendored code, upstream warnings not
  as errors), for every architecture and the instrumented build. **verify R2**: find in the vendored
  `sqlite3.c` how the `long double` path is chosen (search `LONGDOUBLE_TYPE`, `UseLongDouble`,
  `hasHighPrecisionDouble`) and select the double-only path by a compile option or `sqlite3_config`/
  test-control at start-up; record which in `VENDOR.md` and `results.md`.
- [X] T007 libc for SQLite (research R3): link, list the undefined symbols, implement each in `src/vo_c.c`
  (declared in `third_party/shim/` headers); **verify R3** and record the list in `results.md`. `make`
  and `make ARCH=aarch64` and `make ARCH=riscv64` build.
- [X] T008 `src/sqlite_vfs.c` (new, research R5): the `vo` VFS and `sqlite3_os_init`/`sqlite3_os_end`;
  every method of R5's table. `xOpen` checks with `rt_check_path(bit, path)` (bit `file.write` when
  SQLite asks for `SQLITE_OPEN_READWRITE` or `SQLITE_OPEN_CREATE`, else `file.read`) and opens with
  `rt_open_file` (src/rt_effects.c); a nameless (temporary) open returns `SQLITE_CANTOPEN`; `xDelete` and
  `xAccess` go through `rt_open_parent`; locks with `fcntl(F_SETLK)` on the file descriptor, as
  SQLite's Unix VFS uses the lock bytes (`PENDING_BYTE`, `RESERVED_BYTE`, `SHARED_FIRST`, `SHARED_SIZE`);
  `xRandomness` writes a fixed pattern; `xSleep` sleeps with `nanosleep`; `xCurrentTime` and
  `xCurrentTimeInt64` return `SQLITE_ERROR`. The VFS never calls libc.
- [X] T009 `src/sqlite_rt.c` (new): start-up (`sqlite3_config(SQLITE_CONFIG_HEAP, region, 64 MiB, 64)` on
  a region reserved with `mmap` without touching its pages; `ponytail:` comment), the connection table of
  data-model.md ("64 slots; opening a 65th connection is `err("error: too many open databases (64)")`";
  each slot `db`, `pid`, `readonly`, `effects`, `path`, `in_tx`, `stmts`), closing every connection at
  program end on every exit path (normal end, a runtime error, `exit`, a timeout).
- [X] T010 `src/sqlite_rt.c`: values (data-model.md "Value mapping"): binding a `list[Value]` (read with the
  runtime's variant layout, case order `integer, real, text, blob, null`) and building `Value` variants
  from columns (TEXT that is not valid UTF-8 becomes `bytes`); prepared statements cached per connection
  by text (`sqlite3_prepare_v3` with `SQLITE_PREPARE_PERSISTENT`, 64 per connection, round-robin;
  `ponytail:`); error texts `KIND: MESSAGE` with KIND from the primary result code (`SQLITE_CONSTRAINT`
  → `constraint`, `SQLITE_BUSY`/`SQLITE_LOCKED` → `busy`, `SQLITE_READONLY` → `readonly`,
  `SQLITE_CORRUPT`/`SQLITE_NOTADB` → `corrupt`, `SQLITE_NOMEM`/`SQLITE_FULL` → `nomem`, else `error`);
  `last_id` by `sqlite3_set_last_insert_rowid(db, 0)` before each statement and
  `sqlite3_last_insert_rowid` after (data-model.md).
- [X] T011 Primitives (data-model.md "Primitives"): ids 87 `__sql_open` (`"si>n"`), 88 `__sql_close`
  (`"i>v"`), 89 `__sql_exec`, 90 `__sql_query`, 91 `__sql_tx` (`"ii>u"`) in `src/check_expr.c`
  (`builtin_names`, `builtin_sigs`, `B_COUNT`), `src/vo.h` (`B_SQL_*`, `B_LAST` 91), `src/rt_call.c`
  (enum, `_Static_assert`, dispatch), `src/check_effects.c` (all pure: the std wrappers declare the
  effects). `__sql_exec`/`__sql_query` are typed loosely in `check_builtin` like `B_COLL` (arguments
  checked as expressions, the result takes the expected type). A closed, forged or other-process id is
  R0007 at `std_user_call(call)`.
- [X] T012 `src/parse.c`: a type argument list may end with `uses EFFECTS` (the effect list of a `uses`
  clause: names, scopes, effect parameters), stored on the type node; `uses` anywhere but last is a
  parse of its own reported as E0327 by T013 (keep the node, no cascade).
- [X] T013 `src/types.c`, `src/check_generic.c`, `src/check.c`: records and variants with an effect
  parameter (`[E: effects]`, at most one per type) get the effect set of the `uses` argument (empty when
  absent, including the bare name of a type whose only parameter is an effect parameter); type equality
  compares effect sets (same effects, same scopes, no subtyping); inference binds a type's effect
  parameter from values and expected types like its type parameters, and binds a function's effect
  parameter from a `Name[E]` argument, so the existing effect-parameter checks (`src/effects.c`, ES_EVARS)
  make the caller need it (E0501, `vo.effects`). E0327 for effect names given as type arguments without
  `uses` (the fix inserts `uses `), for `uses` not last, and for effect arguments to a type without an
  effect parameter; the error replaces today's E0110 "Expected a type" for those inputs.
- [X] T014 `src/fmt.c`, `src/doc.c`, `src/query.c`, `src/check.c` (`ty_name`): print types with effect
  arguments in the canonical form of research R13 (`uses` last, catalog order, no brackets for an empty
  lone effect argument), in the formatter, `vo doc`, hover and diagnostics.
- [X] T015 `std/sqlite.vo` (new, a vo wrapper over the primitives as `std/regex.vo` is; header and doc comments per
  contracts/library.md; add to `STD_FILES` in `Makefile` and to `doc_modules` in `src/doc.c`): the types
  `Database[E: effects]`, `Statement`, `Value`, `Row`, `Changes`; `open_memory() -> Database`,
  `close[E: effects](db: Database[E])`, `sql` (no check yet), `exec[E: effects](db: Database[E], ...)
  uses E` and `query[E: effects](db: Database[E], ...) uses E` (first list = column names), `get`, `get_int`, `get_float`, `get_str`, `get_bytes`,
  `is_null` with the `err` texts of contracts/library.md. Every item gets a `vo doc` example that runs.
- [X] T016 Make T004 and T005 pass; `python3 tests/doc_conformance.py`; the stress build over `tests/cases/sqlite`.

**Checkpoint**: in-memory SQL works end to end on x86-64; the three architectures build.

---

## Phase 3: User Story 1 — Store and query data (P1) 🎯 MVP

**Goal**: files under scoped effects, typed getters, changes, error kinds.

**Independent Test**: the cases below.

- [X] T017 [P] [US1] Step case `tests/cases/sqlite_steps/inventory`: run 1 (granted `file.read("data/")`,
  `file.write("data/")`) creates `data/inventory.db`, a table and three rows; run 2 (a read-only program
  with `open_readonly` and only `file.read("data/")`) lists them; run 3 tries an insert through
  `open_readonly` and prints the `err` (`readonly: ...`).
- [X] T018 [P] [US1] Golden run cases in `tests/cases/sqlite/`: `getters` (each getter on each `Value` case:
  `ok` where the types agree, the contract's `err` texts otherwise; `get_float` of an `int` is an `err`;
  an unknown column); `null_values` (`is_null`, `get` returning `Value.null`, never `0` or `""`);
  `constraint` (a duplicate in a `UNIQUE` column: `err("constraint: UNIQUE constraint failed: ...")`, the
  program continues); `duplicate_columns` (`select a.id, b.id`: `get_int(row, "id")` is
  `err("ambiguous column id: name each column with AS")`, `AS` names work); `last_id` (an insert reports
  its row id, a following update reports 0); `foreign_keys` (an insert violating a declared
  `REFERENCES` is `err("constraint: FOREIGN KEY constraint failed")`); `double_quotes` (`select "nme"
  from t` with a misspelled column is `err("error: no such column: nme")`, not the string); `no_table` (`error: no such table: ...`); `invalid_utf8` (a blob cast to text read
  with `get_str` is an `err`, with `get_bytes` works).
- [X] T019 [US1] `std/sqlite.vo`: `open(path) -> result[Database[uses file.read, file.write]] uses file.read,
  file.write` (mode 0) and `open_readonly(path) -> result[Database[uses file.read]] uses file.read`
  (mode 1); `src/sqlite_rt.c`: modes 0 and 1
  (`SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE` or `SQLITE_OPEN_READONLY`, always with the `vo` VFS and
  `SQLITE_OPEN_EXRESCODE`); `open(":memory:")` at run time is `err("error: use open_memory() for an
  in-memory database")`.
- [X] T020 [US1] Make T017 and T018 pass.

**Checkpoint**: T017–T018 pass.

---

## Phase 4: User Story 2 — SQL mistakes are caught before the program runs (P1)

**Goal**: E0323–E0326 at check time, spans inside the literal, fixes.

**Independent Test**: check cases; `tests/apply_fixes.py` re-checks each fix.

- [X] T021 [P] [US2] Golden check cases in `tests/cases/sqlite/` (`.cmd` `check --json {}`), messages from
  contracts/diagnostics.md: `syntax` (`selec`, an unclosed string, `#`, incomplete input), `two_statements`
  (with the split fix when the call is a whole `try exec(...)` statement; without it inside an
  expression), `not_literal` (a variable and a `+` expression), `named_params` (`:id`, `@id`, `$id`,
  `?1 ... ?2` in order: fixes; a name used twice: no fix), `param_count` (list literal of the wrong
  length in `exec` and `query`), `random_now` (`random()`, `randomblob(4)`, `current_timestamp`,
  `datetime('now')`), `pragmas` (`pragma temp_store_directory = '/tmp'`, `pragma journal_mode = WAL`:
  E0323 "PRAGMA ... is not available in vo"; `pragma foreign_keys = off`: no diagnostic), `memory_path`
  (`open(":memory:")`, `open_readonly(":memory:")` with fixes),
  `no_schema_check` (a missing table: no diagnostic), `one_pass` (several mistakes and an unrelated
  type error, all reported).
- [X] T022 [P] [US2] `docs/errors.md`: E0323 `sql_invalid`, E0324 `sql_not_literal`, E0325 `sql_parameters`,
  E0326 `sqlite_memory_path`, E0327 `effect_argument_syntax` after E0322, with paragraph, `Fixable:`
  (sometimes / no / sometimes / yes / sometimes),
  Example and Fixed; `contracts/codes.md` already lists them. `python3 tests/catalog.py` passes after T021.
- [X] T023 [P] [US2] Golden run case `tests/cases/sqlite/parse_sql`: `parse_sql` of the bad SQL of T021
  gives the same messages as `err`; a good one runs.
- [X] T024 [US2] `src/sqlite_rt.c`: the authorizer (research R6) on every connection and on the checker's
  scratch connection: `SQLITE_FUNCTION` for `random`/`randomblob` → `SQLITE_DENY` with the E0323
  message kept for the error text; `SQLITE_PRAGMA` for `temp_store_directory`, `data_store_directory`
  and `journal_mode` with the argument `wal` (any case) → `SQLITE_DENY` with "PRAGMA NAME is not available
  in vo" (research R6). **verify R6**: run `select datetime('now')` and `select
  current_timestamp`; if either is not an error, apply R6's fallback; record the outcome.
- [X] T025 [US2] `src/check_sqlite.c` (new, research R9): the SQL tokenizer (strings, quoted identifiers,
  comments, top-level `;`, `?`, `:a @a $a ?N`); `sql(...)` checks (E0324; E0323 for more than one
  statement with the split fix; E0325 with fixes; prepare on a scratch in-memory connection opened once
  per check, classifying errors: syntax, incomplete input, unrecognized token and authorizer refusals
  are E0323 at `sqlite3_error_offset` mapped with `lit_decode`, schema errors ignored); `exec`/`query`
  with a list literal: E0325 count; `open`/`open_readonly` with the literal `":memory:"`: E0326 and fix.
  Called from `check_call` for functions of the std file `sqlite.vo` (as `check_regex_call`); declared
  in `src/vo.h`.
- [X] T026 [US2] Primitive 92 `__sql_prepare(text) -> result[str]` (`"s>r"`, in the same files as T011;
  `B_LAST` 92): prepares on a scratch in-memory connection with the authorizer, applies the tokenizer's
  one-statement and parameter-style rules, and returns `ok("")` or `err` with the E0323/E0325 message;
  `std/sqlite.vo`: `parse_sql` over it. Make T021–T023 pass; `python3 tests/apply_fixes.py`.

**Checkpoint**: T021–T023 pass; catalog clean; fixes resolve.

---

## Phase 5: User Story 3 — Tests use a real database with no setup (P1)

**Goal**: in-memory databases in `vo test`, isolated per test, deterministic.

- [X] T027 [P] [US3] Golden test cases `tests/cases/sqlite/test_memory` (`.cmd` `test --json {}`): two tests
  each creating the same table in `open_memory()` and inserting different rows (each sees only its own),
  a test that stops with a runtime error mid-way followed by one that passes, no `uses` clause anywhere.
- [X] T028 [US3] `src/sqlite_rt.c` and the test runner (`src/testrun.c`): connections opened during a test
  (or property case) are closed when it ends, however it ends; run T027 20 times
  (`tests/run.sh --repeat 20 tests/cases/sqlite/test_memory.vo`) with identical output.

**Checkpoint**: T027 passes 20 times.

---

## Phase 6: User Story 6 — Effects follow the database (P1)

**Goal**: a database handle never lets a function without file effects touch a file (FR-014–FR-016).

**Independent Test**: check cases for every path a handle can take; a run case for a hand-built handle.

- [X] T029 [P] [US6] Golden check cases `tests/cases/sqlite/effects_*` (`.cmd` `check --json {}`), each a
  function without `uses` that reaches a file database's `query` or `exec` and must get E0501 naming
  `file.read`/`file.write` and the call path: `effects_param` (a `Database[uses file.read, file.write]`
  parameter), `effects_field` (a record field holding one), `effects_list` (a list element),
  `effects_closure` (captured by an anonymous function passed to `map`), `effects_return` (returned
  from a helper and used); plus `effects_memory` (the same shapes with `Database`: no diagnostic),
  `effects_readonly` (an `open_readonly` result where `Database[uses file.read, file.write]` is
  expected: E0301 naming both types). SC-009.
- [X] T030 [P] [US6] Golden run case `tests/cases/sqlite/forged_handle` and step case
  `tests/cases/sqlite_steps/effects_record`: a program opens a file database in `main` (with file
  effects), then a function without file effects builds `new Database { id: 0 }` and queries it: R0010
  at the call; `vo effects --update` on a program using a file database lists the file effects of the
  functions that query it.
- [X] T031 [US6] `src/sqlite_rt.c` (research R14): before every `exec`, `query` and transaction step,
  check the connection's effects and path against the running grant with `rt_check_path`
  (`file.read`, plus `file.write` for read-write connections); R0010 at the user's call otherwise;
  in-memory connections skip it. `ponytail:` comment if a per-frame cache is added after T042.
- [X] T032 [US6] Make T029 and T030 pass.

---

## Phase 7: User Story 4 — Transactions (P2)

- [X] T033 [P] [US4] Golden run cases `tests/cases/sqlite/transaction` (commit, `err` midway rolled back and
  returned, nested `err`) and step case `tests/cases/sqlite_steps/tx_crash` (a run stops with R0007
  inside a transaction on a file database; the next run sees no change).
- [X] T034 [US4] `std/sqlite.vo`: `transaction[T, E: effects, F: effects](db: Database[E], f: fn() ->
  result[T] uses F) -> result[T] uses E, F` over `__sql_tx` (1 begin, 2 commit, 3 rollback); `src/sqlite_rt.c`: `in_tx` per
  connection, nested begin is `err("error: a transaction is already open")`. Make T033 pass.

---

## Phase 8: User Story 5 — Safe, scoped and replayable (P2)

- [X] T035 [P] [US5] `tests/sqlite_scope.py` (research R12): programs with `file.read("data/")` and
  `file.write("data/")` running `attach database` and `vacuum into` with paths inside, absolute, with
  `..`, and through a link swapped in after the check; `pragma journal_mode=wal`,
  `pragma temp_store_directory='/tmp'`, `pragma data_store_directory='/tmp'`, `select
  load_extension('x')`, and the allowed `pragma journal_mode=memory`, `=off`, `=truncate`. Each outside
  path stops with R0010 at the user's call; the three refused pragmas are `err("error: PRAGMA NAME is not
  available in vo")`; `load_extension` is `error: no such function`; no file appears outside `data/`.
  `--quick` runs a subset.
- [X] T036 [P] [US5] Golden and step cases: `tests/cases/sqlite/corrupt` (a file of random bytes and a
  truncated database: `open`/`query` give `err("corrupt: ...")`, no crash); `tests/cases/sqlite_steps/
  replay` (record a run that writes and reads `data/app.db`, delete the file, `vo replay` gives identical
  output; a changed statement gives R0014); `tests/cases/sqlite_steps/locked` (a second process holds a
  write lock: `busy: database is locked` after the bounded wait); `tests/cases/sqlite/tasks` (a
  `Database` used inside `concurrent_map`: R0007 "Open the database inside the task.").
- [X] T037 [US5] `src/replay.c` and `src/sqlite_rt.c`: the `sql` event of contracts/recording.md
  (record every primitive call's operation, connection, path, statement, parameters and outcome; replay
  answers from the events in order without calling SQLite; mismatches are R0014 naming the field).
- [X] T038 [US5] `src/sqlite_rt.c`: the progress handler (every 1,000 virtual-machine steps: `rt_budget`,
  `rt_poll`) and the busy handler (10 ms steps, at most 5 s, never past `deadline_ms`) of research R10;
  a golden case `tests/cases/sqlite/timeout` (`.cmd` `run --timeout 1 {}`: a recursive CTE counting to
  10^12 stops with R0009 at the call, exit 124).
- [X] T039 [US5] Make T035–T038 pass.

---

## Phase 9: Polish and verification

- [X] T040 [P] `tests/sqlite_corpus.py` (research R12): 500+ statements over every value type, casts, `printf`
  with float formats (`%.17g`, `%f`, `%e`), math functions, aggregates and string functions; one vo
  program per build; `--cross BIN...` requires identical output on all three (SC-001); a smoke comparison
  of the non-float results with Python's `sqlite3`.
- [X] T041 [P] `tests/sqlite_fuzz.py` (SC-007): 10,000 random statements through `parse_sql` and `query`
  with random parameter lists, and 200 database files with random bytes flipped, opened and queried;
  every outcome `ok` or `err`; `--count N`.
- [X] T042 [P] `tests/sqlite_speed.py` (SC-002): 100,000 inserts in one `transaction` under 2 s, reading
  them back under 1 s, median of 3; add to `make gate` in `Makefile`.
- [X] T043 [P] Docs: `docs/reference.md` std table row and the Generics bullet for effect arguments
  (contracts/library.md "Reference text"; `tests/ref_examples.py`; ≤250 tokens in all, SC-008); `site/docs/guide/standard-library.md` `## std.sqlite` section with a runnable example
  (`open_memory`) and output; `docs/security.md` S2 and S3 (SQL file access through vo's checks, no
  extensions) and S4/S5 (SQLite's heap limit); `docs/roadmap.md` (spec 026 Done, a section with "Not
  done": PostgreSQL, streaming rows, named parameters, scripts, WAL, ORM; and, as for Mbed TLS, the
  standing obligation to apply SQLite security releases in their spec cycle, with the link to
  `sqlite.org/cves.html` also in `third_party/sqlite/VENDOR.md`); `README.md` (status, third
  vendored library); `third_party/shim/README.md` (SQLite's functions).
- [X] T044 Golden expectations changed by the new module (`tests/cases/doc/all*.out`): review and accept.
- [X] T045 `make lint`; `make instrumented` (whole golden suite), then `VO_BIN=build-instrumented/vo` for
  `tests/sqlite_fuzz.py --count 2000` and `tests/sqlite_scope.py --quick`.
- [X] T046 The collector stress build: `tests/run.sh --stress 64 tests/cases/sqlite`.
- [X] T047 `make ARCH=aarch64`, `make ARCH=riscv64`; `python3 tests/sqlite_corpus.py --cross build/vo
  build-aarch64/vo build-riscv64/vo`; record the binary size growth of each build (SC-008: at most 1.5
  MB) in `results.md`.
- [X] T048 `make test` gains `"sqlite_scope.py --quick"` and `"sqlite_fuzz.py --count 1000"`; every
  `ponytail:` comment of this change names its ceiling and upgrade.
- [X] T049 `make test` on x86-64 (make's own exit status), then `make ARCH=aarch64 test` and `make
  ARCH=riscv64 test`, one at a time.
- [X] T050 Fill in `results.md`; spec status Implemented; every task [X].

---

## Dependencies & Execution Order

- **Phase 1**: T001 first (everything builds on the vendored files); T002 and T003 in parallel with it.
- **Phase 2**: T004 and T005 (failing cases) any time; T006 → T007 (build, libc) → T008 (VFS) → T009 →
  T010 → T011 (SQLite runtime) and, independently, T012 → T013 → T014 (effect arguments in types);
  both lines join at T015 (`std/sqlite.vo` needs `Database[E]`) → T016. Blocks every story.
- **US1** (T017–T020), **US2** (T021–T026), **US3** (T027–T028), **US6** (T029–T032): after Phase 2,
  independent of each other; US6's T031 is also needed before US5's scope script (T035).
- **US4** (T033–T034): after Phase 2.
- **US5** (T035–T039): after US1 (file databases), T024 (authorizer) and T031 (run-time re-check).
- **Polish**: T040–T043 in parallel after the stories; T044 → T045 → T046 → T047 → T048 → T049 → T050.

## Parallel Examples

- Setup: T002 and T003 while T001 downloads and pins SQLite.
- Phase 2: the SQLite line (T006–T011) and the language line (T012–T014) by two people or one after the
  other; T004 and T005 written first.
- After Phase 2: T017, T018 (US1), T021, T022, T023 (US2), T027 (US3), T029, T030 (US6), T033 (US4)
  written together.
- US5: T035 and T036 together, then T037 → T038 → T039.
- Polish: T040, T041, T042 and T043 together.

## Implementation Strategy

- **MVP**: Phases 1–3: SQLite in the binary, file and in-memory databases, typed rows.
- **Then**: US2 (checked statements: the reason for `sql("...")`), US6 (effects follow the handle: the
  effect system holds for databases) and US3 (tests).
- **Then**: US4 (transactions) and US5 (scope, replay, time limits, corruption).
- **Last**: cross-platform corpus, fuzzing, speed, docs, the three platforms.

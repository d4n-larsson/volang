# Implementation Plan: SQLite Databases

**Branch**: `026-sqlite` (work on `main`, as for specs 001–025) | **Date**: 2026-11-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/026-sqlite/spec.md`

## Summary

`std.sqlite` over a vendored SQLite (the amalgamation, built into the binary). SQLite runs with its own
OS layer replaced by a small vo file layer (`src/sqlite_vfs.c`): every file it opens goes through
`rt_check_path` and `rt_open_file`, the same scoped, link-safe opening as `std.fs` (spec 013), so SQL
can name no file outside the program's grant. Temporary storage is in memory, extensions and WAL are
compiled out, clock and random SQL functions are refused, and floating-point text is forced onto
SQLite's double-only path so results are bit-identical on the three platforms. Statements are
`sql("literal")`, checked at check time by preparing them against an empty in-memory database inside
the checker (`src/check_sqlite.c`). Statement results are recorded and replayed like HTTP responses.

A database handle's type names the effects it was opened with: `Database[uses file.read, file.write]`,
`Database[uses file.read]`, or `Database` for an in-memory one, and every operation needs them
(`exec[E: effects](db: Database[E], ...) uses E`). This is a small language change made here (research
R13): records and variants could already declare `[E: effects]` and use `E` in signatures; what is new
is writing a concrete effect set as a type argument, after `uses`, as function types do. At run time
each statement re-checks the connection's effects and path against the running grant (R14), so a
hand-built handle cannot bypass the static check.

| Part | File | What |
|---|---|---|
| Vendored engine | `third_party/sqlite/` | `sqlite3.c`, `sqlite3.h` of a pinned release, `VENDOR.md` (version, SHA3-256 and SHA-256, the public-domain dedication, kept files) |
| Build | `Makefile` | the amalgamation with the compile options of research R2, per architecture and in the instrumented build |
| libc shim | `third_party/shim/`, `src/vo_c.c` | the few C functions SQLite needs that vo does not provide yet (research R3) |
| File layer | `src/sqlite_vfs.c` (new) | the "vo" VFS: open/read/write/sync/lock/delete through vo's scoped file access; time and randomness hooks |
| Connections and primitives | `src/sqlite_rt.c` (new) | handle table, `__sql_open`, `__sql_close`, `__sql_exec`, `__sql_query`, `__sql_tx`, values in and out, authorizer, progress handler, busy handler, record/replay events |
| Check-time SQL | `src/check_sqlite.c` (new) | `sql(...)`, `exec`/`query` parameter counts, `open(":memory:")` |
| Library | `std/sqlite.vo` (new) | `Database`, `Statement`, `Value`, `Row`, `Changes`; `open`, `open_readonly`, `open_memory`, `close`, `sql`, `parse_sql`, `exec`, `query`, `transaction`, the row getters |
| Recording | `src/replay.c` | event kind `sql` (contracts/recording.md) |
| Effect arguments in types | `src/parse.c`, `src/types.c`, `src/check_generic.c`, `src/check.c`, `src/fmt.c`, `src/doc.c`, `src/query.c` | `Name[T, uses EFFECTS]`: parsing, the effect set as the binding of the type's effect parameter, inference, equality, E0327, printing (research R13) |

## Technical Context

**Language/Version**: C (C11, freestanding) for the runtime, file layer and checks; vo for
`std/sqlite.vo`; SQLite's C sources vendored unmodified.

**Primary Dependencies**: SQLite (public domain, constitution 1.5.0), the latest 3.x release when T001
runs (pinned; research R1).

**Storage**: SQLite database files in the program's file scopes; rollback journal next to each
database; nothing else on disk.

**Testing**:
- golden cases `tests/cases/sqlite/` (run, check with JSONL and fixes, `vo test` cases);
- step cases `tests/cases/sqlite_steps/` (a second run on the same database file, a read-only program,
  record and replay with the file deleted, a database locked by another process);
- `tests/sqlite_corpus.py`: 500+ statements over every value type and text conversion, compared across
  the three builds (SC-001);
- `tests/sqlite_scope.py`: file-naming SQL never touches a file outside the grant (SC-004);
- `tests/sqlite_fuzz.py`: random statements, random parameters, corrupt database files (SC-007);
- `tests/sqlite_speed.py`: 100,000 inserts and a full read-back (SC-002, `make gate`);
- `tests/apply_fixes.py`, `tests/catalog.py`, `tests/doc_conformance.py`, `tests/ref_examples.py`;
- `make quick AREA=sqlite` between edits, `make test` once per change.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: SC-002 (100,000 inserts in one transaction under 2 s; reading them under 1 s).

**Constraints**:
- Freestanding: SQLite gets no libc; its OS layer is replaced (`SQLITE_OS_OTHER`), its memory comes
  from SQLite's own allocator over one reserved region (research R4).
- Principle III: double-only floating point in SQLite (no `long double`), `-ffp-contract=off`; the math
  functions call CORE-MATH; no clock, no randomness, no locale.
- Security: SQLite is reachable from hostile input in three ways (SQL text from data, parameters, the
  database file itself); fuzzing and the instrumented build cover all three.
- Binary size: at most +1.5 MB (SC-008).

**Scale/Scope**:
- vendored: SQLite amalgamation (~260,000 lines, unmodified);
- C of our own: about 1,100 lines (VFS ~350, runtime ~500, checks ~200, shim ~50);
- vo: `std/sqlite.vo` about 250 lines with documentation;
- 5 catalog codes (E0323–E0327); about 50 golden and 6 step cases; 4 scripts;
- language: effect arguments in types, about 250 lines across the parser, type representation, generic
  inference, formatter and printers.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate | Status |
|---|---|
| I. Agent-first (language change) | PASS. Effect arguments have one form, the one function types already use (`uses` last); the bare name means no effects, as a function type without `uses` is pure; the form without `uses` is an error with a fix (E0327). |
| I. Agent-first | PASS. One parameter form (`?`), one statement per `sql`, statements are literals (string-built SQL cannot be written by accident), no implicit conversions between column types and vo types (getters return `err`), explicit read-only and read-write opens. |
| II. Machine-readable feedback | PASS. Four new codes, with spans inside the SQL literal from SQLite's own error offset, fixes for named parameters and `open(":memory:")`. |
| III. Determinism and defined behavior | PASS. No clock or randomness in SQL; double-only floating point and CORE-MATH; temporary storage in memory; `--timeout` reaches long statements through SQLite's progress callback; every database failure is an `err`; a corrupt file is an `err`. Verified by the cross-platform corpus. |
| IV. Test-first | PASS. Golden and step cases first; the corpus, scope and fuzz scripts. |
| V. Minimal surface | PASS. One std module, one reference row, and one Generics bullet for effect arguments (≤250 tokens in all, SC-008). The language change reuses the existing effect parameters and the `uses` syntax of function types; no subtyping between effect sets. YAGNI: no streaming, no named parameters, no scripts, no ORM, no WAL. |
| Implementation constraints (vendoring, 1.5.0) | PASS. A pinned release with checksums, the public-domain dedication and the kept files in `VENDOR.md`; files unmodified (configuration through compile options and the VFS); built freestanding with `-ffp-contract=off` and no assembly; passes the instrumented build; security releases applied in their cycle. |
| Implementation constraints (C) | PASS. vo's own C under the strict warning set; the vendored file with its upstream warnings (as Mbed TLS and CORE-MATH). |

Post-design re-check: unchanged (all PASS).

## Project Structure

### Documentation (this feature)

```text
specs/026-sqlite/
├── plan.md              # this file
├── research.md          # R1–R12
├── data-model.md        # types, handles, value encoding, limits
├── quickstart.md        # validation scenarios
├── contracts/
│   ├── library.md       # std.sqlite: types, functions, effects, errors, examples
│   ├── diagnostics.md   # E0323–E0326 and run-time reports
│   ├── recording.md     # the `sql` recording event
│   └── codes.md         # catalog contract (tests/catalog.py)
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
third_party/sqlite/      # sqlite3.c, sqlite3.h, VENDOR.md
third_party/shim/        # any added declarations SQLite needs
src/sqlite_vfs.c         # new: the vo VFS
src/sqlite_rt.c          # new: connections, primitives, authorizer, progress, recording
src/check_sqlite.c       # new: check-time SQL
src/check_expr.c, src/vo.h, src/rt_call.c, src/check_effects.c   # primitive ids 87–92
src/replay.c             # event kind `sql`
src/parse.c, src/types.c, src/check_generic.c, src/check.c, src/fmt.c, src/doc.c, src/query.c   # effect arguments in types
src/doc.c                # module list
std/sqlite.vo            # new module
Makefile                 # sqlite objects, STD_FILES, gate step
docs/reference.md, docs/errors.md, docs/roadmap.md, docs/security.md, README.md
site/docs/guide/standard-library.md
tests/cases/sqlite/, tests/cases/sqlite_steps/
tests/sqlite_corpus.py, tests/sqlite_scope.py, tests/sqlite_fuzz.py, tests/sqlite_speed.py
tests/apply_fixes.py, tests/catalog.py, tools/quick.sh
```

**Structure Decision**: three new C files, split by concern (the file layer is security-critical and
reviewed on its own; the checker part runs at check time only). The library follows the std pattern of
vo wrappers over `__` primitives.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| A large vendored C library (~260,000 lines) | A correct SQL engine with durable transactions is the feature; SQLite is the most tested one and embeddable in C | Writing a database: years of work; a client for a server database: needs TCP, a server for every test, and gives up the in-memory test story |
| A custom SQLite OS layer | Every file SQLite touches must pass vo's grant checks, link-safe (spec 013), with no libc | SQLite's Unix VFS: libc, its own path handling (escapes scopes through `ATTACH`), real temp files |
| A language change (effect arguments in types) inside a library spec | A database handle must not let a function without `uses` touch a file (the effect system's promise, security property S1); the handle's type has to carry its effects | Effects on every operation regardless of the handle (in-memory tests would need file effects); two handle types with separate function names (no overloading: `mem_query` and `query` for one concept) |
| The checker runs SQLite | Syntax errors in a literal statement must be check errors with spans (Principle II) | Checking at run time only: against Principle II; a separate SQL parser: drifts from SQLite's grammar |

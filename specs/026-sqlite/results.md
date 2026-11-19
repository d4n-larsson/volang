# Results: SQLite Databases

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | 20 golden cases in `tests/cases/sqlite` and 4 step cases in `tests/cases/sqlite_steps`, run by `make test` on each platform; `tests/sqlite_corpus.py --cross build/vo build-aarch64/vo build-riscv64/vo`: 1,153 statements (every value type, casts, `printf` with `%.17g`/`%f`/`%e`, all math functions, aggregates, window functions, JSON, dates, string functions), floats compared by their exact bits, identical on the three builds |
| SC-002 | met | `tests/sqlite_speed.py` (in `make gate`): 100,000 inserts in one transaction into a file 0.11 s (limit 2), reading them back 0.08 s (limit 1), median of 3, process start included |
| SC-003 | met | `tests/cases/sqlite/sql_check` (E0323 syntax, second statement, `'now'`, clock, randomness, refused pragmas; E0324; E0325 named/numbered parameters with fixes and count mismatches; E0326 with its fix), `tests/cases/generic/effect_args_*` (E0327); `tests/apply_fixes.py` applies every fix and re-checks |
| SC-004 | met | `tests/sqlite_scope.py`: `attach database` and `vacuum into` with 6 inside paths (relative, absolute, `.`, `..` back inside, a link inside) are `ok`; 8 outside paths (relative, `..`, absolute, `/tmp`, a link to a file outside, a link to a directory outside) stop with R0010 at the user's call; WAL and the two directory pragmas are refused, `load_extension` does not exist, the four allowed journal modes work; no file outside `data/` appears or changes. `--quick` is in `make test` |
| SC-005 | met | `tests/cases/sqlite/test_memory` (`vo test`, no `uses` anywhere, a runtime error mid-test, a property of 100 cases each opening a database on a 64-slot table): `tests/run.sh --repeat 20` identical |
| SC-006 | met | `tests/cases/sqlite_steps/replay`: recorded, database file deleted, `vo replay` identical; a changed statement and changed parameters are R0014 |
| SC-007 | met | `tests/sqlite_fuzz.py`: 10,000 statements (token soup and mutations, random parameter lists) and 200 damaged database files: every outcome `ok` or `err`, also on the instrumented build (`--count 2000`); `--count 1000` is in `make test` |
| SC-008 | met | reference +2 lines, about 120 tokens (13,037 of 15,000 in all); binary growth below |
| SC-009 | met | `tests/cases/sqlite/effects_{param,field,list,closure,return}` (E0501 with the fix adding `file.read, file.write`), `effects_memory` (none), `effects_readonly` (E0301 naming both types); `forged_handle` (R0010 at run time for `new Database { id: 0 }`); `sqlite_steps/effects_record` (`vo effects --update` lists the file effects) |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | exit 0: 842 golden cases, every script (with `sqlite_scope.py --quick`, `sqlite_fuzz.py --count 1000` and `sqlite_corpus.py` added); the first run found `doc_conformance.py` probing `sql("a")` (not SQL), fixed with a valid sample |
| ARM64, RISC-V | `make ARCH=aarch64 test` and `make ARCH=riscv64 test` (under qemu), one at a time: exit 0, 842 golden cases each, the sqlite scripts as on x86-64 |
| `make lint` | clean; now also verifies SQLite's kept files (`tools/vendor_sqlite.sh --verify`) |
| `make instrumented` | 842 golden cases pass; `sqlite_fuzz.py --count 2000`, `sqlite_scope.py --quick`, the corpus and the sqlite step cases pass on `build-instrumented/vo` |
| collector stress | `tests/run.sh --stress 64 tests/cases/sqlite` 20 of 20; sqlite and replay step cases with `VO_GC_STRESS=16`; the corpus with `VO_GC_STRESS=64` |

## Research items verified

- **R1**: SQLite 3.54.1 (`3540100`), the amalgamation from `sqlite.org/2026/`, SHA3-256
  `92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30` as published; kept files and their
  SHA-256 in `third_party/sqlite/VENDOR.md` (`tools/vendor_sqlite.sh --verify`).
- **R2 (double-only floating point)**: nothing to switch. SQLite 3.54.1 no longer uses `long double`
  anywhere (its `SQLITE_TESTCTRL_USELONGDOUBLE` is marked "NOT USED"); text-to-float (`sqlite3AtoF`) and
  float-to-text (`sqlite3FpDecode`) use 64-bit integer arithmetic, with no `fma` and no 128-bit types,
  so `-ffp-contract=off` is all that identical results need (verified by the corpus, SC-001).
- **R2 (math functions)**: `SQLITE_ENABLE_MATH_FUNCTIONS` needs `log10`, `log2` and the six hyperbolic
  functions beyond spec 020's CORE-MATH subset. They were added from CORE-MATH at the same pinned commit
  (`tools/vendor_core_math.sh`, 9 more files, no inline assembly, no new patch), so SQL math is
  correctly rounded and identical everywhere, as `std.math`. `fabs`, `trunc`, `fmod` and `sqrt` are the
  exact operations (`src/sqlite_libc.c`).
- **R3 (libc)**: `memchr`, `strncmp`, `strrchr`, `strspn`, `strcspn` and the ASCII `ctype.h` functions
  (`src/sqlite_libc.c`, declarations in `third_party/shim/`); the sanitizer build uses the C library's.
- **System calls**: the file layer adds `pread64`, `pwrite64`, `fsync` and `ftruncate` (43 per platform);
  locks use `fcntl` with open-file-description locks (`F_OFD_SETLK`), file sizes `statx`, waits `ppoll`.

## Size

Stripped binaries, this change against the last commit (everything in it: SQLite, its OS layer and
runtime, the checker, the nine added CORE-MATH functions):

| Build | Before | After | Growth |
|---|---|---|---|
| x86-64 | 1,110,904 | 2,111,480 | 1,000,576 (0.95 MiB) |
| ARM64 | 1,180,520 | 2,242,344 | 1,061,824 (1.01 MiB) |
| RISC-V 64 | 1,328,296 | 2,504,800 | 1,176,504 (1.12 MiB) |

## Deviations

- **Effect arguments in types** (analysis C1, decided by the user): the database's effects are part of
  its type, `Database[uses file.read, file.write]`, a language change (parser, generic instances,
  canonical printing, `vo fmt`, E0327) instead of a run-time-only check. The run-time re-check (R14)
  stays, for handles built by hand; it checks the effect bits (`rt_check_effect`) as well as the path
  scopes, since `rt_check_path` alone passes an unscoped grant.
- **`Value` case names** are `integer`, `real`, `text`, `blob`, `null`: `int`, `float` and `bytes` are
  type keywords.
- **Lock case**: `tests/cases/sqlite/locked` is a run case with two connections to one file in one
  program, not a step case with a second process. Locks are open-file-description locks, so two
  connections conflict exactly as two processes do, and the case is deterministic; it waits the full
  bounded 5 s.
- **Recording encoding**: floats are written as their IEEE-754 bits (16 hex digits) rather than a hex
  float, bytes as strings with `\u00XX` escapes (as every other recording field) rather than base64,
  and a `denied` outcome (`effect`, `target`) was added for files SQLite is refused inside a statement.
  Replay compares the call part of the line byte for byte. `contracts/recording.md` is updated.
- **Refused pragmas in `parse_sql`**: SQLite authorizes when it prepares, so `parse_sql("pragma
  journal_mode=wal")` already returns the refusal, in `parse_sql`'s format (no `error:` prefix).
- **Python smoke comparison** (corpus): 767 results equal Python's SQLite (3.45.1); the 15 that differ
  are all float-to-text conversions that changed between 3.45 and 3.54 (`1e15` as text, `%.0f` of 2.5),
  not platform differences.
- **Fixed on the way**: W0504 (no effects record) counted standard-library files, so every single-file
  program importing a `std` module got it; it now counts the program's own files. `make test` builds of
  SQLite on newer cross compilers warned about an unused variable in the amalgamation
  (`-Wno-unused-variable` for that file only).
- Connections are closed when each test or property case ends and at every program exit
  (`vo_exit`), so an open transaction is rolled back rather than left to the next open.


## Follow-up (2026-11-19, from the voledger experiment)

An agent building a program on `std.sqlite` reported what got in its way; fixed after the spec was
committed:

- **Effect-generic database code**: `fn f[D: effects](db: Database[D]) uses D` (or `Database[uses D]`)
  did not accept `open` or `open_memory` results. An effect parameter as a type argument is now stored as
  the effect set `uses D`, so both spellings are one type; unification binds an effect parameter inside an
  effect set; an argument whose type comes from context (`open_memory()`) is no longer checked against an
  unresolved `Database[uses D]`. Scopes in a type argument are E0327 with a fix that removes them.
  `tests/cases/sqlite_steps/generic_db`, `tests/cases/generic/effect_args_scoped`.
- **`vo.effects` with effect parameters** (older than this spec): `vo effects --update` wrote `uses D`,
  which the record reader rejected (E0510). Names are resolved against the function's declaration.
- **Functions that return a pure function and have effects of their own** (older than this spec) could
  not be written: a `uses` after a function type belongs to that type. Types can now be parenthesized,
  `fn f() -> (fn() -> int) uses console {`; `vo fmt`, type names, `vo doc` and hover write the
  parentheses exactly where a `uses` follows a pure function type, and the E0501 fix adds them.
  `tests/cases/fmt/fn_type_parens`, `tests/cases/effects/fn_return_uses_fix`.
- **Docs**: the reference and the results guide still showed `case ok { }` for `result[void]` (W0402
  since spec 009). Runnable doc examples must now also check without warnings (`tests/doc_examples.py`).

# SQLite 3.54.1 (vendored)

Pinned by `tools/vendor_sqlite.sh 3540100 2026 92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30` (spec 026, constitution 1.5.0).

- Source: <https://sqlite.org/2026/sqlite-amalgamation-3540100.zip>
- Archive SHA3-256 (as published on sqlite.org/download.html): `92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30`
- License: public domain. From the top of `sqlite3.c`:

  > ** The author disclaims copyright to this source code.  In place of
  > ** a legal notice, here is a blessing:
  > **
  > **    May you do good and not evil.
  > **    May you find forgiveness for yourself and forgive others.
  > **    May you share freely, never taking more than you give.

- Security releases: <https://sqlite.org/cves.html>; a release that fixes a vulnerability is applied
  in the spec cycle in which it appears (constitution, Implementation Constraints).

## Kept files (unmodified)

- `sqlite3.c` sha256 0c657666d819f261cc4007526ad57e1669de9949df7a8ea847aa284a2bf0ee52
- `sqlite3.h` sha256 d6c84557261893439b341d2e6c35187ae3ea24fbfd843ad94616f72d89242b8c

## Compile options

See `SQLITE_FLAGS` in the Makefile and research R2 of specs/026-sqlite/research.md.

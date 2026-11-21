# Implementation Plan: Downloading Packages

**Branch**: `041-package-download` (work on `main`, as for specs 001–040) | **Date**: 2026-11-21 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/041-package-download/spec.md`

## Summary

`vo package ENTRY NAME URL` becomes: fetch the archive with the HTTPS client the toolchain already has
(R1), inflate it with a new small gzip reader (R2), read it as tar in memory, refusing everything that
is not a plain file or directory with a safe path (R3), write the `.vo` files to a staging directory
beside the target, pin it with the existing `pkg_hash`, and only then swap it in and write the
manifest (R4). The manifest line gains the URL as an optional fourth field (R5). `--restore` runs the
same steps for entries whose directory is missing and compares the hash with the pin instead of
writing it (R6). The effects of a package are read from what `vo doc NAME.module --json` already
prints, in a child process, and united (R7).

| Part | File | What |
|---|---|---|
| Command | `src/packages.c`, `src/main.c` | URL operand, `--restore`, `--dir`, `--ca-file` for `package`; staging and swap; effects in the report |
| Fetch | `src/fetch.c` (new) | URL parsing, GET over `tls_exchange`, redirects, limits |
| gzip | `src/inflate.c` (new) | DEFLATE (stored, fixed, dynamic), gzip header, CRC-32 and length check |
| tar | `src/untar.c` (new) | ustar, GNU long names, pax paths; the safety rules |
| Manifest | `src/packages.c`, `src/vo.h` | fourth field; `Pkg.url` |
| Codes | `docs/errors.md`, `src/diag.c` (catalog) | E0706 `package_fetch_failed` |
| Docs | `site/docs/tools/package.md`, `site/docs/guide/modules-and-packages.md`, `docs/reference.md`, roadmap, changelog, review | R9 |
| Tests | `tests/package_fetch.py` (new, in `make test`), `tests/http_server.py` (archive routes), `tests/cases/packages/` step cases | R10 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc).

**Primary Dependencies**: none new. The DEFLATE reader is written here (about 250 lines); vendoring
zlib for it would add twenty times the code for one direction of one format.

**Storage**: the project's own files: `vo.packages` and package directories.

**Testing**: `tests/package_fetch.py` serves generated archives from a local HTTPS server (the test
authority of spec 019): a corpus of archives made by Python's `tarfile`, `gzip` and the system `tar`
compared file for file, a set of hostile archives, restore, the effects report, and a system-call
trace showing that other commands open no socket. Step cases for the code's catalog entry. `make
quick AREA=packages` between edits, `make test` once; the gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: none beyond the limits: a 16 MiB archive unpacks in well under a second.

**Constraints**: everything is validated in memory before the first file is written; no static buffer
over 64 KiB (the inflate window is the output buffer itself, mapped); reference ≤ 15,000 tokens
(14,938 now: about +25).

**Scale/Scope**: about 250 lines of inflate, 200 of tar, 150 of fetch, 250 in `packages.c`, 500 of
Python tests.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One command shape; an agent adds a dependency with one line and reads its effects in the same output. | Pass |
| II. Machine-readable feedback | `--json` gives a `package` record with `url`, `effects`, `gained`, `lost`; failures are one new stable code with a catalog entry. | Pass |
| III. Determinism | Programs are unaffected: nothing is fetched by `check`, `run`, `test` or `build`, and a package is what its hash says. | Pass |
| IV. Test-first | Step cases and `tests/package_fetch.py` before each part. | Pass |
| V. Single source of truth, minimal surface | The pin is still the content hash; no lock file, registry or version syntax. The HTTP client, the hash and the doc records are reused. | Pass |
| Vendored code | None: the DEFLATE reader is original and small. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/041-package-download/
├── plan.md
├── research.md          # R1–R10
├── data-model.md
├── quickstart.md
├── contracts/
│   └── package.md       # the command, manifest line, output, limits, codes
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/packages.c, src/fetch.c (new), src/inflate.c (new), src/untar.c (new), src/main.c, src/vo.h
docs/errors.md, docs/reference.md, site/docs/tools/package.md, site/docs/guide/modules-and-packages.md
tests/package_fetch.py (new), tests/http_server.py, tests/cases/packages/
```

**Structure Decision**: three small files with one job each (fetch, inflate, untar), so each can be
tested and fuzzed on its own; the command stays in `packages.c`.

## Order of work

1. The archive routes of the test server and the corpus test; fetch, inflate and tar until the corpus passes.
2. Redirects and limits; staging and swap; the manifest's fourth field; update and `--dir`.
3. Hostile archives; all-or-nothing.
4. `--restore`. Effects in the report.
5. E0706 catalog entry, docs; three architectures; `make test`; results.

## Complexity Tracking

No constitution violations.

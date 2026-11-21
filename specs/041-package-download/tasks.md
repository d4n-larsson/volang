---

description: "Task list for vo-lang spec 041: Downloading Packages"
---

# Tasks: Downloading Packages

**Input**: Design documents from `specs/041-package-download/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R10), data-model.md, contracts/package.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's cases land in `tests/cases/packages/` or
`tests/package_fetch.py` first and fail before its code.

**Organization**:
- Setup: options, the test server's archive routes, the test driver, the results file.
- Foundational: the manifest's fourth field; the new code's catalog entry.
- US1 (P1) add from a URL; US2 (P1) safety and all-or-nothing; US3 (P2) restore; US4 (P2) effects;
  US5 (P3) documentation.
- Polish: fuzz, three architectures, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Forms, output, codes, exit statuses**: contracts/package.md. Manifest line, limits, the JSON
  record: data-model.md.
- **The test driver**: `tests/package_fetch.py` writes archives into a temporary directory, starts
  `tests/https_server.py --cert a.test` with `VO_ARCHIVES` pointing there, and runs `bin/vo package
  --ca-file tests/tls/certs/test-ca.pem ...` (`$VO_BIN`) in temporary projects. Output: `FAIL <case>:
  ...` per failure and a one-line summary; exit 1 on a failure; `--group NAME`.
- **Nothing is written before the archive is fully read and accepted** (research R4); every failure
  test compares the whole project tree with a snapshot taken before the command.
- **No static array over 64 KiB**; buffers for the download, the inflated data and the files are mapped
  or in the permanent arena.
- **Unchanged programs**: no existing golden case may change except those that list every error code
  (which gain E0706) and the usage text of `package`.
- **Reference budget**: `python3 tests/ref_examples.py` after the reference change; at most +25 tokens
  (from about 14,938).
- **Between edits**: `make quick AREA=packages`; `make test` once, through `tools/progress.py run
  test`, reading `build/progress/test.log`; the gate only when asked. Never two suites at once.

## Phase 1: Setup

- [X] T001 In `tests/http_server.py`, routes `/archive/NAME` (the bytes of `$VO_ARCHIVES/NAME`, 404 when missing), `/moved/NAME` (302 to `/archive/NAME`), `/hops/N/NAME` (302 to `/hops/N-1/NAME`, and to `/archive/NAME` at 0) and `/insecure/NAME` (302 to an `http://` URL)
- [X] T002 [P] Create `tests/package_fetch.py` with the driver of Conventions (helpers to build a tar or tar.gz from a dict of paths with `tarfile`, to snapshot a tree, and to pin the same tree locally for comparison) and an empty group list; add it to the test list in `Makefile` and to area `packages` in `tools/quick.sh` with `src/fetch.c`, `src/inflate.c`, `src/untar.c`; create `specs/041-package-download/results.md` with headings for SC-001 to SC-007
- [X] T003 In `src/main.c`, accept for `package`: `--restore` (with exactly ENTRY), `--dir DIRECTORY` and `--ca-file FILE` (only with a URL operand or `--restore`; T0001 otherwise), load `--ca-file` as `run` does, and update the usage text and T0001 messages; golden cases in `tests/cases/packages/` for each wrong combination

## Phase 2: Foundational

- [X] T004 Golden cases in `tests/cases/packages/`: a manifest line with a fourth field `https://...` is read and the package loads; a fourth field that is not `https://`, has a space after it, or a control byte is E0703; the local form `vo package ENTRY NAME DIRECTORY` on an entry that had a URL writes the line without it; `--remove` of one entry keeps the URLs of the others
- [X] T005 In `src/vo.h` and `src/packages.c`, `Pkg.url`; `pkg_parse_line` reads the optional fourth field (rule of data-model.md: "starts with `https://`; no space or control byte; may end in `#PATH`"); `pkg_write` writes it; E0703's message names it
- [X] T006 [P] Add E0706 `package_fetch_failed` to the catalog (`docs/errors.md`, with cause, example and what to do) and wherever codes are registered; a step case `tests/cases/packages/fetch_404/` (`server https a.test`, `vo package --ca-file ... main.vo money https://127.0.0.1:{port}/archive/none.tar.gz`, `vo explain E0706`) that fails until US1

## Phase 3: User Story 1 - Add a package from a URL (P1) 🎯 MVP

**Goal**: fetch, inflate, untar, write, pin.

**Independent Test**: `python3 tests/package_fetch.py --group corpus` and `--group fetch`; the step case `tests/cases/packages/fetch/`.

- [X] T007 [P] [US1] Step case `tests/cases/packages/fetch/`: a small package archive kept in the case folder, served through `server https a.test` with `VO_ARCHIVES` (add the variable to the `server` step of `tests/steps.py`): add it, `keep vo.packages`, check and run a program importing it, add it again (`unchanged`)
- [X] T008 [P] [US1] In `tests/package_fetch.py`, group `corpus` (SC-005): at least 200 archives of one random tree each: `tarfile` in ustar, GNU and pax formats; gzip levels 0 to 9 and uncompressed; the system `tar` with and without `-z`; with a top folder and without; with `#PATH`; names over 100 bytes (prefix, GNU long name, pax path); empty files; a 5 MiB file; a file of every byte value; non-`.vo` files beside (must be absent after); for each, the unpacked tree and the pinned hash equal a local pin of the same `.vo` files (SC-001)
- [X] T009 [P] [US1] In `tests/package_fetch.py`, group `fetch`: `/moved/` followed; five hops followed, six refused; `/insecure/` refused; an `http://` URL and a URL with `user@` are T0001; 404 is E0706 naming the status; a bad certificate (`--cert expired` server) is E0706 with `std.http`'s reason; a refused connection is E0706; a query string is sent, the `#PATH` is not (the server logs the target); update from a second archive shows `updated` with both hashes and replaces the directory; `--dir vendor/m` writes there; a target directory that exists and is not the entry's is refused and untouched; an update over a package directory with an edited `.vo` file is E0701 and leaves the edit; `#../x`, `#/x`, `#a//b` and `#` are T0001
- [X] T010 [US1] Create `src/inflate.c`: `inflate_gzip(in, n, &out, &out_n, limit, &why)` per research R2 (stored, fixed, dynamic blocks; a mapped output that doubles up to `limit` and is the window; gzip header with optional fields; CRC-32 and length check; concatenated members; a `why` text for every failure) with no reads past `in + n` and no writes past the output
- [X] T011 [US1] Create `src/untar.c`: `untar(buf, n, &files, &why)` per research R3 (checksum; octal sizes, base-256 refused; types `0`, NUL, `5`, `g`, `x` with `path=`, `L`; every other type refused; the path rules: "empty, absolute, longer than 4,096 bytes, has a component `..`, a component over 255 bytes, or a byte below 0x20 or 0x7f"; at most 10,000 entries; later entries replace earlier ones), and `untar_select(files, inner)` (drop a single common top folder, keep what is under `inner`, keep `*.vo`)
- [X] T012 [US1] Create `src/fetch.c`: `fetch_url(url, &body, &n, &why)` per research R1 (URL parsing with the fragment split off and checked as a safe relative path, credentials refused; `GET` with `Host`, `Connection: close`, `User-Agent: vo`, `Accept-Encoding: identity` through `tls_exchange` and `http_frame`; up to five redirects to `https://` or same-host paths; status other than 200 fails naming it; 60 s)
- [X] T013 [US1] In `src/packages.c`, the URL form of `cmd_package` per research R4 and contracts/package.md: fetch, inflate when the gzip magic is there, untar, select; E0702 when nothing is left; target `--dir` or `packages/NAME` beside the manifest, refused unless absent or the entry's own and still hashing to its pin (E0701 otherwise); write to `T.new-PID`; `pkg_hash`; `unchanged` removes the staging directory; else swap by renames, write the manifest with the URL, remove the old directory; report with `from URL`; a recursive remove helper for the staging and old directories that never follows links

## Phase 4: User Story 2 - Nothing unsafe, nothing implicit (P1)

**Goal**: hostile input is refused whole; nothing happens unless asked.

**Independent Test**: `python3 tests/package_fetch.py --group hostile` and `--group quiet`.

- [X] T014 [US2] In `tests/package_fetch.py`, group `hostile` (SC-002), each archive built by hand where `tarfile` refuses to: a path with `../`, an absolute path, a symbolic link, a hard link, a character device, a FIFO, a path with a control byte, a wrong header checksum, a base-256 size, a tar cut mid-file, a gzip cut mid-stream, a wrong CRC-32, a wrong length field, a gzip that inflates past 64 MiB, a file over 16 MiB, 10,001 entries, a download over 16 MiB, an archive with no `.vo` file, a `#PATH` that is not in it; each fails with E0706 (E0702 for the last two) and exit 65, the message names the problem, and the project tree (with a package already installed from an earlier good archive) equals its snapshot
- [X] T015 [P] [US2] In `tests/package_fetch.py`, group `quiet` (SC-006): in a project whose manifest has URL entries and whose package directories were deleted, `strace -f -e trace=socket,connect` around `vo check`, `vo run`, `vo test`, `vo build` and `vo doc` shows no call, and each reports E0702 as before; skipped with a message when `strace` is missing
- [X] T016 [US2] Close what T014 finds in `src/inflate.c`, `src/untar.c`, `src/fetch.c` and `src/packages.c`: every limit of data-model.md enforced with its own message; the rollback of research R4 on a failed manifest write

## Phase 5: User Story 3 - Restore on a fresh checkout (P2)

**Independent Test**: `python3 tests/package_fetch.py --group restore`.

- [X] T017 [US3] In `tests/package_fetch.py`, group `restore` (SC-003): a project with three fetched packages and one local one; delete `packages/`; `--restore` prints `restored` three times and `present` once, `vo check` passes and `vo.packages` is byte-identical; replace one archive on the server: `--restore` after deleting gives E0701 for it, restores the others, exits 65 and leaves no directory for it; a local entry whose directory is missing is E0702; a present directory that does not match is E0701 and untouched; `--json` gives one record per entry
- [X] T018 [US3] In `src/packages.c`, `--restore` per research R6, sharing the fetch-to-staging steps of T013 and comparing with the pin instead of writing it; the buffers of one package are unmapped before the next; changes `restored` and `present` in `pkg_report`

## Phase 6: User Story 4 - See what a dependency can do (P2)

**Independent Test**: `python3 tests/package_fetch.py --group effects`.

- [X] T019 [US4] In `tests/package_fetch.py`, group `effects` (SC-004): a pure package prints `effects: none`; one whose exported functions declare `file.read("data/")`, `net("api.example.com")` and an effect set prints the union in canonical order with scopes; a private function's effects are not listed; an update that adds `process("git")` and drops `file.read` prints `gains:` and `loses:` lines; `--json` has `url`, `effects`, `gained`, `lost`; a package with a syntax error, and one that imports a package that is not listed, are each added with `effects: could not be read` and the first error, exit 0; the same output when the command is run from another working directory with a path to `ENTRY`, where the working directory has a manifest of its own
- [X] T020 [US4] In `src/packages.c`, the effects of a package per research R7: a forked child that changes its working directory to the manifest's and runs the doc records for each module of `pkg_doc_modules` with its output captured, the parent reading `effects` with `src/jsonrpc.c`'s reader, the union in canonical order, the difference with the old directory's set on an update, and the lines and JSON fields of contracts/package.md

## Phase 7: User Story 5 - Documented (P3)

**Independent Test**: `python3 tests/ref_examples.py`; `make site-check`.

- [X] T021 [P] [US5] Rewrite `site/docs/tools/package.md` for the four forms (synopsis, options table, what is fetched and what is refused, limits, output for add, update and restore, exit statuses, an example with a forge URL as a `text` block) add a "Getting a package" part to `site/docs/guide/modules-and-packages.md`, and update the `package` row and example of `site/docs/tools/json-output.md` (fields `url`, `effects`, `gained`, `lost`, `note`; values `restored` and `present` of `change`)
- [X] T022 [P] [US5] In `docs/reference.md`, the packages sentence and the command list line for `package` per research R9; keep it under 15,000 tokens
- [X] T023 [P] [US5] Update `docs/roadmap.md` (status row, a "041" section, the "Downloading packages" row of the planned table), `CHANGELOG.md` (the feature, that older toolchains reject manifest lines with a URL, and that the `package` record's `change` has two new values) and the "Fixed since the review" table in `docs/review-2026-11/README.md`

## Phase 8: Polish

- [X] T024 In `tests/package_fetch.py`, group `fuzz`: 2,000 archives made by mutating corpus archives (truncate, flip a bit, overwrite a header field, splice two): every run exits 0 or 65 within 5 s, writes nothing outside the project, and a run that exits 65 leaves the tree as it was
- [X] T025 Run `tests/package_fetch.py --group corpus` and the packages step cases with `VO_BIN` set to `build-aarch64/vo` and `build-riscv64/vo`; check with `nm` that no new static object exceeds 64 KiB; record in `specs/041-package-download/results.md`
- [X] T026 Run `make test` once through `tools/progress.py run test`, read `build/progress/test.log`, fix what it finds, and complete `specs/041-package-download/results.md` (SC-001 to SC-007, deviations, what was not verified, which existing cases changed and why); set the spec's status

## Dependencies

- Setup → Foundational → US1. T006's step case needs T001 (the 404 route) and T003 (`--ca-file`). US2 needs US1. US3 needs US1 (and US2's rollback for its failure cases).
  US4 needs US1. US5 needs the command's final output (US1, US3, US4). Polish last.

## Parallel opportunities

- T002 with T001; T007, T008 and T009; T010, T011 and T012 (three files); T014 with T015; T021, T022
  and T023.

## Implementation strategy

MVP is Setup, Foundational, US1 and US2 together: a package added from a URL, with hostile archives
refused. Then restore, then the effects report, then documentation.

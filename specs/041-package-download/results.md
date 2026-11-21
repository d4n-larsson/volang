# Results: Downloading Packages

Measured on the development machine (x86-64), 2026-11-21. The gate was not run.

## Success criteria

| Criterion | Result |
|---|---|
| SC-001: a package added with one command; the hash equals a local pin | passes: for each of the 200 corpus archives the pinned hash equals `vo package ENTRY NAME DIRECTORY` on the same files; a program importing a fetched package checks and runs (step case `packages/fetch`) |
| SC-002: every hostile archive refused, the project unchanged | passes: 20 archives (path escape, absolute path, symbolic and hard link, device, FIFO, control byte, wrong checksum, base-256 size, truncated tar, truncated gzip, wrong CRC-32, wrong length, 65 MiB bomb, 17 MiB file, 10,001 entries, 17 MiB download, not an archive, no `.vo` file, missing `#PATH`); after each the tree equals its snapshot and nothing was written outside it |
| SC-003: restore after deleting every package directory | passes: three fetched packages restored, a local one reported present, `vo check` passes, the manifest byte-identical; a changed upstream is E0701 and writes nothing |
| SC-004: the effects in the output match the declarations | passes: none; `file.read("data/")`, `file.write("out/")` (through an effect set), `net("api.example.com")`; an update's `gains` and `loses`; private functions and effect parameters left out |
| SC-005: 200 archives from other tools unpack identically | passes: Python `tarfile` in ustar, GNU and pax formats, gzip levels 0 to 9 and uncompressed, the system `tar` with and without `-z`, with and without a top folder, `#PATH`, names of 90 and 120 bytes, empty files, a 5 MiB file, a file of every byte value |
| SC-006: other commands open no socket | passes: `strace` shows no `socket` or `connect` call for `check`, `run`, `test`, `build` and `doc`, with the package present and with it missing (E0702) |
| SC-007: three architectures, reference budget | every group but `quiet` and the package step cases pass on ARM64 and RISC-V under qemu (corpus of 30 and fuzz of 100 there); reference about 14,960 of 15,000 tokens |

Fuzz: 2,000 mutated archives (truncated, bits flipped, header bytes overwritten, two archives
spliced): every run exits 0 or 65, a failed run leaves the tree as it was, nothing is written outside
the project. `make test` passed once on the final code (`package_fetch: 7 groups, 71 cases, 0
failures`; the log ends at the last step). `make site-check` passes. No new static data beyond a 1 KiB
CRC table.

Against a real forge: `vo package main.vo demo https://github.com/octocat/Hello-World/archive/refs/heads/master.tar.gz`
answers E0702 "the archive has no .vo files", which exercises the redirect to the download host, the
machine's trust store, gzip and a pax archive on real data. No published vo package exists to add.

## Deviations from the plan

- **The golden cases of T003, T004 and T006 are two step cases**, `packages/url_field` (the manifest's
  fourth field and every wrong option combination) and `packages/fetch` (which also holds the 404 and
  `vo explain E0706`).
- **A directory that is not the package's own is refused with E0706**, for want of a better code.
- **`--restore` treats only an absent directory as missing.** One that exists without `.vo` files is
  E0702 and left alone.
- **The download itself is not released between packages in `--restore`**: it lives in the toolchain's
  permanent arena (up to 16 MiB per package for the life of the command). The inflated data, the
  larger part, is unmapped.
- **No rollback if writing the manifest fails after the swap**: `write_file_safely` ends the command
  on failure, leaving the new directory in place and the old one beside it under `.old-PID`.
- **When `--dir` moves a package**, the directory it had before is left where it was.
- **Five existing cases changed**, in messages only: the usage text of `package`, the `--ca-file`
  message (it now names `package`), and E0703's text (it now names the optional URL).
- **The hostile and fuzz tests found no defect in the decompressor or the tar reader**; T016 changed
  nothing.
- **Tests and code were written together**; the step cases came after the command worked.
- `make test` is about two minutes longer (the corpus and the fuzz run).

## Found on the way

- **The 10,000-entry limit counts the whole archive**, not what is under `#PATH`. This repository's own
  archive is refused for that reason, although its `std` folder is 16 files. A package kept in a large
  repository needs its own archive, or the limit raised.

## Not verified

- A successful add from a real forge (there is no published vo package).
- zip archives, proxies and private repositories are out of scope and untested.
- The gate (`make gate`).

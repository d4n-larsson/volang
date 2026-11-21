# Feature Specification: Downloading Packages

**Feature Branch**: `041-package-download`

**Created**: 2026-11-21

**Status**: Implemented (gate not run)

**Input**: User description: "Downloading packages, from the review (docs/review-2026-11/ecosystem.md,
Distribution, 'Downloading packages'; order step 3) and the roadmap (spec 007 follow-up): 'The
content-hash design is right. What it needs is a fetch step: `vo package add NAME URL` downloads,
hashes and pins, and nothing is fetched at check or run time. A registry can wait; a git URL and a hash
are enough to start an ecosystem. `vo.effects` showing a dependency's capabilities is the
differentiator, so show it in the add output.' Today `vo package ENTRY NAME DIRECTORY` pins a directory
that is already on disk; getting it there is copying by hand. Must keep: nothing fetched by `check`,
`run`, `test` or `build`; the content hash as the only thing trusted; no libc and no external program
(no `git`, `tar` or `curl`); three architectures; the reference under 15,000 tokens (about 14,938 now);
no large static buffers. Out of scope: a registry, version ranges and resolution, publishing, private
repositories with credentials, signatures."

Background: vo has packages (spec 007): a directory of `.vo` files named in `vo.packages` with the
SHA-256 of its contents, checked every time the program is checked or run. What it does not have is a
way to get someone else's package: the only route is to download an archive in a browser, unpack it
next to the program and run `vo package`. Every forge (GitHub, GitLab, Codeberg, a plain web server)
serves a repository at a tag or commit as one archive over HTTPS, and vo already speaks HTTPS and
already pins by content. A fetch step joins the two, and nothing else about packages changes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Add a package from a URL (Priority: P1)

A developer finds a vo package on a forge and adds it with one command: the toolchain downloads the
archive, unpacks the package's `.vo` files into the project, computes the content hash and writes the
entry in `vo.packages`, with the URL it came from. `import NAME.module.item;` then works.

**Why this priority**: it is the feature.

**Independent Test**: a local HTTPS server serves an archive of a small package; `vo package main.vo
money URL` creates `packages/money/` with its `.vo` files and a manifest line with hash and URL; a
program importing it checks and runs; the hash equals the one `vo package main.vo money DIR` gives for
the same files unpacked by hand.

**Acceptance Scenarios**:

1. **Given** an `https://` URL of a `.tar.gz` (or `.tgz`, or `.tar`) archive, **When** `vo package
   ENTRY NAME URL` runs, **Then** the archive's `.vo` files are written under `packages/NAME/` next to
   the manifest, the entry `NAME packages/NAME sha256:HASH URL` is added or updated, and the command
   prints what it did as the local form does (`added`, `updated`, `unchanged`).
2. **Given** an archive whose files all sit under one top folder (as forges produce:
   `repo-1.2.0/...`), **When** unpacked, **Then** that folder is dropped.
3. **Given** a URL ending in `#PATH`, **When** unpacked, **Then** only the files under `PATH` inside
   the archive (after the top folder is dropped) are taken, and `PATH` is the package's root.
4. **Given** a URL that answers with a redirect, **When** fetched, **Then** up to five redirects to
   `https://` URLs are followed (forges redirect archive links to a download host).
5. **Given** a package already in the manifest, **When** added again from another URL (a newer tag),
   **Then** its directory is replaced, the entry updated, and the output shows the old and new hash.
6. **Given** `--dir DIRECTORY`, **When** adding, **Then** the files go there, not to `packages/NAME`.

---

### User Story 2 - Nothing unsafe, nothing implicit (Priority: P1)

Adding a package cannot write outside its directory, cannot fill the disk, and never happens unless
asked. Only the content hash is trusted afterwards.

**Why this priority**: an archive from the network is the most hostile input the toolchain reads.

**Independent Test**: archives with `../` paths, absolute paths, links, device entries, a
decompression bomb, a truncated stream and a wrong checksum are each refused with a message naming the
problem, and leave the project exactly as it was; `vo check`, `run`, `test` and `build` on a project
whose package directory is missing report E0702 and open no connection.

**Acceptance Scenarios**:

1. **Given** an archive entry whose path is absolute, contains `..`, or is a link, a device or
   anything but a regular file or directory, **When** unpacked, **Then** the whole command fails and
   nothing is written or changed.
2. **Given** an archive, **When** unpacked, **Then** only files ending in `.vo` are written; every
   other file is ignored.
3. **Given** a download larger than 16 MiB, an archive that unpacks to more than 64 MiB or more than
   10,000 files, or a file name longer than 255 bytes, **Then** the command fails with that limit.
4. **Given** a corrupt, truncated or unsupported archive, **Then** the command fails saying so.
5. **Given** an `http://` URL, a redirect to `http://`, or a certificate that does not verify,
   **Then** the command fails; nothing turns verification off (`--ca-file` adds an authority, as for
   `vo run`).
6. **Given** any failure, **Then** the manifest and the package's previous directory are unchanged.
7. **Given** `vo check`, `vo run`, `vo test`, `vo build` or `vo doc`, **When** run on any project,
   **Then** no network connection is made, whatever the manifest says.
8. **Given** an archive with no `.vo` file (under `#PATH` if given), **Then** E0702, as for an empty
   directory.

---

### User Story 3 - Restore on a fresh checkout (Priority: P2)

A team keeps `vo.packages` in version control and the downloaded directories out of it. After a clone,
one command fetches every package that has a URL and is missing, and each must hash to its pinned
value.

**Why this priority**: without it a project either commits its dependencies or cannot be built from a
clone.

**Independent Test**: delete `packages/`, run `vo package --restore main.vo`: every directory is back
and `vo check` passes; change what the server serves: restore fails with E0701 for that package and
writes nothing for it.

**Acceptance Scenarios**:

1. **Given** manifest entries with URLs whose directories are missing, **When** `vo package --restore
   ENTRY` runs, **Then** each is downloaded and unpacked, and one line per package says `restored`,
   `present` (directory there and matching) or names the failure.
2. **Given** a download whose content hash differs from the pinned one, **Then** that package is not
   written and the command fails with E0701 (the URL's content changed); the manifest is never changed
   by `--restore`.
3. **Given** an entry without a URL whose directory is missing, **Then** it is reported as missing
   (E0702) and the others are still restored.
4. **Given** a directory that exists but does not match its pin, **Then** it is left alone and reported
   (E0701), as `vo check` would.

---

### User Story 4 - See what a dependency can do before using it (Priority: P2)

When a package is added or updated, the command shows what its functions declare: which effects, with
which scopes. A package that wants the network or the file system says so before any of its code runs.

**Why this priority**: it is what makes adding a vo dependency different from adding one anywhere else.

**Independent Test**: adding a pure package prints `effects: none`; adding one whose functions declare
`net("api.example.com")` and `file.read` prints those; updating to a version that gains `process`
prints the gain.

**Acceptance Scenarios**:

1. **Given** a package just unpacked, **When** the command reports, **Then** it lists the union of the
   effects its exported functions declare, with scopes, in canonical order, or `none`.
2. **Given** an update, **When** the union gained an effect or a scope, **Then** the gain is shown on
   its own line (`gains: process("git")`); a loss likewise.
3. **Given** `--json`, **Then** the `package` record has `url`, `effects` and, on an update,
   `gained` and `lost`. (`--restore` adds two values of `change`, `restored` and `present`: new
   values of an existing field, named in the changelog and on the JSON output page.)
4. **Given** the command run from any working directory, **Then** the effects are read from the
   manifest beside `ENTRY`, not from one found from the working directory.
5. **Given** a package that does not check (a syntax error, an import that does not resolve),
   **Then** it is still added (pinning is by content), with a note that its effects could not be
   read and why; `vo check` of the program will report the errors.

---

### User Story 5 - Documented (Priority: P3)

The command page, the guide's packages section, the reference line and `vo explain` for the codes
involved describe the URL form, `--restore`, the limits and what is and is not trusted.

**Acceptance Scenarios**:

1. **Given** the site's `vo package` page, **Then** it shows both forms, the options, the limits and an
   example with a forge URL.
2. **Given** the reference, **Then** the packages sentence names the URL form and `--restore`, and the
   whole stays under 15,000 tokens.

### Edge Cases

- The same URL fetched twice gives `unchanged` and touches no file.
- A manifest line with a URL read by an older toolchain: the fourth field is new; older versions
  report the line as malformed. Stated in the changelog.
- A URL with a query string or credentials (`user:pass@`): query strings are kept; credentials in the
  URL are refused.
- `#PATH` that does not exist in the archive: E0702 naming the path. A `#PATH` that is empty,
  absolute, or has a `..` or empty component is a usage error (T0001).
- Two archive entries with the same path: the later one wins, as `tar` does.
- Names that differ only in case, or that are not UTF-8: written as they are when the file system
  allows; a name with a NUL or control character fails the command.
- A package whose files import another package: nothing is fetched for it; `vo check` reports the
  unresolved import and the user adds that package too.
- The package directory already exists: on an update it is replaced as a whole, but only when it
  still hashes to its pin. A directory whose `.vo` files were changed since they were pinned is refused
  (E0701, as `vo check` reports it), so local edits are never overwritten: pin them with the local
  form or remove the directory first. A directory that no manifest entry points at is refused too.
- No network, a name that does not resolve, a server error status: the command fails with the reason
  and exit status 65; nothing changed.
- `--restore` with nothing to do prints `present` lines and exits 0.
- A proxy: not supported in this version.
- The time limit: 60 seconds per download.

## Requirements *(mandatory)*

### Functional Requirements

**Fetching**

- **FR-001**: `vo package ENTRY NAME URL` MUST download the archive at an `https://` URL, following up
  to five redirects to `https://` URLs, and treat it as a tar archive, gzip-compressed or not.
- **FR-002**: It MUST write the archive's `.vo` files, with their directories, under the package
  directory (`packages/NAME` next to the manifest, or `--dir`), dropping a single common top folder
  and, with `#PATH`, taking only what is under `PATH`.
- **FR-003**: It MUST then pin the package exactly as the local form does (same hash, same E0702
  rule; a name that clashes with a directory beside an importing file is still E0705 from `vo check`,
  unchanged) and record the URL as a fourth field of the manifest line. The local form and
  `--remove` keep working; a local add of a package that had a URL drops the URL.
- **FR-004**: Unpacking and pinning MUST be all or nothing: on any failure the previous directory and
  manifest are as before.

**Safety**

- **FR-005**: Entries with absolute paths, `..` components, NUL or control characters, links, devices
  or other special types MUST fail the command. Files not ending in `.vo` are skipped.
- **FR-006**: Limits: 16 MiB downloaded, 64 MiB unpacked, 10,000 entries, 4,096 bytes per path and
  255 per path component, 16 MiB per file, 60 s per download. Exceeding one fails the command naming
  it.
- **FR-007**: Only `https://` is fetched, verified as `std.http` verifies (`--ca-file` accepted);
  no option disables verification. URLs with credentials are refused.
- **FR-008**: No command other than `vo package` with a URL or `--restore` MUST open a network
  connection on behalf of packages.

**Restore**

- **FR-009**: `vo package --restore ENTRY` MUST, for each manifest entry with a URL and a missing
  directory, download and unpack it and accept it only if its content hash equals the pinned one
  (E0701 otherwise); it MUST NOT change the manifest, and reports every entry.

**Capabilities**

- **FR-010**: After an add or update the command MUST print the union of effects (with scopes) declared
  by the package's exported functions, and on an update what was gained and lost; `--json` carries
  the same. A package that cannot be read for effects is added with a note.

**Fit**

- **FR-011**: No external program is run and no libc is used; three architectures; no static buffer
  over 64 KiB.
- **FR-012**: Messages use existing codes where they fit (T0001 usage, E0701 hash mismatch, E0702
  nothing to pin, E0705 name clash) and one new code for a failed fetch or a refused archive, with a
  catalog entry.
- **FR-013**: The site, the reference (under 15,000 tokens), roadmap, changelog and review are updated.

### Key Entities

- **Manifest entry**: name, directory, content hash, and now optionally the URL it was fetched from.
- **Archive**: a tar stream, optionally gzip-compressed, of which only regular `.vo` files are used.
- **Package effects**: the union of what a package's exported functions declare.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A package on a local forge-like server is added with one command and a program importing
  it runs; the pinned hash equals the hash of the same files pinned locally.
- **SC-002**: Every hostile archive of the test set (path escape, absolute path, link, device, bomb,
  truncated, bad checksum, oversized) is refused, and after each the project directory is
  byte-identical to before.
- **SC-003**: After deleting every package directory, `--restore` brings a three-package project back
  to a state where `vo check` passes; a changed upstream is refused.
- **SC-004**: The add output for packages with no effects, with scoped effects, and for an update that
  gains an effect matches the packages' declarations exactly.
- **SC-005**: Archives produced by `tar` and `gzip` at every compression level, and by Python's
  `tarfile` in its three formats, unpack to identical files (a corpus of at least 200 archives).
- **SC-006**: With the network unavailable, `check`, `run`, `test` and `build` behave as before on a
  project with URL entries; a trace of system calls shows no socket.
- **SC-007**: The suite passes on the three architectures and the reference stays under 15,000 tokens.

## Assumptions

- **Archives, not git.** vo runs no external program and does not speak the git protocol. Every forge
  serves a tag or commit as a `.tar.gz` over HTTPS; that URL is the "git URL and a hash" of the review.
  A URL of a branch works but changes under the pin, which `--restore` then refuses.
- **The command keeps one shape**: the third operand is a directory or, when it starts with
  `https://`, a URL. No `add` subcommand (the local form has none).
- **Only `.vo` files are unpacked.** They are all the hash covers and all the toolchain reads; a
  package's README or license is on its forge.
- **Downloaded packages live in `packages/NAME`** by default; whether that directory is committed is
  the project's choice. `--restore` serves those who do not.
- **No transitive fetching, no versions, no registry.** A package that needs another says so through
  an unresolved import.
- **zip archives are not read** in this version; forges offer both.
- **One new error code** for fetch and archive failures.
- The gzip reader is written for this (or vendored under the constitution's rules if that is smaller):
  decided in the plan.
- Out of scope: registries, version ranges, lock files beyond `vo.packages`, publishing, private
  repositories and credentials, signatures, proxies, zip, package scripts.

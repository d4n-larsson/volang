# Feature Specification: Package Manager (Versions and Dependencies of Dependencies)

**Feature Branch**: `061-package-manager`

**Created**: 2026-11-26

**Status**: Draft

**Input**: User description: "specify package manager, we need some better way of handling future
dependencies". Today packages are listed in `vo.packages` (one `NAME DIRECTORY sha256:HASH [URL]` line
per package; `vo package ENTRY NAME DIRECTORY|URL`, `--restore`, `--remove`). Specify what a package
manager for vo needs beyond that: versions, dependencies of dependencies, updates, and fetches that are
reproducible and verifiable, within the constitution (agent-first, determinism, minimal surface).

Background: vo pins packages by content (spec 007) and can download one from an archive URL (spec 041).
Three things are missing, and each gets worse with every package written:

- **A package cannot say what it needs.** A package that imports another package only works when the
  program's author finds that out from an unresolved import and adds the second package by hand, guessing
  which release the first was written against.
- **There are no versions.** A manifest line holds a hash and, at best, a URL with a tag somewhere in
  it. Nothing says "this is 1.2.0", so nothing can say "1.3.0 exists" or "these two packages need
  different releases of the same thing".
- **Updating is re-adding.** Moving to a newer release means finding the new archive URL and running
  the add command again, for each package, in an order the author works out.

What already works stays the base: the content hash is the only thing trusted, nothing is fetched by
`check`, `run`, `test` or `build`, and adding a package shows the effects it declares.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Add a package and get what it needs (Priority: P1)

A developer (or an agent) adds a package by naming where it lives and, optionally, which version. The
toolchain fetches that version, reads the package's own list of dependencies, fetches those too, and
writes every package it added to the program's manifest, each with its version and content hash. The
program then checks and runs with no further step.

**Why this priority**: it is the feature; without it every dependency of a dependency is found by trial.

**Independent Test**: a local server holds three packages, `report` needing `money` 1.2.0 and `money`
needing `pads` 1.0.0. One add command for `report` leaves three entries in the manifest and three
directories on disk; a program importing only `report` checks and runs.

**Acceptance Scenarios**:

1. **Given** a package source and no version, **When** it is added, **Then** its highest released
   version is fetched, and the manifest entry records the name, source, version and content hash.
2. **Given** a source and a version (`SOURCE@1.2.0`), **When** added, **Then** exactly that version is
   fetched; a version that does not exist fails naming the versions that do (the nearest five).
3. **Given** a package whose own manifest lists dependencies, **When** it is added, **Then** each
   dependency (and theirs, to any depth) is added at the version selected by the rule of User Story 2,
   and the output has one line per package: added, updated (old and new version), or unchanged.
4. **Given** any package added or updated by the command, **Then** its declared effects are shown as
   today (spec 041), and for an updated one what it gained and lost; one total line gives the union of
   what the whole command added to the program's reach.
5. **Given** `--dry-run`, **Then** the same report is printed and nothing on disk changes.
6. **Given** any failure (a fetch, a refused archive, a conflict), **Then** the manifest and every
   package directory are as before the command: an add is all or nothing.
7. **Given** a local directory or a plain archive URL (today's forms), **Then** they keep working as
   they do now; such an entry has no version.

---

### User Story 2 - One predictable version of each package (Priority: P1)

A program holds exactly one version of each package. When several packages need the same one, the
version is chosen by a rule a person can apply by reading the manifests, with no search and no
dependence on what happens to be newest on the day: the highest version that any of them asks for.
Requirements that cannot be met together are an error that names who asks for what.

**Why this priority**: resolution that depends on time or on a solver's choices is the opposite of
determinism; an agent must be able to predict the result and a reviewer to check it.

**Independent Test**: with `report` needing `money` 1.2.0 and `ledger` needing `money` 1.4.0, adding
both in either order gives `money` 1.4.0, although 1.6.0 exists; the manifests are byte-identical for
both orders. With `ledger` needing `money` 2.0.0 instead, the command fails naming `report` (1.2.0) and
`ledger` (2.0.0) and changes nothing.

**Acceptance Scenarios**:

1. **Given** a package required at several versions with the same major number, **Then** the highest
   required one is selected; no version newer than every requirement is ever selected.
2. **Given** the program's own entry for a package at 1.5.0 and a dependency requiring 1.2.0, **Then**
   1.5.0 stays; given a dependency requiring 1.7.0, the entry is raised to 1.7.0 and the output says
   which package required it.
3. **Given** requirements with different major numbers (or, below 1.0.0, different minor numbers),
   **Then** the command fails with an error naming each requiring package and its version; there is no
   fix, and nothing is changed.
4. **Given** the same manifest and the same command, **Then** the selected versions and the written
   manifest are identical whatever the machine or the date. Versions never go down unless the author
   names a lower one; where a higher version of a package requires at least what its lower versions do,
   the order of the commands that added the packages does not matter either (amended during
   implementation, research R4).
5. **Given** two different sources required under one package name, or one source under two names,
   **Then** the command fails naming both; a program has one name per source.
6. **Given** a dependency whose own manifest pins a package to the same version as the program's but
   with a different content hash, **Then** the command fails: that version's content changed after
   someone pinned it.

---

### User Story 3 - A fresh clone builds the same thing (Priority: P1)

The manifest is the complete record: every package the program needs, direct or not, with its source,
version and content hash. From it alone, on any machine, one command restores byte-identical package
contents, or fails. No other command touches the network.

**Why this priority**: reproducible and verifiable fetches are the reason to have a manifest.

**Independent Test**: delete every package directory of the three-package program and restore: `vo
check` passes and the directories hash to their pins. Change what the server serves for one version:
restore fails for that package with the existing hash error and writes nothing for it.

**Acceptance Scenarios**:

1. **Given** a manifest and missing directories, **When** restore runs, **Then** every package with a
   source is fetched at its recorded version and accepted only if its content hash equals the pinned
   one; the manifest is never changed by restore.
2. **Given** a manifest that lacks a package one of its packages needs, or holds a lower version than
   one requires (a hand edit, a merge), **Then** `vo check` reports it as an error naming the package,
   the requirer and the command that repairs it; nothing is fetched by `check`.
3. **Given** `check`, `run`, `test`, `build` or `doc` on any project, **Then** no network connection is
   made on behalf of packages (unchanged from spec 041).
4. **Given** two developers who add different packages on separate branches, **When** the manifests are
   merged as text, **Then** a repair command brings the manifest to the state the selection rule gives,
   and reports each change.

---

### User Story 4 - See what is newer, and take it (Priority: P2)

A developer asks which packages have newer versions and updates one, or all, to the newest compatible
version. The report shows, per package, the version change, the hash change and what the package's
effects gained and lost, before or instead of writing anything.

**Why this priority**: without it versions are recorded but never move; with it an update is one
reviewed step instead of a hunt for URLs.

**Independent Test**: with `money` at 1.2.0 and 1.3.0, 1.4.0 and 2.0.0 released, the outdated listing
shows 1.4.0 as the compatible update and 2.0.0 as a newer major; update moves to 1.4.0; an update whose
new version declares `process` shows that gain.

**Acceptance Scenarios**:

1. **Given** a project, **When** the outdated listing runs, **Then** each package with a newer version
   is listed with its current version, the highest compatible version and, if any, the highest version
   of a newer major; nothing is changed. With nothing newer, it says so and exits 0.
2. **Given** an update of one package, **Then** it moves to the highest compatible version, and its
   dependencies are raised where the new version requires it; other packages are untouched.
3. **Given** an update of all, **Then** every direct package moves to its highest compatible version;
   packages that are only needed by others move only as far as their requirers ask.
4. **Given** a move to a newer major, **Then** it happens only when the version is named explicitly
   (`SOURCE@2.0.0`), never by update.
5. **Given** `--dry-run` or `--json`, **Then** the report is the same content, written or not, as
   records.

---

### User Story 5 - Write a package that others can depend on (Priority: P2)

A package author lists the package's dependencies in the package's own manifest, with the same file and
the same commands a program uses, and releases a version by tagging the repository. Nothing is uploaded
anywhere else and no account is needed beyond the host of the repository.

**Why this priority**: dependencies of dependencies exist only if packages can declare them; one
manifest form for programs and packages keeps the surface small.

**Independent Test**: a package directory with its own manifest and tests: `vo test` runs there after a
restore; once tagged on the local server, a program adds it by version and gets its dependencies.

**Acceptance Scenarios**:

1. **Given** a package's repository with a manifest at the package's root, **When** a program adds the
   package, **Then** the manifest's entries with a source and version are its requirements; the
   directories and hashes written there serve the author's own checkout only.
2. **Given** a package's manifest entry without a source (a local directory), **When** a program adds
   the package, **Then** the add fails naming that entry: a dependency nobody else can fetch.
3. **Given** a release, **Then** its version is the repository tag (`v1.2.0` or `1.2.0`); tags that are
   not three numbers are not versions and are ignored by listings and selection.
4. **Given** a package inside a larger repository, **Then** a path within the repository names the
   package's root, as `#PATH` does today.

---

### User Story 6 - Remove a package and what only it needed (Priority: P3)

Removing a package also removes the packages that were there only because it needed them, and refuses
when another package still needs the one named.

**Acceptance Scenarios**:

1. **Given** `report` (direct) with `money` and `pads` only it needs, **When** `report` is removed,
   **Then** all three entries and directories go, each reported.
2. **Given** a package another one requires, **When** its removal is asked, **Then** the command fails
   naming the requirer.
3. **Given** a package the program's own files import, **Then** removal proceeds as today; `vo check`
   reports the unresolved imports.

---

### User Story 7 - Documented (Priority: P3)

The command page, the guide's packages chapter, the reference's packages sentence and `vo explain` for
each new code describe versions, the selection rule, restore, updates and what is trusted.

**Acceptance Scenarios**:

1. **Given** the reference, **Then** it states the selection rule and the commands in the words it has
   room for, and stays under 15,000 tokens (5 tokens of headroom today, so other text is tightened).
2. **Given** the guide, **Then** one worked example adds a package with a dependency, updates it and
   restores it on a fresh clone.

### Edge Cases

- A dependency cycle between packages (A needs B, B needs A): allowed when the versions agree, as
  imports between their files are already checked for cycles; resolution terminates because each
  package is visited once per version.
- A package listed in a dependency's manifest but never imported by it: still added (the manifest is
  the declaration); the author removes it there.
- A source that is unreachable during add or update: the command fails and changes nothing; packages
  already on disk at the needed version are not fetched again.
- A version tag that is deleted upstream after being pinned: restore fails for it with the fetch error;
  a project that keeps its package directories in version control is unaffected.
- A version tag moved to other content: detected by the content hash on restore and by acceptance
  scenario 2.6 on add.
- A manifest written by the previous toolchain (no versions): read as it is; its entries are
  version-less and take no part in selection until re-added with a source and version.
- A manifest with versions read by an older toolchain: reported as malformed there; stated in the
  changelog.
- A program entry with a local directory for a package that a dependency requires by version: the local
  entry wins and the output says so (the way to work on a dependency's source next to the program).
- Pre-release and build suffixes (`1.2.0-rc1`, `1.2.0+linux`): not versions in this feature; ignored.
- Limits of spec 041 (download and unpacked size, entries, time) apply per package; a new limit bounds
  the number of packages in one resolution (1,000).
- Two packages whose files would unpack to the same directory: impossible, a directory belongs to one
  name.
- Private repositories and credentials: not supported; URLs with credentials stay refused.

## Requirements *(mandatory)*

### Functional Requirements

**Sources and versions**

- **FR-001**: A package source MUST be a repository address on any host that serves it over HTTPS, with
  an optional path inside it; a released version is a tag of the form `MAJOR.MINOR.PATCH`, with or
  without a leading `v`.
- **FR-002**: The toolchain MUST list a source's versions and fetch the content of one version without
  running an external program and without host-specific accounts or tokens.
- **FR-003**: A manifest entry for a versioned package MUST record name, directory, content hash,
  source and version; entries without a version (local directories, plain archive URLs) remain valid.
- **FR-004**: The manifest MUST list every package the program needs, direct or not; it is the only
  input to restore and to `check`.

**Adding and selection**

- **FR-005**: Adding a package MUST add, to any depth, the packages its manifest requires, in one
  all-or-nothing step, and report one line per package with its effects (and gains and losses on a
  version change), plus the union for the command.
- **FR-006**: Exactly one version of a package MUST be selected per program: the highest version
  required by the program's manifest or by any selected package's manifest. No newer version is
  selected unless a requirement names it.
- **FR-007**: Versions are compatible when their major numbers are equal (below 1.0.0: major and minor).
  Incompatible requirements for one package MUST fail with an error naming each requirer and version.
- **FR-008**: The result of a command MUST be a function of the manifests and the command alone:
  independent of machine, date and of versions released later. A command never lowers a version it was
  not asked to lower. (Independence of command order holds where higher versions require at least what
  lower ones do; in general it would need the manifest to remember every version ever asked for.)
- **FR-009**: One name MUST map to one source and one source to one name within a program; a violation
  is an error naming both.
- **FR-010**: The same version pinned with two different content hashes (in the program's manifest and
  in a dependency's) MUST be an error.

**Reproducing and verifying**

- **FR-011**: Restore MUST fetch each missing versioned package at its recorded version and accept it
  only when its content hash equals the pin; it never changes the manifest.
- **FR-012**: `vo check` MUST report, without any network access, a manifest that lacks a required
  package or holds a version lower than a selected package requires, naming the repair command.
- **FR-013**: A repair command MUST bring a manifest (after a merge or hand edit) to what the selection
  rule gives, reporting each change, with `--dry-run`.
- **FR-014**: No command other than the package command's add, update, outdated, restore and repair
  forms MUST open a network connection on behalf of packages.

**Updating and removing**

- **FR-015**: An outdated listing MUST show, per package, the current version, the highest compatible
  version and the highest version of a newer major, changing nothing.
- **FR-016**: Update MUST move one named package, or all direct packages, to the highest compatible
  version and raise dependencies only as far as required; a newer major is taken only when named.
- **FR-017**: Remove MUST also remove packages that no remaining package and no direct entry requires,
  and MUST refuse to remove a package that a remaining package requires.
- **FR-018**: The manifest MUST tell direct packages (asked for by the program's author) from the ones
  present only because another package requires them.

**Fit**

- **FR-019**: Every form MUST have `--json` records (schema 1, new values and fields only) and
  `--dry-run`; every failure leaves manifest and directories unchanged.
- **FR-020**: New situations get catalog codes with entries: a version conflict, a name or source
  clash, a missing or too-low required package (its message names the repair command; a manifest
  edit is not a source fix), a changed version, an unknown version. Existing codes are reused where they fit (hash mismatch, missing directory, fetch failure).
- **FR-021**: The safety rules and limits of spec 041 apply to everything fetched; only `https://`,
  verified, no credentials.
- **FR-022**: No libc, no external program, three architectures with byte-identical output, no static
  buffer over 64 KiB.
- **FR-023**: The site, the guide, the reference (within its budget), stability page, changelog and
  roadmap are updated; the repository's own package (`packages/postgres`) gets a manifest if it needs
  one, and one example program uses a versioned package with a dependency in the test suite.

### Key Entities

- **Source**: where a package lives: a repository address and an optional path inside it.
- **Version**: three numbers taken from a tag of the source; ordered; compatible within a major.
- **Manifest entry**: name, directory, content hash, and for a versioned package its source and
  version, and whether it is direct.
- **Requirement**: an entry with source and version in a package's own manifest, read when a program
  uses that package.
- **Selection**: for each name, the highest required compatible version; the content of the program's
  manifest after any add, update, remove or repair.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A package with two levels of dependencies is added with one command, and the program
  importing it runs; before this feature the same took one command per package plus finding each URL.
- **SC-002**: For a set of at least 50 generated dependency graphs (shared dependencies, raised
  versions, conflicts, cycles), every order of add commands gives the manifest an independent
  computation of the rule gives; in the graphs whose versions only add requirements, every order gives a
  byte-identical manifest; every conflict is reported with all requirers named.
- **SC-003**: After deleting all package directories of a 10-package program, one restore command
  brings back contents that hash to their pins; a version whose content was changed upstream is refused
  in 100% of trials.
- **SC-004**: With the network unavailable, `check`, `run`, `test` and `build` behave identically on a
  project with versioned packages; a trace of system calls shows no socket.
- **SC-005**: Resolving and checking a 100-package graph already on disk takes under one second,
  excluding downloads.
- **SC-006**: An update that gains an effect shows the gain for 100% of the cases in the test set, and
  `--dry-run` changes no byte on disk.
- **SC-007**: Manifests written by the previous version are read without change in behavior (every
  existing golden case of packages passes unchanged).
- **SC-008**: An agent given only the reference and the command's output adds, updates and restores a
  package in the benchmark task set without a failed command caused by the package commands' interface.
- **SC-009**: The suite passes on the three architectures and the reference stays under 15,000 tokens.

## Assumptions

- **No registry.** Packages are found by their repository address; discovery (search, an index, names
  without addresses) is out of scope, as the 2026-11 review advised ("a registry can wait").
- **Versions are repository tags**, so releasing needs no service of vo's own. Listing tags and fetching
  one version from any HTTPS git host without an external program is assumed feasible within the
  toolchain's constraints; the plan verifies it first and, if it is not, narrows FR-001/FR-002 (for
  example to hosts whose archive addresses are known) with the user.
- **Highest-required-version selection** (as Go modules do), not newest-available and not ranges: it
  needs no solver, gives the same answer every day, and the manifest stays readable. Version ranges,
  optional dependencies, features/flags, and several versions of one package in a program are out of
  scope.
- **One manifest form.** `vo.packages` serves programs and packages; no second file, and the existing
  command `vo package` grows forms instead of a new command. Exact spellings are for the plan.
- **Out of scope**: signatures and publisher identity (the content hash is what is trusted), private
  repositories and credentials, proxies and mirrors, a shared download cache between projects,
  pre-release versions, yanking, vulnerability advisories, and non-`.vo` files in packages.
- The content hash keeps its definition (the `.vo` files under the package directory), so existing pins
  stay valid.

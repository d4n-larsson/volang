# Research: Package Manager

Decisions for spec 061, from reading `src/packages.c`, `src/fetch.c`, `src/untar.c`, `src/inflate.c`,
`tests/steps.py`, spec 041's contracts, and from probing four hosts on 2026-11-26.

## R1. Versions come from any git host over HTTPS, without `git` (the spec's open assumption)

**Finding** (probe, `GET REPO/info/refs?service=git-upload-pack`, then `POST REPO/git-upload-pack` with
`want SHA shallow no-progress`, `deepen 1`, `done`): GitHub, GitLab, Codeberg (Forgejo) and sourcehut all
answer protocol version 0 with the full ref list, then `shallow`, `NAK` and a version-2 pack holding
exactly one commit and its trees and blobs. GitLab redirects to `REPO.git` (followed as today). Packs
contained REF_DELTA objects on GitLab and Codeberg although none was asked for (977 objects, 197 deltas;
6,637 objects, 522 deltas), none on GitHub and sourcehut for small repositories.

**Decision**: FR-001 and FR-002 stand as written. The toolchain gets a minimal git reader:

- pkt-line parsing of the ref advertisement; tags `refs/tags/[v]MAJOR.MINOR.PATCH` are versions (the
  peeled `^{}` line gives the commit of an annotated tag); when both `v1.2.0` and `1.2.0` exist, `v` wins.
- one `POST` per version: `want COMMIT shallow no-progress ofs-delta`, `deepen 1`, flush, `done`; no
  side-band (the pack follows `NAK` directly), no thin pack (never offered a `have`).
- pack reading: object headers, zlib streams (the existing inflate, plus the 2-byte zlib header and a
  "bytes consumed" result), OFS_DELTA and REF_DELTA application, SHA-1 of every object to index it (new
  `src/sha1.c`, about 80 lines; used for git object names only, never for trust), the pack's trailer
  checksum verified.
- commit → tree → the tree under `#PATH`: only `*.vo` blobs and the root `vo.packages` are written.
  Links (mode 120000) and submodules (160000) under the package root refuse the package, as links in an
  archive do (spec 041 FR-005).

Limits (spec 041 FR-006 applied): 16 MiB per response (ref list or pack), 64 MiB inflated in total,
10,000 objects written, 60 s per request. A repository whose one-commit pack exceeds 16 MiB fails with
that limit named. (`ponytail:` partial fetch with `filter blob:none` would lift it; add when a real
package hits the limit.)

**Rationale**: host-neutral, no account, no host table to maintain; about 550 lines of C on top of what
spec 041 built. **Alternatives**: archive URL templates per forge (a table of hosts, wrong for
self-hosted ones; and tag listing would still need this protocol); protocol v2 `ls-refs` with a
`refs/tags/` prefix (smaller answers, a second code path; not needed under 16 MiB); SHA-256
repositories (not offered by the probed hosts: refused with a message).

## R2. Manifest line format

**Decision**: the fourth field of a `vo.packages` line is, for a versioned package,
`https://HOST/PATH[#FOLDER]@MAJOR.MINOR.PATCH`; a fifth field, the word `indirect`, marks an entry present
only because another package requires it (FR-018). Lines without `@VERSION` are today's forms and stay
valid. `pkg_parse_line` (`packages.c:100`) splits the field at the last `@` when what follows is three
dot-separated numbers (credentials before the host are already refused, `packages.c:897`).

```text
money packages/money sha256:1e5b… https://codeberg.org/acme/money@1.4.0
pads packages/pads sha256:91c0… https://codeberg.org/acme/pads@1.0.2 indirect
```

**Rationale**: one file, one line per package, still sorted by name (`pkg_sort`), still readable and
mergeable as text. **Alternatives**: a second lock file (two files to keep in step; the manifest already
is the lock); a `requires` section (a second line form).

## R3. Telling a repository from an archive in the add command

**Decision**: the third operand is a repository when it has `@VERSION`, or when its path (before `#`)
does not end in `.tar`, `.tar.gz` or `.tgz`; otherwise it is today's archive form. Spec 041's examples
and golden cases all use those endings; an archive URL without one (none known) must now be saved and
added as a directory, stated in the changelog.

## R4. Selection (FR-006 to FR-010)

**Decision**: requirements are (a) every versioned entry of the program's manifest, direct or indirect,
and (b) every versioned entry of the manifest of each selected package. A command works in rounds:

1. `selected[name]` starts from (a), with the package the command names at the version asked.
2. A round reads every selected package not yet read at its version: its content (on disk when the
   entry's version and hash match, else fetched into staging, R1) and its `vo.packages`. When the round
   ends, what they require is applied: a new name is selected, a higher compatible version replaces the
   selected one (read in the next round). Rounds repeat until one reads nothing.
3. A requirement for a name with another source, or a source under another name: E0708. Incompatible
   versions (different major; below 1.0.0 different minor): E0707 listing every requirer. Same version,
   different hash than the selected content: E0710.
4. Indirect entries that no direct entry reaches through the requirements of the final versions are
   dropped (they came from a removed requirer or from a version since replaced), unless a reached
   package could not be read (its directory is missing: `--restore` first).

Versions only rise and each `name@version` is read once, so it terminates. A round's outcome does not
depend on the order inside it, so the result is a function of the manifest and the command (FR-008).
A program entry without a version (a local directory or an archive) for a required name wins and is
reported (`local`), per the spec's edge case. A requirement's entry without a version in a package's
manifest: E0709 at add time (US5 scenario 2).

**Found during implementation (spec amended)**: the result is not independent of the order of commands
in every graph. A version that was read and then replaced has already raised what it required, and a
manifest remembers selected versions, not every version once asked for. Example: `a` 1.0.0 requires `b`
1.1.0 and `a` 1.1.0 requires nothing; adding `a@1.0.0` and then something that needs `a` 1.1.0 leaves
`b` at 1.1.0, the other order never selects `b` 1.1.0. Go's modules behave the same way (a `go.mod`
keeps the higher version). Where a higher version requires at least what its lower versions do, the
order does not matter; `tests/package_graphs.py` checks both (every other generated graph is of that
kind and must give byte-identical manifests; all graphs must match an independent computation of the
rule for the order used). Order independence in every graph would need a second number per direct
package (the version asked for) and a fetch of every replaced version on every command.

Superseded versions fetched in step 2 are discarded (`ponytail:` fetching only `vo.packages` first
would save the download; add when graphs get deep).

**Rationale**: Go's minimal version selection, restricted to one compatible line per name; no solver,
no ranges, nothing time-dependent. `--update` and `--outdated` are the only forms that look at "newest".

## R5. What `vo check` verifies (FR-012) and what is trusted

**Decision**: the unpack step now also writes the package's root `vo.packages`. The content hash keeps
its definition (the `.vo` files), so existing pins hold. At `pkg_load`, for each versioned entry whose
directory has a `vo.packages`, its versioned lines are compared with the program's entries: a missing
name or a lower version is **E0709** at the requirer's line of the program's manifest, naming
`vo package --repair ENTRY`. No network, one small file per package (SC-005).

A dependency's manifest is not covered by the hash. That is safe: it can only add or silence an E0709,
or steer what `add`/`update`/`repair` propose, and every package those write is shown with its hash and
effects and then pinned by content in the program's manifest, which alone decides what code is loaded.
Restore never reads it.

## R6. Commands

**Decision**: all forms stay under `vo package` (contracts/package-command.md): add (existing shape,
`SOURCE[@VERSION]`), `--update ENTRY [NAME]`, `--outdated ENTRY`, `--repair ENTRY`, plus `--dry-run` on
every writing form. `--remove` prunes and refuses per FR-017. `--restore` gains the versioned fetch.
`--dir` applies to the named package of an add only; dependencies go to `packages/NAME`.

**All or nothing**: every package of a command is staged (`packages/NAME.new-PID`, as today), then old
directories are renamed away, new ones renamed in, the manifest written last; any failure before the
manifest write renames everything back. A crash inside that window can leave a directory that does not
match its pin: `vo check` says E0701, and deleting the directory and running `--restore` recovers. (The
same window exists today for one package.)

## R7. Codes

New: **E0707** `package_version_conflict`, **E0708** `package_source_clash`, **E0709**
`package_requirement` (missing or too low; also a requirement without a source), **E0710**
`package_version_changed`, **E0711** `package_version_unknown` (the message lists the nearest five
versions). None has a text fix (the repair is a command). Reused: E0701, E0702, E0703 (a malformed
version field), E0706 (fetch or pack refused).

## R8. Tests need a git host

**Decision**: `tests/git_server.py`, started by a new `server git` step of `tests/steps.py` (beside
`server https`, same certificates): at start it builds one repository per folder of the case's `repos/`
directory (`repos/money/1.2.0/…` becomes a commit tagged `v1.2.0`, fixed author and dates), and serves
the two endpoints by running `git upload-pack --stateless-rpc` (`git` is on the development and CI
machines; the toolchain under test never runs it). Options make it hostile: a moved tag, a truncated
pack, a bad trailer, an oversized answer. Packs with OFS_DELTA and REF_DELTA, which a small repository
does not produce on demand, come from `tests/package_git.py`, which writes packs by hand and compares
the unpacked files with the source for at least 200 generated packs (the counterpart of spec 041's
SC-005). `tests/package_graphs.py` generates the 50 graphs of SC-002.

## R9. Reference, version, migration

The reference has 5 tokens of headroom: the Packages bullet (`reference.md:1171`) is rewritten to the
same length (the `sha256:HASH` example and the remove sentence shortened; the selection rule in one
clause), with details on the site's command page. Ships as **0.7.0**: the manifest gains fields an older
toolchain reports as malformed (as spec 041 did); nothing existing changes meaning, so there is no
migration and no fix. `packages/postgres` imports only `std`: no manifest needed (FR-023).

## R10. SC-008 (agents)

Measured with three tasks added to bench2 (add a package with a dependency, update it, restore a clone)
against the local git server; reported in `results.md`. Not a gate for `make test`.

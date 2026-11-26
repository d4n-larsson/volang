# Implementation Plan: Package Manager (Versions and Dependencies of Dependencies)

**Branch**: `061-package-manager` (work on `main`, as for specs 001–060) | **Date**: 2026-11-26 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/061-package-manager/spec.md`

## Summary

A versioned package is a git repository at a tag, fetched over HTTPS by a small git reader in the
toolchain (research R1: verified against GitHub, GitLab, Codeberg and sourcehut). `vo.packages` gains a
`@VERSION` on the source field and an `indirect` mark (R2); the same file in a package states what it
requires. One rule selects versions: the highest compatible one that any manifest requires (R4). The
program's manifest stays the complete, content-pinned record; `vo check` verifies it is complete without
the network (R5). Commands stay under `vo package`: add, `--update`, `--outdated`, `--repair`, and the
existing `--restore` and `--remove` (R6). Five new codes (R7). Ships as 0.7.0 with no migration (R9).

| Part | File | What |
|---|---|---|
| Git reader | `src/gitfetch.c` (new), `src/sha1.c` (new), `src/inflate.c`, `src/fetch.c` | ref list, one-commit pack, deltas, tree walk; zlib streams with consumed length; `POST` with a body (R1) |
| Manifest | `src/packages.c` | version and `indirect` fields, parse and write (R2); the package's `vo.packages` unpacked |
| Selection | `src/packages.c` | requirements, fixpoint, conflicts, pruning (R4) |
| Commands | `src/packages.c`, `src/main.c` | add by source, `--update`, `--outdated`, `--repair`, `--dry-run`, remove with pruning, multi-package staging and commit (R6), reports and JSON |
| Check | `src/packages.c` (`pkg_load`) | E0709 completeness (R5) |
| Tests | `tests/git_server.py`, `tests/steps.py`, `tests/package_git.py`, `tests/package_graphs.py`, `tests/cases/packages/` | R8 |
| Docs | `docs/reference.md`, `docs/errors.md`, `site/docs/tools/package.md`, guide, `docs/stability.md`, `CHANGELOG.md` | R9 |

## Technical Context

**Language/Version**: C (C11, freestanding) for the toolchain; Python 3 for test fixtures.

**Primary Dependencies**: none in the toolchain (its own TLS, HTTP, inflate, SHA-256). Tests use the
host's `git` to serve repositories.

**Storage**: `vo.packages` (text, extended by two optional fields); package directories under
`packages/NAME`. No cache outside the project.

**Testing**: step cases with `server git` first and failing (one per acceptance scenario and code);
`package_git.py` (200 generated packs with both delta kinds, hostile packs), `package_graphs.py` (50
graphs, every add order), existing `package_fetch.py`, `packages_check.py`, `catalog.py`,
`ref_examples.py`, `site_examples.py`; `make quick AREA=packages` between edits, `make test` once per
change; three platforms at the end.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64, byte-identical output.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: `vo check` of a 100-package program under 1 s (one small manifest read per
package, SC-005); no change for programs without versioned packages.

**Constraints**: no libc, no external program; no static buffer over 64 KiB (packs live in mapped
memory, as archives do); spec 041's limits per response; only `https://`, verified; nothing fetched
outside the package command; the reference has 5 tokens of headroom.

**Scale/Scope**: about 1,400 lines of C (git reader 550, selection 250, commands and reports 450, manifest
and check 150), 700 of Python fixtures; 5 codes; roughly 40 step cases.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One manifest form for programs and packages; one selection rule that can be applied by reading; no ranges, no solver, no flags per dependency. Every error names the packages and versions involved and, where a command repairs it, the command. `--dry-run` shows any change before it is made. | Pass |
| II. Machine-readable feedback | Five codes with catalog entries; `package` JSON records gain fields only (schema 1). No text fix exists for a manifest problem: the message names `vo package --repair`. | Pass |
| III. Determinism | Selection is a function of the manifests (R4); restore accepts content by hash only; `check`, `run`, `test`, `build` never use the network; output byte-identical on three platforms. Only `--update` and `--outdated` look at what is newest, and only when asked. | Pass |
| IV. Test-first | Step cases per scenario and code against a local git host before the code; generated packs and graphs for the parsers and the selection. | Pass |
| V. Single source, minimal surface | Left out: registry, ranges, several versions of one package, optional dependencies, signatures, credentials, mirrors, a shared cache, pre-releases. Two `ponytail:` simplifications with stated ceilings (whole-pack fetch; fetching superseded versions). Reference word-neutral. | Pass |
| VI. Backend network services | **Write**: a service's dependencies (drivers, clients) and theirs arrive with one command at known versions. **Review**: the manifest diff of a change shows every package, version and hash that moved, and the command output shows each one's effects gained. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/061-package-manager/
├── plan.md
├── research.md          # R1–R10
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── package-command.md
│   ├── manifest.md
│   └── codes.md
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/gitfetch.c (new), src/sha1.c (new), src/pkgselect.c (new),
src/packages.c, src/fetch.c, src/inflate.c, src/main.c, src/vo.h, Makefile
tests/git_server.py (new), tests/package_git.py (new), tests/package_graphs.py (new), tests/steps.py,
tests/cases/packages/, tools/quick.sh
docs/reference.md, docs/errors.md, docs/stability.md, docs/roadmap.md, CHANGELOG.md,
site/docs/tools/package.md, site/docs/guide/modules-and-packages.md
```

**Structure Decision**: three new source files, each one job (git reader, SHA-1, selection);
`packages.c` keeps the manifest and the command.

## Order of work

1. **Git reader** (R1): `server git` fixture, `package_git.py`, then ref list, pack, deltas, tree walk;
   usable alone as "fetch SOURCE@VERSION into a directory".
2. **Manifest fields and single add** (R2, R3): `vo package ENTRY NAME SOURCE@VERSION` without
   dependencies; `--restore` for versioned entries. This is a usable increment (versions, no graph).
3. **Selection** (R4): requirements, conflicts, multi-package commit, `--dry-run`, reports (US1, US2).
4. **Check and repair** (R5): E0709, `--repair` (US3).
5. **Update, outdated, remove with pruning** (US4, US6).
6. **Docs, 0.7.0, three platforms, bench tasks** (R9, R10).

## Complexity Tracking

| Choice | Simpler alternative | Why not |
|---|---|---|
| A git pack reader in the toolchain (R1) | archive URL templates per forge | a host table that is wrong for self-hosted forges, and listing versions needs the git protocol anyway |
| SHA-1 beside SHA-256 | — | git names objects by SHA-1; used for lookup inside one pack only, never for trust |
| `indirect` mark in the manifest (R2) | derive it from package manifests | needed when directories are missing (before a restore) and to keep a removal from deleting what the author asked for |

# What a version of vo promises

vo is at version 0.6.0 with output schema 1 (`vo version`). This page says what may change, what may
not, and how a program is carried over when something does.

## Versions

The toolchain has a semantic version `MAJOR.MINOR.PATCH`. Before 1.0:

- a **patch** version (0.1.0 → 0.1.1) fixes defects and changes nothing a correct program can observe;
- a **minor** version (0.1 → 0.2) may add to the language, the library and the commands, and may make a
  breaking change under the rules below.

From 1.0, breaking changes need a new major version.

## What never changes

- **Error codes.** A code (`E0501`, `R0003`, `T0001`) is never reused or given another meaning. A code
  that is no longer produced stays in the catalog, marked retired.
- **The JSON schema within a version.** Every JSON record carries `"v":1`. Within schema 1, fields and
  record kinds are only added, never removed or renamed. A removal or rename is schema 2, announced a
  minor version before it becomes the default.
- **Determinism.** With one toolchain version, the same source, arguments and input give byte-identical
  output and exit status on every supported platform.
- **Exit statuses** of the commands, as listed on the documentation site.

## What may change in a minor version before 1.0

- **The language and the standard library.** A breaking change comes with a diagnostic that explains
  it and, wherever one correction exists, a mechanical fix, so `vo check --json` and applying the fixes
  migrates a program. This has been done six times: declared effects, from the first language version
  to the second; the consistency pass of 0.2.0 (library names, lookups giving an `Option`, durations in
  milliseconds, builtins no local may shadow; E0214 names each library change and its fix); the
  working directory of `std.process` in 0.3.0 (E0214 inserts `"./"`, and E0501's fix declares the
  scopes of literal paths, so no grant widens); module imports in 0.4.0 (E0215 rewrites a file's
  imports and uses in one fix); anonymous functions without written types in 0.5.0 (E0342 removes them
  or moves them onto the binding, E0413 writes `=> expr`; a function nothing gives types to is named or
  bound with its type by hand); and in 0.6.0, `std.time.Repeated` becoming the fields of
  `LocalError.repeated` (E0214 names the case; no fix, as the value is written differently). The first five
  migrations are kept as tests of every later toolchain
  (`tests/migrate.py`, `tests/migrate_056.py`, `tests/libchange_rows.py`, `tests/migrate_058.py`,
  `tests/migrate_059.py`).
- **Messages.** The text of diagnostics and reports may be reworded; their codes, spans and fixes are
  the interface.
- **Files the toolchain writes.** `vo.effects` and `vo.packages` are rewritten by `vo effects --update`
  and `vo package`. A recording (`--record`) replays with the version that made it and with later
  versions of the same schema; a format change that breaks old recordings is listed in the changelog.
  An executable from `vo build` contains its toolchain and keeps working unchanged.
- **Output of a program whose behavior was a defect.** Fixing a wrong result changes the output of
  programs that depended on it; the changelog lists each such fix.

## How changes are announced

Every release has a section in [`CHANGELOG.md`](../CHANGELOG.md) with its breaking changes first. The
reasoning behind each change is in its specification under `specs/`.

## Platforms

Linux on x86-64, ARM64 and RISC-V 64. All three are built and tested for every release, and a program
gives the same output on each.

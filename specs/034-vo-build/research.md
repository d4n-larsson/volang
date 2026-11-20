# Research: One Executable per Program (`vo build`)

## R1. The executable: a toolchain with the program appended

**Decision**: OUTPUT = the base's bytes, then the payload (R2), then a 64-byte trailer at the very end
(data-model.md). The base is the building toolchain (`/proc/self/exe`) unless `--base FILE` names
another. The kernel loads an ELF executable by its program headers and ignores bytes after them, so the
appended data changes nothing about how the binary runs.

**Rationale**: no linker, no ELF rewriting, the same bytes on every architecture; the toolchain already
carries the runtime, the standard library, the reference and the catalog.

**Alternatives considered**: a new ELF section (needs ELF editing per architecture); a compiled
bytecode form (serializing types, nodes and code: a large new surface; the spec chose re-checking at
start); a separate small runtime binary (the runtime is most of the toolchain: little saved).

## R2. What is embedded

**Decision**: the payload holds the toolchain version text, the entry path, and every file the build's
check loaded except the standard library: the program's files and package files in load order
(`ld_files()`), with the bytes the loader saw (for packages, the bytes that were hashed), and the
package manifest `vo.packages` when the program has one. `vo.effects` is not embedded: the build already
compared it (FR-001, E0504), and `--vo-effects` reports what `main` declares (R6). Paths are kept exactly
as the loader saw them during the build, so the app's loader makes the same path computations and
reports the same positions.

**Rationale**: re-checking at start needs exactly the loader's inputs; the standard library is in the
base already and must match its version (R8).

## R3. Integrity and determinism

**Decision**: the trailer holds a magic text, the payload's offset and length, and the payload's SHA-256
(`src/sha256.c`). A `vo` start that finds the magic but a bad offset, length or hash stops with T0014
`damaged_executable` (exit 66) and runs nothing (FR-008). The payload has no time, no absolute path
that was not in the build's own paths, and files in load order, so the same build gives the same bytes
(FR-002). The output is written to a temporary file next to it (`tmp_path`/`open_tmp`), made executable
(`fchmod 0755`) and renamed over OUTPUT, so a failed build leaves nothing (FR-003).

## R4. Loading in an app

**Decision**: at start the app registers every embedded file with `file_add(path, text, len)` (the
manifest too) and sets `ld_app`:

- `load_file` finds them by path (`file_find`) and never reads the disk; an import of a file that is not
  embedded is the ordinary missing-import error (cannot happen for a program that built);
- `inside_tree` answers 1 (only embedded files can load);
- `pkg_load` takes the embedded manifest instead of searching from the directory, `pkg_verify` accepts
  every package (their bytes were verified by the build and are covered by the payload hash), and the
  E0705 directory probe is skipped;
- `vo.effects` is never looked for (only `vo check` reads it; `vo run` does not).

`ld_app` is cleared before the program runs, so the program's own file reads go to the disk as under
`vo run`.

**Rationale**: the loader's paths are strings; with the same strings registered, every step that only
joins strings works unchanged, and the few steps that touch the disk get one branch each.

## R5. The app's command line

**Decision**: the app rewrites its arguments into the toolchain command it stands for and continues
through the ordinary option parser (so each option means exactly what it means for `vo run`, FR-010):

| App arguments | Toolchain command |
|---|---|
| (default) | `vo run [OPTS] --timeout 0 ENTRY -- ARGS` (`--timeout 0` unless `--vo-timeout` is given) |
| `--vo-effects` | the `main` line of `vo effects [--json] ENTRY` (R6) |
| `--vo-replay FILE` | `vo replay [OPTS] FILE` with the embedded entry (R7) |
| `--vo-version` | `vo version [--json]` |

`OPTS` are the given `--vo-NAME [VALUE]` options with the prefix removed: `seed`, `clock-ms`,
`timeout`, `max-memory`, `mem-stats`, `trace`, `trace-limit`, `record`, `ca-file`, `workers`, `json`.
Every argument not starting with `--vo-` is a program argument, in order. An unknown `--vo-` option is
T0001 listing the app options (FR-012).

**Alternatives considered**: a separate option parser for apps: two definitions of each option.

## R6. `--vo-effects`

**Decision**: prints the `main` line of `vo effects` (`main uses console, net("localhost")`) and, with
`--vo-json`, its `capabilities` record; nothing else, exit 0, the program does not run.

## R7. `--vo-replay`

**Decision**: `vo replay` runs the program named in the recording; in an app it runs the embedded entry
instead, whatever path the recording names (a recording made by `vo run` of the same sources replays
too). Everything else is `vo replay`'s behavior, including the note for a changed program.

## R8. Bases for other architectures

**Decision**: `--base FILE` reads FILE; it must start with the ELF magic, contain the building
toolchain's exact version text (`vo 0.1.0 schema 1`, one constant shared with `vo version`) and carry no
trailer (not an app). Otherwise T0001 naming which. The version text pins the embedded standard library
and the checker, so the app checks its program exactly as the build did.

## R9. App start cost

**Decision**: every `vo` start opens `/proc/self/exe`, seeks to its end and reads 64 bytes; without the
magic it closes it and goes on as the toolchain. With the magic it reads the payload with `pread`
(not the whole executable). Linux with `/proc` mounted (containers mount it) is assumed; without it a
`vo` binary is the toolchain, and an app reports T0014.

## R10. Documentation

**Decision**: the reference's command line gains `vo build [--json] [-o OUTPUT] [--base TOOLCHAIN] FILE`
and one bullet on apps (`--vo-` options, `--vo-effects`, no default timeout). The catalog gains T0014.
A new site page `site/docs/guide/shipping.md` (build, run, `--vo-effects`, recording, other
architectures, a `FROM scratch` container) linked from the guide's navigation. Roadmap row and section;
README; the review index.

## R11. Tests

**Decision**:

- `tests/steps.py` gains `app PATH ARGS... [< FILE]`: runs a built executable, natively or under the
  same emulator as `$VO_BIN` (so the cross runs work).
- Step cases `tests/cases/build/`: `basic` (build, then run from another directory with arguments and
  stdin, compare), `multi` (several files and a package), `errors` (check errors: diagnostics, no file),
  `record_mismatch` (E0504 refuses), `effects` (`--vo-effects`, `--vo-json`), `options` (prefix
  options, program `--seed` and `--help` passed through, unknown `--vo-` option), `record_replay`,
  `runtime_error` (positions), `damaged` (a truncated app: T0014), `base` (a bad base, an app as base),
  `output` (default name, unwritable directory, `-o` naming a source file), `same_bytes` (two builds
  equal).
- `tests/build_examples.py`: every example built from its directory; each recording replayed with the
  app and with `vo replay`, outputs compared (SC-001); a run recorded by the app replayed by it (SC-002);
  two builds compared (SC-003); start time against `vo run` on the largest example (SC-004). Part of
  `make test`.

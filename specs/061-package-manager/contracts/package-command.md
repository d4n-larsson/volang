# Contract: `vo package`

```text
vo package [--json] ENTRY NAME DIRECTORY                                   (unchanged)
vo package [--json] [--dir D] [--ca-file F] ENTRY NAME ARCHIVE_URL         (unchanged; ends .tar, .tar.gz, .tgz)
vo package [--json] [--dry-run] [--dir D] [--ca-file F] ENTRY NAME SOURCE[@VERSION]
vo package [--json] [--dry-run] [--ca-file F] --update ENTRY [NAME]
vo package [--json] [--ca-file F] --outdated ENTRY
vo package [--json] [--dry-run] [--ca-file F] --repair ENTRY
vo package [--json] [--ca-file F] --restore ENTRY
vo package [--json] [--dry-run] --remove ENTRY NAME
```

`SOURCE` is `https://HOST/PATH[#FOLDER]`: a git repository served over HTTPS and, optionally, the folder
of the package inside it. `VERSION` is `MAJOR.MINOR.PATCH`; the tag is `vVERSION` or `VERSION`.

## Add

Without `@VERSION` the highest version is taken. The named package becomes a direct entry; everything
selection (research R4) adds is `indirect`. Output, one block per package in name order, then a total
when more than one package changed:

```text
added report 2.1.0 packages/report sha256:77ab… from https://codeberg.org/acme/report
  effects: file.read
added money 1.4.0 packages/money sha256:1e5b… (required by report)
  effects: none
updated pads 1.0.0 -> 1.0.2: sha256:0c11… -> sha256:91c0… (required by money)
  effects: none
total gains: file.read
```

`unchanged NAME VERSION` for a package already as selected. `local NAME (required by X at 1.2.0)` for a
version-less entry kept. With `--dry-run` the same text, the first line `dry run: nothing written`.

## Update, outdated

`--update ENTRY NAME` requires NAME at its highest compatible version, `--update ENTRY` every direct
package; then selection. Output as for add. `--outdated` prints, per package with something newer:

```text
money 1.2.0: 1.4.0 available; 2.0.0 is a new major (add it with @2.0.0)
pads 1.0.2: 2.1.0 is a new major (add it with @2.1.0)
```

or `all packages are at their newest version`. Exit 0 in both cases.

## Repair, restore, remove

`--repair` runs selection on the manifest as it is: adds what is required and missing, raises what is
too low, drops indirect entries nothing requires, fetches what is not on disk. `--restore` is unchanged
in meaning: missing directories only, each accepted at its pin, manifest untouched. `--remove NAME`
removes NAME and the indirect entries only it required, one `removed` line each, and refuses when a
remaining package requires NAME (E0709 naming it).

## JSON

`package` records (schema 1) gain fields, present when they apply: `version`, `old_version`, `source`,
`direct` (bool), `required_by` (list of names). New values of `change`: `local`, `outdated` (with
`compatible` and `newer_major`, each a version or null). A final record
`{"v":1,"kind":"package_total","gained":[...],"lost":[...]}` follows when more than one package changed.

## Statuses

| Code | When | Exit |
|---|---|---|
| T0001 | wrong operands; an option with a form that does not take it; a malformed `@VERSION` | 64 |
| E0706 | a request failed, or the ref list or pack was refused (corrupt, a limit, a link or submodule under the package root, a SHA-256 repository) | 65 |
| E0707 | incompatible versions of one package are required | 65 |
| E0708 | one name for two sources, or one source under two names | 65 |
| E0709 | `check`: a required package is missing or too low; add: a requirement without a source; remove: still required | 65 |
| E0710 | a version's content differs from what another manifest pinned for it | 65 |
| E0711 | the source has no such version (or no versions) | 65 |
| E0701, E0702, E0705 | as today | 65 |

On every failure the manifest and all package directories are as before the command.

## Guarantees

- Only the forms above that name a source, and `--update`, `--outdated`, `--repair`, `--restore`, open
  connections. `check`, `run`, `test`, `build`, `doc` never do.
- The same manifests give the same selection on every machine and day.
- What is loaded is what the program's manifest pins by content; versions and sources say where to ask.

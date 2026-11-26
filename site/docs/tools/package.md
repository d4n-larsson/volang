# vo package

`vo package` adds, updates or removes a package in the program's manifest, `vo.packages`, pinning the package's files by their hash. A package can be a directory, an archive, or a version of a git repository; a versioned package brings the packages it requires.

## Synopsis

```text
vo package [--json] ENTRY NAME DIRECTORY
vo package [--json] [--dir DIRECTORY] [--ca-file FILE] ENTRY NAME ARCHIVE_URL
vo package [--json] [--dry-run] [--dir DIRECTORY] [--ca-file FILE] ENTRY NAME SOURCE[@VERSION]
vo package [--json] [--dry-run] [--ca-file FILE] --update ENTRY [NAME]
vo package [--json] [--ca-file FILE] --outdated ENTRY
vo package [--json] [--dry-run] [--ca-file FILE] --repair ENTRY
vo package [--json] [--ca-file FILE] --restore ENTRY
vo package [--json] [--dry-run] --remove ENTRY NAME
```

A package is a directory of `.vo` files used by more than one program. `vo.packages` (found like
`vo.effects`: next to the entry file or in an ancestor directory) has one line per package,
`NAME DIRECTORY sha256:HASH`, with the URL after it when the package was fetched (see
[The manifest](#the-manifest)); `import NAME.a.b;` then loads `DIRECTORY/a/b.vo`. The hash covers every `.vo` file under the directory. When it no longer
matches, the program is not checked or run ([E0701](../errors/E0701.md)) until someone reviews the change
and runs `vo package` again. See the reference's
[Standard library and packages](../reference/standard-library-and-packages.md) and the guide's
[Modules and packages](../guide/modules-and-packages.md).

- `vo package ENTRY NAME DIRECTORY` computes the hash and adds the entry, or updates its directory and
  hash. The manifest is created next to `ENTRY` when there is none. `DIRECTORY` is written as given.
- `vo package ENTRY NAME ARCHIVE_URL` fetches the package first. The URL starts with `https://` and its
  path ends in `.tar.gz`, `.tgz` or `.tar`; `URL#PATH` takes the folder `PATH` inside the archive. The archive's `.vo`
  files are written to `packages/NAME` next to the manifest (or `--dir`), pinned, and the URL is
  recorded. Then the effects the package's exported functions declare are printed.
- `vo package ENTRY NAME SOURCE[@VERSION]` adds a version of a git repository and every package it
  requires: see [Versioned packages](#versioned-packages). Any other `https://` URL is a `SOURCE`.
- `vo package --update ENTRY [NAME]` moves `NAME`, or every package the program asked for itself, to its
  highest compatible version. `--outdated ENTRY` only lists what is newer.
- `vo package --repair ENTRY` completes a manifest after a merge or a hand edit.
- `vo package --restore ENTRY` fetches every entry that has a URL and whose directory is missing, and
  accepts each only when it hashes to its pin. It never changes the manifest.
- `vo package --remove ENTRY NAME` removes the entry, and for a versioned package the packages only it
  required.

`ENTRY` only places the manifest: it is not read. `NAME` is an identifier other than `std` ([T0001](../errors/T0001.md) otherwise). A directory that is
missing or has no `.vo` files is [E0702](../errors/E0702.md). A package name that is also a directory next to
the importing file is [E0705](../errors/E0705.md): rename one of them.

## Versioned packages

A versioned package is a git repository served over HTTPS; its versions are its tags `v1.2.0` or `1.2.0`
(three numbers; `v1.2`, `v2.0.0-rc1` and `release-3` are not versions). `SOURCE` is
`https://HOST/PATH`, with `#FOLDER` when the package is a folder inside the repository. Nothing is
installed for this: vo speaks git's HTTP protocol itself and asks for the one tagged commit.

```text
$ vo package main.vo report https://codeberg.org/acme/report
added money 1.4.0 packages/money sha256:d511... (required by report)
  effects: none
added pads 1.0.2 packages/pads sha256:0f76... (required by report, money)
  effects: none
added report 2.1.0 packages/report sha256:19ef... from https://codeberg.org/acme/report
  effects: random
total gains: random
```

- **Without `@VERSION`** the highest version is taken. The named package becomes the program's own
  (direct); what it requires is marked `indirect` in the manifest.
- **What a package requires** is the versioned lines of the `vo.packages` at its root (under `#FOLDER`
  for a package in a folder). A line without a version there makes the package unusable for others
  ([E0709](../errors/E0709.md)).
- **One version of each package.** The program holds the highest version that its own manifest or the
  manifest of any selected package asks for. Versions are compatible within one major version, and
  below 1.0.0 within one minor version. Two requirements that are not compatible are
  [E0707](../errors/E0707.md); a newer major is taken only when you name it (`@2.0.0`).
- **Nothing depends on the day you run it.** The result is a function of the manifest and the command:
  a newer release changes nothing until `--update` or an explicit `@VERSION`. A command never lowers a
  version it was not asked to lower. (A version that was selected and then replaced has already raised
  what it required, so two orders of the same adds can differ where a newer version requires less than
  an older one.)
- **All or nothing.** Every package is fetched and checked before the first directory is replaced; on
  any failure the manifest and `packages/` are as they were. `--dry-run` prints the same report and
  writes nothing.
- **Agreement between manifests.** One name with two sources, or one source with two names, is
  [E0708](../errors/E0708.md). A version whose content differs from the hash a requiring package
  pinned is [E0710](../errors/E0710.md). A version the repository does not have is
  [E0711](../errors/E0711.md), with the nearest versions it does have.
- **Your own directory wins.** When the manifest has a line without a version for a required name, it
  stays, and the report says `local money (required by report at 1.4.0)`.

`vo check`, `vo run`, `vo test` and `vo build` fetch nothing. They compare each versioned package's own
manifest with the program's, and report a missing or too-low requirement as E0709 at the requiring
package's line; `vo package --repair ENTRY` then adds what is missing, raises what is too low, drops
`indirect` packages nothing requires any more, and fetches packages whose directories are missing.

```text
$ vo package --outdated main.vo
money 1.4.0: 1.6.0 available; 2.0.0 is a new major (add it with @2.0.0)
pads 1.0.2: 2.1.0 is a new major (add it with @2.1.0)
$ vo package --update main.vo money
updated money 1.4.0 -> 1.6.0: sha256:d511... -> sha256:b21e...
  effects: clock
  gains: clock
```

`--outdated` prints `all packages are at their newest version` when nothing is newer, and exits 0
either way. `--remove` of a package that a listed package requires is E0709 and removes nothing.

### The manifest

```text
NAME DIRECTORY sha256:HEX                                   a directory
NAME DIRECTORY sha256:HEX https://HOST/PATH.tar.gz[#PATH]   an archive
NAME DIRECTORY sha256:HEX https://HOST/PATH[#FOLDER]@1.4.0  a versioned package, the program's own
NAME DIRECTORY sha256:HEX https://HOST/PATH@1.0.2 indirect  one that another package requires
```

The hash covers the package's `.vo` files and is what decides which code is loaded. A package's own
`vo.packages` is not covered by it: it can only steer what `vo package` proposes, and every package a
command writes is shown with its hash and effects and then pinned in the program's manifest.

## Fetching

Nothing but `vo package` with a URL, `--update`, `--outdated`, `--repair` or `--restore` ever opens a
connection for packages.

| What | Rule |
|---|---|
| URL | `https://` only; no user name or password; up to 5 redirects to `https://` URLs; certificates verified as for `std.http` (`--ca-file` adds an authority; nothing turns verification off) |
| archive | tar, gzip-compressed or not (ustar, GNU long names, pax paths) |
| repository | git's smart HTTP protocol, one tagged commit without history; symbolic links and submodules among the package's files are refused |
| taken | regular files ending in `.vo`; one folder that wraps everything is dropped; with `#PATH`, only what is under it |
| refused | a link, a device or a FIFO anywhere in the archive; an absolute path, a `..` component or a control character in a name; a damaged or truncated archive |
| limits | 16 MiB downloaded, 64 MiB unpacked, 16 MiB per file, 10,000 entries, 60 seconds |
| all or nothing | the archive is read and accepted in memory before a file is written; on any failure the manifest and the package's previous directory are as they were |
| replaced | only the package's own directory, and only while it still hashes to its pin: a directory you edited is refused ([E0701](../errors/E0701.md)) |

A failed download or a refused archive is [E0706](../errors/E0706.md). An archive with no `.vo` file
(under `#PATH`) is E0702.

## Options

| Option | Meaning |
|---|---|
| `--json` | Print one `package` record (one per entry for `--restore`). |
| `--remove` | Remove the entry `NAME` instead of adding or updating it. |
| `--restore` | Fetch the listed packages whose directories are missing, each at its pinned hash. |
| `--dir DIRECTORY` | With a URL: where to write the package (default `packages/NAME` beside the manifest). |
| `--update` | Move `NAME`, or every direct package, to its highest compatible version. |
| `--outdated` | List the versioned packages that have a newer version. Writes nothing. |
| `--repair` | Bring the manifest to what its own entries require. |
| `--dry-run` | With a versioned add, `--update`, `--repair` or `--remove`: print the report, write nothing. |
| `--ca-file FILE` | With any form that fetches: also trust the certificate authorities in `FILE` (repeatable). |

## Output

One line on stdout:

```text
added money vendor/money sha256:1e5b...
updated money: sha256:1e5b... -> sha256:91c0...
unchanged money
removed money
```

For a fetched package, where it came from and what it declares (and on an update, the difference):

```text
added money packages/money sha256:1e5b... from https://example.com/money-1.2.0.tar.gz
  effects: none
updated money: sha256:1e5b... -> sha256:91c0...
  effects: net("rates.example.com")
  gains: net("rates.example.com")
  loses: file.read
```

`effects: could not be read (...)` means the package does not check; it is added all the same, and
`vo check` of a program that imports it reports the errors. `--restore` prints `restored NAME` or
`present NAME` per entry, or the entry's error.

A versioned package's lines carry its version, and for a package the program did not ask for itself,
who requires it: `added money 1.4.0 packages/money sha256:... (required by report)`,
`updated money 1.2.0 -> 1.4.0: sha256:... -> sha256:...`, `unchanged report 2.1.0`, `removed pads 1.0.2`,
`restored report 2.1.0`. When more than one package changed, `total gains:` and `total loses:` lines
sum the effects.

With `--json`, a versioned package's record also has `source`, `version`, `old_version`, `direct` and
`required_by`; `--outdated` records have the change `outdated` with `version`, `compatible` and
`newer_major` (a version or `null`); a `package_total` record with `gained` and `lost` follows when
more than one package changed.

With `--json`, a `package` record: `name`, `change` (`added`, `updated`, `unchanged`, `removed`), `directory`,
`hash` and, for an update, `old_hash`; a fetched package also has `url`, `effects` (a list of texts such as `net("rates.example.com")`), on an update `gained` and `lost`, and `note` when its effects could not be read. `--restore` uses the changes `restored` and `present`:

```json
{"v":1,"kind":"package","name":"money","change":"updated","directory":"money","hash":"sha256:50e85daff40266c785493bf611ae4d71e8f4994e2469a5c9742d42732cec3a32","old_hash":"sha256:6059faa4d82bd941b8c55d874f70121629a390173dd85164c4ffb864fb441e43"}
```

After a package's files change, `vo check` and `vo run` report:

```text
main.vo:1:1: error[E0701]: Package money changed: the manifest pins sha256:6059faa4d82bd941b8c55d874f70121629a390173dd85164c4ffb864fb441e43, its files hash to sha256:50e85daff40266c785493bf611ae4d71e8f4994e2469a5c9742d42732cec3a32. Review the change, then run `vo package main.vo money money`.
```

Package functions appear in `vo.effects` as `@NAME/path.vo:function`, and [vo doc](doc.md) documents
package modules: `vo doc money.format`.

## Exit statuses

| Status | When |
|---|---|
| 0 | The manifest was written (or the entry was unchanged); `--restore`: every package is present. |
| 64 | Usage error ([T0001](../errors/T0001.md)): wrong arguments, a bad name, or `--remove` of a name that is not listed. |
| 65 | Package error: the directory is missing or has no `.vo` files ([E0702](../errors/E0702.md)); a fetch failed or an archive was refused ([E0706](../errors/E0706.md)); a hash does not match ([E0701](../errors/E0701.md)); a versioned selection was refused ([E0707](../errors/E0707.md) to [E0711](../errors/E0711.md)). |

## Example

A package in `vendor/money`, used by `main.vo`:

```vo
# file: vendor/money/format.vo

# n cents as euros and cents, with two digits after the point.
export fn cents(n: int) -> str {
    return str(n / 100) + "." + two_digits(n % 100);
}

fn two_digits(n: int) -> str {
    if n < 10 {
        return "0" + str(n);
    }
    return str(n);
}
```

```vo
# file: main.vo
import money.format;

fn main() uses console {
    print(format.cents(1205));
}
```

```console
$ vo package main.vo money vendor/money
added money vendor/money sha256:1e5b240df8daf715941d8af2629ab6404c7f9d1d369e493f0e9eb2d01fc162c3
$ vo run main.vo
12.05
$ vo doc money.format
fn cents(n: int) -> str
    n cents as euros and cents, with two digits after the point.
$ vo package --json main.vo money vendor/money
{"v":1,"kind":"package","name":"money","change":"unchanged","directory":"vendor/money","hash":"sha256:1e5b240df8daf715941d8af2629ab6404c7f9d1d369e493f0e9eb2d01fc162c3"}
$ vo package --remove main.vo money
removed money
$ vo check main.vo
main.vo:2:1: error[E0205]: Cannot find `money/format.vo` for this import.
   2 | import money.format;
     | ^^^^^^^^^^^^^^^^^^^^
1 error, 0 warnings
```

`vo.packages` held `money vendor/money sha256:1e5b...` until the removal; without the entry, `money` is
looked up as a directory next to `main.vo` ([E0205](../errors/E0205.md)). Only exported items are visible
outside the package: `two_digits` is private to its file.

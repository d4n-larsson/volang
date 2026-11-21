# vo package

`vo package` adds, updates or removes a package in the program's manifest, `vo.packages`, pinning the package's files by their hash.

## Synopsis

```text
vo package [--json] ENTRY NAME DIRECTORY
vo package [--json] [--dir DIRECTORY] [--ca-file FILE] ENTRY NAME URL
vo package [--json] [--ca-file FILE] --restore ENTRY
vo package [--json] --remove ENTRY NAME
```

A package is a directory of `.vo` files used by more than one program. `vo.packages` (found like
`vo.effects`: next to the entry file or in an ancestor directory) has one line per package,
`NAME DIRECTORY sha256:HASH`, with the URL after it when the package was fetched; `import NAME.a.b.item;`
then loads `DIRECTORY/a/b.vo`. The hash covers every `.vo` file under the directory. When it no longer
matches, the program is not checked or run ([E0701](../errors/E0701.md)) until someone reviews the change
and runs `vo package` again. See the reference's
[Standard library and packages](../reference/standard-library-and-packages.md) and the guide's
[Modules and packages](../guide/modules-and-packages.md).

- `vo package ENTRY NAME DIRECTORY` computes the hash and adds the entry, or updates its directory and
  hash. The manifest is created next to `ENTRY` when there is none. `DIRECTORY` is written as given.
- `vo package ENTRY NAME URL` fetches the package first. `URL` starts with `https://` and names a
  `.tar.gz`, `.tgz` or `.tar` archive; `URL#PATH` takes the folder `PATH` inside it. The archive's `.vo`
  files are written to `packages/NAME` next to the manifest (or `--dir`), pinned, and the URL is
  recorded. Then the effects the package's exported functions declare are printed.
- `vo package --restore ENTRY` fetches every entry that has a URL and whose directory is missing, and
  accepts each only when it hashes to its pin. It never changes the manifest.
- `vo package --remove ENTRY NAME` removes the entry.

`ENTRY` only places the manifest: it is not read. `NAME` is an identifier other than `std` ([T0001](../errors/T0001.md) otherwise). A directory that is
missing or has no `.vo` files is [E0702](../errors/E0702.md). A package name that is also a directory next to
the importing file is [E0705](../errors/E0705.md): rename one of them.

## Fetching

Nothing but `vo package` with a URL or `--restore` ever opens a connection for packages.

| What | Rule |
|---|---|
| URL | `https://` only; no user name or password; up to 5 redirects to `https://` URLs; certificates verified as for `std.http` (`--ca-file` adds an authority; nothing turns verification off) |
| archive | tar, gzip-compressed or not (ustar, GNU long names, pax paths) |
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
| `--ca-file FILE` | With a URL or `--restore`: also trust the certificate authorities in `FILE` (repeatable). |

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
| 65 | Package error: the directory is missing or has no `.vo` files ([E0702](../errors/E0702.md)); a fetch failed or an archive was refused ([E0706](../errors/E0706.md)); a hash does not match ([E0701](../errors/E0701.md)). |

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
import money.format.cents;

fn main() uses console {
    print(cents(1205));
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
   2 | import money.format.cents;
     | ^^^^^^^^^^^^^^^^^^^^^^^^^^
1 error, 0 warnings
```

`vo.packages` held `money vendor/money sha256:1e5b...` until the removal; without the entry, `money` is
looked up as a directory next to `main.vo` ([E0205](../errors/E0205.md)). Only exported items are visible
outside the package: `two_digits` is private to its file.

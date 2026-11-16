# vo fmt

`vo fmt` rewrites source files in the one canonical layout, or with `--check` only lists the files that are not in it.

## Synopsis

```text
vo fmt [--json] [--check] PATH...
```

`PATH` is a `.vo` file or a directory (every `*.vo` file under it, recursively, in sorted order). Only
the files given are formatted, not the files they import.

The canonical form is described in the reference's [Command line](../reference/command-line.md) section:
4-space indent, `{` on the same line, one statement per line, no blank lines inside blocks, one blank line
between items, single spaces around binary operators and after `,` and `:`, parentheses only where needed,
and effects in canonical order. Comments are kept.

`fmt` needs the files to parse, not to check: a file with a type error is formatted. A syntax error in
any of the files stops the command before it writes anything.

## Options

| Option | Meaning |
|---|---|
| `--json` | Report one JSON object per line (see [JSON output](json-output.md)). |
| `--check` | Change nothing; list the files that are not canonical, and exit with status 1 if there is one. |

## Output

Text, on stdout: one line per file that changed (`formatted main.vo`), or with `--check` per file that
would change (`not formatted: main.vo`), then `1 of 2 files changed` (or `1 of 2 files need formatting`).

With `--json`: one `fmt` record per file, changed or not, then a `summary` with `changed`:

```json
{"v":1,"kind":"fmt","file":"main.vo","changed":true}
{"v":1,"kind":"summary","command":"fmt","errors":0,"warnings":0,"truncated":0,"files":1,"changed":1}
```

Syntax errors are reported as `diagnostic` records, as by [vo check](check.md).

Files are replaced safely: the new text is written to a temporary file next to the source, which then
takes its place.

## Exit statuses

| Status | When |
|---|---|
| 0 | Done (files formatted, or with `--check` all already canonical). |
| 1 | `--check`: at least one file is not canonical. |
| 64 | Usage error, such as no path ([T0001](../errors/T0001.md)). |
| 65 | A file has a syntax error; nothing was written. |
| 66 | A path cannot be read ([T0002](../errors/T0002.md)). |

## Example

A program written without care for layout:

```vo
fn main() uses console{
  let x=(1+2)*3;
    if (x>2){print("big");}
}
```

```console
$ vo fmt --check main.vo
not formatted: main.vo
1 of 1 files need formatting
$ vo fmt --check --json main.vo
{"v":1,"kind":"fmt","file":"main.vo","changed":true}
{"v":1,"kind":"summary","command":"fmt","errors":0,"warnings":0,"truncated":0,"files":1,"changed":1}
$ vo fmt main.vo
formatted main.vo
1 of 1 files changed
$ vo fmt --check main.vo
0 of 1 files need formatting
```

Before and after:

```vo-fmt
fn main() uses console{
  let x=(1+2)*3;
    if (x>2){print("big");}
}
```

```vo
fn main() uses console {
    let x = (1 + 2) * 3;
    if x > 2 {
        print("big");
    }
}
```

The parentheses around `1 + 2` are needed and stay; those around the `if` condition are not and go.
`vo fmt --check .` in a CI job fails the job when any file under the directory is not formatted.

# vo edit

`vo edit` renames a name everywhere it is used, or adds a field to a record and to every place that builds one, and refuses any edit that would break the program.

## Synopsis

```text
vo edit [--json] [--dry-run] rename ENTRY TARGET NEW_NAME
vo edit [--json] [--dry-run] add-field ENTRY RECORD FIELD: TYPE = DEFAULT
```

`ENTRY` is the program's entry file; only the files it imports are changed. A `TARGET` is `name` (a
function, record or variant), `Record.field`, `Variant.case`, or `FILE:LINE:COL` (anything at that
position, including parameters, locals and type parameters), as for [vo query](query.md).

- `rename` changes the declaration and every reference, in every file, and the function's line in
  `vo.effects` (see [vo effects](effects.md)).
- `add-field` adds `FIELD: TYPE;` at the end of the record and `FIELD: DEFAULT` to every
  `new RECORD { ... }`. The words after `RECORD` are joined with spaces, so `depth: int = 1` needs no quotes.

Before writing, the edited program is checked again. The edit is refused, and no file changes, when it
would add a check error or make a name mean something else (a new local hiding a function, for example):
that is [T0006](../errors/T0006.md), printed after the diagnostics the edit would cause, with their places
in the current text. A new name must be an identifier that is not a keyword or a builtin. All changed
files are written, or none. The rules are in the reference's
[Querying and editing programs](../reference/querying-and-editing-programs.md).

## Options

| Option | Meaning |
|---|---|
| `--json` | One `edit` record per change, then a `summary` record. |
| `--dry-run` | List the changes and the verdict; write nothing. |

`--dry-run` only works with `edit` ([T0001](../errors/T0001.md) elsewhere).

## Output

Text, on stdout: one line per change, `FILE:LINE:COL OLD -> NEW` for a replacement or
`FILE:LINE:COL + TEXT` for an insertion (a newline shown as `\n`), then
`renamed area to box_area: 2 changes in 1 file` (or `added field depth to Box: ...`), with
`(dry run, nothing written)` after `--dry-run`. The changes are listed after the files are written.

With `--json`, each change is an `edit` record: a fix (`file`, `start`, `end`, `replacement`) plus `text`,
the source it replaces (empty for an insertion). The `summary` adds `changes`, `changed` (files) and
`written`:

```json
{"v":1,"kind":"edit","file":"main.vo","start":{"line":3,"col":5},"end":{"line":3,"col":6},"text":"h","replacement":"height"}
{"v":1,"kind":"summary","command":"edit","errors":0,"warnings":0,"truncated":0,"files":1,"changes":3,"changed":1,"written":true}
```

A refused edit prints the diagnostics it would cause (as `diagnostic` records with `--json`, without
fixes), then a `toolchain_error` with code T0006 and the reason.

## Exit statuses

| Status | When |
|---|---|
| 0 | The edit was written (or, with `--dry-run`, would be). |
| 64 | Usage error ([T0001](../errors/T0001.md)), unknown target ([T0004](../errors/T0004.md)) or ambiguous target ([T0005](../errors/T0005.md)). |
| 65 | The edit was refused ([T0006](../errors/T0006.md)), or a file has a syntax error. |
| 66 | A file cannot be read ([T0002](../errors/T0002.md)). |

## Example

```vo
record Box {
    w: int;
    h: int;
}

fn area(b: Box) -> int {
    return b.w * b.h;
}

fn main() uses console {
    let b = new Box { w: 3, h: 4 };
    print("area " + str(area(b)));
}
```

A dry run first, then the renames (a function by name, a parameter by position), a new field with its
default, a refused rename, and a field rename with `--json`:

```console
$ vo edit rename --dry-run main.vo area box_area
main.vo:6:4 area -> box_area
main.vo:12:25 area -> box_area
renamed area to box_area: 2 changes in 1 file (dry run, nothing written)
$ vo edit rename main.vo area box_area
main.vo:6:4 area -> box_area
main.vo:12:25 area -> box_area
renamed area to box_area: 2 changes in 1 file
$ vo edit rename main.vo main.vo:6:13 box
main.vo:6:13 b -> box
main.vo:7:12 b -> box
main.vo:7:18 b -> box
renamed b to box: 3 changes in 1 file
$ vo edit add-field main.vo Box depth: int = 1
main.vo:4:1 +     depth: int;\n
main.vo:11:33 + , depth: 1
added field depth to Box: 2 changes in 1 file
$ vo edit rename main.vo main.vo:12:9 box_area
main.vo:12:9: error[E0203]: `box_area` is already defined in an outer scope.
  12 |     let b = new Box { w: 3, h: 4, depth: 1 };
     |         ^
  note: main.vo:7:4: `box_area` is defined here.
main.vo:13:25: error[E0303]: `box_area` is not a function.
  13 |     print("area " + str(box_area(b)));
     |                         ^^^^^^^^
error[T0006]: Renaming `b` to `box_area` would not check (2 new errors): nothing was written.
$ vo edit --json rename main.vo Box.h height
{"v":1,"kind":"edit","file":"main.vo","start":{"line":3,"col":5},"end":{"line":3,"col":6},"text":"h","replacement":"height"}
{"v":1,"kind":"edit","file":"main.vo","start":{"line":8,"col":24},"end":{"line":8,"col":25},"text":"h","replacement":"height"}
{"v":1,"kind":"edit","file":"main.vo","start":{"line":12,"col":29},"end":{"line":12,"col":30},"text":"h","replacement":"height"}
{"v":1,"kind":"summary","command":"edit","errors":0,"warnings":0,"truncated":0,"files":1,"changes":3,"changed":1,"written":true}
$ vo run main.vo
area 12
```

The refused rename would have made `box_area` inside `main` a local ([E0203](../errors/E0203.md)) and its
call no longer a function call ([E0303](../errors/E0303.md)); nothing was written. The file now reads:

```vo
record Box {
    w: int;
    height: int;
    depth: int;
}

fn box_area(box: Box) -> int {
    return box.w * box.height;
}

fn main() uses console {
    let b = new Box { w: 3, height: 4, depth: 1 };
    print("area " + str(box_area(b)));
}
```

```output
area 12
```

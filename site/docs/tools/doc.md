# vo doc

`vo doc` prints the documentation of the builtins, the standard library and the program's packages: every module, one module's items, or one item with its exact signature, effects and an example.

## Synopsis

```text
vo doc [--json] [NAME]
```

`NAME` is one of:

- nothing: every builtin (one line each, with its note), then every standard module and every package
  module of the nearest `vo.packages` (looked up from the current directory), each with its one-line
  description;
- a module, such as `std.json` or a package module `money.format`: every item in source order, its
  signature and description;
- an item, such as `std.json.parse_json` or a builtin such as `print`: its signature, description and example.

The same documentation is on this site under [Standard library](../std/index.md). How to import it is in
the reference's [Standard library and packages](../reference/standard-library-and-packages.md).

## Options

| Option | Meaning |
|---|---|
| `--json` | One `doc` record per item, then a `summary` record. |

## Output

Text, on stdout: one signature per line, its description indented below it, and for a single item an
`example:` line written `EXPR is VALUE`. A function's effects are part of its signature
(`fn get(url: str) -> result[Response] uses net`).

With `--json`, each item is a `doc` record: `module`, `name`, `item` (`fn`, `record`, `variant`, `builtin`
or `module`), `signature`, `description` and, when there is one, `example`. Functions add `tparams`,
`params` (`{name, type}`), `returns` and `effects`; records add `fields`, variants `cases`. A `summary`
with `"command":"doc"` and `items` ends the output:

```json
{"v":1,"kind":"doc","module":"std.text","name":"pad_left","item":"fn","signature":"fn pad_left(s: str, width: int, fill: str) -> str","tparams":[],"params":[{"name":"s","type":"str"},{"name":"width","type":"int"},{"name":"fill","type":"str"}],"returns":"str","effects":[],"description":"s padded on the left with fill (one character) to width characters.","example":"pad_left(\"7\", 3, \"0\") is \"007\""}
{"v":1,"kind":"summary","command":"doc","errors":0,"warnings":0,"truncated":0,"files":0,"items":1}
```

An unknown name is [T0010](../errors/T0010.md), with the closest name when there is one:
``error[T0010]: Nothing is documented as `std.text.pad_lft`; `vo doc` lists the builtins and modules. Did you mean `std.text.pad_left`?``

## Exit statuses

| Status | When |
|---|---|
| 0 | Printed. |
| 64 | Unknown name ([T0010](../errors/T0010.md)), or more than one name ([T0001](../errors/T0001.md)). |
| 65 | A package to document fails its check (for example its hash changed, [E0701](../errors/E0701.md)). |

## Example

Looking up one function before using it:

```vo
import std.text.pad_left;

fn main() uses console {
    print(pad_left("7", 3, "0"));
}
```

```console
$ vo doc std.text.pad_left
fn pad_left(s: str, width: int, fill: str) -> str
    s padded on the left with fill (one character) to width characters.
    example: pad_left("7", 3, "0") is "007"
$ vo doc --json std.text.pad_left
{"v":1,"kind":"doc","module":"std.text","name":"pad_left","item":"fn","signature":"fn pad_left(s: str, width: int, fill: str) -> str","tparams":[],"params":[{"name":"s","type":"str"},{"name":"width","type":"int"},{"name":"fill","type":"str"}],"returns":"str","effects":[],"description":"s padded on the left with fill (one character) to width characters.","example":"pad_left(\"7\", 3, \"0\") is \"007\""}
{"v":1,"kind":"summary","command":"doc","errors":0,"warnings":0,"truncated":0,"files":0,"items":1}
$ vo doc print
print (s: str)
    stdout, adds `\n`
$ vo doc std.text
fn starts_with(s: str, prefix: str) -> bool
    Whether s starts with prefix.
fn ends_with(s: str, suffix: str) -> bool
    Whether s ends with suffix.
fn contains(s: str, sub: str) -> bool
    Whether sub occurs in s.
fn replace(s: str, old: str, replacement: str) -> str
    s with every occurrence of old replaced by replacement, left to right (an empty old is R0007).
fn repeat(s: str, n: int) -> str
    s repeated n times ("" when n <= 0).
fn pad_left(s: str, width: int, fill: str) -> str
    s padded on the left with fill (one character) to width characters.
fn pad_right(s: str, width: int, fill: str) -> str
    s padded on the right with fill (one character) to width characters.
fn align_left(s: str, width: int) -> str
    s, then spaces up to width characters: s aligned left in a column.
fn align_right(s: str, width: int) -> str
    Spaces, then s, up to width characters: s aligned right in a column.
fn lines(s: str) -> list[str]
    The lines of s: split on newlines, a trailing carriage return dropped, no final empty line.
fn words(s: str) -> list[str]
    The words of s: runs of characters between spaces, tabs, newlines and carriage returns.
fn char_count(s: str) -> int
    The number of characters (code points) in s; len(s) counts bytes.
fn equal_fold(a: str, b: str) -> bool
    Whether a and b are equal when letter case is ignored (lower(a) == lower(b)): Latin, Greek and Cyrillic letters.
```

A package's modules are documented the same way once it is in `vo.packages`; see [vo package](package.md).

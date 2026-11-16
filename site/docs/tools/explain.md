# vo explain

`vo explain` prints the error catalog's entry for one code: what it means, whether it has an automatic fix, an example that produces it and the example fixed.

## Synopsis

```text
vo explain [--json] CODE
```

`CODE` is any code a diagnostic, runtime error or toolchain error carries: `E####` (errors), `W####`
(warnings), `R####` (runtime errors) or `T####` (toolchain errors). Every entry is also on this site, in
the [error catalog](../errors/index.md) (for example [E0301](../errors/E0301.md)), and the ranges are
described in the reference's [Error codes](../reference/error-codes.md).

Read `vo explain` when a diagnostic has no fix, before changing code.

## Options

| Option | Meaning |
|---|---|
| `--json` | Print one `explain` record instead of the text. |

## Output

Text, on stdout: the entry in Markdown, `## CODE name`, a one-line summary, `Fixable: yes`, `no` or
`sometimes`, an explanation, then `### Example` and `### Fixed`. For a toolchain error the examples are
command lines:

~~~text
## T0006 edit_refused

`vo edit` refused an edit, and changed no file.

Fixable: no

Every edit is checked before it is written: the program is checked again as it would be after the edit, and the edit is refused when that adds a check error, or makes a name mean something other than before. The new name of a rename must be an identifier that is not a keyword, a builtin or already used where it would clash; an added field must be new and its default must have its type and be visible at every record literal. When the program already has errors, the edit must leave them as they are. The report gives the reason and the diagnostics the edit would cause. Choose another name or default, or fix the program first.

### Example

```sh
vo edit rename main.vo count total   # main already has a local named total
```

### Fixed

```sh
vo edit rename main.vo count item_count
```
~~~

With `--json`, one record: `code`, `name`, `fixable` (`true` when the entry says `yes` or `sometimes`) and
`text`, the same Markdown as a string.

## Exit statuses

| Status | When |
|---|---|
| 0 | The entry was printed. |
| 64 | No code, more than one, or an unknown code ([T0001](../errors/T0001.md)). |

## Example

`vo check` reports a code; `vo explain` says what it means:

```vo
fn main() uses console {
    let n = 5;
    print("n is " + n);
}
```

```console
$ vo check main.vo
main.vo:3:21: error[E0301]: Expected `str`, found `int`.
   3 |     print("n is " + n);
     |                     ^
  expected `str`, found `int`
  fix: replace with `str(n)`
1 error, 0 warnings
$ vo explain --json E0301
{"v":1,"kind":"explain","code":"E0301","name":"type_mismatch","fixable":true,"text":"## E0301 type_mismatch\n\nAn expression has a different type than the one required.\n\nFixable: sometimes\n\nvo has no implicit conversions, so a value must have exactly the type that is expected. The diagnostic gives `expected` and `found`. Change the value, or convert it with `str(...)`, `int(...)` or `float(...)`; the fix offers the conversion when there is one.\n\n### Example\n\n```vo\nfn main() uses console {\n    let n: int = \"five\";\n    print(str(n));\n}\n```\n\n### Fixed\n\n```vo\nfn main() uses console {\n    let n: int = 5;\n    print(str(n));\n}\n```\n\n"}
```

Here the diagnostic has a fix, `str(n)`, so the entry only confirms it. `vo explain E0301` prints the same
entry as text.

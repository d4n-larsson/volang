# vo check

`vo check` reads programs and reports every error and warning, each with its code, its place and, when there is one, a fix, without running anything.

## Synopsis

```text
vo check [--json] PATH...
```

`PATH` is a `.vo` file or a directory. A directory stands for every `*.vo` file under it, recursively, in
sorted order. Each file is checked as the entry of its own program: the files it imports are loaded and
checked with it (see [Programs in several files](../reference/programs-in-several-files.md)).

`check` also compares the program with its effects record, `vo.effects`, when there is one: a function
that gained an effect is an error, [E0504](../errors/E0504.md), until `vo effects --update` approves it
(see [vo effects](effects.md)). A program whose packages changed since `vo package` pinned them is
[E0701](../errors/E0701.md) (see [vo package](package.md)).

## Options

| Option | Meaning |
|---|---|
| `--json` | Report one JSON object per line instead of text (see [JSON output](json-output.md)). |

Options and paths can come in any order.

## Output

Everything goes to stdout. In text, each diagnostic is a header line `FILE:LINE:COL: SEVERITY[CODE]: message`,
the source line with the span underlined, then `expected`/`found`, the first fix (`fix: replace with ...`,
`fix: insert ...`) and notes on related places. The last line counts them: `1 error, 1 warning`. A clean
program prints only `0 errors, 0 warnings`.

With `--json`, each diagnostic is a `diagnostic` record and the last record is a `summary`:

```json
{"v":1,"kind":"diagnostic","code":"E0301","name":"type_mismatch","severity":"error","file":"main.vo","start":{"line":4,"col":11},"end":{"line":4,"col":16},"text":"total","message":"Expected `str`, found `int`.","expected":"str","found":"int","fixes":[{"file":"main.vo","start":{"line":4,"col":11},"end":{"line":4,"col":16},"replacement":"str(total)"}],"related":[]}
{"v":1,"kind":"summary","command":"check","errors":1,"warnings":1,"truncated":0,"files":1}
```

- `fixes` lists edits as `{file, start, end, replacement}`; `start == end` is an insertion. Apply the
  first fix to resolve the diagnostic. When there is none, read `vo explain CODE` (see [vo explain](explain.md)).
- At most 50 diagnostics are listed; `truncated` counts the rest.
- `files` counts the paths given (after expanding directories).

The fields are described in [JSON output](json-output.md) and in the reference's
[Machine-readable output](../reference/machine-readable-output.md).

## Exit statuses

| Status | When |
|---|---|
| 0 | No errors. Warnings never change the status. |
| 64 | Usage error, such as no path ([T0001](../errors/T0001.md)). |
| 65 | At least one error. |
| 66 | A path cannot be read ([T0002](../errors/T0002.md)). |

See [Exit statuses](exit-statuses.md) for every command.

## Example

This program has a mistake on line 4 and an unused local on line 3:

```vo
fn main() uses console {
    let total = 5;
    let unused = 1;
    print(total);
}
```

```console
$ vo check main.vo
main.vo:3:9: warning[W0201]: `unused` is never used.
   3 |     let unused = 1;
     |         ^^^^^^
main.vo:4:11: error[E0301]: Expected `str`, found `int`.
   4 |     print(total);
     |           ^^^^^
  expected `str`, found `int`
  fix: replace with `str(total)`
1 error, 1 warning
$ vo check --json main.vo
{"v":1,"kind":"diagnostic","code":"W0201","name":"unused_variable","severity":"warning","file":"main.vo","start":{"line":3,"col":9},"end":{"line":3,"col":15},"text":"unused","message":"`unused` is never used.","fixes":[],"related":[]}
{"v":1,"kind":"diagnostic","code":"E0301","name":"type_mismatch","severity":"error","file":"main.vo","start":{"line":4,"col":11},"end":{"line":4,"col":16},"text":"total","message":"Expected `str`, found `int`.","expected":"str","found":"int","fixes":[{"file":"main.vo","start":{"line":4,"col":11},"end":{"line":4,"col":16},"replacement":"str(total)"}],"related":[]}
{"v":1,"kind":"summary","command":"check","errors":1,"warnings":1,"truncated":0,"files":1}
```

The error is [E0301](../errors/E0301.md) and its fix converts the value; the warning is
[W0201](../errors/W0201.md), which has no automatic fix. With the fix applied and the local removed, the
program checks clean:

```vo
fn main() uses console {
    let total = 5;
    print(str(total));
}
```

```console
$ vo check main.vo
0 errors, 0 warnings
$ vo check --json main.vo
{"v":1,"kind":"summary","command":"check","errors":0,"warnings":0,"truncated":0,"files":1}
```

`vo check .` checks every program under the current directory; `vo check` with no path is a usage error.

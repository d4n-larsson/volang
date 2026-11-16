# vo reference

`vo reference` prints the complete language reference built into the toolchain, the one document an agent reads before writing vo.

## Synopsis

```text
vo reference [--json]
```

The text is exactly `docs/reference.md` of the toolchain's source, so it always matches the `vo` that
prints it. It is about 50 KB of Markdown in 20 sections, from Lexical rules to Error codes, each with
checked examples. On this site it is split into one page per section under [Reference](../reference/index.md),
for example [Command line](../reference/command-line.md).

The reference opens with the working loop it expects:

```text
1. Write `main.vo`.
2. Run `vo check --json main.vo`. For each `diagnostic`, apply its first `fixes` entry if it has one;
   otherwise read `vo explain CODE` (for example `vo explain E0301`).
3. Repeat until the check is clean, then run `vo run main.vo` or `vo test main.vo`.
```

## Options

| Option | Meaning |
|---|---|
| `--json` | Print one `reference` record whose `text` is the whole document. |

## Output

Text: the Markdown document on stdout, starting with `# vo-lang Reference`. With `--json`, one line:

```json
{"v":1,"kind":"reference","text":"# vo-lang Reference\n\nvo is a small, statically typed language with one way to write each construct. ..."}
```

(`text` shortened here.)

## Exit statuses

| Status | When |
|---|---|
| 0 | Printed. |
| 64 | Usage error, such as an unknown option ([T0001](../errors/T0001.md)). |

## Example

The loop the reference describes, on a program written as in another language (`count = ...` instead of
`set count = ...`):

```vo
fn main() uses console {
    let name = "vo";
    var count = 0;
    for i in 0..3 {
        count = count + i;
    }
    print("hello " + name + ", count = " + str(count));
}
```

```console
$ vo check --json main.vo
{"v":1,"kind":"diagnostic","code":"E0114","name":"statement_not_call","severity":"error","file":"main.vo","start":{"line":5,"col":9},"end":{"line":5,"col":14},"text":"count","message":"Assignments start with `set`: write `set count = ...;`.","fixes":[{"file":"main.vo","start":{"line":5,"col":9},"end":{"line":5,"col":9},"replacement":"set "}],"related":[]}
{"v":1,"kind":"summary","command":"check","errors":1,"warnings":0,"truncated":0,"files":1}
```

The diagnostic is [E0114](../errors/E0114.md) and its first fix inserts `set ` at line 5, column 9
(`start == end`: an insertion). With it applied, the program checks clean and runs:

```vo
fn main() uses console {
    let name = "vo";
    var count = 0;
    for i in 0..3 {
        set count = count + i;
    }
    print("hello " + name + ", count = " + str(count));
}
```

```console
$ vo check --json main.vo
{"v":1,"kind":"summary","command":"check","errors":0,"warnings":0,"truncated":0,"files":1}
$ vo run main.vo
hello vo, count = 3
```

To give an agent the reference, put the output of `vo reference` in its context, or point it at this
site's `llms-full.txt`.

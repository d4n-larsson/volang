# Your first program

Write a vo program, run it, break it on purpose, and fix it the way the toolchain suggests, in text or in JSON.

This page assumes `vo` is on your `PATH` (see [Installing vo](install.md)). Make an empty directory and
work there.

## Hello

Save this as `main.vo`:

```vo
fn greeting(name: str) -> str {
    return "hello, " + name + "!";
}

fn main() uses console {
    print(greeting("world"));
}
```

```console
$ vo run main.vo
hello, world!
```

A few things to notice, each of which holds for every vo program:

- A program starts at `fn main()`. Items (functions, records, variants, tests) can appear in any order
  in the file.
- Parameters and return values always have types: `name: str`, `-> str`. Inside a function, types are
  inferred.
- `uses console` says that `main` may print. A function without a `uses` clause, like `greeting`, is
  pure: it can only compute its result from its arguments. The checker enforces this; you will see how
  on [the next page](tests-and-effects.md).
- Every simple statement ends with `;`, and every block has braces.

`vo run` checks the program first and runs it only if there are no errors. The program's own output goes
to stdout; anything vo reports goes to stderr, so the output stays clean.

## A deliberate mistake

vo has no implicit conversions: an `int` never turns into a `str` by itself. This program tries to join
a number to a string:

```vo
fn main() uses console {
    let names = ["ada", "grace", "linus"];
    for name in names {
        print(name + " has " + len(name) + " letters");
    }
}
```

```console
$ vo check main.vo
main.vo:4:32: error[E0301]: Expected `str`, found `int`.
   4 |         print(name + " has " + len(name) + " letters");
     |                                ^^^^^^^^^
  expected `str`, found `int`
  fix: replace with `str(len(name))`
1 error, 0 warnings
```

`vo check` only checks; it never runs anything. The error is [E0301](../errors/E0301.md), a type
mismatch. `vo run main.vo` would print the same report and stop with exit status 65 instead of running.

## Reading a diagnostic

Every diagnostic has the same parts:

| Part | Here | Meaning |
|---|---|---|
| location | `main.vo:4:32` | file, line and column (1-based) where the problem starts |
| severity and code | `error[E0301]` | `error` or `warning`, and a stable code |
| message | `Expected str, found int.` | one sentence |
| source | the line with `^^^^^^^^^` | the exact span the message is about |
| details | `expected ...`, `note: ...` | the types involved, and related places in the code |
| fix | `replace with str(len(name))` | an edit that resolves it, when one correction is unambiguous |

The last line counts errors and warnings. Errors make the command fail (exit status 65); warnings never
change the exit status.

When a diagnostic has a fix, apply it. When it has none, ask the toolchain what the code means:
`vo explain E0301` prints the catalog entry, with a broken example and its corrected version. The same
entries are in the [error catalog](../errors/index.md) on this site.

Applying the fix gives a program that checks and runs:

```vo
fn main() uses console {
    let names = ["ada", "grace", "linus"];
    for name in names {
        print(name + " has " + str(len(name)) + " letters");
    }
}
```

```output
ada has 3 letters
grace has 5 letters
linus has 5 letters
```

## Diagnostics as JSON

Every command takes `--json` and then reports one JSON object per line. This is the form to use from
scripts, editors and coding agents. For the broken program above:

```vo
fn main() uses console {
    let names = ["ada", "grace", "linus"];
    for name in names {
        print(name + " has " + len(name) + " letters");
    }
}
```

```console
$ vo check --json main.vo
{"v":1,"kind":"diagnostic","code":"E0301","name":"type_mismatch","severity":"error","file":"main.vo","start":{"line":4,"col":32},"end":{"line":4,"col":41},"text":"len(name)","message":"Expected `str`, found `int`.","expected":"str","found":"int","fixes":[{"file":"main.vo","start":{"line":4,"col":32},"end":{"line":4,"col":41},"replacement":"str(len(name))"}],"related":[]}
{"v":1,"kind":"summary","command":"check","errors":1,"warnings":0,"truncated":0,"files":1}
```

The first line is the diagnostic. Its span runs from `start` (inclusive) to `end` (exclusive), and each
entry in `fixes` is an edit: replace the text between `start` and `end` in `file` with `replacement`
(when `start` equals `end`, the edit is an insertion). Applying the first fix resolves the diagnostic.
The second line is the `summary` that ends every `check`.

A program that fixes code automatically needs only this loop:

1. Run `vo check --json main.vo`.
2. For each `diagnostic`, apply its first `fixes` entry if it has one; otherwise read `vo explain CODE`
   and change the code by hand.
3. Repeat until the summary says `"errors":0`.

Every record kind and field is described in [Machine-readable output](../reference/machine-readable-output.md).

## Formatting

vo has exactly one layout, and `vo fmt` produces it. Indentation means nothing to the language, so a
program written carelessly still checks:

```vo
fn main() uses console{
  let name="vo";
    if len(name)>1 {print("hello, "+name);}
}
```

```console
$ vo fmt --check main.vo
not formatted: main.vo
1 of 1 files need formatting
$ vo fmt main.vo
formatted main.vo
1 of 1 files changed
$ vo fmt --check main.vo
0 of 1 files need formatting
```

`vo fmt --check` changes nothing and fails (exit status 1) when a file is not canonical; `vo fmt`
rewrites the file. Afterwards it reads:

```vo
fn main() uses console {
    let name = "vo";
    if len(name) > 1 {
        print("hello, " + name);
    }
}
```

The canonical form uses 4-space indentation, `{` on the same line, one statement per line and single
spaces around operators. There are no options. The full rules are in
[Command line](../reference/command-line.md).

## Next

[Tests and effects](tests-and-effects.md): add tests, a property and a file read to a program, and see
how vo keeps track of what each function may touch.

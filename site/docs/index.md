/// html | div.vo-hero

<p class="vo-hero__meta"><span>VO</span><span>@@VO_VERSION@@</span></p>
<p class="vo-hero__kicker">LANGUAGE / TOOLCHAIN / REFERENCE</p>

//// html | div.vo-hero__grid

///// html | div.vo-hero__text

# A language for programs that are easy to inspect

A small, statically typed language designed for AI coding agents and the people who review what they
write.

[Get started](start/install.md){ .md-button .md-button--primary }
[Language guide](guide/basics.md){ .md-button }

/////

///// html | div.vo-hero__code

```vo title="main.vo"
record Task {
    title: str;
    due_ms: int;
}

# No `uses`: pure, so it cannot touch files, the network or the clock.
fn overdue(tasks: list[Task], now: int) -> list[Task] {
    return filter(tasks, fn(t) => t.due_ms < now);
}

fn main() uses console, clock {
    let tasks = [new Task { title: "ship 0.5", due_ms: 0 }];
    for t in overdue(tasks, now_ms()) {
        print("late: " + t.title);
    }
}
```

/////

////

///

Agents write code in one shot, read the compiler's output and fix what it reports, often with nobody
watching. vo is built for that loop.

/// tab | A program

```vo
fn greeting(name: str) -> str {
    return "hello, " + name;
}

fn main() uses console {
    print(greeting("vo"));
}
```

```output
hello, vo
```

`greeting` has no `uses` clause, so it is pure. `main` prints, so it says `uses console`.
///

/// tab | A mistake and its fix

```vo
fn load(path: str) -> result[str] {
    return read_file(path);
}

fn main() uses console {
    match load("notes.txt") {
        case ok(text) {
            print(text);
        }
        case err(e) {
            print("cannot read the notes: " + e);
        }
    }
}
```

```console
$ vo check main.vo
main.vo:1:4: error[E0501]: `load` does not declare file.read, which it needs.
   1 | fn load(path: str) -> result[str] {
     |    ^^^^
  fix: insert `uses file.read `
main.vo:5:4: error[E0501]: `main` does not declare file.read, which it needs.
   5 | fn main() uses console {
     |    ^^^^
  fix: replace with `uses console, file.read `
2 errors, 0 warnings
```

The program reads a file without saying so. Each error names its code, its place and the edit that fixes it.
///

/// tab | Tests and properties

```vo-test
fn longest(words: list[str]) -> str {
    var best = "";
    for w in words {
        if len(w) > len(best) {
            set best = w;
        }
    }
    return best;
}

test "picks the longest word" {
    expect_eq(longest(["a", "abc", "ab"]), "abc");
}

prop "no word is longer than the longest" (words: list[str]) {
    for w in words {
        expect(len(w) <= len(longest(words)));
    }
}
```

```output
PASS picks the longest word (0ms)
PASS no word is longer than the longest (100 cases, 0ms)
2 passed, 0 failed
```

Tests live next to the code. A `prop` runs on 100 generated inputs, the same ones on every run.
///

/// tab | JSON for agents

```vo
fn area(width: int, height: int) -> int {
    return width * hieght;
}

fn main() uses console {
    print(str(area(3, 4)));
}
```

```console
$ vo check --json main.vo
{"v":1,"kind":"diagnostic","code":"E0201","name":"undefined_name","severity":"error","file":"main.vo","start":{"line":2,"col":20},"end":{"line":2,"col":26},"text":"hieght","message":"`hieght` is not defined.","fixes":[{"file":"main.vo","start":{"line":2,"col":20},"end":{"line":2,"col":26},"replacement":"height"}],"related":[{"file":"main.vo","start":{"line":1,"col":21},"end":{"line":1,"col":27},"message":"`height` is defined here."}]}
{"v":1,"kind":"summary","command":"check","errors":1,"warnings":0,"truncated":0,"files":1}
```

With `--json` every command reports one JSON object per line. The fix is an exact replacement an agent can apply.
///

## Why vo

/// html | div.grid.cards

- **Diagnostics that come with a fix**

    ---

    Every error and warning has a code, a place and, when there is one, a fix, as text or as
    [JSON lines](tools/json-output.md). The [error catalog](errors/index.md) explains each code.

- **Signatures that say what code may touch**

    ---

    A function declares its [effects](guide/effects.md): files, network, clock, environment. The checker
    makes the declaration true, and `vo run` allows nothing beyond it.

- **Runs that repeat**

    ---

    The same inputs give the same output. Fix the seed and the clock, record a run and
    [replay](guide/determinism.md) it exactly.

- **A language that fits in one read**

    ---

    The [complete reference](reference/index.md) is under 15,000 tokens, so an agent can read all of it
    before writing a line.

///

## How the toolchain fits together

![The vo toolchain. An agent or a person edits .vo files. vo check reads them and the effects record
vo.effects, and reports diagnostics with their fixes back to the author. A program that checks clean goes
on to vo test, which runs tests and properties, to vo run, which runs main within the effects it declares,
or to vo build, which writes one executable file.](assets/toolchain.svg)

Every command checks the program first, so nothing runs, is tested or is built with an error in it.
[`vo check`](tools/check.md) also compares the program with its effects record, `vo.effects`: a function
that gained an effect is an error until a reviewer approves it with [`vo effects`](tools/effects.md).
From there, [`vo test`](tools/test.md) runs the tests and properties, [`vo run`](tools/run.md) runs `main`
within the effects it declares (and [`vo replay`](tools/replay.md) repeats a recorded run), and
[`vo build`](tools/build.md) writes one executable file.

## Where to start

/// html | div.grid.cards

- **[Getting started](start/install.md)**

    ---

    Build the toolchain and write, run and test a first program.

- **[Language guide](guide/basics.md)**

    ---

    Every feature explained with worked examples.

- **[Toolchain](tools/check.md)**

    ---

    Every command, its options and its output.

- **[Reference](reference/index.md)**

    ---

    The complete language reference, the [error catalog](errors/index.md) and the
    [standard library](std/index.md), generated from the toolchain itself.

///

## For agents

Every page is also available as plain Markdown: add `.md` to its address. [`llms.txt`](llms.txt) lists
every page with a one-line summary, and [`llms-full.txt`](llms-full.txt) has all of them in one file.

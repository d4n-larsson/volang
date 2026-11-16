# Effects

Every function and test declares in a `uses` clause what it may touch outside the program, such as the console, files, the network or the clock; the checker enforces the declarations and the running program enforces their scopes.

## Why effects

When you review code, the first question is often not "is this correct?" but "what can this touch?".
Can this parsing function write files? Can this report generator reach the network? Does this helper
read environment variables, where the secrets are? In most languages the only way to answer is to read
the function and everything it calls.

In vo the answer is in the function's signature. A function lists its effects after its return type:

```text
fn save(r: Report) -> result[void] uses console, file.write("out/") { ... }
```

and the checker makes the list true: a function must declare everything it does, directly or through
the functions it calls. A function with no `uses` clause is **pure**: it computes its result from its
arguments and nothing else, so it cannot print, read a file or look at the clock, and it returns the
same result every time it is called with the same arguments.

The declarations are also checked while the program runs: a file path computed at run time must lie
inside the directories the function declared. And `vo.effects` records every function's effects in one
file, so a code review can see a capability being added as a one-line diff.

The rules are in the [Effects](../reference/effects.md) section of the reference.

## `uses` clauses

```vo
fn area(w: int, h: int) -> int {
    return w * h;
}

fn report(w: int, h: int) uses console {
    print("area " + str(area(w, h)));
}

fn main() uses console {
    report(3, 4);
}
```

```output
area 12
```

`area` is pure. `report` prints, so it declares `console`. `main` calls `report`, so it declares
`console` too, even though it does not print itself: a declaration covers everything reachable from
the function. Reading `main`'s clause tells you everything the whole program can do.

The clause comes after the return type (or after the parameters when there is none) and before any
contract (`requires`, `ensures`). Effects are separated by commas; `vo fmt` puts them in a canonical
order: console, input, file.read, file.write, net, clock, random, env, serve.

## The effects

| Effect | Allows | Scope |
|---|---|---|
| `console` | `print`, `write`, `eprint` (standard output and error) | |
| `input` | `read_line`, `read_all` (standard input) | |
| `file.read` | `read_file` | directories |
| `file.write` | `write_file` | directories |
| `net` | `std.http.get`, `std.http.post` | hosts |
| `clock` | `now_ms` (and `std.time.now`) | |
| `random` | `random_int` (and `std.random`) | |
| `env` | `env` (environment variables) | variable names |
| `serve` | `std.http.serve` (answering HTTP requests) | addresses |

Everything else is pure: arithmetic, strings, lists and maps, the collections, `std.json`,
`std.text`. Program arguments (`args()`) and `exit` need no effect.

## Scopes

An effect without a scope is not narrowed: `file.read` may read any file the process can read, and
`env` any variable. A scope, in parentheses after the effect, narrows it:

| Effect | Scope | Example |
|---|---|---|
| `file.read`, `file.write` | directories, relative to where the program starts | `file.write("out/")`; `"./"` is that directory itself |
| `env` | variable names | `env("HOME", "LANG")` |
| `net` | host names | `net("api.example.com")` |
| `serve` | addresses | `serve("8000")` (this machine only), `serve("0.0.0.0:8000")` (the network) |

A directory scope covers the directories under it: `file.read("data/")` covers a callee that declares
`file.read("data/2026/")`. A caller's scopes must cover its callees' scopes, so the scope in `main`'s
clause is the widest the program can use.

```vo
fn load_notes(name: str) -> result[str] uses file.read("data/notes/") {
    return read_file("data/notes/" + name);
}

fn load_config() -> result[str] uses file.read("data/") {
    return read_file("data/config.txt");
}

fn main() -> result[void] uses console, file.read("data/") {
    let config = try load_config();
    let notes = try load_notes("today.txt");
    print(str(len(config) + len(notes)));
    return ok();
}
```

A scope must be a non-empty relative directory or name. An absolute path, a path with `..`, or an
empty scope is [E0506](../errors/E0506.md): a program cannot declare its way out of the directory it
runs in.

```vo
fn main() uses console, file.read("/etc/") {
    print("reading /etc");
}
```

```error E0506
```

## What the checker reports

A function that does something it did not declare is [E0501](../errors/E0501.md). The message lists
every missing effect, and the fix rewrites the clause to declare them:

```vo
fn greet(name: str) {
    print("hello " + name);
}

fn main() uses console {
    greet("vo");
}
```

```error E0501
```

A caller whose scope does not cover a callee's is [E0503](../errors/E0503.md). Here `main` may read
only `HOME`, but `user` reads `USER`; the fix widens `main`'s scope to `env("HOME", "USER")`:

```vo
fn user() -> result[str] uses env("USER") {
    return env("USER");
}

fn main() uses console, env("HOME") {
    match user() {
        case ok(u) {
            print(u);
        }
        case err(e) {
            print(e);
        }
    }
}
```

```error E0503
```

A literal target is checked the same way: `read_file("secrets/key.txt")` in a function that declares
`file.read("data/")` is [E0503](../errors/E0503.md) at check time, before anything runs.

Declaring an effect nothing needs is warning [W0501](../errors/W0501.md), with a fix that removes it.
Declarations are capabilities, so an unneeded one is removed rather than kept "just in case":

```vo
fn main() uses console, clock {
    print("no time needed");
}
```

```error W0501
```

## Effects through function values and generics

Effects are part of function types. `fn(int) -> int` is a pure function; `fn(int) -> int uses console`
may print. A function value fits a function type whose effects cover its own, so a printing function
cannot be passed where a pure one is expected ([E0502](../errors/E0502.md)).

Anonymous functions never write `uses` ([E0507](../errors/E0507.md)): their effects come from their
body, and the function that calls them needs those effects.

```vo
fn main() uses console {
    let shout = fn(s: str) uses console {
        print(s + "!");
    };
    shout("hi");
}
```

```error E0507
```

Builtins that take a function, such as `map`, `filter` and `sort_by`, pass the function's effects on to
the caller: `map` with a function that prints needs `uses console`. Your own higher-order functions do
the same with an effect parameter, `E: effects`:

```vo
fn each[T, E: effects](xs: list[T], f: fn(T) uses E) uses E {
    for x in xs {
        f(x);
    }
}

fn main() uses console {
    each(["a", "b"], fn(s: str) {
        print(s);
    });
}
```

```output
a
b
```

`each` is pure when it is given a pure function and prints when it is given one that prints. See
[Generics](generics.md#effect-parameters) for more.

## The run-time check

File paths and variable names are often computed, so the checker cannot see them. vo checks them again
when the program runs, after resolving `..`, absolute paths and symbolic links. A target outside the
declared scopes stops the program with runtime error [R0010](../errors/R0010.md) (exit status 70)
before anything is read or written:

```vo
fn save(name: str, text: str) -> result[void] uses file.write("out/") {
    return write_file("out/" + name, text);
}

fn main() -> result[void] uses console, file.write("out/") {
    try save("../notes.txt", "hello");
    print("saved");
    return ok();
}
```

```error R0010
```

```text
main.vo:2:12: runtime error[R0010]: file.write("out/../notes.txt") is not allowed: `save` declares file.write("out/").
   2 |     return write_file("out/" + name, text);
     |            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
trace:
  at save (main.vo:2:12)
      name: str = "../notes.txt", text: str = "hello"
  at main (main.vo:6:9)
```

Two declarations apply to every access:

- the declaration of the **running function**, the one that calls `write_file` (here `save`);
- the **root grant**: `main`'s clause in `vo run`, or the running test's clause in `vo test`.

The target must be inside both. Since every caller's scopes cover its callees', the root grant is the
outer limit of the whole program: no function deep in the call chain can reach further than `main`
declared.

[R0010](../errors/R0010.md) is a runtime error, not a `result`: it means the program tried something its declarations do
not allow, which is a bug or an attack, not a condition to handle. A file that is inside the scope but
does not exist is an ordinary `err` from `read_file`.

### Library functions take the caller's grant

Functions of the standard library are not listed in `vo.effects` and have no scope of their own. Their
effects work like a builtin's: the caller declares them, with any scope, and the call runs with the
caller's grant. So `uses net("localhost")` limits `std.http.get` to that host:

```vo
import std.http.get;

fn main() -> result[void] uses console, net("localhost") {
    let host = "example" + ".com";
    let page = try get("http://" + host + "/");
    print(page.body);
    return ok();
}
```

```error R0010
```

The request is refused before any connection is made.

## Effects in tests

A test has a `uses` clause like a function, and that clause is the grant of the test: a test without
`uses file.read(...)` cannot read a file, even through the functions it calls. `vo test` runs every
test with seed 1 and a clock that starts at 0, so tests that use `random` or `clock` give the same
result every time:

```vo-test
fn load(path: str) -> result[str] uses file.read("fixtures/") {
    return read_file(path);
}

fn stamp(label: str) -> str uses clock {
    return label + "@" + str(now_ms());
}

test "a missing fixture is an error" uses file.read("fixtures/") {
    expect_eq(load("fixtures/none.txt"), err("cannot read fixtures/none.txt"));
}

test "the clock is fixed" uses clock {
    expect_eq(stamp("a"), "a@0");
    expect_eq(stamp("b"), "b@1");
}
```

```output
PASS a missing fixture is an error (0ms)
PASS the clock is fixed (0ms)
2 passed, 0 failed
```

In `vo run`, `--seed N` fixes `random_int` and `--clock-ms N` makes `now_ms` return N, N+1, ...
(see [Determinism](determinism.md)).

## Reviewing effects: `vo.effects`

`vo.effects` is a plain text file next to the entry file (or in a directory above it) that lists the
effects of every function and test of the program, one line each, sorted. It is the record a reviewer
approves:

```vo
# file: report.vo
export fn render(lines: list[str]) -> str {
    return join(lines, "\n") + "\n";
}

export fn save(name: str, text: str) -> result[void] uses file.write("out/") {
    return write_file("out/" + name, text);
}
```

```vo
# file: main.vo
import report.render;
import report.save;

fn main() -> result[void] uses console, file.write("out/") {
    try save("summary.txt", render(["3 open", "1 closed"]));
    print("saved out/summary.txt");
    return ok();
}
```

```console
$ vo check main.vo
main.vo:1:1: warning[W0504]: This program has no effects record: create vo.effects with `vo effects --update`.
   1 | # file: main.vo
     | ^
0 errors, 1 warning
$ vo effects --update main.vo
+ main.vo:main: console, file.write("out/")
+ report.vo:save: file.write("out/")
0 errors, 0 warnings
$ vo check main.vo
0 errors, 0 warnings
$ vo effects main.vo
main uses console, file.write("out/")

main.vo:main uses console, file.write("out/")
report.vo:render pure
report.vo:save uses file.write("out/")
0 errors, 0 warnings
```

A program in several files without a record is warning [W0504](../errors/W0504.md).
`vo effects --update` writes the record and prints the changes it makes. `vo effects main.vo` prints
the capabilities of `main` and of each test first, then the record lines. The record itself is:

```text
# vo effects record: update with `vo effects --update`
main.vo:main uses console, file.write("out/")
report.vo:render pure
report.vo:save uses file.write("out/")
```

Commit `vo.effects` with the code. From then on, `vo check` compares the program with it:

- A function or test that **gained** an effect or a wider scope is error [E0504](../errors/E0504.md),
  with notes on the calls that brought it in. There is no automatic fix: approving a new capability is
  a review decision.
- A function with **fewer** effects, or one that was removed, is warning [W0502](../errors/W0502.md).
- A function that was **renamed** without changing its effects is warning
  [W0503](../errors/W0503.md).

Only `vo effects --update` changes the record. Suppose `save` starts stamping the file with the time
(`now_ms()`, so `uses file.write("out/"), clock`) and `main` adds `clock` to its clause. `vo check`
now fails:

```text
main.vo:5:4: error[E0504]: `main` gains clock; approve with `vo effects --update`.
   5 | fn main() -> result[void] uses console, file.write("out/"), clock {
     |    ^^^^
  note: main.vo:6:9: `main` calls `save`, which uses file.write("out/"), clock.
report.vo:6:11: error[E0504]: `save` gains clock; approve with `vo effects --update`.
   6 | export fn save(name: str, text: str) -> result[void] uses file.write("out/"), clock {
     |           ^^^^
2 errors, 0 warnings
```

and `vo effects --update` prints exactly what it approves:

```text
+ main.vo:main: clock
+ report.vo:save: clock
```

The reviewer reads "`save` gains clock" in the diff of `vo.effects` instead of reading every line of
the change. Standard library functions are not listed; package functions are, as
`@NAME/path.vo:fn` (see [Modules and packages](modules-and-packages.md)). The
[`vo effects`](../tools/effects.md) page has the command's options and JSON output.

## Error codes

| Code | Meaning |
|---|---|
| [E0501](../errors/E0501.md) | a function or test does something it does not declare |
| [E0502](../errors/E0502.md) | a function value has effects its expected type does not allow |
| [E0503](../errors/E0503.md) | a caller's scope does not cover a callee's, or a literal target |
| [E0504](../errors/E0504.md) | a function gained effects that `vo.effects` does not list |
| [E0506](../errors/E0506.md) | a scope is empty, absolute or uses `..` |
| [E0507](../errors/E0507.md) | an anonymous function has a `uses` clause |
| [W0501](../errors/W0501.md) | a declared effect is never needed |
| [W0502](../errors/W0502.md) | a function has fewer effects than `vo.effects` lists, or was removed |
| [W0503](../errors/W0503.md) | a function was renamed with unchanged effects |
| [W0504](../errors/W0504.md) | a program in several files has no `vo.effects` |
| [R0010](../errors/R0010.md) | at run time, a path, name or host outside the declared scopes |

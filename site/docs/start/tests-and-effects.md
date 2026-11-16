# Tests and effects

Write tests and a property next to your code, run them with `vo test`, and declare what a program may touch with `uses`.

## Tests

A `test` block is an item, like a function, and lives in the same file as the code it tests. Inside it,
`expect(cond)` checks that a condition holds and `expect_eq(actual, expected)` checks that two values are
equal. `vo test FILE` runs every test in the file; `vo run` ignores them. A file with tests does not need
a `main`.

```vo-test
fn initials(name: str) -> str {
    var out = "";
    for word in split(name, " ") {
        if word != "" {
            set out = out + upper(slice(word, 0, 1));
        }
    }
    return out;
}

test "two words" {
    expect_eq(initials("ada lovelace"), "AL");
}

test "extra spaces" {
    expect_eq(initials("  grace   hopper "), "GH");
}
```

```output
PASS two words (0ms)
PASS extra spaces (0ms)
2 passed, 0 failed
```

A test stops at its first failing check, and `expect_eq` prints both values. `vo test` exits with status 1
if any test did not pass, and `--filter TEXT` runs only the tests whose name contains `TEXT`. Tests and
their output are covered in full in the [Tests](../guide/tests.md) chapter.

## A property

Examples only test the inputs you thought of. A `prop` is a test with parameters: `vo test` runs it on 100
generated inputs, and when one fails, it shrinks the input to a small one that still fails. Write what
must hold for every input. Here: `initials` never returns more letters than the name has words.

```vo-test
fn initials(name: str) -> str {
    var out = "";
    for word in split(name, " ") {
        if word != "" {
            set out = out + upper(slice(word, 0, 1));
        }
    }
    return out;
}

prop "at most one letter per word" (name: str) {
    expect(len(chars(initials(name))) <= len(split(name, " ")));
}
```

```output
ERROR at most one letter per word (5 cases, 0ms)
  main.vo:5:35: runtime error[R0007]: Slice boundary 1 splits a UTF-8 character.
    at initials (main.vo:5:35)
        name: str = "é", out: str = "", word: str = "é"
    at prop "at most one letter per word" (main.vo:12:22)
        name: str = "é"
    counterexample: name = "é"
    shrunk in 1 step
0 passed, 0 failed, 1 errored
```

The property found a bug the examples missed. Strings are UTF-8, `slice` counts bytes, and `é` takes two
bytes, so `slice(word, 0, 1)` cuts it in half: runtime error [R0007](../errors/R0007.md). A runtime error
in a test is reported as `ERROR`, with the values of the parameters and locals in each frame, and the
remaining tests still run. `chars` splits a string into characters, so `chars(word)[0]` is the first
character whatever its size:

```vo-test
fn initials(name: str) -> str {
    var out = "";
    for word in split(name, " ") {
        if word != "" {
            set out = out + upper(chars(word)[0]);
        }
    }
    return out;
}

test "two words" {
    expect_eq(initials("ada lovelace"), "AL");
}

prop "at most one letter per word" (name: str) {
    expect(len(chars(initials(name))) <= len(split(name, " ")));
}
```

```output
PASS two words (0ms)
PASS at most one letter per word (100 cases, 0ms)
2 passed, 0 failed
```

Generated inputs are fixed by a seed (1 unless you pass `--seed N`), so the same file always gives the
same report. See [Properties](../guide/properties.md) for the generators, `assume` and shrinking.

## Effects

`initials` has no `uses` clause, so it is pure: it cannot print, read files, use the network, read the
clock or environment, or draw random numbers. `main` above was written `fn main() uses console`, because
`print` writes to the console. These declarations are effects, and the checker makes them true: a
function must declare everything it does and everything the functions it calls declare.

This program reads a to-do list from a file, but its functions do not say so:

```vo
fn count_items(text: str) -> int {
    var n = 0;
    for line in split(text, "\n") {
        if trim(line) != "" {
            set n = n + 1;
        }
    }
    return n;
}

fn load(path: str) -> result[str] {
    return read_file(path);
}

fn main() uses console {
    match load("data/todo.txt") {
        case ok(text) {
            print(str(count_items(text)) + " items");
        }
        case err(e) {
            print("cannot read the list: " + e);
        }
    }
}
```

```error E0501
```

`vo check` reports [E0501](../errors/E0501.md) twice: once for `load`, which calls `read_file`, and once for
`main`, which calls `load`:

```text
main.vo:11:4: error[E0501]: `load` does not declare file.read, which it needs.
  11 | fn load(path: str) -> result[str] {
     |    ^^^^
  fix: insert `uses file.read `
main.vo:15:4: error[E0501]: `main` does not declare file.read, which it needs.
  15 | fn main() uses console {
     |    ^^^^
  fix: replace with `uses console, file.read `
2 errors, 0 warnings
```

The fixes declare `file.read` without a scope, which allows reading any file. You can do better: give the
effect a scope, the directory the program actually needs, `file.read("data/")`. Scopes are relative to
the directory the program starts in, and cover everything under them.

```vo
fn count_items(text: str) -> int {
    var n = 0;
    for line in split(text, "\n") {
        if trim(line) != "" {
            set n = n + 1;
        }
    }
    return n;
}

fn load(path: str) -> result[str] uses file.read("data/") {
    return read_file(path);
}

fn main() uses console, file.read("data/") {
    match load("data/todo.txt") {
        case ok(text) {
            print(str(count_items(text)) + " items");
        }
        case err(e) {
            print("cannot read the list: " + e);
        }
    }
}

test "counts non-blank lines" {
    expect_eq(count_items("milk\n\n  \neggs\n"), 2);
}
```

```console
$ vo run main.vo
cannot read the list: cannot read data/todo.txt
$ vo test main.vo
PASS counts non-blank lines (0ms)
1 passed, 0 failed
```

Here `data/todo.txt` does not exist, so `read_file` returns an error value instead of stopping the
program; `match` handles both outcomes (see [Results and errors](../guide/results-and-errors.md)).
With a `data/todo.txt` of three non-blank lines, the program prints `3 items`.

`main`'s clause is also the grant of the whole run: `vo run` lets the program read files under `data/`
and nothing else. A path outside the scope is rejected at check time when it is written literally
([E0503](../errors/E0503.md)), and at run time otherwise ([R0010](../errors/R0010.md)): calling
`load("secrets.txt")` stops the program with exit status 70 before the file is opened. A test's `uses`
clause is the grant of that test in the same way.

The full list of effects (`console`, `input`, `file.read`, `file.write`, `net`, `clock`, `random`, `env`,
`serve`) and their scopes is in the [Effects](../guide/effects.md) chapter.

## Reviewing effects: vo.effects

Effects make a function's reach visible in its signature. `vo.effects` makes changes to that reach
visible in review. It is a plain text file next to the program that lists the effects of every function
and test. `vo effects --update` writes it and prints exactly the changes:

```vo
fn count_items(text: str) -> int {
    var n = 0;
    for line in split(text, "\n") {
        if trim(line) != "" {
            set n = n + 1;
        }
    }
    return n;
}

fn load(path: str) -> result[str] uses file.read("data/") {
    return read_file(path);
}

fn main() uses console, file.read("data/") {
    match load("data/todo.txt") {
        case ok(text) {
            print(str(count_items(text)) + " items");
        }
        case err(e) {
            print("cannot read the list: " + e);
        }
    }
}
```

```console
$ vo effects --update main.vo
+ main.vo:load: file.read("data/")
+ main.vo:main: console, file.read("data/")
0 errors, 0 warnings
$ vo check main.vo
0 errors, 0 warnings
```

The file it writes:

```text
# vo effects record: update with `vo effects --update`
main.vo:count_items pure
main.vo:load uses file.read("data/")
main.vo:main uses console, file.read("data/")
```

Commit it with the code. From then on, `vo check` compares the program with the record. If a later change
makes `load` read from `"./"` (the whole directory) instead of `data/`, the check fails with
[E0504](../errors/E0504.md), naming the call that brought the new effect in:

```text
main.vo:11:4: error[E0504]: `load` gains file.read("./"); approve with `vo effects --update`.
  11 | fn load(path: str) -> result[str] uses file.read("./") {
     |    ^^^^
main.vo:15:4: error[E0504]: `main` gains file.read("./"); approve with `vo effects --update`.
  15 | fn main() uses console, file.read("./") {
     |    ^^^^
  note: main.vo:16:11: `main` calls `load`, which uses file.read("./").
2 errors, 0 warnings
```

Only `vo effects --update` changes the record, so the new capability shows up as a one-line change in the
diff of `vo.effects`, and a reviewer approves "`load` gains `file.read("./")`" instead of reading every
line of the code. A function that lost an effect is only a warning ([W0502](../errors/W0502.md)).

## Where to go next

- The [language guide](../guide/basics.md) explains every feature, starting with values, variables and
  control flow.
- The [toolchain pages](../tools/check.md) describe every command and its options.
- The [reference](../reference/index.md) is the whole language on one page; `vo reference` prints the
  same text.
- The [examples](../examples/index.md) are complete programs with tests, effects and a `vo.effects` record.

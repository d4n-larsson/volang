# Results and errors

A function that can fail returns a `result[T]`, which the caller must handle with `match` or pass on with `try`, while bugs such as a bad index stop the program with a runtime error.

## Two kinds of failure

vo has no exceptions and no `null`. To decide how a failure is handled, ask: **is it the program's
fault, or the world's?**

- **The world did not cooperate.** The program is correct, but something outside it is not: a user
  typed `forty` where a number was expected, a file does not exist, a server did not answer. This will
  happen however good the code is, and the program has to decide what to do: ask again, use a default,
  report it, skip the line. Such a function returns a `result[T]`, and the checker makes every caller
  deal with it.
- **The program is wrong.** Reading past the end of a list, dividing an `int` by zero, reading a map key
  that is not there, overflowing an `int`. Nothing outside caused it: the code has a bug. No caller can
  sensibly recover, because the right response is to fix the code. These are runtime errors with an `R`
  code: the program stops with a report that points at the failing expression, and exit status 70. They
  are not results and cannot be caught.

### Why not one mechanism for both?

If every failure were a `result`, every index, division and map lookup would return one:

```text
let x = try xs[i];
let avg = try total / n;
```

(neither is real vo). Almost every function would then return a `result`, so a signature would no longer
tell you anything, and people would write `try` everywhere without reading it. The failures that matter,
like the missing file, would be lost among thousands that cannot happen.

If every failure stopped the program, as an uncaught exception does in many languages, a user typing
`forty` would crash it. Nothing in `parse_int`'s signature would warn its callers. Code that writes
`let n = parse_int(s);` would work in every test with good input and fail in production on the first
bad one.

So vo separates them: **failures you must plan for are in the type, and the checker makes you handle
them; bugs stay out of the types and stop the program loudly and precisely.**

### What `try` does

`try` means "if this failed, fail too, with the same error". In a function that returns a `result`,

```text
let n = try parse_int(text);
```

does the same as

```text
var n = 0;
match parse_int(text) {
    case ok(v) {
        set n = v;
    }
    case err(e) {
        return err(e);
    }
}
```

That is why `try` works only inside a function that itself returns a `result`
([E0406](../errors/E0406.md) otherwise). A function that does not return one has to `match` and decide
what to do. An error travels up the call chain, visible at every step, until some function handles it:
often `main`, which prints a message.

### The two kinds side by side

```vo
fn average(lines: list[str]) -> result[int] {
    # Text that is not a number is the world's doing: try passes the err on.
    var total = 0;
    for line in lines {
        set total = total + try parse_int(line);
    }
    # So is an empty input. Checking for it here turns what would be a
    # division by zero, a bug, into an err the caller can handle.
    if len(lines) == 0 {
        return err("no numbers");
    }
    return ok(total / len(lines));
}

fn show(r: result[int]) uses console {
    match r {
        case ok(v) {
            print("average " + str(v));
        }
        case err(e) {
            print("error: " + e);
        }
    }
}

fn main() uses console {
    show(average(["4", "8", "9"]));
    show(average(["4", "forty"]));
    show(average(new list[str]));
}
```

```output
average 7
error: invalid int: forty
error: no numbers
```

Without the check for an empty list, `average(new list[str])` would divide by zero: a runtime error, a
bug. The fix is not to make division return a `result`. It is to see that empty input is something the
world can hand you, and to turn it into an `err` before the division.

The rule of thumb: **if the caller can reasonably do something about it, return a `result`; if it means
the code is wrong, check for it first with `if`, `len` and `has`, so it never happens.**

### Why this matters when an agent writes the code

A reviewer who reads `fn load(path: str) -> result[Config]` knows from that one line that `load` can
fail and that every caller had to deal with it. Nothing can be thrown past a caller that did not expect
it, and no error can be silently dropped. This is the same idea as [effects](effects.md): what a function
can do, including failing, is in its signature, not hidden in its body.

The rules are in the [Results and errors](../reference/results-and-errors.md) section of the
reference.

## `result[T]`, `ok` and `err`

A `result[T]` is either `ok(value)`, holding a `T`, or `err(message)`, holding a `str`. The error is
always a string: there are no error types to declare or convert between.

```vo
fn parse_percent(text: str) -> result[int] {
    let n = parse_int(trim(text));
    match n {
        case ok(v) {
            if v < 0 or v > 100 {
                return err("out of range: " + str(v));
            }
            return ok(v);
        }
        case err(e) {
            return err(e);
        }
    }
}

fn show(r: result[int]) uses console {
    match r {
        case ok(v) {
            print("ok " + str(v));
        }
        case err(e) {
            print("error: " + e);
        }
    }
}

fn main() uses console {
    show(parse_percent(" 42 "));
    show(parse_percent("120"));
    show(parse_percent("forty"));
}
```

```output
ok 42
error: out of range: 120
error: invalid int: forty
```

`ok(...)` and `err(...)` do not say which `result` type they build, so the type must come from the
context: the function's return type, the parameter they are passed to, or an annotation.
`let r: result[int] = ok(1);` works; `let r = ok(1);` is [E0307](../errors/E0307.md).

A `match` on a result must have both `case ok(...)` and `case err(...)`
([E0401](../errors/E0401.md) if one is missing). Results also compare with `==` like any other value,
which is handy in conditions and tests: `parse_int("3") == ok(3)` is `true`.

Several builtins return results. Their error messages are fixed strings you can show to a user:

| Builtin | Error |
|---|---|
| `parse_int(s)`, `parse_float(s)` | `invalid int: s`, `invalid float: s` |
| `pop(xs)` | `empty` |
| `read_line()` | `eof` at the end of input |
| `read_file(path)` | `cannot read path` |
| `env(name)` | `not set` |

## Passing errors on with `try`

Most code does not handle an error where it happens: it passes it to its caller. `try e` does that in
one word. If `e` is `ok(v)`, `try e` is `v`; if `e` is `err(m)`, the current function returns `err(m)`
at once.

```vo
record Size {
    w: int;
    h: int;
}

fn parse_size(text: str) -> result[Size] {
    let parts = split(text, "x");
    if len(parts) != 2 {
        return err("expected WxH, found " + text);
    }
    let w = try parse_int(parts[0]);
    let h = try parse_int(parts[1]);
    return ok(new Size { w: w, h: h });
}

fn area(text: str) -> result[int] {
    let s = try parse_size(text);
    return ok(s.w * s.h);
}

fn main() uses console {
    for text in ["3x4", "3xfour", "3by4"] {
        match area(text) {
            case ok(a) {
                print(text + " -> " + str(a));
            }
            case err(e) {
                print(text + " -> " + e);
            }
        }
    }
}
```

```output
3x4 -> 12
3xfour -> invalid int: four
3by4 -> expected WxH, found 3by4
```

The rules:

- `try` is only allowed in a function whose return type is a `result` ([E0406](../errors/E0406.md)).
  That includes `main` when it returns `result[void]` (below) and anonymous functions that return a
  `result`. Tests do not return a result, so they cannot use `try`.
- The error passes through any `result` type: a `result[int]` error can leave a function that returns
  `result[Size]`, because both carry a `str`.
- `try` binds tighter than arithmetic: `try e + 1` means `(try e) + 1`.
- `try f();` as a statement is only for `result[void]`. With a value, use it or discard it explicitly:
  `let _ = try f();`. A bare `try parse_int(s);` is [E0405](../errors/E0405.md), a dropped value.

`try` in a function that does not return a result:

```vo
fn main() uses console {
    let n = try parse_int("12");
    print(str(n));
}
```

```error E0406
```

`try` passes the message on unchanged. When the message alone would not tell the user where the
problem is, catch it with `match` and add context. `invalid int: four` above is accurate, but in a file
of a thousand lines the reader also needs to know which line:

```vo
fn parse_line(n: int, line: str) -> result[int] {
    match parse_int(trim(line)) {
        case ok(v) {
            return ok(v);
        }
        case err(e) {
            return err("line " + str(n) + ": " + e);
        }
    }
}

fn total(lines: list[str]) -> result[int] {
    var sum = 0;
    for i in 0..len(lines) {
        set sum = sum + try parse_line(i + 1, lines[i]);
    }
    return ok(sum);
}

fn main() uses console {
    match total(["10", "20", "3O"]) {
        case ok(v) {
            print(str(v));
        }
        case err(e) {
            print(e);
        }
    }
}
```

```output
line 3: invalid int: 3O
```

## A result cannot be ignored

Calling a function that returns a result and doing nothing with it is
[E0404](../errors/E0404.md), whether the call is a statement on its own or bound to `_`. The error
would otherwise be lost without a trace.

```vo
fn main() -> result[void] uses file.write("out/") {
    write_file("out/log.txt", "started");
    return ok();
}
```

```error E0404
```

When the function returns a result, the fix adds `try`. Otherwise, write a `match` and decide what an
error means at that point.

## `result[void]`

A function that can fail but has no value to return, such as one that writes a file or validates
input, returns `result[void]`. It succeeds with `ok()`, which has no value inside. Match its success
with `case ok` (or `case ok()`, as it is built), and pass it on with `try f();` as a statement.

```vo
fn check_name(name: str) -> result[void] {
    if len(name) == 0 {
        return err("empty name");
    }
    if find(name, " ") >= 0 {
        return err("space in name: " + name);
    }
    return ok();
}

fn check_all(names: list[str]) -> result[void] {
    for n in names {
        try check_name(n);
    }
    return ok();
}

fn main() uses console {
    match check_all(["ana", "bo"]) {
        case ok {
            print("all valid");
        }
        case err(e) {
            print(e);
        }
    }
    match check_all(["ana", "bo b", ""]) {
        case ok {
            print("all valid");
        }
        case err(e) {
            print(e);
        }
    }
}
```

```output
all valid
space in name: bo b
```

## `main` returning a result

`main` may be `fn main() -> result[void]`, ending with `return ok();`. This lets `main` use `try`, so a
short program can pass every error up to the top instead of matching each one. When `main` returns
`err(message)`, vo prints `error: message` to standard error and the program exits with status 1.

```vo
fn main() -> result[void] uses console {
    print("reading the limit");
    let limit = try parse_int("4O");
    print("limit " + str(limit));
    return ok();
}
```

```output
reading the limit
```

Standard error gets the message, and the exit status is 1:

```text
error: invalid int: 4O
```

To exit with another status, call `exit(code)` with a code from 0 to 63. A code outside that range is
runtime error [R0008](../errors/R0008.md): 64 and above belong to the toolchain.

| Exit status of `vo run` | Meaning |
|---|---|
| 0 | `main` ended normally or returned `ok()` |
| 1 | `main` returned `err(...)` |
| 0 to 63 | the code passed to `exit` |
| 65 | the program has check errors and did not run |
| 70 | a runtime error stopped the program |
| 124 | the time limit was exceeded ([R0009](../errors/R0009.md)) |

The full table, including the other commands, is on the [exit statuses](../tools/exit-statuses.md)
page.

## Runtime errors

A runtime error stops the whole program: it is not a `result`, and no caller can recover from it. vo
prints a report to standard error and exits with status 70 (124 for a timeout). The report names the
code, points at the expression, and lists every active call with its parameters and the locals
declared before the failing statement:

```vo
fn average(total: int, count: int) -> int {
    return total / count;
}

fn main() uses console {
    print(str(average(10, 2)));
    print(str(average(10, 0)));
}
```

```output
5
```

```text
main.vo:2:12: runtime error[R0002]: Division by zero.
   2 |     return total / count;
     |            ^^^^^^^^^^^^^
trace:
  at average (main.vo:2:12)
      total: int = 10, count: int = 0
  at main (main.vo:7:15)
```

The first `print` ran, so its output is on standard output; the program stopped at the second call.
With `vo run --json`, the same report is a `runtime_error` record (see
[JSON output](../tools/json-output.md)).

| Code | Cause | Prevent it with |
|---|---|---|
| [R0001](../errors/R0001.md) | `int` overflow in `+ - * / %` or unary `-` | range checks, or `float` for very large values |
| [R0002](../errors/R0002.md) | `int` division or `%` by zero | `if d != 0` before dividing |
| [R0003](../errors/R0003.md) | index or slice bound outside a list or string | `0 <= i and i < len(xs)` |
| [R0004](../errors/R0004.md) | `m[k]` for a key the map does not have | `has(m, k)` or `get_or(m, k, default)` |
| [R0005](../errors/R0005.md) | recursion too deep | a base case, or a loop |
| [R0006](../errors/R0006.md) | more live data than the memory limit | keep less; `--max-memory` |
| [R0007](../errors/R0007.md) | a builtin got an argument it cannot use | check the value first |
| [R0009](../errors/R0009.md) | the program ran past `--timeout` (default 10 s) | make sure loops end |

A few of them in action. Overflow never wraps around:

```vo
fn main() uses console {
    var n = 1;
    for _ in 0..70 {
        set n = n * 2;
    }
    print(str(n));
}
```

```error R0001
```

Indexes start at 0, so the last element is `xs[len(xs) - 1]`:

```vo
fn last(xs: list[int]) -> int {
    return xs[len(xs)];
}

fn main() uses console {
    print(str(last([1, 2, 3])));
}
```

```error R0003
```

Reading a missing key stops the program; `get_or` gives a default instead:

```vo
fn main() uses console {
    let stock = new map[str, int];
    set stock["apple"] = 3;
    print(str(get_or(stock, "pear", 0)));
    print(str(stock["pear"]));
}
```

```error R0004
```

A recursion without a base case fills the call stack:

```vo
fn depth(n: int) -> int {
    return depth(n + 1) + 1;
}

fn main() uses console {
    print(str(depth(0)));
}
```

```error R0005
```

The limits on time and memory are options of `vo run` and `vo test`: `--timeout SEC` (default 10
seconds, `0` for none) and `--max-memory MiB` (default 512). A loop that never ends:

```vo
fn main() uses console {
    var n = 0;
    while true {
        set n = n + 1;
        if n < 0 {
            break;
        }
    }
    print(str(n));
}
```

is stopped by `vo run --timeout 1 main.vo` with [R0009](../errors/R0009.md) and exit status 124:

```text
main.vo:3:5: runtime error[R0009]: Time limit of 1 s exceeded.
   3 |     while true {
     |     ^^^^^^^^^^^^
trace:
  at main (main.vo:3:5)
```

Holding more live data than the limit is [R0006](../errors/R0006.md). A list that grows forever:

```vo
fn main() {
    let xs = new list[str];
    while true {
        push(xs, "some text that takes room");
    }
}
```

stops under `vo run --max-memory 1 main.vo` with a message that gives the amount:

```text
main.vo:4:9: runtime error[R0006]: The program holds 1.7 MiB of live data, over the 1 MiB limit.
```

Other runtime errors belong to other chapters: [R0010](../errors/R0010.md) (an effect used outside
its declared scope, see [Effects](effects.md)), [R0011](../errors/R0011.md) (a `todo` hole was
reached, see [Holes](holes.md)) and [R0012](../errors/R0012.md) and [R0013](../errors/R0013.md)
(a contract does not hold, see [Contracts](contracts.md)).

In `vo test`, a test that hits a runtime error is reported as `ERROR` and the other tests still run
(see [Tests](tests.md)).

## Error codes

| Code | Meaning |
|---|---|
| [E0307](../errors/E0307.md) | the result type of `ok(...)` or `err(...)` is not known: annotate it |
| [E0401](../errors/E0401.md) | a `match` is missing a case, such as `err` |
| [E0404](../errors/E0404.md) | a result is ignored |
| [E0405](../errors/E0405.md) | a call's value is ignored, including `try f();` when `f` has a value |
| [E0406](../errors/E0406.md) | `try` in a function that does not return a result |
| [R0001](../errors/R0001.md) to [R0009](../errors/R0009.md) | runtime errors, in the table above |

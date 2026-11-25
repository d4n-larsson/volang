# vo-lang Reference

vo is a small, statically typed language with one way to write each construct. Do not guess syntax from other languages: check every construct against this page.

1. Write `main.vo`.
2. Run `vo check --json main.vo`. For each `diagnostic`, apply its first `fixes` entry if it has one; otherwise read `vo explain CODE` (for example `vo explain E0301`).
3. Repeat until the check is clean, then run `vo run main.vo` or `vo test main.vo`.

Two rules cover most syntax: every statement starts with a keyword (`let var set if while for match return break continue try`) or is a function call, and every simple statement ends with `;`.

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

```output
hello vo, count = 3
```

## Lexical rules

- Source is UTF-8. Spaces, tabs and newlines only separate tokens; indentation has no meaning.
- **Comments** are whole lines whose first non-blank character is `#`. A `#` after code is E0101. No block comments.
- **Names**: functions, variables, parameters, record fields and variant cases are `snake_case` (`[a-z][a-z0-9_]*`). Records, variants and effect sets are `PascalCase` (`[A-Z][A-Za-z0-9]*`). Other spellings are E0207, whose fix is the converted name.
- `_` alone is the discard name: `let _ = ...;`, a `case` binding such as `case err(_)`, and a `for` variable (`for _ in 0..n`). It is not allowed as a parameter.
- **Integers**: `0`, `42`, `1000000`; `0xFF` and `0b1010` are 64-bit patterns (`0xFFFFFFFFFFFFFFFF` is `-1`). No `_` separators, leading zeros or suffixes. Negative numbers use unary `-`: `-5`. The maximum is 9223372036854775807 (larger literals are E0105).
- **Floats**: `1.0`, `0.5`, `1e9`, `1.5e-3` (not `1E9`, `1e+9`). Both sides of `.` are required (`1.` and `.5` are invalid); at most 17 significant digits.
- **Strings**: `"..."` on one line, with the escapes `\n`, `\t`, `\r`, `\\`, `\"` and `\u{HEX}` (any other is E0103). A raw string, `` `...` ``, is its text as written (no escapes, any lines, no backtick), only for text with a backslash, a `"` or a line break (W0102). No single quotes, character type or interpolation: use `format` or `+`.
- **Booleans**: `true`, `false`.
- **Keywords** (cannot be used as names): `fn record variant test import let var set if else while for in match case return break continue try new and or not true false int float bool str list map result void uses prop requires ensures export bytes`; `const` and `effects` only where they start an item.
- No trailing commas anywhere (E0113).

```vo
fn main() uses console {
    # A comment stands on its own line.
    let quote = "she said \"hi\"\tthen left\\";
    print(quote);
    print("line one\nline two");
    print(str(-42) + " " + str(2.50));
}
```

```output
she said "hi"	then left\
line one
line two
-42 2.5
```

## Types

| Type | Values | Notes |
|---|---|---|
| `int` | 64-bit signed | overflow is runtime error R0001 |
| `float` | 64-bit IEEE | `str` gives the fewest digits that read back as the same value, without an exponent: `str(0.1 + 0.2)` is `"0.30000000000000004"` |
| `bool` | `true`, `false` | conditions must be `bool`: no truthiness |
| `str` | immutable UTF-8 text, always valid | `len` counts bytes; text from files, input, arguments, the environment, programs and HTTP is checked when read (`err`, or R0007 where no `result` is returned) |
| `bytes` | immutable bytes 0–255 | no literal: `std.bytes`; `len`, `b[i]` (an `int`), `slice`, `+`, `==`, `<`; never mixed with `str` |
| `list[T]` | ordered, growable | reference type |
| `map[K, V]` | insertion-ordered | `K` is a key: `int`, `bool`, `str`, `bytes`, or a record or variant made of those (E0309); reference type |
| `ordered_map[K, V]` | keys ascending | `K` ordered; reference type |
| `ordered_set[T]` | ascending, each once | `T` ordered; reference type |
| `priority_queue[T]` | `pop` gives the smallest | ties leave in push order; reference type |
| `deque[T]` | cheap at both ends | reference type |
| `result[T]` | `ok(T)` or `err(str)` | `result[void]` for "ok or error, no value"; `result[T, E]` fails with an `E` |
| `Name` | a declared `record` or `variant` | records are reference types |

Types nest: `list[list[int]]`, `map[str, list[Point]]`, `result[list[str]]`.

No implicit conversions, not even `int` to `float` or anything to `str`: convert with `str(x)` (from `int`, `float` or `bool`), `float(i)`, `int(f)` (truncates toward zero), and `parse_int(s)` / `parse_float(s)`, which return a `result`. There is no `null`, no `any` and no tuple type. Function types and generics have their own sections below.

A variable's type is inferred from its initializer; `: type` states it: `let total: float = 0.0;`.

`print` takes only a `str`, so numbers go through `str(...)`:

```vo
fn main() uses console {
    let total = 5;
    print(total);
}
```

```error E0301
```

```vo
fn main() uses console {
    let total = 5;
    let ratio = float(total) / 2.0;
    print(str(total) + " " + str(ratio) + " " + str(int(ratio)));
    print(str(1.0) + " " + str(true));
}
```

```output
5 2.5 2
1.0 true
```

**Formatting**: `format("{} of {} done", 3, 10)` is `"3 of 10 done"`. The template must be a string literal or a constant; each `{}` takes the next argument (`int`, `float`, `bool` or `str`), and `{{`/`}}` are braces. Options go after a colon, in this order: alignment `<` `>` `^`, `+`, `0` (zero-pad), width, `,` (thousands), `.Nf` (N decimals, floats only), `x` or `b` (hex or binary, ints only). Numbers align right, text left. `vo check` reports a wrong count or option, with a fix where it can (also for `%d` and `f"..."`).

```vo
fn main() uses console {
    print(format("{:<8}|{:>8.2f}|{:05}|{:x}", "tea", 3.14159, 42, 255));
    print(format("{:,} {:+}", 1234567, 5));
}
```

```output
tea     |    3.14|00042|ff
1,234,567 +5
```

## Declarations

A file contains `import` lines first, then items in any order: `fn`, `record`, `variant`, `const`, `effects` (each optionally after `export`) and `test`. An item can use items declared later. No global variables, methods, classes or nested functions.

### Functions

```text
fn name(param: type, param: type) -> return_type uses effects requires cond ensures cond { ... }
```

`uses ...` lists what the function may touch (see Effects); without it the function is pure. Omit `-> type` when the function returns nothing (`-> void` is not allowed). Parameters are read-only (`set` on one is E0208). Every path of a function with a return type must reach `return` (E0403). A function without one may use `return;`. No overloading, default arguments or variadic functions. Recursion works. Functions are values (see Functions as values).

The program starts at `fn main()` or `fn main() -> result[void]` (see Results and errors).

```vo
fn clamp(x: int, low: int, high: int) -> int {
    if x < low {
        return low;
    }
    if x > high {
        return high;
    }
    return x;
}

fn greet(name: str) uses console {
    print("hi " + name);
}

fn main() uses console {
    greet("ana");
    print(str(clamp(15, 0, 10)));
}
```

```output
hi ana
10
```

### Contracts

`requires COND` and `ensures COND` clauses come after `uses`, each on its own line (`vo fmt` puts them there), any number of each:

- `requires` conditions see the parameters and are checked on every call, before the body. A false one is R0012 at the call, with the trace starting at the caller.
- `ensures` conditions also see `ret`, the returned value (only in functions with a return type; a parameter named `ret` is E0203). They are checked on every exit: `return`, a `try` that passes an error on, and the end of a function without a return type. A false one is R0013 at that exit.
- Conditions are `bool` and pure (E0603); they never change a function's effects.
- Calls through function values and builtins such as `map` are checked too, in `vo run` and `vo test`. Functions without contracts run exactly as fast as before.

```vo
fn isqrt(n: int) -> int
    requires n >= 0
    ensures ret * ret <= n and (ret + 1) * (ret + 1) > n {
    var r = 0;
    while (r + 1) * (r + 1) <= n {
        set r = r + 1;
    }
    return r;
}

fn main() uses console {
    print(str(isqrt(10)));
    print(str(isqrt(-1)));
}
```

```output
3
```

### Records

A record is a named set of fields, each ending with `;`. Create one with `new Name { field: value, ... }`, giving every field exactly once (E0305 if one is missing), in any order; without fields, `new Name {}`. Annotations on the lines above a field limit what `from_json` and std.web accept: `@size(min, max)` (characters of a `str`, items of a list or map), `@range(min, max)` (`int`, `float`), `@pattern("re")` (the whole `str`), `@default(v)` (the field may be absent: a literal or constant, an enum-like case or `[]`), `@header("Name")` (std.web input) (E0329). Read with `p.x` and write with `set p.x = v;`. Records are references: a copy of the variable, an argument or a list element points to the same record.

```vo
record Point {
    x: int;
    y: int;
}

fn moved(p: Point, dx: int) -> Point {
    return new Point { x: p.x + dx, y: p.y };
}

fn main() uses console {
    let p = new Point { x: 1, y: 2 };
    let q = moved(p, 10);
    set p.y = 7;
    print(str(p.x) + "," + str(p.y) + " " + str(q.x) + "," + str(q.y));
}
```

```output
1,7 11,2
```

### Variants

A variant is a value that is exactly one of several cases. A case carries at most one value (use a record for several) and ends with `;`. Build a value with `Name.case(value)` or, without a value, `Name.case` (no parentheses). Read it with `match` (see Statements); inside `match`, cases are written without the variant name.

```vo
variant Shape {
    circle(float);
    rect(Size);
    empty;
}

record Size {
    w: float;
    h: float;
}

fn area(s: Shape) -> float {
    match s {
        case circle(r) {
            return 3.0 * r * r;
        }
        case rect(size) {
            return size.w * size.h;
        }
        case empty {
            return 0.0;
        }
    }
}

fn main() uses console {
    let shapes = [Shape.circle(1.0), Shape.rect(new Size { w: 2.0, h: 1.5 }), Shape.empty];
    for s in shapes {
        print(str(area(s)));
    }
}
```

```output
3.0
3.0
0.0
```

### Constants

`const name = value;` (or `const name: type = value;`) names a value at the top level, written in `snake_case`. Its type is `int`, `float`, `bool` or `str` (E0332), and its value is made of literals, other constants and operators: no calls, not even `len` or `str` (E0212). The checker computes it once, by the run-time rules, so an overflow or a division by zero is a check error (E0333). A constant cannot be changed (E0208) or declared inside a function (E0122: use `let`). It works wherever a value of its type does, and where a literal is required: effect scopes, field annotations, and the templates of `format`, `regex`, `sql` and routes. `vo.effects` and every report show the value.

```vo
const out_dir = "out/";
const report = out_dir + "report.txt";
const max_lines = 3;

fn save(text: str) -> result[void] uses file.write(out_dir) {
    return write_file(report, text);
}

fn main() uses console {
    print(report + " " + str(max_lines * 2));
}
```

```output
out/report.txt 6
```

### Tests and imports

`test "name" { ... }` declares a test (see Tests). `import path;` makes another file's items visible as `module.name` (see Programs in several files).

## Effects

Every function and test declares what it may touch after its return type:

```text
fn save(r: Report) -> result[void] uses console, file.write("out/") { ... }
test "loads fixtures" uses file.read("fixtures/") { ... }
```

| Effect | Covers |
|---|---|
| console | `print`, `write`, `eprint` (stdout and stderr) |
| input | `read_line`, `read_all` |
| file.read, file.write | `read_file`, `write_file`; `std.fs` |
| file.delete | `std.fs.delete` and `rename` |
| net | `std.http` (`get`, `post`), `std.tcp.connect` |
| clock | `now_ms`, `std.time.sleep`, `local_zone` |
| random | `random_int` (seeded: not for secrets) |
| env | `env` |
| serve | `std.http.serve` (answering HTTP requests), `std.tcp.listen` |
| process | `std.process` (running other programs) |
| entropy | `std.crypto.random_bytes`, `hash_password` (the system's random source) |
| log | log records and metrics (std.log, std.metrics) |

- No `uses` clause means pure: the function cannot do any of these, so the same arguments always give the same result.
- A function must declare everything it does and everything its callees declare (E0501; its fix adds exactly the missing effects). An effect nothing needs is W0501; its fix removes it.
- Scopes (string literals or constants) narrow an effect: directories for `file.read`/`file.write`/`file.delete` (relative to where the program starts, e.g. `"out/"`; `"./"` is that directory), variable names for `env`, hosts for `net` (`"db"`: any port; `"db:5432"`: that port), addresses for `serve` (`"8000"`: this machine only; `"0.0.0.0:8000"`: the network), programs for `process` (`"git"`: found through `PATH`; `"tools/gen"`, `"/usr/bin/git"`: that file; required, E0512; matched exactly). A scope covers the directories under it. A caller's scopes must cover its callees' scopes (E0503); a callee's effect without scopes takes its callers'; an empty or absolute scope, or one with `..`, is E0506.
- Canonical order (`vo fmt`): console, input, file.read, file.write, file.delete, net, clock, random, env, serve, process, entropy, log; scopes sorted; then effect parameters and sets, alphabetically.
- An effect set names effects a program repeats: `effects Storage = file.read("data/"), file.write("data/");` (an item, `export` like others). `uses console, Storage` means the union; a set may include sets (a cycle is E0513), and an unused private set is W0505. In a type's effect argument (`Db[uses Storage]`) its scopes are dropped. Records and reports show the effects it stands for.
- `main`'s clause is the grant of `vo run`, a test's clause the grant of that test. Paths and names are also checked at run time, after resolving `..`, absolute paths and symbolic links, against every scoped declaration on the call path: one outside is R0010 (exit 70). A literal target outside the scopes is E0503 at check time.
- `--seed N` fixes `random_int`; `--clock-ms N` makes `now_ms` return N, N+1, ... (`sleep` then does not wait but moves the clock on) `vo test` uses seed 1 and clock 0 for every test.

```vo
fn area(w: int, h: int) -> int {
    return w * h;
}

fn report(w: int, h: int) uses console {
    print("area " + str(area(w, h)));
}

fn main() uses console, env("VO_DEMO") {
    report(3, 4);
    match env("VO_DEMO") {
        case ok(v) {
            print(v);
        }
        case err(e) {
            print(e);
        }
    }
}
```

```output
area 12
not set
```

### Reviewing effects

`vo.effects`, next to the entry file (or in an ancestor directory), lists the effects of every function and test, one line each, sorted:

```text
# vo effects record: update with `vo effects --update`
main.vo:main uses console, file.write("out/")
report.vo:save_report uses file.write("out/reports/")
util.vo:clamp pure
```

`vo check` compares the program with it. A function or test that gained an effect or a wider scope is E0504, noting the calls that brought it in. Fewer effects are W0502, a rename with unchanged effects is W0503, and a multi-file program without a record is W0504. Only `vo effects --update` changes the record, printing exactly the changes it writes, so a reviewer approves "save_report gains file.write" instead of reading every line. `vo effects FILE` prints the capabilities of `main` and of each test.

## Statements

**First-token rule**: every statement starts with one of `let`, `var`, `set`, `if`, `while`, `for`, `match`, `return`, `break`, `continue`, `try`, or is a call `name(...)`. Anything else, such as `x = 1;`, `x += 1;`, `i++;` or a bare expression, is an error.

Simple statements (`let`, `var`, `set`, `return`, `break`, `continue`, calls, `try`) end with `;`. Statements ending with a block (`if`, `while`, `for`, `match`) take no `;` after `}`. Braces are required, even around one statement.

| Statement | Form |
|---|---|
| immutable binding | `let name = expr;` or `let name: type = expr;` |
| mutable binding | `var name = expr;` (an initializer is required) |
| assignment | `set name = expr;`, `set p.field = expr;`, `set xs[i] = expr;`, `set m[k] = expr;` |
| discard a value | `let _ = expr;` (not for `result` values: E0404) |
| condition | `if c { } else if c { } else { }` |
| loops | `while c { }`, `for x in list { }`, `for i, x in list { }`, `for k, v in map { }`, `for c in text { }`, `for i in a..b { }`, `for _ in 0..n { }` |
| match | `match e { case name(binding) { } case other { } }` |
| leave | `return;`, `return expr;`, `break;`, `continue;` |
| call | `f(args);` (only if `f` returns nothing; see below) |
| propagate | `try f();` (only if `f` returns `result[void]`) |

Details:

- `let` cannot be reassigned (E0208, fix: use `var`). `set` only works on a `var`, a record field, a list element or a map entry.
- A name cannot be declared again inside a nested block while it is visible (shadowing, E0203). Sibling blocks may reuse names. A local never read is W0201.
- A call statement must not drop a value: `len(s);` alone is E0405 (write `let _ = len(s);` or use the value), and ignoring a `result` is E0404.
- `if` and `while` conditions must be `bool` (E0301). Parentheses around a condition are optional.
- `for x in xs` visits list elements, `for c in s` the characters of a `str`. `for i in a..b` visits the ints `a, a+1, ..., b-1` (empty when `a >= b`). `for _ in 0..n` repeats n times without a counter. `..` exists only in `for`. `for k, v in m` visits a map's keys with their values, `for i, x in xs` a list's indexes with their elements (`_` for either): what was there when the loop began; additions are not visited, removed entries are skipped. No C-style `for (;;)`, `do`, `loop` or `until`.
- `break` and `continue` apply to the innermost `while` or `for` (E0407 outside a loop), also from inside a `match` arm.
- `match` works on variant and `result` values only, not on `int` or `str` (use `if`/`else if`). Each case must appear exactly once (E0401 if one is missing, whose fix inserts it; E0402 if repeated). No default arm. Bind a case's value with `case name(v)`, ignore it with `case name(_)`, and write a case without a value as `case name`.

```vo
fn main() uses console {
    var total = 0;
    var n = 10;
    while n > 0 {
        set n = n - 3;
        if n % 2 == 0 {
            continue;
        }
        set total = total + n;
    }
    for i in 0..10 {
        if i == 3 {
            break;
        } else if i == 1 {
            print("one");
        } else {
            print(str(i));
        }
    }
    for word in ["a", "b"] {
        print(word);
    }
    print(str(total) + " " + str(n));
}
```

```output
0
one
2
a
b
8 -2
```

Assignment always starts with `set`:

```vo
fn main() uses console {
    var x = 1;
    x = 2;
    print(str(x));
}
```

```error E0114
```

## Expressions and operators

Precedence, from tightest to loosest:

| Level | Operators | Notes |
|---|---|---|
| 1 | `f(...)`, `x.field`, `xs[i]`, `(e)` | call, field, index, grouping |
| 2 | `-x`, `try e` | unary minus; `try` (see Results and errors) |
| 3 | `*` `/` `%` | left to right |
| 4 | `+` `-` | left to right; `+` also joins two `str` |
| 5 | `==` `!=` `<` `<=` `>` `>=` | at most one per expression: no chaining |
| 6 | `not` | `not a == b` means `not (a == b)` |
| 7 | `and` | short-circuit |
| 8 | `or` | short-circuit |

- Logic uses words: `and`, `or`, `not`. `&&`, `||` and `!` do not exist. Operands must be `bool`.
- Both operands of `+ - * /` must have the same type, `int` or `float`. `int + float` is E0301: convert one side with `float(i)` or `int(f)`. `%` is `int` only.
- `int` division truncates toward zero: `7 / 2` is `3`, `-7 / 2` is `-3`, `-7 % 3` is `-1`. Dividing an `int` by zero is R0002. Overflow, including `-9223372036854775807 - 2`, is R0001. `float` arithmetic follows IEEE: `1.0 / 0.0` is `inf`, `0.0 / 0.0` is `nan`.
- `<`, `<=`, `>`, `>=` compare two values of one ordered type: `int`, `float`, `str` and `bytes` (byte order), `bool` (`false < true`), and records and variants of ordered parts (in declaration order: the fields, or the case, then its payload).
- `==` and `!=` compare structurally: lists, maps, records, variants and results are equal when their contents are equal (`nan != nan`, also inside). Not functions, nor values holding one (E0308).
- `x.field` reads a record field. Strings cannot be indexed (`s[0]` is E0308): use `slice`, `for c in s` or `byte_at`. No methods: write `len(xs)`, not `xs.len()`.
- There is no ternary `a ? b : c`, no `+=`, no `++`, no `**` and no bitwise operators (`&`, `<<`: use `std.bits`).

```vo
fn main() uses console {
    print(str(1 + 2 * 3) + " " + str((1 + 2) * 3));
    print(str(7 / 2) + " " + str(-7 / 2) + " " + str(7 % 3) + " " + str(-7 % 3));
    print(str(7.0 / 2.0) + " " + str(float(7) / 2.0));
    let a = 5;
    print(str(a > 1 and a < 10) + " " + str(not (a == 5) or false));
    print(str("apple" < "banana") + " " + str([1, 2] == [1, 2]));
}
```

```output
7 9
3 -3 1 -1
3.5 3.5
true false
true true
```

Mixing `int` and `float` is an error:

```vo
fn main() uses console {
    let count = 3;
    let mean = 7.5 / count;
    print(str(mean));
}
```

```error E0301
```

Comparisons do not chain. Write `0 < x and x < 10`:

```vo
fn main() {
    let x = 5;
    print(str(0 < x < 10));
}
```

```error E0110
```

## Functions as values

```text
fn(x: int) -> int { return x * 2; }        anonymous function (as `fn`, without a name, type parameters, `uses` or contracts)
fn(int) -> int uses console                 function type (parameter types, return type, effects)
```

- A named function used without `(...)` is a value: `map(xs, double)`. A generic one takes its type and effect arguments from the function type expected there (`serve_workers("8080", 4, open_db, handle)` with `handle[D: effects](db: Database[D], ...)`; E0310 when nothing fixes them). Builtins are not values: wrap one, `fn(s: str) -> int { return len(s); }`. Anything that evaluates to a function can be called: `f(1)`, `make_adder(1)(2)`, `handlers[0](x)`, `box.on_save(x)` (a record field of function type).
- Anonymous functions write parameter and return types like named functions. They never write `uses`: their effects come from their body (E0507). A function value fits a function type whose effects cover its own (E0502 otherwise).
- They capture the `let` bindings and parameters they use, by value, when they are created. Capturing a `var` is E0505; copy it into a `let` first.
- Function types go anywhere a type can: parameters, return types, `let`, record fields, lists, maps.
- A function returning a function type writes the type's `uses` first: `fn f() -> fn(str) uses console {` declares that the returned function prints. To declare that `f` itself prints and returns a pure function, parenthesize the type: `fn f() -> (fn(str)) uses console {`.

| Builtin | Signature | Notes |
|---|---|---|
| sort_by | `(xs: list[T], key: fn(T) -> C)` | in place, stable; `C` is an ordered type (E0313) |
| map | `(xs: list[T], f: fn(T) -> U) -> list[U]` | `f` of each element, in order |
| filter | `(xs: list[T], f: fn(T) -> bool) -> list[T]` | the elements for which `f` is true, in order |
| fold | `(xs: list[T], init: A, f: fn(A, T) -> A) -> A` | `f(f(init, xs[0]), xs[1])` ..., `init` for an empty list |
| any, all | `(xs: list[T], f: fn(T) -> bool) -> bool` | stop at the first element that decides |
| concurrent_map | `(xs: list[T], limit: int, f: fn(T) -> U) -> list[U]` | like `map`, at most `limit` (1 to 64, else R0007) tasks at a time |
| map_err | `(r: result[T, E1], f: fn(E1) -> E2) -> result[T, E2]` | an `ok` unchanged; `err(e)` becomes `err(f(e))` |

The function's effects become the caller's: `map` with a function that prints needs `uses console`.

```vo
record Person {
    name: str;
    age: int;
}

fn make_adder(n: int) -> fn(int) -> int {
    return fn(x: int) -> int {
        return x + n;
    };
}

fn main() uses console {
    let people = [new Person { name: "cy", age: 30 }, new Person { name: "al", age: 25 }];
    sort_by(people, fn(p: Person) -> int {
        return p.age;
    });
    print(join(map(people, fn(p: Person) -> str {
        return p.name;
    }), ","));
    let total = fold([1, 2, 3], 0, fn(acc: int, x: int) -> int {
        return acc + x;
    });
    print(str(make_adder(total)(4)));
}
```

```output
al,cy
10
```

**Tasks**: `concurrent_map` runs `f` on each element as a separate task, for waiting on several things at once (HTTP requests). Results, and each task's whole output, come in the order of `xs`, however the tasks finish. Each task works on its own copy of everything it gets or captures: changes stay in the task, only its result comes back. The first task in `xs` order that stops with a runtime error or `exit` stops the others and the program, as `map` would. Results cannot hold functions (E0314) or a collection with its own order (R0015); `f` cannot read stdin (E0511). Inside a task, tasks run one at a time. Each task has its own random sequence (fixed by `--seed`) and its own `--max-memory`.

```vo
fn work(n: int) -> int uses console {
    print("task " + str(n));
    return n * n;
}

fn main() uses console {
    let squares = concurrent_map([3, 1, 2], 2, work);
    print(str(squares[0] + squares[1] + squares[2]));
}
```

```output
task 3
task 1
task 2
14
```

## Generics

```text
fn largest[T: ordered](xs: list[T], start: T) -> T { ... }
record Pair[A, B] { first: A; second: B; }
variant Tree[T] { leaf; node(Node[T]); }
fn each[T, E: effects](xs: list[T], f: fn(T) uses E) uses E { ... }
```

- Type parameters follow the name. A plain `T` supports only `==` and `!=`; `T: ordered` also `<`, `<=`, `>`, `>=`, `min`, `max` and `sort`, and must be an ordered type (Operators; E0311, E0313). `T: json` is any type made of scalars, `bytes`, lists, `map[str, V]`, records and variants (E0328 otherwise). `K: key` is a map key type (E0309). `E: effects` is an effect parameter: whatever effects the function passed in has, the caller needs.
- Generic types are written with their arguments: `Pair[int, str]` (E0312).
- Type arguments are never written at calls, and may be left out of `new` and of cases: they come from the arguments or field values, then from the expected type (a parameter, a return type, an annotated variable): `insert(Tree.leaf, 5)`, `new Pair { first: 1, second: "a" }`. Effect arguments come from the expected type first when it has them, so values with fewer effects fit a declared `list[Route[uses file.read]]`; a type's effects allow any scope. When they cannot be inferred, annotate the variable (E0310).
- Generic code is checked once, where it is written.
- Records and variants can take an effect parameter, `record Database[E: effects] { ... }`. In a type, effects come last, after `uses`: `Database[uses file.read, file.write]`; without them the type has no effects (`Database`). A function using such a value declares `uses E`, as for a function value; `fn save[D: effects](db: Database[D]) uses D` takes any database. Types carry no scopes (E0327).

```vo
record Pair[A, B] {
    first: A;
    second: B;
}

fn swap[A, B](p: Pair[A, B]) -> Pair[B, A] {
    return new Pair[B, A] { first: p.second, second: p.first };
}

fn largest[T: ordered](xs: list[T], start: T) -> T {
    var best = start;
    for x in xs {
        set best = max(best, x);
    }
    return best;
}

fn each[T, E: effects](xs: list[T], f: fn(T) uses E) uses E {
    for x in xs {
        f(x);
    }
}

fn main() uses console {
    let p = swap(new Pair[int, str] { first: 1, second: "one" });
    print(p.first + " " + str(largest([3, 9, 4], 0)));
    each(["a", "b"], fn(s: str) {
        print(s);
    });
}
```

```output
one 9
a
b
```

## Results and errors

vo has no exceptions and no `null`. A function that can fail returns `result[T]`, either `ok(value)` or `err(message)`; the error is always a `str`. One with no value returns `result[void]` and succeeds with `ok()`.

- Build: `ok(v)`, `ok()`, `err("message")`. Match the success of a `result[void]` with `case ok()`, as it is built. The result type must be known from the function's return type or an annotation: `let r: result[int] = ok(1);` (`let r = ok(1);` is E0307).
- Handle with `match`, which must cover both `ok` and `err`.
- Propagate with `try`: `try e` gives the `ok` value, or immediately returns the `err` from the current function. `try` is only allowed in a function that returns a `result` (E0406). Any `result[T]` error can propagate through any `result[U]` function, since both carry a `str`.
- `try f();` as a statement is only for `result[void]`. For other results use the value, or write `let _ = try f();`.
- A `result` can never be ignored: `f();` alone or `let _ = f();` when `f` returns one is E0404.
- `main` may be `fn main() -> result[void]`, ending with `return ok();`. If it returns `err(msg)`, vo prints `error: msg` to stderr and exits with status 1.
- Runtime errors (division by zero, index out of bounds, missing map key, overflow...) are not results: they stop the program with exit status 70 and an R code. Check for them first with `if`, `len` and `has`.

```vo
fn half(n: int) -> result[int] {
    if n % 2 != 0 {
        return err("odd: " + str(n));
    }
    return ok(n / 2);
}

fn quarter(n: int) -> result[int] {
    let h = try half(n);
    return half(h);
}

fn show(r: result[int]) uses console {
    match r {
        case ok(v) {
            print("ok " + str(v));
        }
        case err(e) {
            print("error " + e);
        }
    }
}

fn main() uses console {
    show(quarter(8));
    show(quarter(6));
    show(parse_int("12x"));
}
```

```output
ok 2
error odd: 3
error invalid int: 12x
```

Using `try` in `main` and `result[void]`:

```vo
fn check_positive(n: int) -> result[void] {
    if n <= 0 {
        return err("not positive: " + str(n));
    }
    return ok();
}

fn main() -> result[void] uses console {
    let n = try parse_int("42");
    try check_positive(n);
    match check_positive(-1) {
        case ok() {
            print("fine");
        }
        case err(e) {
            print(e);
        }
    }
    let total = try parse_int("8") + n;
    print(str(total));
    return ok();
}
```

```output
not positive: -1
50
```

**Typed errors**: `result[T, E]` fails with a value of type `E`, usually a variant of the ways to fail, so callers `match` the error and the checker sees every case (`result[T]` is `result[T, str]`). `err(e)` takes an `E`. `try` passes an error up only to a function with the same error type (E0412); `map_err(r, f)` converts one. `main` and tests fail with a `str`.

```vo
variant LoadError {
    missing(str);
    bad_number(str);
}

fn load(text: str) -> result[int, LoadError] {
    if text == "" {
        return err(LoadError.missing("count"));
    }
    return map_err(parse_int(text), fn(m: str) -> LoadError {
        return LoadError.bad_number(text);
    });
}

fn describe(e: LoadError) -> str {
    match e {
        case missing(what) {
            return "no " + what;
        }
        case bad_number(text) {
            return "not a number: " + text;
        }
    }
}

fn main() -> result[void] uses console {
    print(str(try map_err(load("7"), describe)));
    match load("x") {
        case ok(n) {
            print(str(n));
        }
        case err(e) {
            print(describe(e));
        }
    }
    return ok();
}
```

```output
7
not a number: x
```

`try e + 1` means `(try e) + 1`. A `result` must be handled:

```vo
fn main() {
    parse_int("1");
}
```

```error E0404
```

## Lists and maps

**Lists**: `[1, 2, 3]` builds a list from at least one element of one type. An empty list is `new list[int]`, or `[]` where the context gives the type (E0306 otherwise). `xs[i]` reads (0-based; out of range is R0003), `set xs[i] = v;` replaces an existing element, `push(xs, v)` appends, `pop(xs)` removes the last element and returns a `result`. `len(xs)` is the length. `slice(xs, a, b)` returns a new list with elements `a` to `b - 1`. `sort(xs)` sorts a list of `int`, `float` or `str` in place.

**Maps**: no map literal: create with `new map[str, int]`. A record key (`map[Cell, str]`) replaces a key glued together from text. `set m[k] = v;` inserts or updates. `m[k]` reads, stopping with R0004 if the key is missing: check `has(m, k)` first. `remove(m, k)` deletes. `keys(m)` lists keys in insertion order; updating an existing key keeps its position. `len(m)` counts entries.

**References**: lists, maps and records are shared, not copied, by assignment, by passing them to a function and by storing them in another collection. `let` only stops rebinding the name; the contents can still change. Copy a list with `slice(xs, 0, len(xs))`.

```vo
fn add_one(xs: list[int]) {
    push(xs, 1);
}

fn main() uses console {
    let xs = new list[int];
    push(xs, 3);
    add_one(xs);
    let alias = xs;
    set alias[0] = 9;
    print(str(len(xs)) + " " + str(xs[0]));
    let copy = slice(xs, 0, len(xs));
    push(copy, 5);
    print(str(len(xs)) + " " + str(len(copy)));
    let grid = [[1, 2], [3, 4]];
    print(str(grid[1][0]));
    let words = ["pear", "fig", "apple"];
    sort(words);
    print(join(words, ","));
}
```

```output
2 9
2 3
3
apple,fig,pear
```

```vo
record Cell {
    row: int;
    col: int;
}

fn main() uses console {
    let seen = new map[Cell, str];
    set seen[new Cell { row: 1, col: 2 }] = "x";
    print(seen[new Cell { row: 1, col: 2 }]);
    let counts = new map[str, int];
    for w in split("b a b c b", " ") {
        if has(counts, w) {
            set counts[w] = counts[w] + 1;
        } else {
            set counts[w] = 1;
        }
    }
    remove(counts, "c");
    for k, n in counts {
        print(k + "=" + str(n));
    }
    print(str(len(counts)));
}
```

```output
x
b=3
a=1
2
```

```vo
fn main() uses console {
    let xs = [];
    print(str(len(xs)));
}
```

```error E0306
```

## Builtins

`T` is any type, `K` a key type, `C` is `int`, `float` or `str`. Builtins are called like functions: `len(xs)`, never `xs.len()`.

| Builtin | Signature | Notes |
|---|---|---|
| print | `(s: str)` | stdout, adds `\n` |
| write | `(s: str)` | stdout, no newline |
| eprint | `(s: str)` | stderr, adds `\n` |
| read_line | `() -> result[str]` | one line of stdin without its `\n` (or `\r\n`); `err("eof")` at end of input |
| read_all | `() -> str` | the rest of stdin |
| read_file | `(path: str) -> result[str]` | the whole file; `err` if it cannot be read or is not UTF-8 |
| write_file | `(path: str, s: str) -> result[void]` | creates or replaces the file |
| args | `() -> list[str]` | arguments after `--` in `vo run f.vo -- a b` |
| exit | `(code: int)` | ends the program; code 0–63, else R0008 |
| now_ms | `() -> int` | wall-clock milliseconds; with `--clock-ms N`: N, N+1, ... |
| random_int | `(lo: int, hi: int) -> int` | uniform in `lo..hi` inclusive; R0007 if `lo > hi`; fixed by `--seed N` |
| env | `(name: str) -> result[str]` | an environment variable; `err("not set")` |
| str | `(x: int \| float \| bool) -> str` | a float as the fewest digits `parse_float` reads back as the same value, without an exponent; `str(1.0)` is `"1.0"`; `nan`, `inf`, `-inf` (not read back) |
| format | `(template: str, args...) -> str` | `template` a string literal; each `{}` or `{:spec}` takes the next argument (see Types) |
| int | `(f: float) -> int` | truncates toward zero; R0007 for NaN or out of range |
| float | `(i: int) -> float` | exact up to 2^53, else the nearest float |
| parse_int | `(s: str) -> result[int]` | the whole text must be the number: no spaces, no `+` (`trim` first); else `err("invalid int: ...")` |
| parse_float | `(s: str) -> result[float]` | `-`, digits, optionally `.` and digits and an exponent (`1e9`), at most 17 significant digits; else `err("invalid float: ...")` |
| len | `(x: str \| list[T] \| map[K, T]) -> int` | for `str`: bytes, not characters |
| slice | `(s: str, a: int, b: int) -> str` | bytes `a` to `b - 1`; R0003 out of range; R0007 if it splits a UTF-8 character |
| slice | `(xs: list[T], a: int, b: int) -> list[T]` | a new list; R0003 out of range |
| find | `(s: str, sub: str) -> int` | byte offset of the first match, or -1 |
| split | `(s: str, sep: str) -> list[str]` | `split("a,,b", ",")` is `["a", "", "b"]`; empty `sep` is R0007 |
| join | `(xs: list[str], sep: str) -> str` | the strings with `sep` between them |
| trim | `(s: str) -> str` | removes ASCII whitespace at both ends |
| chars | `(s: str) -> list[str]` | one string per code point |
| byte_at | `(s: str, i: int) -> int` | byte `i`, 0 to 255, without a list; R0003 out of range |
| upper, lower | `(s: str) -> str` | Latin, Greek and Cyrillic letters |
| ord | `(s: str) -> int` | code point of a one-character string (R0007 otherwise) |
| chr | `(i: int) -> str` | the one-character string for a code point |
| abs | `(x: int \| float) -> int \| float` | same type as `x` |
| min, max | `(a: C, b: C) -> C` | `C` is an ordered type |
| push | `(xs: list[T], v: T)` | appends |
| pop | `(xs: list[T]) -> result[T]` | removes the last element; `err("empty")` |
| sort | `(xs: list[C])` | in place, ascending, stable |
| has | `(m: map[K, T], k: K) -> bool` | whether the map has the key |
| get_or | `(m: map[K, T], k: K, default: T) -> T` | the default when the key is missing; never adds it |
| remove | `(m: map[K, T], k: K)` | no effect if the key is missing |
| keys | `(m: map[K, T]) -> list[K]` | in insertion order |
| ok | `(v: T) -> result[T]` | success; `ok()` for `result[void]`; its type comes from its place |
| err | `(message: str) -> result[T]` | failure with a message |
| todo | `(what: str) -> T` | a hole of any type: the program checks; reaching it is R0011 |
| assume | `(cond: bool)` | only in `prop`: discards an input that does not apply |
| expect | `(cond: bool)` | only inside `test` (E0410) |
| expect_eq | `(actual: T, expected: T)` | only inside `test` (E0410); reports both values |

Indexing is not a builtin but follows the same rules: `xs[i]` (R0003 out of range), `m[k]` (R0004 missing key), `set xs[i] = v;`, `set m[k] = v;`.

Create the collections like maps: `new ordered_map[str, int]`, `new deque[int]`. `len`, `has`, `remove`, `get_or`, `keys`, `push`, `pop`, indexing and `for` work on them: `push` adds to a set or queue (a deque: at the back), `pop` takes a queue's smallest item or a deque's back element. `std.collections` has the rest (`first_key`, `after`, `union`, `push_front`, ...) and `ordered_map_by(cmp)` etc. for another order. A `for` over a set, deque or queue visits a copy taken first (all of it, even with `break`); over an `ordered_map`, as over a `map`. Changing a record while it is a key or set element (here or in a `map`) loses it: remove it, change it, add it again.

```vo
fn main() uses console {
    let s = "  héllo, world ";
    let t = trim(s);
    print("[" + t + "] " + str(len(t)) + " " + str(len(chars(t))));
    print(slice(t, 0, 6) + "|" + str(find(t, "world")) + "|" + str(find(t, "xyz")));
    let parts = split("a,b,,c", ",");
    print(str(len(parts)) + " " + join(parts, "|"));
    let xs = [3, 1, 2];
    sort(xs);
    match pop(xs) {
        case ok(v) {
            print("popped " + str(v) + ", left " + str(len(xs)));
        }
        case err(e) {
            print(e);
        }
    }
    write("no newline, ");
    print(str(int(-3.99)) + " " + str(float(2)));
}
```

```output
[héllo, world] 13 12
héllo|8|-1
4 a|b||c
popped 3, left 2
no newline, -3 2.0
```

Reading input line by line until the end (stdin is empty here, so this prints `0`):

```vo
fn main() -> result[void] uses console, input {
    var total = 0;
    while true {
        match read_line() {
            case ok(line) {
                set total = total + try parse_int(trim(line));
            }
            case err(_) {
                break;
            }
        }
    }
    print(str(total));
    return ok();
}
```

```output
0
```

## Holes

`todo("description")` stands for code not written yet. It has whatever type its place expects, so the rest of the program checks, runs and tests while the gap is open.

- Each hole is warning W0601, whose `expected` is the type needed and whose `scope` lists the names in scope with their types. With `--json` the `check` summary counts them: `"holes":N`.
- The place must have a type: `let x = todo("...");` is E0601 (write `let x: int = todo("...");`).
- `todo("...");` as a statement ends its path like `return` and needs the function's return type.
- While a function has a hole, unused locals (W0201) and unused effects (W0501) are not reported.
- Running into a hole stops with R0011 (in `vo test`, that test is `error`).

```vo-test
fn mean(xs: list[int]) -> int {
    todo("the average, 0 for an empty list");
}

test "mean of three" {
    expect_eq(mean([1, 2, 3]), 2);
}
```

```output
main.vo:2:5: warning[W0601]: Hole: `"the average, 0 for an empty list"` needs a value of type `int`.
   2 |     todo("the average, 0 for an empty list");
     |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    in scope: xs: list[int]
ERROR mean of three (0ms)
  main.vo:2:5: runtime error[R0011]: Hole reached: the average, 0 for an empty list.
    at mean (main.vo:2:5)
        xs: list[int] = [1, 2, 3]
    at test "mean of three" (main.vo:6:15)
0 passed, 0 failed, 1 errored
```

## Tests

A `test "name" uses ... { ... }` item (the clause is optional, see Effects) holds statements that check behavior with `expect(cond)` and `expect_eq(actual, expected)`, which work only inside tests. A test fails at its first failing check, and `expect_eq` reports both values. Test names are unique in a file. Tests cannot use `try` (they return no result): `match` instead, or compare whole results with `expect_eq(parse_int("3"), ok(3))`.

`vo test file.vo` runs every test and property of the program: the file and the files it imports (a file with tests needs no `main`); `vo run` ignores tests. `--filter TEXT` runs only tests whose name contains `TEXT`. A test that hits a runtime error is reported as `error`, and the remaining tests still run. The exit status is 1 if any test did not pass.

Each test has an identifier `FILE::NAME` (`main.vo::doubles`). `--list` checks and lists the selected tests (identifier, kind, location, `@spec` IDs, effects, the comment above) without running anything. `--test ID` runs exactly that test, `--spec ID` the tests naming `ID` (or `ID/...`), `--diff FILE` the tests `vo impact` reports. Options combine; an empty selection passes. A failing `expect` on a comparison shows each operand (for `and`, the false part); a failing `expect_eq` on larger values shows the first difference. What a test prints is kept with its report: under a test that did not pass, and as `output` in JSON. Tests share the working directory and run in listed order, so a test can see files another wrote.

```vo-test
fn clamp(x: int, hi: int) -> int {
    if x > hi {
        return hi;
    }
    return x;
}

test "clamps" uses console {
    let limit = 5;
    print("checking");
    expect(clamp(7, limit) < limit);
}
```

```output
FAIL clamps (0ms)
  main.vo:11:5: `expect` failed: `clamp(7, limit) < limit` is false.
    clamp(7, limit): 5
    limit:           5
  output:
    checking
0 passed, 1 failed
```

```vo-test
fn double(x: int) -> int {
    return x * 2;
}

test "doubles" {
    expect_eq(double(4), 8);
    expect(double(-1) < 0);
}

test "parses" {
    expect_eq(parse_int("21"), ok(21));
}
```

```output
PASS doubles (0ms)
PASS parses (0ms)
2 passed, 0 failed
```

A failing check prints its location and both values under the `FAIL` line:

```text
FAIL doubles (1ms)
  main.vo:6:5: `expect_eq` failed: the values differ.
    expected: 8
    actual:   9
```

### Property tests

`prop "name" (x: T, ...) uses ... { ... }` is a test with parameters: `vo test` runs the body on 100 generated inputs. Write what must hold for every input, not one example.

- Types: `int` (mostly -100..100, often 0, ±1 and the extremes; build larger values from several), `float` (finite), `bool`, `str` (short, with separators and non-ASCII characters), lists, maps, `result`, records and variants. A parameter whose type contains a function type, or has no finite value, is E0602.
- Inputs are fixed by `--seed N` (default 1): the same file and seed give the same report. Each case gets fresh arguments and the default seed and clock; `--timeout` covers the whole property.
- `assume(cond)` discards an input that does not apply, only in a `prop` (E0604). Fewer than 100 accepted inputs in 1,000 attempts fails the property.
- A failing input is shrunk to a smaller one that fails the same way, and reported as `counterexample` lines (JSON: `prop`, `cases`, `discarded`, `shrinks`, `counterexample`).

```vo-test
fn distance(a: int, b: int) -> int {
    if a > b {
        return a - b;
    }
    return a - b;
}

prop "distance is symmetric" (a: int, b: int) {
    assume(a > -1000 and a < 1000 and b > -1000 and b < 1000);
    expect_eq(distance(a, b), distance(b, a));
}

prop "sorting twice changes nothing" (xs: list[str]) {
    let once = slice(xs, 0, len(xs));
    sort(once);
    let twice = slice(once, 0, len(once));
    sort(twice);
    expect_eq(twice, once);
}
```

```output
FAIL distance is symmetric (1 case, 0ms)
  main.vo:10:5: `expect_eq` failed: the values differ.
    expected: 1
    actual:   -1
    counterexample: a = -1
    counterexample: b = 0
    shrunk in 0 steps
PASS sorting twice changes nothing (100 cases, 0ms)
1 passed, 1 failed
```

`@spec("FR-003", "US1/AC2")` on the line right above a `test` or `prop` names the requirements it proves (an ID is letters, digits, `-` and `_`, parts separated by `/`). It changes nothing about the test: `vo test` adds the IDs to a failing test's line and ends with a summary per ID, and `vo query specs main.vo` lists every ID with its tests. The other annotation is `@allow` (Structure).

### Coverage and mutation testing

`vo test --coverage` lists what the selected tests never ran in the program's own files: functions, branches (also an `if` whose condition was never false), `match` cases, loop bodies, then totals. `vo test --mutate` runs the tests (they must pass, T0012), then changes the functions they reach one step at a time (`<` to `<=`, `+` to `-`, `and` to `or`, `true` to `false`, dropping `not`, `n` to `n + 1`) and reruns the tests that reach it. Each change no test notices (`SURVIVED`) is a check you are probably missing; a failing, erroring or much slower test counts as noticing. `--max-mutants N` (default 500) caps the changes, `--diff` limits them to changed functions; neither option changes the exit status. W0602 is a test with no check, W0603 a check that cannot depend on the program.

## Programs in several files

`import a.b;` loads the file `a/b.vo`, relative to the importing file's directory, and binds its module name `b`: its top-level items (functions, records, variants, constants, effect sets) are written `b.name`, `b.Point`, `b.Shape.circle(1.0)`, `uses b.Storage`. Only items declared with `export` can be used; the others are private (E0209, whose fix adds `export`). An exported function, record or variant cannot use a private type of its file (E0210). Write one `import` per module, before all items; no local may have a module's name (E0203). Paths cannot contain `..`, and import cycles are E0206. A module used without its import is E0204, whose fix adds it. Run the program through the file containing `main`: `vo run main.vo`.

```vo
# file: geometry.vo
export record Point {
    x: int;
    y: int;
}

export fn dist2(p: Point) -> int {
    return p.x * p.x + p.y * p.y;
}
```

```vo
# file: main.vo
import geometry;

fn main() uses console {
    print(str(geometry.dist2(new geometry.Point { x: 3, y: 4 })));
}
```

```output
25
```

### Structure

A program is a thin shell over a pure core: `main` and the other functions with effects gather input, call functions without `uses` and act on their result, so the decisions can be tested with no grant. Write one concept per file, tests next to the code they test, and one function instead of copies (with an effect parameter when copies differ only in effects). `vo check` warns about a function with effects and 15 or more statements using none (W0701), a file over 400 code lines (W0702), a function over 50 statements (W0703), repeated statements (W0704) and a test far from its code (W0705). `@allow("W0703", "reason")` above a `fn`, `test` or `prop` accepts one of these (or W0506) there; an unused one is W0706.

## Standard library and packages

`import std.MODULE;` imports a module of the standard library (written in vo, built into `vo`), its items written `MODULE.NAME`. Its items are not listed here: `vo doc` prints every module and builtin, `vo doc std.json` one module, and `vo doc std.json.parse_json` one item with its signature, effects and an example (`--json` gives records).

| Module | Contents |
|---|---|
| std.json | `Json` values: `parse_json`, `write_json`, `write_json_pretty`, field readers; `to_json(v)`, `from_json(j)` for any `T: json` (strict: every problem as `path: reason`) |
| std.time | calendar times in UTC and zones (`load_zone`, `in_zone`), ISO 8601 text, durations (ms), `sleep` (clock) |
| std.text | `replace`, `contains`, `pad_left`, `lines`, `words`, `char_count`, ... |
| std.random | `unit`, `pick`, `shuffled` (random) |
| std.http | HTTP(S) clients `get`, `post`, `send`, `send_stream` (net); servers `serve`, `serve_workers` (serve, HTTP only); `*_bytes` forms |
| std.collections | neighbors (an `Option`) and ranges of the ordered collections, set algebra, deque fronts, `_by(cmp)` orders |
| std.fs | `info`, `list_dir`, `walk`, `read_bytes`, `each_line` (file.read); `append_file`, `write_bytes`, `make_dir` (file.write); `delete`, `rename` (file.delete) |
| std.process | `run`, `run_with`, `run_lines` (and `_bytes` forms): a program run to its end, no shell (process) |
| std.bytes | text, ints, hex and base64 to and from `bytes`; `read_stdin`, `write_stdout` |
| std.decimal | exact amounts: `parse_decimal`, arithmetic, `round_to` (a `Rounding` is always stated), `split_evenly`, ... |
| std.bits | `bit_and`, `shift_left`, `parse_hex`, ... on the 64-bit form of an `int` |
| std.math | `sqrt`, `pow`, `exp`, `log`, trigonometry; `floor`, `ceil`, `round` to `int` (out of domain is R0007) |
| std.regex | `regex("...")` (checked by `vo check`), `parse_regex`, matching, `replace_matches` (`$1`), `split_on`: linear time |
| std.sqlite | `open`, `open_readonly`, `open_memory` give a `Database[E]` with the file effects used (none in memory); `exec`, `query` (need `E`) with `sql("...")` (checked) and `?` parameters; `transaction`; `int_column` and other readers |
| std.crypto | `sha256`, `hmac_sha256`, `same_bytes` (constant time); `random_bytes(n)`, `hash_password` (entropy); `verify_password` |
| std.tcp | TCP as `bytes`: `connect`, `connect_tls`, `start_tls` (net), `listen`, `accept` (serve); `write_all`, `read`, `read_until`, `close_conn` on a `Conn[E]` (need `E`); `timeout_ms` 0 to 600000 on every wait (0: the most); `TcpError` |
| std.option | `Option[T]` (`some`, `none`) for absence, `or_else`, `is_some`, `to_result` |
| std.web | APIs over `serve_workers`: `web_app`, `on_get` (and other methods), `serve_app`, steps (`request_logs`, `request_metrics`), `metrics_route`, `health_routes`, `load_settings` |
| std.log | JSON log records (`log`): `log_info(message, [field(name, v)])`, `log_warn`, `logged()` in tests; secret-like names redacted |
| std.metrics | `counter`, `gauge`, `histogram`; `add_to`, `set_to`, `observe` (`log`); `metrics_text` |
| std.security | access control for `std.web`: `policy(rules, ways, now)`, `rule(path, Access.signed_in)`, `secure`, ways `tokens`, `basic`, `browser(b)` (session cookie); `require_user` |
| std.lists | `has_item`, `index_of`, `reversed`, `sort_by_desc`, `min_by`, `sum`, `group_by`, ... |
| std.workers | processes beside the program: `start_worker(f)`, `tell`, `receive(w, Wait.forever)`, `receive_first`, `stop_worker`, `with_deadline(ms, f)`; inside: `next_message`, `emit` |

- A library function's effects work like a builtin's: the caller declares them, with any scope (E0501 otherwise), and the call runs with the caller's grant, so `uses net("localhost")` limits `std.http.get` to that host at run time (R0010). Library functions are not listed in `vo.effects`.
- `https://` URLs are verified: the server's certificate must lead to an authority in the machine's trust store (or a file given with `--ca-file`), be valid and name the host, else an `err`. Nothing turns verification off. TLS 1.3 and 1.2 only. Replay needs no network.
- `std.http.serve(address, handle)` calls `handle(request) -> Response` for one request at a time, in the order they arrive complete, until Ctrl-C or SIGTERM, then returns `ok()`. The caller declares `serve` with that address and the handler's effects. A handler's runtime error answers 500 and the server goes on; `--timeout` limits each handler call, not the server.
- `serve_workers(address, workers, setup, handle)` handles up to `workers` requests at once (1 to 64; `vo run --workers N` overrides it): each worker is a process that runs `setup() -> result[S]` once (open the database there) and then `handle(state, request)` for one request at a time. Requests share no memory: keep state in a database.
- `std.web`: an app is `web_app([on_get("/notes/{id}", show), ...])` (or a `new App` with steps and error kinds); an operation is `fn(ctx: C, input: I) -> result[Response]`, `I` a record whose fields take the path variable of their name, the JSON body (field `body`), a header (`@header("Name")`) or the query parameter of their name, checked by its annotations (else a 400/415/422 problem). `err("not_found: detail")` answers 404 (also `bad_request`, `unauthorized`, `forbidden`, `conflict`, `gone`, `unprocessable`, and `App.errors`); other errors 500. An operation with a typed error is wrapped, `typed(op, answer)`, `answer` mapping each error to a `Problem { status, detail }`. Patterns are checked literals (E0330, E0331). `serve_app(address, workers, setup, app)` serves it; tests call `handle_request(app, ctx, request(method, path, body))`. Answers: `ok(json_reply(200, note))`, `created("/notes/7", note)`, `no_content()`. Bodies are JSON or an HTML form, whose fields bind like query parameters. Cookies: `read_cookie(header, name)`, `with_cookie(resp, cookie(name, value))` (HttpOnly, Secure, SameSite=Lax unless changed).
- `std.process.run(program, args)` runs a program with no shell, no input and an empty environment and returns `Output { code, stdout, stderr }`; `run_with` adds input text, `NAME=value` variables and a working directory (`"./"`, or inside a `file.read` scope). A non-zero exit is `ok`. `--timeout` covers the program and everything it starts. A started program is not confined: `process("git")` grants whatever `git` can do.
- `parse_X` reads text, `from_X` decodes (`from_hex`); a failure is `err("invalid X: ...")`.
- Large inputs come in pieces: `each_line(path, f)`, `each_chunk`, `send_stream` and `run_lines` call `f` per piece; `f` returns `ok(true)` to go on, `ok(false)` to stop.
- File operations fail with `err` values (`"not found"`, `"already exists"`, `"not empty"`, ...; reading one: `"cannot read PATH"`); `info` reports `missing` as a value. `delete` and `rename` never remove a scope directory itself.
- Packages: `vo.packages` (found like `vo.effects`) has one `NAME DIRECTORY sha256:HASH` line per package, and `import NAME.a.b;` loads `DIRECTORY/a/b.vo`. The hash covers every `.vo` file under the directory; when it no longer matches, the program is not checked or run (E0701). `vo package ENTRY NAME DIRECTORY` (or an `https://` archive URL; `--restore ENTRY` fetches missing ones) adds or updates an entry and prints the change; `vo package --remove ENTRY NAME` removes one. Package functions appear in `vo.effects` as `@NAME/path.vo:fn`, and `vo doc NAME.module` documents them.

```vo
import std.json;
import std.text;

fn main() -> result[void] uses console {
    let doc = try json.parse_json("{\"count\": 7}");
    print(text.pad_left(str(try json.int_field(doc, "count")), 3, "0"));
    return ok();
}
```

```output
007
```

```vo
import std.fs;
import std.process;

fn main() -> result[void] uses console, file.read("./"), process("echo") {
    match try fs.info("no-such-file.txt") {
        case missing {
            print("missing");
        }
        case file(size) {
            print(str(size));
        }
        case directory {
            print("directory");
        }
        case other {
            print("other");
        }
    }
    let out = try process.run("echo", ["a b", "$HOME"]);
    write(out.stdout);
    print(str(out.code));
    return ok();
}
```

```output
missing
a b $HOME
0
```

```console
$ vo doc std.text.pad_left
fn pad_left(s: str, width: int, fill: str) -> str
    s padded on the left with fill (one character) to width characters.
    example: pad_left("7", 3, "0") is "007"
```

## Querying and editing programs

Ask the toolchain instead of searching the text: answers follow the checker, so strings, comments and other things with the same name never match. Every command takes the program's entry file first; only the files it imports are searched or changed.

```text
vo query callers ENTRY NAME              call sites, and uses as a value, of a function or builtin
vo query specs ENTRY                     every `@spec` ID and the tests that name it
vo query refs ENTRY TARGET               the declaration and every use
vo query type-at ENTRY FILE:LINE:COL     the type of the name or expression there, its declaration and documentation
vo impact ENTRY --diff FILE              functions and tests a unified diff (git diff, diff -u) affects; - is stdin
vo edit rename [--dry-run] ENTRY TARGET NEW_NAME
vo edit add-field [--dry-run] ENTRY RECORD FIELD: TYPE = DEFAULT
```

Comment lines directly above a function, record, variant, field or case document it (a line `#     EXPR is VALUE` is its example): `type-at` adds the declared signature and that text (`doc` in JSON, as `vo doc --json` prints it), and `vo doc ENTRY NAME` prints it for the program's own items.

A TARGET is `name` (a function, record or variant), `Record.field`, `Variant.case`, or `FILE:LINE:COL` (anything at that position: also parameters, locals and type parameters). A name declared in two files is T0005; a missing one T0004. Answers list `FILE:LINE:COL ROLE in FUNCTION: line`, with ROLE `declaration`, `call`, `value`, `type`, `import` or `use`.

`rename` changes every reference (and the function's line in `vo.effects`); `add-field` adds the field and `FIELD: DEFAULT` to every `new RECORD { ... }`. Before writing, the edited program is checked again: if it gains an error, or a name would mean something else, the edit is refused (T0006) and no file changes. All changed files are written, or none. `--dry-run` only lists the changes.

```vo
fn area(w: int, h: int) -> int {
    return w * h;
}

fn main() uses console {
    let a = area(3, 4);
    print("area " + str(a));
}
```

```console
$ vo query callers main.vo area
main.vo:6:13 call in main: let a = area(3, 4);
1 reference
$ vo query type-at main.vo main.vo:7:25
main.vo:7:25 a: int (declared at main.vo:6:9)
$ vo edit rename --dry-run main.vo area rect_area
main.vo:1:4 area -> rect_area
main.vo:6:13 area -> rect_area
renamed area to rect_area: 2 changes in 1 file (dry run, nothing written)
```

## Traces and replay

- `vo run --trace FILE` (and `vo test`) writes JSON lines: `call` (function, call site, depth, `args` by name) and `return` (value) for every vo function, `error` when a runtime error ends it, `test` before each test, `task` (`index`) before each task's records, and `end` (`records`, `omitted`, `status`). Values are shortened to 200 bytes. After `--trace-limit N` records (default 100000) the rest are only counted.
- A runtime error lists, under each frame, its parameters and the locals declared before the failing statement: `i: int = 3` (in JSON, `locals` of each `trace` entry). Timeouts show no values: they depend on the machine's speed.
- `vo run --record FILE` saves everything the program receives: arguments, stdin, environment variables, file reads, results of file writes, HTTP responses and requests, the clock, the seed and the bytes of `random_bytes`. `vo replay FILE` runs the current program on exactly that, reading and writing nothing outside: the same program gives the same output and exit status (unless it timed out). A changed program is run with a note; one that asks for a different input (another file, variable or HTTP request: host, port, method or path) stops with R0014. A server gets the recorded requests again, opens no port, and each response that changed is a note. A recording holds the file contents and variables the program read.
- `vo test --record DIR` records each test that receives anything from outside into `DIR/FILE/NAME.jsonl` (`FILE` without `.vo`; also the bodies and headers it sends); `vo test --replay DIR` runs each test on its file, offline. A test asking for an input its recording lacks is that test's R0014. `Authorization`, `Cookie` and `Set-Cookie` values, and headers, query parameters and variables named by `--redact`, are recorded as `[redacted]`.

```vo
fn last(xs: list[int]) -> int {
    let i = len(xs);
    return xs[i];
}

fn main() uses console {
    print(str(last([1, 2, 3])));
}
```

```error R0003
```

## Command line

```sh
vo check     [--json] PATH...
vo run       [--json] [--timeout SEC] [--seed N] [--clock-ms N] [--max-memory MiB] [--mem-stats] [--ca-file FILE] [--workers N] [--log-level LEVEL] [--drain SEC] FILE [-- ARG...]
vo build     [--json] [-o OUTPUT] [--base TOOLCHAIN] FILE
vo test      [--json] [--timeout SEC] [--seed N] [--clock-ms N] [--max-memory MiB] [--mem-stats] [--filter TEXT] [--test ID] [--spec ID] [--diff FILE] [--ca-file FILE] [--record DIR [--redact NAME] | --replay DIR] [--list | --coverage | --mutate [--max-mutants N]] PATH...
vo fmt       [--json] [--check] PATH...
vo effects   [--json] [--update] FILE
vo explain   [--json] CODE
vo reference [--json]
vo version   [--json]
vo query     [--json] callers|refs|type-at ENTRY TARGET | specs ENTRY
vo impact    [--json] ENTRY --diff FILE
vo edit      [--json] [--dry-run] rename|add-field ENTRY ...
vo replay    [--json] [--trace FILE] [--timeout SEC] [--max-memory MiB] [--mem-stats] RECORDING
vo doc       [--json] [MODULE | ENTRY NAME]
vo package   [--json] [--ca-file FILE] [--dir DIR] ENTRY NAME DIRECTORY|URL | --remove ENTRY NAME | --restore ENTRY
vo mcp
vo lsp
```

`run` and `test` also take `--trace FILE` and `--trace-limit N`; `run` takes `--record FILE` (see Traces and replay).

- `PATH` is a `.vo` file or a directory (every `*.vo` under it, recursively, in sorted order). Each file is checked as the entry of its own program.
- `run` checks first and runs only without errors. Arguments after `--` reach the program through `args()`.
- `build` checks like `check` (with the effects record) and writes one executable holding the program: `./app ARG...` runs as `vo run FILE -- ARG...`, with no source tree. Its `vo run` options are `--vo-NAME` (`--vo-seed 3`, `--vo-record FILE`); it has no default timeout; `--vo-effects` prints what `main` declares; `--vo-replay FILE` replays. `--base` takes another architecture's `vo`.
- `--timeout SEC` is in whole seconds, default 10; `0` disables it. For `test` it applies to each test.
- Memory: unreachable values are reclaimed. The limit applies to live data (512 MiB by default; `--max-memory MiB`, 1 to 65536); holding more is R0006. `--mem-stats` reports the peak live size, the total allocated and the collections (on stderr; a `memory` record with `--json`).
- `fmt` rewrites files in canonical form; `fmt --check` only lists the non-canonical files.
- `mcp` serves the commands as Model Context Protocol tools (the same JSON; this document as `vo://reference`) and `lsp` the Language Server Protocol (diagnostics, fixes, hover, navigation, rename, formatting), on stdin/stdout.
- `reference` prints this document. `version` prints `vo <semver> schema 1` and `tzdata <release>`.
- `--json` works with every command but `mcp` and `lsp` and switches reports to JSON lines (see Machine-readable output).
- `run` writes its own reports (diagnostics, runtime errors) to stderr, so the program's stdout stays clean. Every other command reports on stdout.

Exit statuses:

| Status | Meaning |
|---|---|
| 0 | success (warnings never change the status) |
| 0–63 | `run`: the code passed to `exit()`; normal end of `main` is 0, `main` returning `err` is 1 |
| 1 | `test`: a test did not pass (T0012: `--mutate` needs them all to); `fmt --check`: a file is not canonical |
| 64 | usage error (T0001); a missing or unfit target, test, diff, recording, certificate or name (T0004, T0005, T0007, T0008, T0010, T0011, T0013) |
| 65 | check errors; an edit that would add one (T0006) |
| 66 | input file or directory cannot be read (T0002); a damaged built executable (T0014) |
| 70 | runtime error |
| 74 | a trace or recording cannot be written (T0009) |
| 124 | timeout (R0009) |
| 125 | internal error in vo (T0003) |

Canonical formatting (`vo fmt`): 4-space indent, `{` on the same line, one statement per line, no blank lines inside blocks, one blank line between items (a comment right above an item is its documentation), single spaces around binary operators and after `,` and `:`, parentheses only where needed. A line over 100 characters is broken inside the outermost list, argument list or `new` record that ends it, one item per line; a long header puts `uses ...` on its own line.

```vo-fmt
fn main() uses console{
  let x=(1+2)*3;
    if (x>2){print("big");}

}
```

```vo
fn main() uses console {
    let x = (1 + 2) * 3;
    if x > 2 {
        print("big");
    }
}
```

## Machine-readable output

With `--json`, every report is one JSON object per line, starting with `{"v":1,"kind":...}`. Ignore fields you do not know. Positions are `{"line": L, "col": C}`, 1-based, with `col` counted in characters; spans run from `start` (inclusive) to `end` (exclusive). The program's own output is never wrapped.

**diagnostic** (from `check`, `run`, `test`, `fmt`): `code`, `name`, `severity` (`"error"` or `"warning"`), `file`, `start`, `end`, `text` (the source in the span), `message`, optional `expected` and `found`, `fixes` and `related`. A fix is `{file, start, end, replacement}`; `start == end` means insert. Apply the first fix to resolve the diagnostic; fixes of one diagnostic never overlap. This program checks clean:

```vo
fn main() uses console {
    let total = 5;
    print(str(total));
}
```

With `print(total);` on line 3 instead, `vo check --json main.vo` prints:

```json
{"v":1,"kind":"diagnostic","code":"E0301","name":"type_mismatch","severity":"error","file":"main.vo","start":{"line":3,"col":11},"end":{"line":3,"col":16},"text":"total","message":"Expected `str`, found `int`.","expected":"str","found":"int","fixes":[{"file":"main.vo","start":{"line":3,"col":11},"end":{"line":3,"col":16},"replacement":"str(total)"}],"related":[]}
```

**runtime_error** (from `run`, on stderr): `code`, `name`, `message`, `file`, `start`, `end`, and `trace`, a list of `{function, file, start, locals}` with the innermost call first (`locals`: `{name, type, value}` of the parameters and locals there). Here `return total / count;` on line 2 of `average(10, 0)` divided by zero, called from `main` on line 6:

```json
{"v":1,"kind":"runtime_error","code":"R0002","name":"division_by_zero","message":"Division by zero.","file":"main.vo","start":{"line":2,"col":12},"end":{"line":2,"col":25},"trace":[{"function":"average","file":"main.vo","start":{"line":2,"col":12},"locals":[{"name":"total","type":"int","value":"10"},{"name":"count","type":"int","value":"0"}]},{"function":"main","file":"main.vo","start":{"line":6,"col":15},"locals":[]}]}
```

**test** (from `test`): `id`, `name`, `status` (`"pass"`, `"fail"`, `"error"`, `"timeout"`), `file`, `start`, `ms`, the property members above, `output` when it printed, and unless the status is `pass`, `cause` (`check_failed`, `too_few_inputs`, `error_in_code`, `error_in_test`, `requires_broken_by_test`, `requires_broken_by_code`, `ensures_broken`, `hole`, `outside_grant`, `timeout`, `out_of_memory`) and `failure`: `{file, start, end, message, expected?, actual?, operands?, difference?}` (operands: `{text, value}`; difference: `{path, expected, actual}`, `character` for strings; plus `code` and `trace` for `error`). Values are shown in vo syntax: `"abc"`, `[1, 2]`, `new Point { x: 1, y: 2 }`, `Shape.circle(1.0)`, `ok(1)`; maps as `{k: v}`.

```json
{"v":1,"kind":"test","name":"doubles","status":"fail","file":"main.vo","start":{"line":5,"col":1},"ms":0,"failure":{"file":"main.vo","start":{"line":6,"col":5},"end":{"line":6,"col":28},"message":"`expect_eq` failed: the values differ.","expected":"8","actual":"9"}}
```

**summary**: the last record of every command that reads a program (`check`, `test`, `fmt`, `effects`, `query`, ...), of `build` when it fails, and of `run` when it stops before the program starts: `{"v":1,"kind":"summary","command":"check","errors":1,"warnings":0,"truncated":0,"files":1}`. `test` adds `passed`, `failed`, `errored`, `timed_out`; `fmt` adds `changed`. At most 50 diagnostics are listed; `truncated` counts the rest.

Other kinds: `toolchain_error` (`code`, `name`, `message`, optional `path` and `related` declarations) for T codes, `ref`, `type_at`, `impact` and `edit` (see Querying and editing programs; `edit` is a fix with the replaced `text`), `replay` (a changed program), `explain` (`code`, `name`, `fixable`, `text`), `reference` (`text`), `version` (`version`, `schema`), `fmt` (`file`, `changed`), `capabilities` (`vo effects`: `entry`, `file`, `uses`), `build` (`output`, `files`, `bytes`), and from `test`: `test_entry` (`--list`: `id`, `name`, `test_kind`, `file`, `start`, `end`, `doc`, `uses`, `spec`), `coverage` (`file`, `start`, `end`, `point`, `function`), `coverage_summary`, `mutant` (`file`, `start`, `end`, `function`, `text`, `mutated`, `tests`) and `mutation_summary` (`mutants`, `caught`, `survived`, `not_tried`). The test summary adds `selected` (or `listed`).

## Error codes

Every diagnostic, runtime error and toolchain error has a stable code, and `vo explain CODE` prints what it means, an example and the fix. Ranges: E01xx syntax, E02xx names, imports and library changes (E0214), E03xx types, E04xx control flow, `match` and results, E05xx effects, E06xx holes, tests and contracts, E07xx packages, Wxxxx warnings (they never fail a command; W07xx: structure), R00xx runtime errors (exit status 70; R0009 timeout is 124) and T00xx toolchain errors. When a diagnostic has no fix, read `vo explain` before changing code.

```vo
fn main() uses console {
    match parse_int("4") {
        case ok(v) {
            print(str(v));
        }
        case err(_) {
            print("not a number");
        }
    }
}
```

```output
4
```

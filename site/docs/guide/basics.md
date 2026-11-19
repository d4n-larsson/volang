# Basics

Values, variables and assignment, the statements that make up a function body, control flow, operators and their precedence, and comments.

vo is deliberately small and regular: every statement starts with a keyword or is a call, every simple
statement ends with `;`, every block has braces, and there is one way to write each thing. If you know a
C-family language, most of this page will look familiar; the differences are listed as they come up.
The rules stated here are in the reference sections [Lexical rules](../reference/lexical-rules.md),
[Statements](../reference/statements.md) and
[Expressions and operators](../reference/expressions-and-operators.md).

## A first look

```vo
fn main() uses console {
    let name = "vo";
    var total = 0;
    for i in 1..5 {
        set total = total + i;
    }
    if total > 5 {
        print(name + ": " + str(total));
    }
}
```

```output
vo: 10
```

- `let` binds a name that cannot be reassigned; `var` binds one that can.
- `set` changes a `var` (and, later, record fields, list elements and map entries).
- `for i in 1..5` visits 1, 2, 3 and 4.
- `print` takes a `str`, so the `int` goes through `str(...)` first. vo never converts implicitly.

## Values

The basic values are integers (`42`, `-5`), floats (`3.14`, `0.5`), booleans (`true`, `false`) and
strings (`"hello"`). Their types are `int`, `float`, `bool` and `str`, described in [Types](types.md).

Literals are strict, so that every value has one spelling:

- Integers are plain decimal digits: no `_` separators, hexadecimal, octal or suffixes. A negative number
  is unary `-` applied to a literal.
- Floats need digits on both sides of the `.`: `1.0` and `0.5`, never `1.` or `.5`. There is no exponent
  form (`1e3` is invalid).
- Strings use double quotes and stay on one line. The only escapes are `\n`, `\t`, `\\` and `\"`. There
  is no interpolation: build strings with `format("{} of {}", 3, 10)` or with `+` and `str(...)`.

Lists (`[1, 2, 3]`), maps, records and variants come in later chapters. There is no `null`: a value that
may be missing is a variant or a `result`.

## Variables: let, var and set

`let name = expr;` binds an immutable name. `var name = expr;` binds a mutable one, and `set name = expr;`
gives it a new value. A `var` always needs an initializer. The type comes from the initializer, or you
can state it: `let ratio: float = 0.5;` (see [Types](types.md)).

```vo
fn main() uses console {
    let limit = 3;
    var tries = 0;
    var log = "";
    while tries < limit {
        set tries = tries + 1;
        set log = log + str(tries) + " ";
    }
    print(log + "done");
}
```

```output
1 2 3 done
```

Use `let` unless the value really changes. A `let` cannot be changed later ([E0208](../errors/E0208.md),
whose fix turns the `let` into a `var`):

```vo
fn main() uses console {
    let count = 1;
    set count = 2;
    print(str(count));
}
```

```error E0208
```

Function parameters and `for` loop variables cannot be changed either; copy them into a `var` if you
need to.

Assignment always starts with `set`. There is no `=` statement, no `+=` and no `++`
([E0114](../errors/E0114.md)):

```vo
fn main() uses console {
    var hits = 0;
    hits += 1;
    print(str(hits));
}
```

```error E0114
```

Write `set hits = hits + 1;` instead.

### Scope and shadowing

A name is visible from its declaration to the end of the block it is declared in. Inside that region it
cannot be declared again, not even in a nested block ([E0203](../errors/E0203.md)): in vo a name means
one thing wherever you read it. Sibling blocks, such as the two branches of an `if` or two loops in a
row, may reuse a name.

```vo
fn main() uses console {
    let x = 1;
    if x > 0 {
        let x = 2;
        print(str(x));
    }
}
```

```error E0203
```

A local that is declared and never read is warning [W0201](../errors/W0201.md). Warnings do not make a
command fail, but they usually point at a mistake.

## Statements

The **first-token rule**: every statement starts with one of `let`, `var`, `set`, `if`, `while`, `for`,
`match`, `return`, `break`, `continue`, `try`, or is a call such as `print("hi");`. Nothing else is a
statement, so a bare expression like `x + 1;` is an error, and so is a typo that turns a statement into
an expression. This is what makes vo programs easy to read and to generate: the first word says what a
line does.

| Statement | Form |
|---|---|
| immutable binding | `let name = expr;` |
| mutable binding | `var name = expr;` |
| assignment | `set name = expr;` (also `set p.field`, `set xs[i]`, `set m[k]`) |
| discard a value | `let _ = expr;` |
| condition | `if c { } else if c { } else { }` |
| loops | `while c { }`, `for x in xs { }`, `for i in a..b { }` |
| match | `match e { case ... { } }`, see [Records and variants](records-and-variants.md) |
| leave | `return;`, `return expr;`, `break;`, `continue;` |
| call | `f(args);` |
| propagate an error | `try f();`, see [Results and errors](results-and-errors.md) |

Simple statements end with `;`. Statements that end with a block (`if`, `while`, `for`, `match`) take no
`;` after the `}`. Braces are always required, even around one statement.

A call used as a statement must not throw a value away. If a function returns something, use it, or say
that you mean to ignore it with `let _ = ...;` ([E0405](../errors/E0405.md)):

```vo
fn main() {
    let word = "hello";
    len(word);
}
```

```error E0405
```

A `result` can never be ignored, not even with `let _` ([E0404](../errors/E0404.md)): an error must be
handled or passed on.

## Control flow

### if

Conditions must be `bool`. There is no truthiness: `if count { }` with an `int` is
[E0301](../errors/E0301.md); write `if count != 0 { }`. Parentheses around a condition are allowed but
not needed, and `vo fmt` removes them.

```vo
fn describe(n: int) -> str {
    if n < 0 {
        return "negative";
    } else if n == 0 {
        return "zero";
    } else {
        return "positive";
    }
}

fn main() uses console {
    print(describe(-4) + " " + describe(0) + " " + describe(9));
}
```

```output
negative zero positive
```

`if` is a statement, not an expression, and there is no `a ? b : c`. To choose a value, declare a `var`
and `set` it in each branch, or move the choice into a function that `return`s it, as above.

### while

`while cond { }` repeats while the condition is true. It is the only general loop: there is no
`do`/`while`, `loop` or C-style `for (;;)`. Write an endless loop as `while true { }` and leave it with
`break` or `return`.

```vo
fn collatz_steps(start: int) -> int {
    var n = start;
    var steps = 0;
    while n != 1 {
        if n % 2 == 0 {
            set n = n / 2;
        } else {
            set n = 3 * n + 1;
        }
        set steps = steps + 1;
    }
    return steps;
}

fn main() uses console {
    print(str(collatz_steps(6)) + " " + str(collatz_steps(27)));
}
```

```output
8 111
```

### for

`for` has two forms. `for x in xs { }` visits the elements of a list in order. `for i in a..b { }` visits
the integers `a`, `a + 1`, ..., `b - 1`; the range is empty when `a >= b`. The bounds can be any `int`
expressions, evaluated once before the loop starts. `for _ in 0..n { }` repeats `n` times without naming
the counter.

```vo
fn main() uses console {
    let fruits = ["apple", "fig", "pear"];
    for fruit in fruits {
        print(fruit);
    }
    for i in 0..len(fruits) {
        print(str(i) + ": " + fruits[i]);
    }
    var stars = "";
    for _ in 0..3 {
        set stars = stars + "*";
    }
    print(stars);
    for i in 5..2 {
        print("never printed " + str(i));
    }
}
```

```output
apple
fig
pear
0: apple
1: fig
2: pear
***
```

The `..` range exists only in `for`; it is not a value you can store. To loop over a map, loop over its
keys with `for k in keys(m)` (see [Lists and maps](lists-and-maps.md)).

### break and continue

`break` leaves the innermost `while` or `for`; `continue` skips to its next round. Both also work from
inside a `match` arm within the loop. Outside a loop they are [E0407](../errors/E0407.md).

```vo
fn main() uses console {
    var found = -1;
    let xs = [4, 7, 10, 13, 16];
    for i in 0..len(xs) {
        if xs[i] % 2 == 0 {
            continue;
        }
        if xs[i] > 10 {
            set found = i;
            break;
        }
    }
    print("first odd number over 10 is at " + str(found));
}
```

```output
first odd number over 10 is at 3
```

There are no labels: to leave two nested loops at once, move them into a function and `return`.

## Expressions and operators

| Operators | Operands | Result |
|---|---|---|
| `+` `-` `*` `/` | two `int` or two `float` | the same type |
| `%` | two `int` | `int` |
| `+` | two `str` | `str` (joins them) |
| `-x` | `int` or `float` | the same type |
| `==` `!=` | any two values of the same type | `bool` |
| `<` `<=` `>` `>=` | two `int`, two `float` or two `str` | `bool` |
| `and` `or` `not` | `bool` | `bool` |

- Both sides of an arithmetic operator must have the same type: `int + float` is
  [E0301](../errors/E0301.md). Convert one side with `float(i)` or `int(f)` (see [Types](types.md)).
- `int` division truncates toward zero: `7 / 2` is `3` and `-7 / 2` is `-3`; `%` keeps the sign of the
  left side, so `-7 % 3` is `-1`. Dividing an `int` by zero stops the program with
  [R0002](../errors/R0002.md); overflow stops it with [R0001](../errors/R0001.md).
- Logic uses words: `and`, `or`, `not`. `&&`, `||` and `!` do not exist. `and` and `or` short-circuit:
  the right side is evaluated only when it can change the result.
- `==` compares by value, including lists, maps, records and variants: two lists are equal when their
  elements are. Functions cannot be compared.
- Strings compare by bytes with `<`, so `"Zebra" < "apple"` (uppercase letters come first in UTF-8).
- There is no ternary operator, no `+=` or `++`, no bitwise operators and no `**`.

```vo
fn main() uses console {
    print(str(7 / 2) + " " + str(-7 / 2) + " " + str(7 % 3) + " " + str(-7 % 3));
    print(str(7.0 / 2.0) + " " + str(2.0 * 0.25));
    let age = 34;
    print(str(age >= 18 and age < 65) + " " + str(not (age == 34) or age > 30));
    print(str("Zebra" < "apple") + " " + str([1, 2] == [1, 2]) + " " + str("ab" + "c" == "abc"));
}
```

```output
3 -3 1 -1
3.5 0.5
true true
true true true
```

### Precedence

From tightest to loosest. Operators on the same level group left to right.

| Level | Operators |
|---|---|
| 1 | calls `f(...)`, fields `x.field`, indexing `xs[i]`, parentheses `(e)` |
| 2 | unary `-x`, `try e` |
| 3 | `*` `/` `%` |
| 4 | `+` `-` |
| 5 | `==` `!=` `<` `<=` `>` `>=` |
| 6 | `not` |
| 7 | `and` |
| 8 | `or` |

Two consequences differ from many languages:

- `not` binds more loosely than comparisons, so `not a == b` means `not (a == b)`.
- A comparison cannot be chained: `0 < x < 10` is an error ([E0110](../errors/E0110.md)). Write
  `0 < x and x < 10`.

```vo
fn main() uses console {
    let x = 5;
    print(str(0 < x < 10));
}
```

```error E0110
```

```vo
fn main() uses console {
    let x = 5;
    print(str(1 + 2 * 3) + " " + str((1 + 2) * 3));
    print(str(0 < x and x < 10) + " " + str(not x == 5));
    print(str(x > 1 or x > 2 and x > 100));
}
```

```output
7 9
true false
true
```

The last line is `x > 1 or (x > 2 and x > 100)`, because `and` binds tighter than `or`. When an
expression mixes them, `vo fmt` keeps the parentheses you wrote around an `and`, so you can make the
grouping visible.

## Comments

A comment is a whole line whose first non-blank character is `#`. A `#` after code on the same line is an
error ([E0101](../errors/E0101.md), whose fix moves the comment onto its own line). There are no block
comments. A comment directly above an item documents it.

```vo
# The number of seconds in a day.
fn day_seconds() -> int {
    # 24 hours of 60 minutes of 60 seconds.
    return 24 * 60 * 60;
}

fn main() uses console {
    print(str(day_seconds()));
}
```

```output
86400
```

```vo
fn main() uses console {
    let rate = 3; # per hour
    print(str(rate));
}
```

```error E0101
```

## Names

Functions, variables, parameters, record fields and variant cases are `snake_case`; records and variants
are `PascalCase`. Other spellings are [E0207](../errors/E0207.md), whose fix is the converted name.
Keywords such as `list`, `map`, `result`, `str` and `new` cannot be used as names; the full list is in
[Lexical rules](../reference/lexical-rules.md). `_` alone is the discard name, allowed only in
`let _ = ...;` and in a `case` binding such as `case err(_)`.

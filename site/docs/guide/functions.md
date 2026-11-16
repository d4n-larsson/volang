# Functions

Declaring functions, passing them around as values, writing anonymous functions and closures, and how a function's effects travel with it.

Functions are the only unit of code in vo: there are no methods, classes or nested named functions. A
function can be called, but it is also a value that can be stored, passed and returned, which is how vo
does callbacks, sorting by a key and other higher-order code. The rules for declarations are in
[Declarations](../reference/declarations.md#functions), and those for function values in
[Functions as values](../reference/functions-as-values.md).

## Declaring a function

```text
fn name(param: type, param: type) -> return_type uses effects { ... }
```

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

fn report(label: str, value: int) uses console {
    if value < 0 {
        print(label + ": invalid");
        return;
    }
    print(label + ": " + str(value));
}

fn main() uses console {
    report("volume", clamp(140, 0, 100));
    report("offset", -1);
}
```

```output
volume: 100
offset: invalid
```

- Every parameter has a type. The return type follows `->`; a function that returns nothing has no
  `->` at all (`-> void` is an error) and may leave early with `return;`.
- Every path through a function with a return type must reach a `return` with a value
  ([E0403](../errors/E0403.md)).
- `uses ...` lists the function's effects, what it may touch. Without it the function is pure. See
  [Effects](effects.md).
- Parameters are read-only: `set` on a parameter is [E0208](../errors/E0208.md). Copy one into a `var`
  to change it. Lists, maps and records are passed as references, so a function can still change their
  contents (see [Records are references](records-and-variants.md#records-are-references)).
- There is no overloading (a second function with the same name is [E0202](../errors/E0202.md)), no
  default arguments and no variadic functions. Give functions that do different things different names.
- Functions can be declared in any order and can call themselves or each other recursively.

A call whose value is not used must be explicit about it: `clamp(1, 0, 2);` alone is
[E0405](../errors/E0405.md). Use the value, or write `let _ = clamp(1, 0, 2);`.

Functions with `requires` and `ensures` clauses are covered in [Contracts](contracts.md), and functions
with type parameters in [Generics](generics.md).

## Functions as values

A named function used without parentheses is a value. Its type is written like a declaration without
names: `fn(int) -> int` takes an `int` and returns an `int`; `fn(str)` takes a `str` and returns nothing;
`fn() -> bool` takes nothing.

```vo
fn double(x: int) -> int {
    return x * 2;
}

fn is_even(x: int) -> bool {
    return x % 2 == 0;
}

fn apply_twice(f: fn(int) -> int, x: int) -> int {
    return f(f(x));
}

fn main() uses console {
    let xs = [1, 2, 3, 4];
    let f = double;
    print(str(f(5)) + " " + str(apply_twice(double, 3)));
    print(join(map(map(xs, double), fn(x: int) -> str {
        return str(x);
    }), " "));
    print(str(len(filter(xs, is_even))) + " " + str(any(xs, is_even)) + " " + str(all(xs, is_even)));
}
```

```output
10 12
2 4 6 8
2 true false
```

Function types can appear anywhere a type can: parameters, return types, `let` bindings, record fields,
list elements and map values. Anything that evaluates to a function can be called: `f(1)`,
`handlers[0](x)`, `button.on_click(x)` (a record field holding a function) or `make_adder(1)(2)`.

Two limits:

- Builtins such as `len`, `print` or `str` are not values ([E0201](../errors/E0201.md)). Wrap one in an
  anonymous function: `fn(s: str) -> int { return len(s); }`.
- Functions cannot be compared with `==` ([E0308](../errors/E0308.md)).

### The higher-order builtins

| Builtin | What it does |
|---|---|
| `map(xs, f)` | a new list of `f(x)` for each element |
| `filter(xs, f)` | a new list of the elements for which `f` is `true` |
| `fold(xs, init, f)` | combines the elements left to right: `f(f(init, x0), x1)` ... |
| `any(xs, f)`, `all(xs, f)` | whether `f` holds for some or every element; stop as soon as the answer is known |
| `sort_by(xs, key)` | sorts the list in place, stably, by `key(x)`, which is an `int`, `float` or `str` |

Their exact signatures are in [Functions as values](../reference/functions-as-values.md).

## Anonymous functions

An anonymous function (a lambda) is written like a declaration without a name, with the same parameter
and return types. It is an expression, so it can be passed directly:

```vo
record Task {
    title: str;
    priority: int;
}

fn main() uses console {
    let tasks = [new Task { title: "write docs", priority: 2 }, new Task { title: "fix bug", priority: 1 }, new Task { title: "lunch", priority: 3 }];
    sort_by(tasks, fn(t: Task) -> int {
        return t.priority;
    });
    let titles = map(tasks, fn(t: Task) -> str {
        return t.title;
    });
    print(join(titles, ", "));
    let total = fold(tasks, 0, fn(acc: int, t: Task) -> int {
        return acc + t.priority;
    });
    print(str(total));
}
```

```output
fix bug, write docs, lunch
6
```

To sort from largest to smallest, return a key that reverses the order, such as `0 - t.priority`.

An anonymous function never writes `uses`: its effects are whatever its body does
([E0507](../errors/E0507.md) if you write a clause). The section on effects below explains where they go.

## Closures

An anonymous function can use the names around it: the `let` bindings, parameters and loop variables
visible where it is written. It captures them **by value, when it is created**. A loop therefore creates
a fresh capture on each round:

```vo
fn make_adder(n: int) -> fn(int) -> int {
    return fn(x: int) -> int {
        return x + n;
    };
}

fn main() uses console {
    let add5 = make_adder(5);
    print(str(add5(1)) + " " + str(make_adder(10)(1)));
    let steps = new list[fn() -> int];
    for i in 0..3 {
        push(steps, fn() -> int {
            return i * 10;
        });
    }
    print(join(map(steps, fn(f: fn() -> int) -> str {
        return str(f());
    }), ","));
}
```

```output
6 11
0,10,20
```

A `var` cannot be captured ([E0505](../errors/E0505.md)). Its value may change after the function is
created, and vo will not make you guess whether the function sees the old value or the new one:

```vo
fn main() uses console {
    var count = 0;
    set count = count + 1;
    let show = fn() -> int {
        return count;
    };
    print(str(show()));
}
```

```error E0505
```

The fix copies the `var` into a `let` and captures that, which makes the moment of the copy explicit:

```vo
fn main() uses console {
    var count = 0;
    set count = count + 1;
    let count_now = count;
    let show = fn() -> int {
        return count_now;
    };
    set count = 10;
    print(str(show()) + " " + str(count));
}
```

```output
1 10
```

Capturing by value copies the binding, not the data behind it. A captured list, map or record is still a
reference to the same value, so changes made to its contents later are visible to the function:

```vo
fn main() uses console {
    let seen = new list[str];
    let remember = fn(word: str) {
        push(seen, word);
    };
    remember("a");
    remember("b");
    print(str(len(seen)));
}
```

```output
2
```

This is how a closure keeps state between calls: hold the state in a list, map or record and capture
that.

## Functions with effects

A function type includes the function's effects, written after the return type: `fn(str) uses console`
is a function that takes a `str` and may print. A function value fits a function type only when the
type's effects cover everything the function does. Passing a printing function where `fn(int) -> int` is
expected is [E0502](../errors/E0502.md):

```vo
fn apply(f: fn(int) -> int, x: int) -> int {
    return f(x);
}

fn main() uses console {
    let r = apply(fn(x: int) -> int {
        print("called with " + str(x));
        return x + 1;
    }, 1);
    print(str(r));
}
```

```error E0502
```

The fix adds `uses console` to the parameter's type. `apply` then calls a function that may print, so it
needs `uses console` itself ([E0501](../errors/E0501.md) until it has it):

```vo
fn apply(f: fn(int) -> int uses console, x: int) -> int uses console {
    return f(x);
}

fn main() uses console {
    let r = apply(fn(x: int) -> int {
        print("called with " + str(x));
        return x + 1;
    }, 1);
    print(str(r));
}
```

```output
called with 1
2
```

The same holds for the higher-order builtins: their caller takes on the effects of the function passed
in, so `map` with a function that prints needs `uses console` in the function that calls `map`.

Creating a function does nothing by itself, so a function that only builds and returns another one is
pure, even if the function it returns has effects. Its return type says what the returned function may
do; when it returns a function type with effects, the type's `uses` comes right after the return type:

```vo
record Button {
    label: str;
    on_click: fn(str) uses console;
}

fn make_logger(prefix: str) -> fn(str) uses console {
    return fn(msg: str) {
        print(prefix + msg);
    };
}

fn shout(msg: str) uses console {
    print(upper(msg));
}

fn main() uses console {
    let buttons = [new Button { label: "save", on_click: make_logger("log: ") }, new Button { label: "quit", on_click: shout }];
    for b in buttons {
        b.on_click(b.label);
    }
}
```

```output
log: save
QUIT
```

A pure function also fits a type with effects, since doing less than allowed is always fine.

A function that takes a callback and should work with any effects the callback has, such as an `each`
over a list, uses an effect parameter, `fn each[T, E: effects](xs: list[T], f: fn(T) uses E) uses E`.
See [Generics](generics.md).

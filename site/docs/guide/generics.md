# Generics

Type parameters let one function, record or variant work for many element types, while the checker still knows the exact type at every use.

## Why generics

Without generics, a function that returns the last element of a list would have to be written once
for `list[int]`, once for `list[str]` and once for every record type. vo has no `any` type and no
casts, so there is no way to write it once and check the type later. A type parameter is the way to
write it once: the function says "for any type `T`", and every call fills in `T` with a real type.

vo's generics are deliberately small. There are type parameters, one constraint (`ordered`) and effect
parameters for higher-order functions. There are no interfaces, no traits, no methods and no
specialization. Generic code is checked once, where it is written, against what its parameters
allow, so an error inside a generic function is reported in that function, not at some distant call.

The rules are in the [Generics](../reference/generics.md) section of the reference.

## Type parameters on functions

Type parameters are written in square brackets after the function's name. Inside the function, `T` is
a type like any other: it can be used in parameters, the return type, locals and nested types such as
`list[T]` or `result[T]`.

```vo
fn last_or[T](xs: list[T], fallback: T) -> T {
    if len(xs) == 0 {
        return fallback;
    }
    return xs[len(xs) - 1];
}

fn count_of[T](xs: list[T], wanted: T) -> int {
    var n = 0;
    for x in xs {
        if x == wanted {
            set n = n + 1;
        }
    }
    return n;
}

fn main() uses console {
    print(str(last_or([3, 5, 8], 0)));
    print(last_or(new list[str], "none"));
    print(str(count_of(["a", "b", "a"], "a")));
}
```

```output
8
none
2
```

A plain type parameter supports only what every type supports: passing values around, storing them
in lists, maps and records, and comparing them with `==` and `!=`. Anything more needs a constraint
(see [Ordered type parameters](#ordered-type-parameters)).

Type arguments are never written at a call. `last_or([3, 5, 8], 0)` is a call with `T = int`, worked
out from the arguments; `last_or[int](...)` is a syntax error.

## Generic records and variants

Records and variants take type parameters the same way. In a type, a generic record or variant is
always written with its arguments: `Stack[int]`, `Pair[str, float]`, `Tree[str]`.

```vo
record Stack[T] {
    items: list[T];
}

fn push_item[T](s: Stack[T], x: T) {
    push(s.items, x);
}

fn pop_item[T](s: Stack[T]) -> result[T] {
    return pop(s.items);
}

fn main() uses console {
    let s = new Stack[str] { items: new list[str] };
    push_item(s, "first");
    push_item(s, "second");
    match pop_item(s) {
        case ok(x) {
            print(x);
        }
        case err(e) {
            print(e);
        }
    }
    print(str(len(s.items)));
}
```

```output
second
1
```

A generic variant can refer to itself and to generic records, which is how recursive structures are
built. This binary search tree works for any ordered element type:

```vo
variant Tree[T] {
    leaf;
    node(Node[T]);
}

record Node[T] {
    left: Tree[T];
    value: T;
    right: Tree[T];
}

fn insert[T: ordered](t: Tree[T], x: T) -> Tree[T] {
    match t {
        case leaf {
            return Tree.node(new Node { left: Tree.leaf, value: x, right: Tree.leaf });
        }
        case node(n) {
            if x < n.value {
                return Tree.node(new Node { left: insert(n.left, x), value: n.value, right: n.right });
            }
            return Tree.node(new Node { left: n.left, value: n.value, right: insert(n.right, x) });
        }
    }
}

fn in_order[T](t: Tree[T], out: list[T]) {
    match t {
        case leaf {
            return;
        }
        case node(n) {
            in_order(n.left, out);
            push(out, n.value);
            in_order(n.right, out);
        }
    }
}

fn main() uses console {
    var t = insert(Tree.leaf, "m");
    for word in ["c", "x", "a"] {
        set t = insert(t, word);
    }
    let words = new list[str];
    in_order(t, words);
    print(join(words, " "));
}
```

```output
a c m x
```

Writing a generic type without its arguments, or with the wrong number of them, is
[E0312](../errors/E0312.md):

```vo
record Pair[A, B] {
    first: A;
    second: B;
}

fn main() uses console {
    let p: Pair = new Pair { first: 1, second: "one" };
    print(p.second);
}
```

```error E0312
```

## Inference: where type arguments come from

Type arguments are left out of calls, and may be left out of `new` and of variant cases. The checker
works them out in this order:

1. **From the arguments or field values.** `last_or([3, 5, 8], 0)` has `T = int` because of its
   arguments; `new Pair { first: 1, second: "a" }` is a `Pair[int, str]` because of its fields.
2. **From the expected type**: a parameter the value is passed to, the function's return type, or the
   annotation of a variable. `insert(Tree.leaf, 5)` works because `insert`'s other argument fixes
   `T = int`, so `Tree.leaf` is a `Tree[int]`.

```vo
record Pair[A, B] {
    first: A;
    second: B;
}

variant Option[T] {
    none;
    some(T);
}

fn empty[T]() -> list[T] {
    return new list[T];
}

fn total(xs: list[int]) -> int {
    var sum = 0;
    for x in xs {
        set sum = sum + x;
    }
    return sum;
}

fn nothing() -> Option[str] {
    return Option.none;
}

fn show(o: Option[str]) -> str {
    match o {
        case none {
            return "-";
        }
        case some(s) {
            return s;
        }
    }
}

fn main() uses console {
    let p = new Pair { first: 1, second: "a" };
    print(str(p.first) + p.second);
    print(str(total(empty())));
    let names: list[str] = empty();
    push(names, "ana");
    print(names[0] + " " + show(nothing()) + " " + show(Option.some("b")));
}
```

```output
1a
0
ana - b
```

Here `total(empty())` gets `T = int` from `total`'s parameter, `let names: list[str] = empty();` from
the annotation, and `return Option.none;` from the return type.

When neither source says what a type argument is, the checker stops with
[E0310](../errors/E0310.md). A case without a value, or a function whose type parameter appears only
in its return type, gives no information on its own:

```vo
variant Option[T] {
    none;
    some(T);
}

fn main() uses console {
    let o = Option.none;
    match o {
        case none {
            print("none");
        }
        case some(_) {
            print("some");
        }
    }
}
```

```error E0310
```

The fix is to annotate the variable, `let o: Option[int] = Option.none;`. Later uses of the variable
do not count: inference looks only at the expression and the type it is expected to have.

## Ordered type parameters

`T: ordered` allows `<`, `<=`, `>`, `>=`, `min`, `max` and `sort` on values of type `T`. An ordered type
is one of:

- `int`, `float` or `str` (strings compare by bytes);
- a record whose fields are all ordered. Records compare field by field, in the order the fields are
  declared: the first field that differs decides.

`bool`, lists, maps, variants, results, functions and records with any such field have no order.

```vo
record Version {
    major: int;
    minor: int;
    patch: int;
}

fn largest[T: ordered](xs: list[T], start: T) -> T {
    var best = start;
    for x in xs {
        set best = max(best, x);
    }
    return best;
}

fn show(v: Version) -> str {
    return str(v.major) + "." + str(v.minor) + "." + str(v.patch);
}

fn main() uses console {
    print(str(largest([3, 9, 4], 0)));
    print(largest(["pear", "apple"], ""));
    let versions = [new Version { major: 1, minor: 10, patch: 0 }, new Version { major: 1, minor: 2, patch: 7 }, new Version { major: 0, minor: 9, patch: 9 }];
    let zero = new Version { major: 0, minor: 0, patch: 0 };
    print(show(largest(versions, zero)));
    sort(versions);
    print(show(versions[0]) + " " + show(versions[1]) + " " + show(versions[2]));
}
```

```output
9
pear
1.10.0
0.9.9 1.2.7 1.10.0
```

`1.10.0` is larger than `1.2.7` because `major` is equal and `minor` decides, as integers. Put the most
significant field first when you declare a record you intend to order.

Comparing a plain `T` is [E0311](../errors/E0311.md), reported inside the generic function; its fix
adds `: ordered`:

```vo
fn bigger[T](a: T, b: T) -> T {
    if a > b {
        return a;
    }
    return b;
}

fn main() uses console {
    print(str(bigger(1, 2)));
}
```

```error E0311
```

Passing a type without an order where `ordered` is required is [E0313](../errors/E0313.md); the
message names the field that has no order:

```vo
record Task {
    title: str;
    tags: list[str];
}

fn earliest[T: ordered](a: T, b: T) -> T {
    return min(a, b);
}

fn main() uses console {
    let a = new Task { title: "a", tags: ["x"] };
    let b = new Task { title: "b", tags: ["y"] };
    print(earliest(a, b).title);
}
```

```error E0313
```

Map keys are not generic: `map[K, V]` needs `K` to be exactly `int` or `str`, so `map[K, int]` with a
type parameter `K` is [E0309](../errors/E0309.md). For a generic keyed collection use an
`ordered_map[K, V]` with `K: ordered` (see [Collections](collections.md)).

## Effect parameters

A higher-order function calls a function it was given. Effects are part of function types, so
`f: fn(T)` accepts only pure functions, and a function that prints does not fit
([E0502](../errors/E0502.md)):

```vo
fn each[T](xs: list[T], f: fn(T)) {
    for x in xs {
        f(x);
    }
}

fn main() uses console {
    each([1, 2], fn(n: int) {
        print(str(n));
    });
}
```

```error E0502
```

Writing `f: fn(T) uses console` would fix this call but make `each` print-capable for every caller,
even callers passing a pure function. An effect parameter, `E: effects`, says instead "whatever effects
the given function has, this function has too":

```vo
fn each[T, E: effects](xs: list[T], f: fn(T) uses E) uses E {
    for x in xs {
        f(x);
    }
}

fn doubled(xs: list[int]) -> list[int] {
    let out = new list[int];
    each(xs, fn(x: int) {
        push(out, x * 2);
    });
    return out;
}

fn main() uses console {
    each(doubled([1, 2, 3]), fn(n: int) {
        print(str(n));
    });
}
```

```output
2
4
6
```

At each call, `E` becomes the effects of the function passed in. In `doubled` the function is pure, so
`E` is empty and `doubled` stays pure. In `main` it prints, so `E` is `console` and `main` needs
`uses console`, exactly as if it had printed itself. A function with an effect parameter can declare
other effects next to it (`uses E, console`), and the parameter can stand alone:
`fn twice[E: effects](f: fn() uses E) uses E`. The standard library uses the same pattern, for
example `std.http.serve`, whose handler's effects become the caller's.

## Limits

- Type arguments are never written at calls, only in types and optionally in `new` and variant cases.
- The only constraints are `ordered` and `effects`. There is no way to require `+`, a field or a
  function on `T`: pass a function value instead (`fold`, `sort_by` and `ordered_set_by` work this way).
- A generic function cannot be used as a function value, not even where the expected type is known:
  `map(xs, ident)` with a generic `ident` is a type mismatch. Wrap the call in an anonymous function,
  `map(xs, fn(x: int) -> int { return ident(x); })`.
- Map keys cannot be a type parameter ([E0309](../errors/E0309.md) above).

## Error codes

| Code | Meaning |
|---|---|
| [E0309](../errors/E0309.md) | a map key type is not `int` or `str`, including a type parameter |
| [E0310](../errors/E0310.md) | a type argument cannot be inferred: annotate the variable |
| [E0311](../errors/E0311.md) | a plain type parameter is compared or sorted: add `: ordered` |
| [E0312](../errors/E0312.md) | a generic type is written with the wrong number of arguments |
| [E0313](../errors/E0313.md) | a type without an order is used where an order is needed |
| [E0502](../errors/E0502.md) | a function value has effects its expected type does not allow |

# Records and variants

Records group named fields into one value, variants hold exactly one of several cases, and `match` takes a variant apart with a check that every case is handled.

vo has no classes, no inheritance, no methods and no `null`. Data is modeled with two kinds of declared
types: a **record** when a value has several parts at once (a point has an `x` and a `y`), and a
**variant** when a value is one of several alternatives (a shape is a circle or a rectangle). Functions
work on them from the outside. The rules are in the reference section
[Declarations](../reference/declarations.md); `match` is under [Statements](../reference/statements.md).

## Records

A record declaration lists its fields, each with a type and a `;`. Create a value with
`new Name { field: value, ... }`, read a field with `.`, and change one with `set`:

```vo
record Item {
    name: str;
    price: int;
    tags: list[str];
}

fn label(item: Item) -> str {
    return item.name + " (" + str(item.price) + ", " + join(item.tags, "/") + ")";
}

fn main() uses console {
    let lamp = new Item { price: 25, name: "lamp", tags: ["home", "light"] };
    print(label(lamp));
    set lamp.price = 20;
    push(lamp.tags, "sale");
    print(label(lamp));
}
```

```output
lamp (25, home/light)
lamp (20, home/light/sale)
```

- `new` must give every field exactly once, in any order. A missing field is
  [E0305](../errors/E0305.md); an unknown one is [E0304](../errors/E0304.md). There are no default values
  and no constructors: to build records with defaults, write a function that returns a `new` value.
- `set lamp.price = 20;` works even though `lamp` is a `let`: `let` only stops the name `lamp` from
  pointing to another record, not the record's fields from changing.
- Fields can have any type, including lists, maps, other records, variants and functions. Fields follow
  the naming rule for variables (`snake_case`); record names are `PascalCase`.
- Declarations can appear in any order in the file, so a record can use a type declared below it.

```vo
record Point {
    x: int;
    y: int;
}

fn main() uses console {
    let p = new Point { x: 1 };
    print(str(p.x));
}
```

```error E0305
```

### Records are references

A record value is a reference to one shared record. Assigning it to another name, passing it to a
function, or storing it in a list does not copy it: every copy of the reference sees the same fields.

```vo
record Counter {
    hits: int;
}

fn bump(c: Counter) {
    set c.hits = c.hits + 1;
}

fn main() uses console {
    let a = new Counter { hits: 0 };
    let b = a;
    bump(a);
    bump(b);
    let all = [a, b];
    set all[0].hits = 10;
    print(str(a.hits) + " " + str(b.hits));
    let fresh = new Counter { hits: a.hits };
    bump(fresh);
    print(str(a.hits) + " " + str(fresh.hits));
}
```

```output
10 10
10 11
```

A function that receives a record can change it, as `bump` does, even though the parameter itself is
read-only (a parameter cannot be `set` to another record). When you want a separate value, create one
with `new`, as `fresh` above. Returning a new record instead of changing the old one, as `moved` does in
the [reference example](../reference/declarations.md#records), keeps a function easy to test.

Comparing records with `==` compares their contents, field by field, not whether they are the same
reference:

```vo
record Point {
    x: int;
    y: int;
}

fn main() uses console {
    let p = new Point { x: 1, y: 2 };
    let q = new Point { x: 1, y: 2 };
    print(str(p == q) + " " + str(p == new Point { x: 2, y: 1 }));
}
```

```output
true false
```

## Variants

A variant is a value that is exactly one of several **cases**. Each case may carry one value, its
payload, of any type; a case that needs several values carries a record. Build a value with
`Name.case(value)`, or `Name.case` for a case without a payload:

```vo
variant Shape {
    circle(float);
    rect(Size);
    point;
}

record Size {
    w: float;
    h: float;
}

fn area(s: Shape) -> float {
    match s {
        case circle(r) {
            return 3.14159 * r * r;
        }
        case rect(size) {
            return size.w * size.h;
        }
        case point {
            return 0.0;
        }
    }
}

fn main() uses console {
    let shapes = [Shape.circle(2.0), Shape.rect(new Size { w: 3.0, h: 1.5 }), Shape.point];
    for s in shapes {
        print(str(area(s)));
    }
}
```

```output
12.56636
4.5
0.0
```

Variants replace several features of other languages: enumerations (cases without payloads), tagged
unions, "nullable" values (a `found(Item)` case and a `missing` case) and class hierarchies with one
subclass per kind of thing. Values of different cases all have the variant's type, so they fit in one
list, as `shapes` above.

## match

`match` is how you find out which case a variant holds, and the only way to reach a payload. Each `case`
names one case of the variant, without the variant's name, and runs its block when the value is that
case:

- `case circle(r)` binds the payload to a new name, `r`, visible inside that block.
- `case circle(_)` matches the case and ignores the payload.
- `case point` matches a case without a payload. Writing `case point()` is
  [E0411](../errors/E0411.md).

`match` works on variants and on `result` values only. To branch on an `int` or a `str`, use
`if`/`else if`. `match` is a statement, not an expression: to compute a value, `return` it from each arm
(as `area` does) or `set` a `var`.

### Every case, exactly once

The checker requires each case to appear exactly once, and there is no default arm. A missing case is
[E0401](../errors/E0401.md), and its fix inserts the missing arm; a repeated one is
[E0402](../errors/E0402.md).

```vo
variant Light {
    red;
    amber;
    green;
}

fn next(l: Light) -> Light {
    match l {
        case red {
            return Light.green;
        }
        case green {
            return Light.amber;
        }
    }
}

fn main() uses console {
    print(str(next(Light.red) == Light.green));
}
```

```error E0401
```

This is the point of having no default arm: when you add a case to a variant, every `match` on it stops
checking until it handles the new case, and the checker lists each one. In a function that returns a
value, the missing arm also means a path can reach the end without `return`, so the same mistake reports
[E0403](../errors/E0403.md) too. Fix the `match`, and both go away.

### Matching results

A `result[T]` is a built-in variant with two cases, `ok(T)` and `err(str)`, matched the same way. For a
`result[void]`, which carries no value on success, write `case ok()`, as it is built with `ok()`:

```vo
fn check_age(age: int) -> result[void] {
    if age < 0 {
        return err("age cannot be negative: " + str(age));
    }
    return ok();
}

fn main() uses console {
    match parse_int("41") {
        case ok(n) {
            print("parsed " + str(n));
        }
        case err(e) {
            print(e);
        }
    }
    match check_age(-3) {
        case ok() {
            print("valid");
        }
        case err(e) {
            print(e);
        }
    }
}
```

```output
parsed 41
age cannot be negative: -3
```

[Results and errors](results-and-errors.md) covers `try`, which saves writing most of these `match`
statements.

## Recursive variants

A variant can refer to itself, directly in a payload or through a record. This is how trees and other
recursive data are built, with a case without a payload (or a non-recursive one) at the leaves. Here an
arithmetic expression is a number, a negation, or an operation on two expressions:

```vo
variant Expr {
    num(int);
    neg(Expr);
    add(Pair);
    mul(Pair);
}

record Pair {
    left: Expr;
    right: Expr;
}

fn eval(e: Expr) -> int {
    match e {
        case num(n) {
            return n;
        }
        case neg(inner) {
            return 0 - eval(inner);
        }
        case add(p) {
            return eval(p.left) + eval(p.right);
        }
        case mul(p) {
            return eval(p.left) * eval(p.right);
        }
    }
}

fn show(e: Expr) -> str {
    match e {
        case num(n) {
            return str(n);
        }
        case neg(inner) {
            return "-" + show(inner);
        }
        case add(p) {
            return "(" + show(p.left) + " + " + show(p.right) + ")";
        }
        case mul(p) {
            return show(p.left) + " * " + show(p.right);
        }
    }
}

fn main() uses console {
    let sum = Expr.add(new Pair { left: Expr.num(2), right: Expr.num(3) });
    let e = Expr.mul(new Pair { left: sum, right: Expr.neg(Expr.num(4)) });
    print(show(e) + " = " + str(eval(e)));
}
```

```output
(2 + 3) * -4 = -20
```

Because there is no `null`, a record cannot end a chain by leaving a field empty. A linked structure puts
the "nothing here" case in a variant instead:

```vo
variant Chain {
    end;
    link(Node);
}

record Node {
    value: str;
    next: Chain;
}

fn describe(c: Chain) -> str {
    match c {
        case end {
            return "end";
        }
        case link(node) {
            return node.value + " -> " + describe(node.next);
        }
    }
}

fn main() uses console {
    let c = Chain.link(new Node { value: "a", next: Chain.link(new Node { value: "b", next: Chain.end }) });
    print(describe(c));
}
```

```output
a -> b -> end
```

For everyday collections, prefer the built-in [lists and maps](lists-and-maps.md) and
[collections](collections.md); recursion over deep structures uses the call stack like any recursion.

## Going further

- Records and variants can take type parameters, as in `record Pair[A, B]` or `variant Tree[T]`: see
  [Generics](generics.md).
- To use a record or variant from another file, declare it with `export`: see
  [Modules and packages](modules-and-packages.md).
- To add a field to a record and to every `new` of it at once, use `vo edit add-field` (see
  [Querying and editing programs](../reference/querying-and-editing-programs.md)).

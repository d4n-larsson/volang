# Modules and packages

A vo program can span several files: each file is a module that marks what it shares with `export`, other files bring items in one at a time with `import`, and outside code comes in as packages pinned by a content hash.

## Why files and imports work this way

There is no separate module declaration in vo, no package registry and no build file. A file is a module,
its path is its name, and an import names exactly one item. Reading the top of a file tells you every
name it uses from elsewhere and where each one lives, and the toolchain can always find the declaration
of a name without searching.

Packages, code you did not write, are pinned by a hash of their files. A package cannot change under
you: if one byte of it changes, the program stops checking until someone reviews the change and pins it
again. Its functions also appear in the program's effects record, so what a package may do is reviewed
like your own code.

The rules are in [Programs in several files](../reference/programs-in-several-files.md) and
[Standard library and packages](../reference/standard-library-and-packages.md).

## Importing from another file

`import path.name;` makes the item `name` of the file `path.vo` visible. Only items declared with
`export` can be imported:

```vo
# file: geometry.vo
export record Point {
    x: int;
    y: int;
}

export fn dist2(a: Point, b: Point) -> int {
    return square(a.x - b.x) + square(a.y - b.y);
}

fn square(n: int) -> int {
    return n * n;
}
```

```vo
# file: main.vo
import geometry.Point;
import geometry.dist2;

fn main() uses console {
    let home = new Point { x: 0, y: 0 };
    let shop = new Point { x: 3, y: 4 };
    print(str(dist2(home, shop)));
}
```

```output
25
```

The rules:

- Imports come first in a file, before every item.
- One `import` per name. There is no `import geometry.*` and no renaming, so two imports of the same name,
  or an import and a declaration with the same name, are [E0202](../errors/E0202.md).
- `export` goes before `fn`, `record` or `variant`. Everything else is private to its file: `square` above
  can be used only inside `geometry.vo`.
- Importing a private item is [E0209](../errors/E0209.md); its fix adds `export` to the declaration.
- Using a name that another file declares, without importing it, is [E0204](../errors/E0204.md); its fix
  adds the `import` line.
- An import of a file that does not exist, or of a name it does not declare, is
  [E0205](../errors/E0205.md).

Run and test the program through the file that contains `main`: `vo run main.vo`. Only the files it
imports, directly or through other files, are part of the program.

### Exports cannot leak private types

An exported function, record or variant is used from other files, so every type in its signature must be
importable too. A private record in the parameters of an exported function is
[E0210](../errors/E0210.md), and the fix exports the record:

```vo
# file: prices.vo
record Rate {
    percent: int;
}

export fn with_tax(cents: int, r: Rate) -> int {
    return cents + cents * r.percent / 100;
}
```

```vo
# file: main.vo
import prices.with_tax;

fn main() uses console {
    print("no rate to pass");
}
```

```error E0210
```

The same applies to the fields of an exported record and the cases of an exported variant.

## Directories

A path with more parts names a file in a directory, relative to the directory of the file that imports:
`import shop.cart.subtotal;` in `main.vo` loads `shop/cart.vo` next to `main.vo`. A file in `shop/`
importing `tax.rate` loads `shop/tax.vo`, next to itself.

```vo
# file: shop/cart.vo
export record Item {
    name: str;
    cents: int;
    quantity: int;
}

export fn subtotal(items: list[Item]) -> int {
    var sum = 0;
    for it in items {
        set sum = sum + it.cents * it.quantity;
    }
    return sum;
}
```

```vo
# file: main.vo
import shop.cart.Item;
import shop.cart.subtotal;

fn main() uses console {
    let items = [
        new Item { name: "tea", cents: 450, quantity: 2 },
        new Item { name: "scone", cents: 305, quantity: 1 }
    ];
    print("subtotal " + str(subtotal(items)) + " cents");
}
```

```output
subtotal 1205 cents
```

Paths cannot contain `..`: a file imports only from its own directory and the directories below it. Put
the entry file at the top of the program and shared code below it.

## Import cycles

Imports must point one way. Files that import each other, directly or through other files, are
[E0206](../errors/E0206.md), and the report lists the chain:

```vo
# file: orders.vo
import customers.Customer;

export record Order {
    customer: Customer;
    cents: int;
}
```

```vo
# file: customers.vo
import orders.Order;

export record Customer {
    name: str;
}

export fn last_order(orders: list[Order]) -> int {
    return orders[len(orders) - 1].cents;
}
```

```vo
# file: main.vo
import orders.Order;
import customers.Customer;

fn main() uses console {
    let o = new Order { customer: new Customer { name: "ana" }, cents: 900 };
    print(o.customer.name);
}
```

```error E0206
```

The fix is a design decision, so there is no automatic one. Usually a function such as `last_order`
belongs next to the type it works on, or the shared types move into a third file that both sides import.

## The effects record of a program

A program in several files should have a `vo.effects` record next to its entry file, listing the effects
of every function and test. Without one, `vo check` warns ([W0504](../errors/W0504.md)); create it with
`vo effects --update main.vo`. Every function is listed with its file, so a reviewer sees which file
gained which effect. See [Effects](effects.md).

## The standard library

The standard library is imported the same way, with `std` as the first part of the path:
`import std.text.pad_left;`. It is built into `vo`, needs no installation, and its effects work like
those of builtins: the caller declares them. See [The standard library](standard-library.md).

## Packages

A package is a directory of vo files that your program uses but does not own: a library copied from
another project, a vendored dependency. `vo.packages` (next to the entry file, or in a directory above
it) lists each package with one line:

```text
NAME DIRECTORY sha256:HASH
```

`import NAME.a.b.item;` then loads `DIRECTORY/a/b.vo`. The directory is relative to `vo.packages`, and
can be anywhere: inside the project, or next to it.

`vo package ENTRY NAME DIRECTORY` adds a package, or updates its hash, and prints what it changed. Here a
small money-formatting package lives in `vendor/money`:

```vo
# file: vendor/money/format.vo

# Cents as units with two decimals: 1205 is "12.05".
export fn money(cents: int) -> str {
    return str(cents / 100) + "." + two_digits(cents % 100);
}

fn two_digits(n: int) -> str {
    if n < 10 {
        return "0" + str(n);
    }
    return str(n);
}
```

```vo
# file: main.vo
import money.format.money;

fn main() uses console {
    print("total " + money(1205));
}
```

```console
$ vo package main.vo money vendor/money
added money vendor/money sha256:2770bcbd0abbd4df166ce893ff9d8844bdd671aec325c8ec2620b8b7a7c30c42
$ vo run main.vo
total 12.05
$ vo doc money.format
fn money(cents: int) -> str
    Cents as units with two decimals: 1205 is "12.05".
```

`vo.packages` now holds the line, with a header comment:

```text
# vo packages: update with `vo package ENTRY NAME DIRECTORY`
money vendor/money sha256:2770bcbd0abbd4df166ce893ff9d8844bdd671aec325c8ec2620b8b7a7c30c42
```

`vo doc NAME.module` documents a package's exported items as it does the standard library's.
`vo package --remove ENTRY NAME` removes the line.

### The hash

The hash covers every `.vo` file under the package's directory: their paths, lengths and contents. Any
change, one edited byte, a new file, a deleted or renamed one, gives a different hash. When the files
no longer match the pinned hash, nothing of the package is checked or run, and every program that
imports it fails with [E0701](../errors/E0701.md) (hashes shortened here):

```text
main.vo:1:1: error[E0701]: Package money changed: the manifest pins sha256:6764...d486, its files hash to sha256:76cd...b270. Review the change, then run `vo package main.vo money vendor/money`.
   1 | import money.format.money;
     | ^^^^^^^^^^^^^^^^^^^^^^^^^^
```

This is deliberate. Updating a package is two steps: change its files (copy in the new version), then
review the difference and run `vo package` again, which prints the old and the new hash:

```text
updated money: sha256:6764...d486 -> sha256:76cd...b270
```

A package whose directory is missing, unreadable or holds no `.vo` file is [E0702](../errors/E0702.md);
restore the directory or point the line elsewhere with `vo package`.

### A package's effects

Package functions are listed in the program's `vo.effects` like your own, with the package name in
front: `@money/format.vo:money pure`. If a new version of a package makes a function print, read files
or use the network, `vo check` reports that function's new effects ([E0504](../errors/E0504.md)) until
`vo effects --update` records them, so the change shows up in review as one line, not as a diff of the
package's code.

Inside a package, the usual rules hold: items are private unless exported (an unexported item has no fix
to offer: use one that `vo doc` lists), and a package function's effects are declared by every caller.

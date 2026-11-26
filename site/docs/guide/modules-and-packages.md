# Modules and packages

A vo program can span several files: each file is a module that marks what it shares with `export`, other files import the module and write its items with its name (`geometry.dist2`), and outside code comes in as packages pinned by a content hash.

## Why files and imports work this way

There is no separate module declaration in vo, no package registry and no build file. A file is a module,
its path is its name, and an import names one module. Reading the top of a file tells you every module
it uses, and every use says which module it comes from (`sqlite.exec`, `web.request`), so a reader and the
toolchain find the declaration of a name without searching.

Packages, code you did not write, are pinned by a hash of their files. A package cannot change under
you: if one byte of it changes, the program stops checking until someone reviews the change and pins it
again. Its functions also appear in the program's effects record, so what a package may do is reviewed
like your own code.

The rules are in [Programs in several files](../reference/programs-in-several-files.md) and
[Standard library and packages](../reference/standard-library-and-packages.md).

## Importing from another file

`import path;` makes the items of the file `path.vo` visible as `path.NAME` (the last part of the path
before the name: `import shop.cart;` gives `cart.total`). Only items declared with `export` can be used:

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
import geometry;

fn main() uses console {
    let home = new geometry.Point { x: 0, y: 0 };
    let shop = new geometry.Point { x: 3, y: 4 };
    print(str(geometry.dist2(home, shop)));
}
```

```output
25
```

The rules:

- Imports come first in a file, before every item.
- One `import` per module. There is no renaming, so two imports that end in the same name, or a module
  and a declaration with the same name, are [E0202](../errors/E0202.md); no local may have a module's name
  ([E0203](../errors/E0203.md), whose fix renames it `NAME_value`).
- `export` goes before `fn`, `record` or `variant`. Everything else is private to its file: `square` above
  can be used only inside `geometry.vo`.
- Using a private item (`geometry.square`) is [E0209](../errors/E0209.md); its fix adds `export` to the
  declaration.
- Using a module without importing it is [E0204](../errors/E0204.md); its fix adds the `import` line. An item
  of an imported module written without the module's name is [E0201](../errors/E0201.md), whose fix adds it.
- An import of a file that does not exist is [E0205](../errors/E0205.md); a name the module does not declare
  is [E0201](../errors/E0201.md). A module imported and not used is [W0203](../errors/W0203.md).
- The 0.3 form, `import geometry.dist2;`, is [E0215](../errors/E0215.md): its fix rewrites the imports and
  every use in one edit.

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
import prices;

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
import shop.cart;

fn main() uses console {
    let items = [
        new cart.Item { name: "tea", cents: 450, quantity: 2 },
        new cart.Item { name: "scone", cents: 305, quantity: 1 }
    ];
    print("subtotal " + str(cart.subtotal(items)) + " cents");
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
import customers;

export record Order {
    customer: customers.Customer;
    cents: int;
}
```

```vo
# file: customers.vo
import orders;

export record Customer {
    name: str;
}

export fn last_order(orders_value: list[orders.Order]) -> int {
    return orders_value[len(orders_value) - 1].cents;
}
```

```vo
# file: main.vo
import customers;
import orders;

fn main() uses console {
    let o = new orders.Order { customer: new customers.Customer { name: "ana" }, cents: 900 };
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
`import std.text;`, then `text.pad_left(...)`. It is built into `vo`, needs no installation, and its effects work like
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
import money.format;

fn main() uses console {
    print("total " + format.money(1205));
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

### Getting a package

A package someone else published is fetched with the same command, with a URL where the directory was:

```text
$ vo package main.vo money https://codeberg.org/example/money/archive/v1.2.0.tar.gz
added money packages/money sha256:1e5b...62c5 from https://codeberg.org/example/money/archive/v1.2.0.tar.gz
  effects: none
```

The URL is an archive (`.tar.gz` or `.tar`) served over HTTPS, which every forge offers for a tag or
a commit. vo downloads it, takes its `.vo` files (a folder that wraps everything, such as
`money-1.2.0/`, is dropped; `URL#src/money` takes one folder inside), writes them to `packages/NAME`
next to `vo.packages` (or `--dir DIRECTORY`), and pins them exactly as it pins a local directory. The
URL is kept as a fourth word of the manifest line.

- **The hash is what is trusted, not the URL.** After the fetch, the package is a directory with a
  pinned hash like any other. `vo check`, `vo run`, `vo test` and `vo build` never download anything.
- **The output shows what the package can do**: the effects its exported functions declare, with
  scopes. A package for formatting money that lists `net("...")` is worth a second look before you
  import it. An update shows what the new version `gains` and `loses`.
- **An archive is refused as a whole** if it holds a link, a device, or a path that would leave the
  package ([E0706](../errors/E0706.md)); a failed fetch changes nothing.
- **`vo package --restore main.vo`** fetches every listed package whose directory is missing, and
  accepts each only if it hashes to its pin. Keep `vo.packages` in version control; whether you also
  commit `packages/` is your choice.
- A package directory you edited is never replaced by a fetch: pin your change with the local form,
  or remove the directory.
- Use the archive link of a tag or commit. A branch's archive changes, and `--restore` then refuses it
  ([E0701](../errors/E0701.md)).
- An archive has no version and brings nothing with it: a package in it that imports another shows as
  an unresolved import, and you add that one too. Versioned packages, next, do both.

### Versions and dependencies

A package that lives in a git repository is added by the repository's URL. Its versions are the
repository's tags (`v1.2.0` or `1.2.0`), and the packages it requires come with it:

```text
$ vo package main.vo report https://codeberg.org/acme/report
added money 1.4.0 packages/money sha256:d511...451b (required by report)
  effects: none
added report 2.1.0 packages/report sha256:19ef...0b01 from https://codeberg.org/acme/report
  effects: random
total gains: random
```

`report` is now a line of `vo.packages` with `@2.1.0` after its URL, and `money`, which `report`
requires, a line marked `indirect`. There is no registry and no account: the repository is the
package, on any host that serves git over HTTPS.

- **One version of each package**: the highest that your manifest or any package's manifest asks for.
  `1.2.0` and `1.4.0` can stand in for each other; `1.4.0` and `2.0.0` cannot, and two packages that
  need both are an error that names them ([E0707](../errors/E0707.md)).
- **Nothing moves by itself.** A release of `money` 1.6.0 changes nothing in your program.
  `vo package --outdated main.vo` lists what is newer, and `vo package --update main.vo money` takes
  it, printing the old and new version, the hashes, and what the package's effects gained and lost.
  A new major version is taken only when you name it: `...money@2.0.0`.
- **A clone restores exactly.** `vo package --restore main.vo` fetches each version again and accepts
  it only at its pinned hash.
- **A merge can leave the manifest incomplete**: a package requires something the manifest no longer
  lists, or lists lower. `vo check` says so ([E0709](../errors/E0709.md)) and
  `vo package --repair main.vo` fixes it.
- **Removing** a package removes the indirect packages only it required.

### Releasing a package

A package is released by tagging its repository. If it uses other packages, add them in the package's
own folder with the same command a program uses; the `vo.packages` at the package's root is then the
list of what it requires, and whoever adds your package gets those too:

```text
$ vo package lib.vo pads https://codeberg.org/acme/pads
$ vo test lib.vo
$ git add . && git commit -m "kit 1.0.0" && git tag v1.0.0 && git push --tags
```

Every requirement must be a versioned one: a line for a local directory serves only your own checkout,
and a package with such a line cannot be added by others. A released version must never change; a fix
is a new version.

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

A package function that is given its target (a client's `connect(host, ...)`, a function that reads
the file it is handed) declares the effect without scopes: `uses net`. Your program still declares
`net("db.internal:5432")`, and that scope applies to everything the package does on your behalf: a
host outside it stops the program with [R0010](../errors/R0010.md). See
[Functions that are given their target](effects.md#functions-that-are-given-their-target). The [PostgreSQL](postgres.md) package is such a package.

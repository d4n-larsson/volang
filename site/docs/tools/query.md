# vo query

`vo query` answers questions about a program from the checker itself: who calls a function, every reference to a name, the type at a position, and which tests prove which requirement.

## Synopsis

```text
vo query [--json] callers ENTRY NAME
vo query [--json] refs ENTRY TARGET
vo query [--json] type-at ENTRY FILE:LINE:COL
vo query [--json] specs ENTRY
```

`ENTRY` is the program's entry file, the one `vo run` or `vo test` would be given. Only the files it
imports, directly or not, are searched. Because answers come from the checker, a string, a comment or an
unrelated local with the same name never matches, which a text search cannot promise.

| Query | Answers |
|---|---|
| `callers ENTRY NAME` | the call sites, and uses as a value, of a function or builtin |
| `refs ENTRY TARGET` | the declaration and every use of a target |
| `type-at ENTRY FILE:LINE:COL` | the type of the name or expression at that position, where the name is declared, and its documentation |
| `specs ENTRY` | every `@spec` ID and the tests and properties that name it |

A `TARGET` is:

| Form | Means | Example |
|---|---|---|
| `name` | a top-level function, record or variant, in any file of the program | `tax` |
| `Record.field` | a record field | `Item.price` |
| `Variant.case` | a variant case | `Shape.circle` |
| `FILE:LINE:COL` | whatever is at that position, including parameters, locals and type parameters | `main.vo:6:9` |

`LINE` and `COL` are 1-based, and `COL` counts characters, as in diagnostics. A missing name is
[T0004](../errors/T0004.md), with the closest name when there is one. A name declared in two files of the
program is [T0005](../errors/T0005.md), which lists the declarations: give the one you mean as
`FILE:LINE:COL`. The rules are in the reference's
[Querying and editing programs](../reference/querying-and-editing-programs.md).

A program with check errors is still answered, for the parts that check; a syntax error that stops the
checker is not.

## Options

| Option | Meaning |
|---|---|
| `--json` | One record per answer (`ref`, `type_at` or `spec`), then a `summary` record. |

## Output

Text, on stdout. `callers` and `refs` print one line per reference, `FILE:LINE:COL ROLE in FUNCTION: LINE`,
where `ROLE` is `declaration`, `call`, `value` (a function used as a value), `type` (in a type), `import`
or `use` (anything else: a read, a field, a record literal's field, a case), and `FUNCTION` is the enclosing
function, `test "name"` or `prop "name"` (absent at top level). `refs` lists the declaration first; `callers`
lists only `call` and `value`. Then a count: `6 references`.

`type-at` prints `FILE:LINE:COL NAME: TYPE (declared at FILE:LINE:COL)` on a name, or
`FILE:LINE:COL EXPRESSION: TYPE` on an expression. When the name is a function, record, variant, field,
case, library item or builtin, two indented lines follow: its declared signature and its description.

The description comes from the comment lines directly above the declaration, the same convention the
standard library uses; a line `#     EXPR is VALUE` is its example. This is what an editor shows on hover:

```text
# The squared distance between a and b,
# cheaper than the distance.
#     dist2(new Point { x: 0.0, y: 0.0 }, new Point { x: 3.0, y: 4.0 }) is 25.0
export fn dist2(a: Point, b: Point) -> float {
```

In JSON the answer gains `"doc"`, the record `vo doc --json` prints for the item (`signature`,
`description`, `example`, `effects`, …; fields get `"item":"field"` and `"owner"`, cases `"item":"case"`).
Locals and parameters have no `doc`. `vo doc ENTRY NAME` prints the same for an item of the program. `specs` prints `ID  FILE:LINE:COL test "name"` per pair,
IDs sorted, then a count of IDs.

When the program has errors, the text ends with `note: the program has N errors; answers cover the parts that check`.

With `--json`:

```json
{"v":1,"kind":"ref","target":"tax","role":"call","file":"pricing.vo","start":{"line":14,"col":38},"end":{"line":14,"col":41},"text":"tax","in":"total"}
{"v":1,"kind":"type_at","file":"main.vo","start":{"line":6,"col":9},"end":{"line":6,"col":14},"text":"items","type":"list[Item]","name":"items","declaration":{"file":"main.vo","start":{"line":6,"col":9},"end":{"line":6,"col":14}}}
{"v":1,"kind":"spec","id":"FR-001","tests":[{"name":"total adds tax","kind":"test","file":"main.vo","start":{"line":11,"col":1}}]}
{"v":1,"kind":"summary","command":"query","errors":0,"warnings":0,"truncated":0,"files":2,"results":1}
```

`type_at` has `name` and `declaration` only on a name. The `summary` counts the program's check errors and
warnings, the files loaded and the `results`.

## Exit statuses

| Status | When |
|---|---|
| 0 | Answered, even when the program has check errors. |
| 64 | Usage error ([T0001](../errors/T0001.md)), unknown target ([T0004](../errors/T0004.md)) or ambiguous target ([T0005](../errors/T0005.md)). |
| 65 | A file has an error that stops the checker (a syntax error). |
| 66 | A file cannot be read ([T0002](../errors/T0002.md)). |

## Example

A program in two files:

```vo
# file: pricing.vo
export record Item {
    name: str;
    price: int;
}

export fn tax(amount: int) -> int {
    return amount / 4;
}

export fn total(items: list[Item]) -> int {
    var sum = 0;
    for item in items {
        set sum = sum + item.price + tax(item.price);
    }
    return sum;
}
```

```vo
# file: main.vo
import pricing.Item;
import pricing.total;

fn main() uses console {
    let items = [new Item { name: "pen", price: 4 }, new Item { name: "book", price: 20 }];
    print("total " + str(total(items)));
}

@spec("FR-001")
test "total adds tax" {
    expect_eq(total([new Item { name: "pen", price: 4 }]), 5);
}
```

```console
$ vo query callers main.vo total
main.vo:7:26 call in main: print("total " + str(total(items)));
main.vo:12:15 call in test "total adds tax": expect_eq(total([new Item { name: "pen", price: 4 }]), 5);
2 references
$ vo query refs main.vo Item.price
pricing.vo:4:5 declaration: price: int;
main.vo:6:42 use in main: let items = [new Item { name: "pen", price: 4 }, new Item { name: "book", price: 20 }];
main.vo:6:79 use in main: let items = [new Item { name: "pen", price: 4 }, new Item { name: "book", price: 20 }];
main.vo:12:46 use in test "total adds tax": expect_eq(total([new Item { name: "pen", price: 4 }]), 5);
pricing.vo:14:30 use in total: set sum = sum + item.price + tax(item.price);
pricing.vo:14:47 use in total: set sum = sum + item.price + tax(item.price);
6 references
$ vo query type-at main.vo main.vo:6:9
main.vo:6:9 items: list[Item] (declared at main.vo:6:9)
$ vo query type-at main.vo main.vo:7:20
main.vo:7:11 "total " + str(total(items)): str
$ vo query specs main.vo
FR-001  main.vo:11:1 test "total adds tax"
1 requirement ID
$ vo query --json callers main.vo tax
{"v":1,"kind":"ref","target":"tax","role":"call","file":"pricing.vo","start":{"line":14,"col":38},"end":{"line":14,"col":41},"text":"tax","in":"total"}
{"v":1,"kind":"summary","command":"query","errors":0,"warnings":0,"truncated":0,"files":2,"results":1}
$ vo query refs main.vo totl
error[T0004]: Nothing named `totl` in this program. Did you mean `total`?
```

`Item.price` matched the field in record literals and field reads, in both files; a local or a map key
named `price` would not have. On the `+` at column 20, `type-at` answers for the whole expression it
belongs to. To change what a query found, use [vo edit](edit.md); to see what a change affects, use
[vo impact](impact.md).

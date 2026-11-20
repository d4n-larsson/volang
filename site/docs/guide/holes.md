# Holes

A hole, `todo("description")`, stands for code not written yet: it takes whatever type its place needs, so the rest of the program checks, runs and tests while the gap is open.

## Why holes

Programs are rarely written top to bottom in one pass. You know the shape first, the records, the
functions and how they call each other, and fill in the bodies one at a time. In most languages the
unfinished parts must be faked: a dummy return value that type checks but is wrong, or a comment that
the compiler ignores and everyone forgets.

A hole is an explicit placeholder that the toolchain understands. The checker treats it as a value of
the type its place expects, reports it as a warning with that type and the names you can use to fill
it, and the runtime stops with a clear error if the program ever reaches it. Nothing about a hole is
silently wrong.

The rules are in the [Holes](../reference/holes.md) section of the reference.

## What a hole reports

Each hole is warning [W0601](../errors/W0601.md). The report says which type the hole must produce and
lists every name in scope there with its type: exactly what you, or an agent, need to write the
missing expression.

```vo
record Expense {
    category: str;
    cents: int;
}

# The total for each category, in the order they first appear.
fn totals(xs: list[Expense]) -> map[str, int] {
    todo("sum the cents per category");
}

# One line per category: its name and its total.
fn report(sums: map[str, int]) -> str {
    var out = "";
    for k in keys(sums) {
        set out = out + k + " " + todo("the cents as units and two decimals") + "\n";
    }
    return out;
}

fn main() uses console {
    let xs = [
        new Expense { category: "food", cents: 1250 },
        new Expense { category: "rent", cents: 90000 }
    ];
    print(str(len(xs)) + " expenses");
    write(report(totals(xs)));
}
```

```console
$ vo check main.vo
main.vo:8:5: warning[W0601]: Hole: `"sum the cents per category"` needs a value of type `map[str, int]`.
   8 |     todo("sum the cents per category");
     |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    in scope: xs: list[Expense]
main.vo:15:35: warning[W0601]: Hole: `"the cents as units and two decimals"` needs a value of type `str`.
  15 |         set out = out + k + " " + todo("the cents as units and two decimals") + "\n";
     |                                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    in scope: k: str, out: str, sums: map[str, int]
0 errors, 2 warnings (2 holes)
```

The program has no errors: warnings never change the exit status, so `vo check` exits 0 and `vo run`
runs it. The summary line counts the holes, so you can see at a glance how much is left.

The first hole is a whole function body, written as a statement. The second sits inside an expression,
where it stands for one `str`. Both forms are common: a body you have not started, and one piece of an
expression whose shape you already know.

With `--json`, a hole is a `diagnostic` with `expected` (the type) and `scope` (a list of
`{name, type}`), and the `check` summary adds `"holes":N`:

```json
{"v":1,"kind":"diagnostic","code":"W0601","name":"hole","severity":"warning","file":"main.vo","start":{"line":8,"col":5},"end":{"line":8,"col":39},"text":"todo(\"sum the cents per category\")","message":"Hole: `\"sum the cents per category\"` needs a value of type `map[str, int]`.","expected":"map[str, int]","scope":[{"name":"xs","type":"list[Expense]"}],"fixes":[],"related":[]}
```

## Running into a hole

A program with holes runs normally until it reaches one. Then it stops with runtime error
[R0011](../errors/R0011.md) and exit status 70, with the hole's description and the usual stack with the
values of the locals. The program above prints its first line before it reaches `totals`:

```vo
record Expense {
    category: str;
    cents: int;
}

fn totals(xs: list[Expense]) -> map[str, int] {
    todo("sum the cents per category");
}

fn main() uses console {
    let xs = [new Expense { category: "food", cents: 1250 }];
    print(str(len(xs)) + " expenses");
    let sums = totals(xs);
    print(str(len(sums)) + " categories");
}
```

```output
1 expenses
```

On stderr, after the hole's warning:

```text
main.vo:7:5: runtime error[R0011]: Hole reached: sum the cents per category.
   7 |     todo("sum the cents per category");
     |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
trace:
  at totals (main.vo:7:5)
      xs: list[Expense] = [new Expense { category: "food", cents: 1250 }]
  at main (main.vo:13:16)
      xs: list[Expense] = [new Expense { category: "food", cents: 1250 }]
```

The same in `vo test`: a test that reaches a hole is reported as `ERROR`, and the other tests still run.
This is what makes holes useful while you work: the parts that are done can be tested before the rest
exists.

## Writing a program step by step

Here is the program from above one step later. `totals` is written and has a test; `report` still has
its hole, and its test is already written, waiting for it:

```vo-test
record Expense {
    category: str;
    cents: int;
}

# The total for each category, in the order they first appear.
fn totals(xs: list[Expense]) -> map[str, int] {
    let sums = new map[str, int];
    for e in xs {
        set sums[e.category] = get_or(sums, e.category, 0) + e.cents;
    }
    return sums;
}

# One line per category: its name and its total.
fn report(sums: map[str, int]) -> str {
    var out = "";
    for k in keys(sums) {
        set out = out + k + " " + todo("the cents as units and two decimals") + "\n";
    }
    return out;
}

test "totals add up per category" {
    let xs = [new Expense { category: "food", cents: 1250 }, new Expense { category: "rent", cents: 90000 }, new Expense { category: "food", cents: 305 }];
    let sums = totals(xs);
    expect_eq(keys(sums), ["food", "rent"]);
    expect_eq(sums["food"], 1555);
}

test "the report has a line per category" {
    let sums = new map[str, int];
    set sums["rent"] = 90000;
    expect_eq(report(sums), "rent 900.00\n");
}
```

```output
main.vo:19:35: warning[W0601]: Hole: `"the cents as units and two decimals"` needs a value of type `str`.
  19 |         set out = out + k + " " + todo("the cents as units and two decimals") + "\n";
     |                                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    in scope: k: str, out: str, sums: map[str, int]
PASS totals add up per category (0ms)
ERROR the report has a line per category (0ms)
  main.vo:19:35: runtime error[R0011]: Hole reached: the cents as units and two decimals.
    at report (main.vo:19:35)
        sums: map[str, int] = {"rent": 90000}, out: str = "", k: str = "rent"
    at test "the report has a line per category" (main.vo:34:15)
        sums: map[str, int] = {"rent": 90000}
1 passed, 0 failed, 1 errored
```

The hole's `in scope` line says what is available: `sums` and `k`, so the missing value is the amount
`sums[k]` written as money. Filling it closes the last hole:

```vo
record Expense {
    category: str;
    cents: int;
}

fn totals(xs: list[Expense]) -> map[str, int] {
    let sums = new map[str, int];
    for e in xs {
        set sums[e.category] = get_or(sums, e.category, 0) + e.cents;
    }
    return sums;
}

fn money(cents: int) -> str {
    let c = cents % 100;
    if c < 10 {
        return str(cents / 100) + ".0" + str(c);
    }
    return str(cents / 100) + "." + str(c);
}

fn report(sums: map[str, int]) -> str {
    var out = "";
    for k in keys(sums) {
        set out = out + k + " " + money(sums[k]) + "\n";
    }
    return out;
}

fn main() uses console {
    let xs = [
        new Expense { category: "food", cents: 1250 },
        new Expense { category: "rent", cents: 90000 },
        new Expense { category: "food", cents: 305 }
    ];
    write(report(totals(xs)));
}
```

```output
food 15.55
rent 900.00
```

The same loop suits agents: write the outline with holes, run `vo check --json`, and fill each hole from
its `expected` type and `scope`, checking again after each one until the summary has no `holes`.

## The rules

**The place must have a type.** A hole takes its type from where it stands: a parameter, a return, an
operand, an annotated variable. A `let` without a type gives it nothing to go on, which is
[E0601](../errors/E0601.md); annotate the variable, `let x: int = todo("...");`.

```vo
fn shipping(weight_grams: int) -> int {
    let rate = todo("the rate per kilogram");
    return weight_grams * rate / 1000;
}

fn main() uses console {
    print(str(shipping(2500)));
}
```

```error E0601
```

**A statement hole ends its path.** `todo("...");` on its own line works like a `return`: the checker
does not ask for a `return` after it ([E0403](../errors/E0403.md) does not apply), and its type is the
function's return type. In a function that returns nothing, it simply marks the body as unwritten.

**Warnings about unfinished code are held back.** While a function has a hole, its unused locals
([W0201](../errors/W0201.md)) and unused effects ([W0501](../errors/W0501.md)) are not reported: a
half-written function often declares what its finished body will need. They come back once the last
hole in that function is filled.

```vo
fn save_report(text: str) uses console, file.write("out/") {
    let lines = split(text, "\n");
    print(str(len(lines)) + " lines");
    todo("write the text to out/report.txt");
}

fn main() uses console, file.write("out/") {
    save_report("a\nb");
}
```

```console
$ vo check main.vo
main.vo:4:5: warning[W0601]: Hole: `"write the text to out/report.txt"` needs a value of type `void`.
   4 |     todo("write the text to out/report.txt");
     |     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    in scope: lines: list[str], text: str
0 errors, 1 warning (1 hole)
```

`file.write("out/")` is not used yet, but it is not reported as [W0501](../errors/W0501.md): the hole
says the body is not finished.

**Holes are only warnings, but they are counted.** A program with holes checks, runs and tests, so it is
never blocked. Before you ship, make sure the count is zero: `vo check` ends with `(N holes)` and the
JSON summary has `"holes":N` while any remain.

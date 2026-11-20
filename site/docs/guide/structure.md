# Structuring a program

A vo program is a thin shell of effects over a pure core, split into files by concept, with each test next to the code it tests. `vo check` warns when a program drifts from that shape.

## Why programs are shaped this way

Effects are part of every function's signature, so vo can tell code that decides from code that acts.
That makes the most useful structural advice in software checkable: keep the decisions in pure functions
and the effects at the edges. A pure function is one a test can call with no `uses` clause, no files and
no grant; a function that mixes logic with reading, writing or printing can only be tested by giving it
those effects, and usually is not tested at all.

The other rules are the ones good code reviews give everywhere, made measurable:

- **One concept per file.** A file is a module; a module holds one idea (the money arithmetic, the
  parser, the report). A file past 400 code lines does several jobs.
- **Small functions.** A function past 50 statements does several things in a row; each deserves a name.
- **Don't repeat yourself.** Copied code drifts: a fix in one copy misses the others. Make one function
  and pass what differs.
- **Tests next to the code.** A test lives in the file whose code it tests, so moving or changing the code
  takes its tests along. `vo test main.vo` runs the tests of every file the program loads.

The limits are loose on purpose: they catch the clearly unstructured program, not matters of taste.

## The rules

| Code | `vo check` warns when | What to do |
|---|---|---|
| [W0701](../errors/W0701.md) | a function with effects has 15 or more statements that use no effect | move that logic into pure functions |
| [W0702](../errors/W0702.md) | a file has more than 400 code lines | split it into modules, one concept each |
| [W0703](../errors/W0703.md) | a function, test or property has more than 50 statements | split it into functions that each do one thing |
| [W0704](../errors/W0704.md) | 6 or more statements repeat earlier ones, up to names and literals | make one function; an effect parameter when copies differ only in effects |
| [W0705](../errors/W0705.md) | a test calls only functions of one other file | move it into that file |

They are warnings: the program still checks, runs and tests. They are reported only by `vo check`, only
for the program's own files (never the standard library or packages), and only once the program has no
errors.

## Before and after

A small expense report, written the quick way: `main` reads, computes and prints, all in one place.

```vo
fn main() uses console {
    let amounts = [1250, 830, 4999, 120];
    let limit = 2000;
    var total = 0;
    var largest = 0;
    for a in amounts {
        set total = total + a;
        if a > largest {
            set largest = a;
        }
    }
    let count = len(amounts);
    let average = total / count;
    var over = 0;
    for a in amounts {
        if a > limit {
            set over = over + 1;
        }
    }
    let units = total / 100;
    let rest = total % 100;
    print("total: " + str(units) + "." + str(rest));
    print(
        "average: " + str(average) + ", largest: " + str(largest) + ", over the limit: " + str(over)
    );
}
```

```error W0701
```

`main` has 16 statements that use no effect: everything but the two `print`s. None of it can be tested
without printing. Moved into a pure module, with its tests beside it, the same program is:

```vo
# file: report.vo
export record Report {
    total: int;
    average: int;
    largest: int;
    over: int;
}

export fn report(amounts: list[int], limit: int) -> Report {
    var total = 0;
    var largest = 0;
    var over = 0;
    for a in amounts {
        set total = total + a;
        set largest = max(largest, a);
        if a > limit {
            set over = over + 1;
        }
    }
    return new Report { total: total, average: total / len(amounts), largest: largest, over: over };
}

export fn money(cents: int) -> str {
    return str(cents / 100) + "." + str(cents % 100);
}

test "report" {
    let r = report([100, 300], 200);
    expect_eq(r.total, 400);
    expect_eq(r.largest, 300);
    expect_eq(r.over, 1);
}

test "money" {
    expect_eq(money(1250), "12.50");
}
```

```vo
# file: main.vo
import report.money;
import report.report;

fn main() uses console {
    let r = report([1250, 830, 4999, 120], 2000);
    print("total: " + money(r.total));
    print(
        "average: " + str(r.average) + ", largest: " + str(r.largest) + ", over the limit: " + str(r.over)
    );
}
```

```output
total: 71.99
average: 1799, largest: 4999, over the limit: 1
```

`main` now only does the effects; `report` and `money` are tested with no `uses` clause, and
`vo test main.vo` runs those tests from `report.vo`.

## Accepting a warning

Sometimes the structure the rule questions is right: a scanner that reads best top to bottom, a module
that must stay one file because its import path is its name. Say so on the line above the function, test
or property, with the reason:

```vo
@allow("W0703", "one scanner, read top to bottom")
fn scan(text: str) -> int {
    return len(text);
}

fn main() uses console {
    print(str(scan("abc")));
}
```

```error W0706
```

`@allow` accepts that one warning there (for W0702, put it above the file's first function), and
`vo check` counts accepted warnings in its summary: `0 errors, 0 warnings (1 accepted)`. When the warning
goes away, the `@allow` silences nothing and is itself a warning, [W0706](../errors/W0706.md), whose fix
removes it, as above. The reason is required ([E0121](../errors/E0121.md)): it tells the next reader why
the code stays as it is.

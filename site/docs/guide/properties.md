# Property tests

A property is a test with typed parameters: `vo test` runs it on 100 generated inputs and, when one fails, shrinks it to a smaller input that fails the same way.

## Why properties

An example test checks the inputs you thought of. The bugs are usually in the ones you did not: the
negative number, the empty list, the string that contains the separator. A property states a rule that
must hold for every input, and lets the toolchain look for an input that breaks it.

A property is declared like a test, with parameters between the name and the body:

```text
prop "name" (x: T, y: U, ...) uses effects { ... }
```

`vo test` calls the body with 100 generated sets of arguments. Inside, `expect` and `expect_eq` work as
in a [test](tests.md). If every case passes, the property passes; at the first failing case, it stops
and reports that input, made as small as it can.

The rules are in [Property tests](../reference/tests.md#property-tests), part of the
[Tests](../reference/tests.md) section of the reference.

## A first property

A common property is a round trip: writing a value and reading it back gives the value again. Here
`money` writes cents as units and decimals, and `parse_money` reads them back:

```vo-test
# Cents as units with two decimals: 1205 is "12.05".
fn money(cents: int) -> str {
    let c = cents % 100;
    if c < 10 {
        return str(cents / 100) + ".0" + str(c);
    }
    return str(cents / 100) + "." + str(c);
}

# The cents in a text written by `money`.
fn parse_money(text: str) -> result[int] {
    let parts = split(text, ".");
    if len(parts) != 2 {
        return err("not an amount: " + text);
    }
    let units = try parse_int(parts[0]);
    let cents = try parse_int(parts[1]);
    return ok(units * 100 + cents);
}

prop "money reads back" (cents: int) {
    expect_eq(parse_money(money(cents)), ok(cents));
}
```

```output
FAIL money reads back (1 case, 0ms)
  main.vo:22:5: `expect_eq` failed: the values differ.
    expected: ok(-1)
    actual:   err("invalid int: 0-1")
    counterexample: cents = -1
    shrunk in 0 steps
0 passed, 1 failed
```

The first case already fails: for `-1`, `-1 / 100` is `0` and `-1 % 100` is `-1` (integer division
truncates toward zero), so `money` writes `0.0-1`. The report names the failing check, the values it
compared, and the input, one `counterexample` line per parameter.

Negative amounts are not meant to reach `money`. `assume(cond)` says so: an input for which `cond` is
false is discarded, and does not count as a case.

```vo-test
fn money(cents: int) -> str {
    let c = cents % 100;
    if c < 10 {
        return str(cents / 100) + ".0" + str(c);
    }
    return str(cents / 100) + "." + str(c);
}

fn parse_money(text: str) -> result[int] {
    let parts = split(text, ".");
    if len(parts) != 2 {
        return err("not an amount: " + text);
    }
    let units = try parse_int(parts[0]);
    let cents = try parse_int(parts[1]);
    return ok(units * 100 + cents);
}

prop "money reads back" (cents: int) {
    assume(cents >= 0);
    expect_eq(parse_money(money(cents)), ok(cents));
}
```

```output
PASS money reads back (100 cases, 0ms)
1 passed, 0 failed
```

A passing property reports how many cases ran. When negative amounts must never reach `money` at all,
in production as well as in tests, say it with a [contract](contracts.md): `requires cents >= 0`.

## What can be generated

A parameter can have any type that has finite values:

| Type | Generated values |
|---|---|
| `int` | mostly -100 to 100, often 0, 1 and -1, and sometimes the extremes (the largest and smallest `int`) |
| `float` | finite values: never `inf` or `nan` |
| `bool` | `true` and `false` |
| `str` | short strings, including separators such as spaces and commas, and non-ASCII characters |
| `list[T]`, `map[K, V]` | short collections of generated elements, often empty |
| `result[T]` | `ok` with a generated value, or `err` with a generated message |
| records, variants | every field generated; any case of a variant, with its value |

Generic records and variants work with their arguments written out, such as `Pair[int, str]`. A
recursive variant such as a tree is generated with a limited depth.

Two kinds of type cannot be generated, and are [E0602](../errors/E0602.md) at check time: a type that
contains a function type, and a type with no finite value, such as a record whose field is the record
itself.

```vo
prop "applying twice" (f: fn(int) -> int) {
    expect_eq(f(f(1)), f(f(1)));
}
```

```error E0602
```

```vo
record Chain {
    next: Chain;
}

prop "chains" (c: Chain) {
    expect(c == c);
}
```

```error E0602
```

Since small integers dominate, a property finds bugs at small values quickly, and bugs that need one
specific large value rarely. A rule that breaks only above 1000 may pass all 100 cases. Write those
boundaries as ordinary [tests](tests.md).

Strings deserve the same care. The generator includes separators on purpose, and a property often finds
the case where the data contains the character used to split it:

```vo-test
fn word_counts(text: str) -> map[str, int] {
    let counts = new map[str, int];
    for w in split(text, " ") {
        set counts[w] = get_or(counts, w, 0) + 1;
    }
    return counts;
}

prop "every word is counted once" (words: list[str]) {
    assume(len(words) > 0);
    let counts = word_counts(join(words, " "));
    var total = 0;
    for k in keys(counts) {
        set total = total + counts[k];
    }
    expect_eq(total, len(words));
}
```

```output
FAIL every word is counted once (27 cases, 0ms)
  main.vo:16:5: `expect_eq` failed: the values differ.
    expected: 1
    actual:   2
    counterexample: words = [" "]
    shrunk in 4 steps
0 passed, 1 failed
```

One "word" that is a space joins to `" "`, which splits into two empty words. Whether that is a bug in
`word_counts` or in the property is a decision for you; the property made it visible.

## Shrinking

The first failing input is often large and noisy. Before reporting it, `vo test` shrinks it: it tries
smaller versions (shorter lists and strings, numbers closer to zero, simpler variant cases, smaller
fields) and keeps any that still fails the same way, until no smaller version fails. The report shows the
result and how many steps it took.

```vo-test
# The elements of xs without repeats, in the order they first appear.
fn unique(xs: list[int]) -> list[int] {
    let out = new list[int];
    for x in xs {
        if len(out) == 0 or out[len(out) - 1] != x {
            push(out, x);
        }
    }
    return out;
}

fn count(xs: list[int], x: int) -> int {
    var n = 0;
    for y in xs {
        if y == x {
            set n = n + 1;
        }
    }
    return n;
}

prop "unique keeps every element once" (xs: list[int]) {
    let u = unique(xs);
    for x in xs {
        expect_eq(count(u, x), 1);
    }
}
```

```output
FAIL unique keeps every element once (25 cases, 0ms)
  main.vo:25:9: `expect_eq` failed: the values differ.
    expected: 1
    actual:   2
    counterexample: xs = [1, 0, 1]
    shrunk in 2 steps
0 passed, 1 failed
```

`[1, 0, 1]` says exactly what is wrong: `unique` only compares each element with the one before it, so
it removes neighbors that repeat but not repeats further apart.

"The same way" means the same kind of failure: a failing check stays a failing check, and a runtime
error stays the same runtime error. A property that hits a runtime error is reported as `ERROR`, with the
error, the stack and the shrunk input.

Shrinking reaches a small input, not always the smallest one. It changes one part of the input at a
time, so when the failure needs two parts to stay related (two equal elements, a price larger than a
discount), the values that remain can be larger than necessary. Large integers in particular may stop
some distance from the boundary that matters.

## Records, variants and `assume`

A property over a record or variant gets values of every field and every case. Here a line of an order
has a price, a quantity and a discount, and the property says no line costs less than nothing:

```vo-test
variant Discount {
    none;
    percent(int);
    fixed(int);
}

record Line {
    cents: int;
    quantity: int;
    discount: Discount;
}

# The price of a line in cents after its discount.
fn price(l: Line) -> int {
    let gross = l.cents * l.quantity;
    match l.discount {
        case none {
            return gross;
        }
        case percent(p) {
            return gross - gross * p / 100;
        }
        case fixed(c) {
            return gross - c;
        }
    }
}

# Lines a shop could see: prices and quantities up to a limit, discounts of 0 to 100 percent
# or of up to 10.00.
fn plausible(l: Line) -> bool {
    var ok_discount = true;
    match l.discount {
        case none {
        }
        case percent(p) {
            set ok_discount = p >= 0 and p <= 100;
        }
        case fixed(c) {
            set ok_discount = c >= 0 and c <= 1000;
        }
    }
    return ok_discount and l.cents >= 0 and l.cents <= 100000 and l.quantity >= 0 and l.quantity <= 1000;
}

prop "a price is never negative" (l: Line) {
    assume(plausible(l));
    expect(price(l) >= 0);
}
```

```output
FAIL a price is never negative (58 cases, 0ms)
  main.vo:48:5: `expect` failed: the condition is false.
    counterexample: l = new Line { cents: 0, quantity: 0, discount: Discount.fixed(1) }
    shrunk in 2 steps
0 passed, 1 failed
```

A fixed discount larger than the line's total makes the price negative. The counterexample is written
in vo syntax, so it can be pasted into an example test that keeps the bug fixed.

The bounds in `plausible` matter for a second reason: without them, the extremes of `int` make
`cents * quantity` overflow ([R0001](../errors/R0001.md)), and the property reports that instead.
Decide which inputs are meaningful and say so with `assume`; a helper function such as `plausible`
keeps the condition readable.

The rules for `assume`:

- It works only in a `prop` ([E0604](../errors/E0604.md)), not in a test or a function.
- A discarded input is not a case. The property needs 100 accepted inputs in at most 1,000 attempts;
  with fewer, it fails with `too many inputs were discarded by assume`. When that happens, the
  condition rejects most of what is generated: compute the input from what is generated instead, for
  example `let digit = x % 10;` (always between -9 and 9) rather than
  `assume(x > -10 and x < 10);`.

```vo
fn half(n: int) -> int {
    assume(n % 2 == 0);
    return n / 2;
}

fn main() uses console {
    print(str(half(4)));
}
```

```error E0604
```

## Determinism and `--seed`

The inputs come from a seed, 1 by default. The same file and the same seed always generate the same
inputs and give the same report, on any machine, so a failure seen once can be seen again. `--seed N`
picks other inputs:

```vo
fn unique(xs: list[int]) -> list[int] {
    let out = new list[int];
    for x in xs {
        if len(out) == 0 or out[len(out) - 1] != x {
            push(out, x);
        }
    }
    return out;
}

prop "unique leaves no element twice" (xs: list[int]) {
    let u = unique(xs);
    for i in 0..len(u) {
        for j in 0..i {
            expect(u[i] != u[j]);
        }
    }
}
```

```console
$ vo test main.vo
FAIL unique leaves no element twice (23 cases, 0ms)
  main.vo:15:13: `expect` failed: the condition is false.
    counterexample: xs = [0, -1, 0]
    shrunk in 3 steps
0 passed, 1 failed
$ vo test --seed 2 main.vo
FAIL unique leaves no element twice (21 cases, 0ms)
  main.vo:15:13: `expect` failed: the condition is false.
    counterexample: xs = [3, 0, 3]
    shrunk in 1 step
0 passed, 1 failed
```

Both runs find the bug in `unique` from the section on shrinking, through different inputs. The
inputs also depend on where the property stands among the file's tests and properties: adding a test
above it gives it different inputs, as a different seed would.

Each seed explores a different 100 inputs; a property that passes with several seeds has been tried on
more of them. When a different seed finds a failure, keep the counterexample as an example test, since
the next run goes back to seed 1.

Each case runs with fresh arguments, and with the default seed and clock for `random_int` and `now_ms`,
like a test (see [Determinism](determinism.md)). A property declares effects as a test does:
`prop "name" (x: int) uses console { ... }`. `--timeout` covers the whole property, all its cases together,
not each case.

## JSON output

With `vo test --json`, a property's `test` record adds `prop` (`true`), `cases`, `discarded`,
`shrinks` and `counterexample`, a list of `{name, value}` with each value as vo text. For the order
line above:

```json
{"v":1,"kind":"test","name":"a price is never negative","status":"fail","file":"main.vo","start":{"line":46,"col":1},"uses":"","ms":2,"prop":true,"cases":58,"discarded":366,"shrinks":2,"counterexample":[{"name":"l","value":"new Line { cents: 0, quantity: 0, discount: Discount.fixed(1) }"}],"failure":{"file":"main.vo","start":{"line":48,"col":5},"end":{"line":48,"col":26},"message":"`expect` failed: the condition is false."}}
```

## When to write a property

Properties work best for rules that are easy to state and hard to enumerate:

- **round trips**: parse what you print, decode what you encode;
- **invariants**: totals add up, a sorted list is in order, a price is never negative;
- **comparison with a simple version**: a fast function agrees with an obvious, slow one;
- **idempotence**: sorting, trimming or normalizing twice is the same as once.

Keep example [tests](tests.md) for specific cases and boundaries, and for documenting behavior a reader
should see. `@spec` works on properties exactly as on tests.

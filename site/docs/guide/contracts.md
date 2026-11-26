# Contracts

Contracts are `requires` and `ensures` conditions on a function, checked on every call and every return, which state what the function expects from its caller and what it promises in return.

## Why contracts

Some rules about a function are not about one input but about every call: the divisor is never zero, the
list is never empty, the result is never negative, the shares add up to the total. Comments state such
rules but nothing checks them. Tests check them only for the inputs a test happens to use.

A contract states the rule on the function itself, where the reader looks for it, and the runtime
checks it on every call, in `vo run` as in `vo test`. When it does not hold, the program stops at once
with a report that says which side broke it: the caller, by passing an argument the function does not
accept, or the function, by returning something it promised not to.

The rules are in [Contracts](../reference/declarations.md#contracts), part of the
[Declarations](../reference/declarations.md) section of the reference; the [Tests](../reference/tests.md)
section describes how they work with tests and properties.

## `requires` and `ensures`

Contract clauses come after the return type and the `uses` clause, each on its own line:

```text
fn name(params) -> type uses effects
    requires condition
    ensures condition {
    ...
}
```

- `requires COND` is a precondition. It sees the parameters and is checked on every call, before the
  body runs.
- `ensures COND` is a postcondition. It sees the parameters and `ret`, the value being returned, and is
  checked every time the function exits.
- A function can have any number of each, in any order. All of them must hold.

```vo
# The integer square root of n: the largest r with r * r <= n.
fn isqrt(n: int) -> int
    requires n >= 0
    ensures ret * ret <= n and (ret + 1) * (ret + 1) > n {
    var r = 0;
    while (r + 1) * (r + 1) <= n {
        set r = r + 1;
    }
    return r;
}

fn main() uses console {
    for n in [0, 1, 10, 99, 100] {
        print(str(n) + " -> " + str(isqrt(n)));
    }
}
```

```output
0 -> 0
1 -> 1
10 -> 3
99 -> 9
100 -> 10
```

The `ensures` line is a complete specification of the result: whatever loop or formula computes it, a
wrong answer stops the program at the `return` that produced it.

## When a contract fails

A false `requires` is runtime error [R0012](../errors/R0012.md). It is reported at the call, and the
stack starts at the caller, because the caller is the one that broke the contract:

```vo
# Add an item to a basket that holds at most three.
fn add(basket: list[str], item: str)
    requires len(basket) < 3 {
    push(basket, item);
}

fn main() uses console {
    let basket = new list[str];
    for item in ["tea", "milk", "bread", "jam"] {
        add(basket, item);
        print(join(basket, ", "));
    }
}
```

```output
tea
tea, milk
tea, milk, bread
```

The program prints three lines and then stops with exit status 70. On stderr:

```text
main.vo:10:9: runtime error[R0012]: Precondition `len(basket) < 3` of `add` does not hold.
  10 |         add(basket, item);
     |         ^^^^^^^^^^^^^^^^^
trace:
  at main (main.vo:10:9)
      basket: list[str] = ["tea", "milk", "bread"], item: str = "jam"
```

A false `ensures` is [R0013](../errors/R0013.md). It is reported at the exit that returned the wrong
value, inside the function, because the function broke its promise:

```vo
fn half(n: int) -> int
    ensures ret * 2 == n {
    return n / 2;
}

fn main() uses console {
    for n in [4, 7] {
        print(str(half(n)));
    }
}
```

```error R0013
```

```text
main.vo:3:5: runtime error[R0013]: Postcondition `ret * 2 == n` of `half` does not hold.
   3 |     return n / 2;
     |     ^^^^^^^^^^^^^
trace:
  at half (main.vo:3:5)
      n: int = 7
  at main (main.vo:8:19)
      n: int = 7
```

Both are runtime errors like any other: they are not `result` values, nothing can catch them, and with
`--json` they are `runtime_error` records with the code and the trace. In `vo test`, a test that breaks a
contract is reported as `ERROR`.

## The rules for conditions

**Conditions are `bool`.** `requires n` with an `int` `n` is [E0301](../errors/E0301.md).

**Conditions are pure.** A condition cannot print, read the clock, draw a random number or call a
function that does ([E0603](../errors/E0603.md)). Contracts run on every call, so a condition with an
effect would change what the program does. For the same reason, a contract never adds to or changes a
function's effects.

```vo
fn logged_positive(n: int) -> bool uses console {
    print("checking " + str(n));
    return n > 0;
}

fn reciprocal(n: int) -> float
    requires logged_positive(n) {
    return 1.0 / float(n);
}

fn main() uses console {
    print(str(reciprocal(4)));
}
```

```error E0603
```

A condition can call any pure function, including your own. That keeps a long condition readable:
`requires is_sorted(xs)`.

**`ret` exists only in `ensures` of a function with a return type.** In a function that returns
nothing there is no `ret` ([E0201](../errors/E0201.md)), and a parameter named `ret` conflicts with it
when the function has an `ensures` ([E0203](../errors/E0203.md)).

**Every exit is checked.** `ensures` is checked at each `return`, at the end of a function without a
return type, and when a `try` passes an error on. In a function returning a `result`, `ret` is the whole
result, `ok` or `err`, so a postcondition about the value must allow the error case:

```vo
# The quantity on an order line: a whole number, never zero.
fn quantity(text: str) -> result[int]
    ensures ret != ok(0) {
    let n = try parse_int(trim(text));
    if n == 0 {
        return err("a quantity cannot be zero");
    }
    return ok(n);
}

fn main() uses console {
    for text in ["3", " 12 ", "x", "0"] {
        match quantity(text) {
            case ok(n) {
                print("ok " + str(n));
            }
            case err(e) {
                print("error: " + e);
            }
        }
    }
}
```

```output
ok 3
ok 12
error: invalid int: x
error: a quantity cannot be zero
```

**Conditions see the current values.** An `ensures` reads the parameters as they are at the exit. A list,
map or record passed in is shared, so if the body changed it, the condition sees the change. There is
no way to refer to the value a parameter had on entry: when that matters, compute it in the body and
return it.

## Contracts through function values

A contract belongs to the function, not to the call: it is checked however the function is called,
directly, through a variable of function type, or by a builtin such as `map`, `filter` or `sort_by`:

```vo
fn score(n: int) -> int
    requires n >= 0 and n <= 10 {
    return n * 10;
}

fn main() uses console {
    print(join(map([1, 5, 10], fn(n) => str(score(n))), " "));
    let scores = map([3, 12], score);
    print(str(len(scores)));
}
```

```output
10 50 100
```

The second `map` calls `score(12)` and stops with [R0012](../errors/R0012.md) at the `map` call. A function
without contracts runs exactly as fast as before: there is no cost for the feature when it is not used.

## Contracts, tests and properties

Contracts and tests check different things and work best together.

- A **test** checks a function on inputs you choose, and documents what it does.
- A **contract** checks every call, with whatever inputs the program meets, in tests and in production.
- A **[property](properties.md)** generates inputs. Calling a function with a contract from a property
  turns the contract into a test over 100 inputs, with a shrunk counterexample when it fails.

Here the property contains no `expect` at all: the `ensures` is the check.

```vo-test
# Split an amount in cents into `ways` shares that differ by at most one cent.
fn shares(cents: int, ways: int) -> list[int]
    requires cents >= 0 and ways > 0
    ensures len(ret) == ways and sum(ret) == cents {
    let out = new list[int];
    for _ in 0..ways {
        push(out, cents / ways);
    }
    return out;
}

fn sum(xs: list[int]) -> int {
    var s = 0;
    for x in xs {
        set s = s + x;
    }
    return s;
}

test "three ways" {
    expect_eq(shares(900, 3), [300, 300, 300]);
}

prop "shares add up" (cents: int, ways: int) {
    assume(cents >= 0 and ways > 0 and ways < 50);
    let _ = shares(cents, ways);
}
```

```output
PASS three ways (0ms)
ERROR shares add up (1 case, 0ms)
  main.vo:9:5: runtime error[R0013]: Postcondition `len(ret) == ways and sum(ret) == cents` of `shares` does not hold.
    at shares (main.vo:9:5)
        cents: int = 1, ways: int = 2, out: list[int] = [0, 0]
    at prop "shares add up" (main.vo:26:13)
        cents: int = 1, ways: int = 2
    counterexample: cents = 1
    counterexample: ways = 2
    shrunk in 2 steps
1 passed, 0 failed, 1 errored
```

The example test passed, because 900 divides by 3. The property found that one cent split two ways
loses the cent: the remainder must go somewhere.

```vo-test
fn shares(cents: int, ways: int) -> list[int]
    requires cents >= 0 and ways > 0
    ensures len(ret) == ways and sum(ret) == cents {
    let out = new list[int];
    for i in 0..ways {
        if i < cents % ways {
            push(out, cents / ways + 1);
        } else {
            push(out, cents / ways);
        }
    }
    return out;
}

fn sum(xs: list[int]) -> int {
    var s = 0;
    for x in xs {
        set s = s + x;
    }
    return s;
}

test "a remainder goes to the first shares" {
    expect_eq(shares(1000, 3), [334, 333, 333]);
}

prop "shares add up" (cents: int, ways: int) {
    assume(cents >= 0 and ways > 0 and ways < 50);
    let _ = shares(cents, ways);
}
```

```output
PASS a remainder goes to the first shares (0ms)
PASS shares add up (100 cases, 0ms)
2 passed, 0 failed
```

## When to use a contract

Use a `requires` for a condition the caller must meet and can check: a range, a non-empty list, a
positive divisor. It replaces the comment "n must be positive" with a check that cannot go stale.

Use an `ensures` for what the function promises in every case: the result is in range, sorted, adds up,
has the right length. It is often the shortest exact description of a function.

Prefer a `result` when the failure comes from outside the program, such as user input, a file or the
network: the caller then handles it as data (see [Results and errors](results-and-errors.md)). A
contract failure is a bug in the program, and it stops the program.

Keep conditions cheap. They run on every call: a condition that walks a long list doubles the cost of a
function that would otherwise be quick.

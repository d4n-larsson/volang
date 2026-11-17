# Tests

Tests are `test` blocks written next to the code they check, run by `vo test` and reported one line each, with both values shown when a check fails.

## Why tests are part of the language

In vo a test is an item of the file, like a function. There is no framework to install, no naming
convention to follow and no runner to configure: `vo test main.vo` finds every `test` block in the file
and runs it. `vo run` ignores tests, so a program and its tests can live in the same file.

Tests are checked like the rest of the program. A test declares its effects, so a test without
`uses file.read(...)` cannot read a file, and a test of pure code needs no setup and no cleanup. Every
test gets the same seed and the same clock, so a test that passes once passes every time.

The rules are in the [Tests](../reference/tests.md) section of the reference.

## Writing a test

A test is `test "name" { ... }`: a name in a string, then statements. Two builtins check behavior:

- `expect(cond)` fails the test when `cond` is `false`.
- `expect_eq(actual, expected)` fails it when the two values differ, and reports both.

```vo-test
# The price of an order line in cents, after a discount in whole percent.
fn line_total(unit_cents: int, quantity: int, discount_pct: int) -> int {
    let gross = unit_cents * quantity;
    return gross - gross * discount_pct / 100;
}

test "no discount" {
    expect_eq(line_total(250, 4, 0), 1000);
}

test "ten percent off" {
    expect_eq(line_total(250, 4, 10), 900);
    expect(line_total(250, 4, 10) < line_total(250, 4, 0));
}
```

```output
PASS no discount (0ms)
PASS ten percent off (0ms)
2 passed, 0 failed
```

Each test is one line, `PASS`, `FAIL`, `ERROR` or `TIMEOUT`, with its name and how long it took. The
last line counts them. A file with tests needs no `main`: the example above has none.

The rules:

- Test names must be unique in a file ([E0202](../errors/E0202.md)).
- `expect` and `expect_eq` work only inside a test (or a [property](properties.md)); anywhere else they
  are [E0410](../errors/E0410.md).
- `expect_eq` takes two values of the same type, of any type: numbers, strings, lists, maps, records,
  variants and results all compare by content, as `==` does.
- A test stops at its first failing check. The checks after it do not run.
- Tests cannot use `try`, because a test does not return a `result`
  ([E0406](../errors/E0406.md)). Compare whole results instead, `expect_eq(parse_int("3"), ok(3))`, or
  `match` on them.

## Reading a failure

A failing check prints its location under the `FAIL` line. For `expect_eq` it also prints both values
in vo syntax, so a string shows its quotes and a result shows whether it was `ok` or `err`. A test that
hits a runtime error is `ERROR`, with the error, the stack and the values of the locals, and the other
tests still run:

```vo-test
# Cents as units with two decimals: 1250 is "12.50".
fn money(cents: int) -> str {
    return str(cents / 100) + "." + str(cents % 100);
}

test "money shows two decimals" {
    expect_eq(money(1250), "12.50");
    expect_eq(money(1205), "12.05");
}

test "parses a quantity" {
    expect_eq(parse_int("12"), ok(12));
    expect_eq(parse_int(" 12"), ok(12));
}

test "the second line" {
    let lines = split("only one", "\n");
    expect_eq(lines[1], "");
}
```

```output
FAIL money shows two decimals (0ms)
  main.vo:8:5: `expect_eq` failed: the values differ.
    expected: "12.05"
    actual:   "12.5"
FAIL parses a quantity (0ms)
  main.vo:13:5: `expect_eq` failed: the values differ.
    expected: ok(12)
    actual:   err("invalid int:  12")
ERROR the second line (0ms)
  main.vo:18:15: runtime error[R0003]: Index 1 is out of bounds for length 1.
    at test "the second line" (main.vo:18:15)
        lines: list[str] = ["only one"]
0 passed, 2 failed, 1 errored
```

Three different findings: `money` drops the leading zero of the cents, `parse_int` does not trim
spaces (that is its documented behavior: call `trim` first), and the third test indexes past the end of
a list ([R0003](../errors/R0003.md)). The first check of `money shows two decimals` passed; the report
names the line of the one that failed.

A failing `expect` on a comparison shows each side that is not a literal, with its value; for
`expect(a and b)` it shows the part that was false. On larger values `expect_eq` also shows where they
first differ, `first difference at [412].name: expected "bob", actual "bab"`, so a long list of records
does not have to be compared by eye. Prefer `expect_eq` whenever there is an expected value.

What a test prints is kept with its report. It is shown under a test that did not pass and dropped for
a passing one, so a long run stays short:

```vo-test
fn clamp(x: int, hi: int) -> int {
    if x > hi {
        return hi;
    }
    return x;
}

test "clamps" uses console {
    let limit = 5;
    print("checking");
    expect(clamp(7, limit) < limit);
}
```

```output
FAIL clamps (0ms)
  main.vo:11:5: `expect` failed: `clamp(7, limit) < limit` is false.
    clamp(7, limit): 5
    limit:           5
  output:
    checking
0 passed, 1 failed
```

## Effects in tests

A test has its own effect clause, written like a function's: `test "name" uses console { ... }`. It
lists everything the test does, including what the functions it calls declare. A test that calls a
printing function without declaring `console` does not check
([E0501](../errors/E0501.md)), and its fix adds the missing effect:

```vo
fn greet(name: str) uses console {
    print("hi " + name);
}

test "greets" {
    greet("ana");
    expect(true);
}
```

```error E0501
```

A test's clause is that test's grant, as `main`'s clause is the grant of `vo run`: a test that declares
`file.read("fixtures/")` can read under `fixtures/` and nowhere else
([R0010](../errors/R0010.md) at run time otherwise). The rules for scopes are in
[Effects](effects.md).

Randomness and time are fixed in tests: `vo test` runs every test with seed 1 and a clock that starts
at 0, so a test that uses `random_int` or `now_ms` sees the same values on every run:

```vo-test
fn roll() -> int uses random {
    return random_int(1, 6);
}

fn stamp() -> int uses clock {
    return now_ms();
}

test "a roll is a die face" uses random {
    let r = roll();
    expect(r >= 1 and r <= 6);
    expect_eq(r, 3);
}

test "the clock starts at zero" uses clock {
    expect_eq(stamp(), 0);
    expect_eq(stamp(), 1);
}
```

```output
PASS a roll is a die face (0ms)
PASS the clock starts at zero (0ms)
2 passed, 0 failed
```

`--seed N` and `--clock-ms N` change the starting values for every test of a run; see
[Determinism](determinism.md).

## Running tests

`vo test PATH...` takes files or directories. For a file, it runs every test and property in that file;
a test in an imported file runs when that file is tested. For a directory, it tests every `.vo` file
under it, each as the entry of its own program. Tests run in the order they are written.

`--filter TEXT` runs only the tests whose name contains `TEXT`:

```vo
fn word_count(text: str) -> int {
    var n = 0;
    for w in split(text, " ") {
        if w != "" {
            set n = n + 1;
        }
    }
    return n;
}

test "count words" {
    expect_eq(word_count("to be or not"), 4);
}

test "count words with double spaces" {
    expect_eq(word_count("to  be"), 2);
}

test "count no words" {
    expect_eq(word_count(""), 0);
}
```

```console
$ vo test --filter spaces main.vo
PASS count words with double spaces (0ms)
1 passed, 0 failed
$ vo test --filter count main.vo
PASS count words (0ms)
PASS count words with double spaces (0ms)
PASS count no words (0ms)
3 passed, 0 failed
```

Every test has an identifier, `FILE::NAME`. `--list` checks the program and lists the tests with their
identifiers, `@spec` IDs, effects and the comment above each, without running anything; `--test ID`
then runs exactly one, where `--filter` would also run every name that contains the text:

```vo
# Word counting ignores repeated spaces.
fn word_count(text: str) -> int {
    var n = 0;
    for w in split(text, " ") {
        if w != "" {
            set n = n + 1;
        }
    }
    return n;
}

# A plain sentence.
test "count" {
    expect_eq(word_count("to be or not"), 4);
}

test "count spaces" {
    expect_eq(word_count("to  be"), 2);
}
```

```console
$ vo test --list main.vo
main.vo::count (test, main.vo:13:1)
  A plain sentence.
main.vo::count spaces (test, main.vo:17:1)
2 tests
$ vo test --test main.vo::count main.vo
PASS count (0ms)
1 passed, 0 failed
```

`--spec ID` runs the tests that name a requirement (see below), and `--diff FILE` the tests that a
change can affect, as [vo impact](../tools/impact.md) finds them. These options combine; an
identifier that names no test is [T0011](../errors/T0011.md). Tests share the working directory and run
in the listed order, so a test can see files that an earlier one wrote.

Other options:

- `--timeout SEC` limits each test (default 10 seconds); a test that runs out is `TIMEOUT`.
- `--seed N`, `--clock-ms N` and `--max-memory MiB` work as for `vo run`.
- `--trace FILE` writes a trace of every call, with a `test` record before each test (see
  [Traces and replay](../reference/traces-and-replay.md)).

The exit status is 0 when every test passed and 1 when any did not. Check errors stop the run before
any test starts (exit status 65). The full list of options is on the [vo test](../tools/test.md) page.

### JSON output

With `--json`, each test is one `test` record and the run ends with a `summary`. A failing record has a
`cause` (`check_failed`, `error_in_code`, `error_in_test`, `hole`, `timeout`, ...: who is likely at
fault) and a `failure` with the location and, for `expect_eq`, both values as text. What the test printed
is in `output`, so every line of the stream is JSON. Here is
`vo test --json --filter money main.vo` on the failing example above:

```json
{"v":1,"kind":"test","id":"main.vo::money shows two decimals","name":"money shows two decimals","status":"fail","file":"main.vo","start":{"line":6,"col":1},"uses":"","ms":0,"cause":"check_failed","failure":{"file":"main.vo","start":{"line":8,"col":5},"end":{"line":8,"col":36},"message":"`expect_eq` failed: the values differ.","expected":"\"12.05\"","actual":"\"12.5\""}}
{"v":1,"kind":"summary","command":"test","errors":0,"warnings":0,"truncated":0,"files":1,"passed":0,"failed":1,"errored":0,"timed_out":0,"selected":1}
```

`status` is `pass`, `fail`, `error` or `timeout`; an `error` record's `failure` adds the runtime error's
`code` and `trace`. All record kinds are described in [Machine-readable
output](../reference/machine-readable-output.md).

## Tracing tests to requirements

`@spec("ID", ...)` on the line right above a test names the requirements that test proves. An ID is
letters, digits, `-` and `_`, with parts separated by `/`: `"FR-003"`, `"US1/AC2"`. The annotation
changes nothing about how the test runs. It changes the report:

- a failing test's line ends with its IDs, so a failure says which requirement is broken;
- the run ends with one line per ID, before the totals;
- `vo query specs main.vo` lists every ID with the tests that name it.

```vo
record Ticket {
    title: str;
    severity: int;
}

# The most urgent ticket first: severity 1 before severity 3.
fn most_urgent(tickets: list[Ticket]) -> Ticket
    requires len(tickets) > 0 {
    var best = tickets[0];
    for t in tickets {
        if t.severity < best.severity {
            set best = t;
        }
    }
    return best;
}

fn label(t: Ticket) -> str {
    return "[S" + str(t.severity) + "] " + t.title;
}

@spec("FR-001")
test "the most severe ticket comes first" {
    let a = new Ticket { title: "printer jam", severity: 3 };
    let b = new Ticket { title: "site down", severity: 1 };
    expect_eq(most_urgent([a, b]), b);
}

@spec("FR-001", "FR-002")
test "ties keep the first ticket" {
    let a = new Ticket { title: "disk full", severity: 2 };
    let b = new Ticket { title: "cert expiring", severity: 2 };
    expect_eq(most_urgent([a, b]).title, "disk full");
}

@spec("FR-003")
test "labels show the severity" {
    expect_eq(label(new Ticket { title: "site down", severity: 1 }), "S1 site down");
}
```

```console
$ vo test main.vo
PASS the most severe ticket comes first (0ms)
PASS ties keep the first ticket (0ms)
FAIL labels show the severity (0ms) [FR-003]
  main.vo:38:5: `expect_eq` failed: the values differ.
    expected: "S1 site down"
    actual:   "[S1] site down"
spec FR-001: 2 tests, all passing
spec FR-002: 1 test, all passing
spec FR-003: 1 test, 1 not passing
2 passed, 1 failed
$ vo query specs main.vo
FR-001  main.vo:23:1 test "the most severe ticket comes first"
FR-001  main.vo:30:1 test "ties keep the first ticket"
FR-002  main.vo:30:1 test "ties keep the first ticket"
FR-003  main.vo:37:1 test "labels show the severity"
3 requirement IDs
```

With `--json`, each test record carries its IDs in `spec`, and the per-ID lines are `spec_result`
records (`id`, `tests`, `passed`). `@spec` works the same on [properties](properties.md). It is the only
annotation in vo. The [help-desk example](../examples/helpdesk.md) uses it throughout.

## What the tests never ran

`--coverage` lists, after the results, every function, branch, `match` case and loop body of your own
files that no test ran (the standard library and packages are left out), then the totals. An `if`
without `else` counts its "condition was false" path too:

```vo
fn shipping(cents: int) -> int {
    if cents >= 5000 {
        return 0;
    }
    return 495;
}

test "small order pays shipping" {
    expect_eq(shipping(1200), 495);
}
```

```console
$ vo test --coverage main.vo
PASS small order pays shipping (0ms)
not run: main.vo:2:22 then in shipping
coverage: 1 of 1 function, 1 of 2 branches
1 passed, 0 failed
```

## Would the tests catch a mistake?

Code that ran is not code whose result was checked. `--mutate` first runs the tests, which must all
pass ([T0012](../errors/T0012.md)). It then changes the code one small step at a time (`>=` to `>`, `+`
to `-`, `and` to `or`, `true` to `false`, dropping a `not`, a number `n` to `n + 1`) and runs the tests
that reach the change. A change that no test notices is a check you are missing:

```vo
fn is_adult(age: int) -> bool {
    return age >= 18;
}

test "adults" {
    expect(is_adult(30));
}

test "children" {
    expect(not is_adult(10));
}
```

```console
$ vo test --mutate main.vo
PASS adults (0ms)
PASS children (0ms)
SURVIVED main.vo:2:12 in is_adult: `age >= 18` -> `age > 18` (2 tests ran)
SURVIVED main.vo:2:19 in is_adult: `18` -> `19` (2 tests ran)
mutation: 2 changes, 0 caught, 2 survived, 0 not tried
survivors are candidates: a change can behave exactly like the original
2 passed, 0 failed
```

Nobody tested the age of 18: add `expect(is_adult(18))` and `expect(not is_adult(17))`, and both changes
are caught. A change that makes a test fail, stop with an error or run far longer counts as caught.
Some changes cannot be told apart from the original by any test, so a survivor is a question to look
at, not a failure: the exit status stays 0. `--max-mutants N` (default 500) caps the changes and
`--diff FILE` limits them to the functions a change touches.

Two warnings point at tests that cannot fail: [W0602](../errors/W0602.md) for a test without
`expect` or `expect_eq`, and [W0603](../errors/W0603.md) for a check that compares literals with
literals.

## Where tests go

Put tests in the file whose code they test. Tests of pure functions are the cheapest kind: they need no
effects, no files and no setup. This is one reason to keep logic in pure functions and the effects at
the edges, in `main` and a few functions it calls; see [Effects](effects.md).

When a function has a rule that must hold for every input, not just a few examples, write a
[property](properties.md). When it has conditions that must hold on every call, in tests and in
production, write a [contract](contracts.md).

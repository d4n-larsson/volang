# Quickstart: Validating Spec 005

How to show that holes, properties and contracts work end to end. The exact report formats are in [contracts/reports.md](contracts/reports.md). The rules are in [contracts/language.md](contracts/language.md).

## Prerequisites

```sh
make            # build/vo
make test       # the full suite, now including tests/cases/{hole,prop,contract}
```

## 1. Holes: check and run an unfinished program

```vo
# main.vo
fn parse(line: str) -> int {
    todo("parse the header");
}

fn main() uses console {
    print("start");
    print(str(parse("a,b")));
}
```

- `bin/vo check main.vo`:
  - exit 0;
  - one W0601 naming the expected type `int` and `line: str` in scope;
  - no W0201 for `line`;
  - the summary ends with `(1 hole)`.
- `bin/vo check --json main.vo`: the diagnostic has `"expected":"int"` and `"scope":[{"name":"line","type":"str"}]`, and the summary has `"holes":1`.
- `bin/vo run main.vo`:
  - prints `start`;
  - then R0011 `Hole reached: parse the header.` at line 2, with a trace through `main`;
  - exit 70.
- Change `fn parse` to `let x = todo("x");` inside `main`: the result is E0601, exit 65.

## 2. Properties: a pass, a shrunk failure, determinism

```vo
fn dedupe(xs: list[int]) -> list[int] {
    let out = new list[int];
    for x in xs {
        var seen = false;
        for y in out {
            if y == x {
                set seen = true;
            }
        }
        if not seen and len(out) < 3 {
            push(out, x);
        }
    }
    return out;
}

prop "dedupe keeps every value" (xs: list[int]) {
    let d = dedupe(xs);
    for x in xs {
        var found = false;
        for y in d {
            if y == x {
                set found = true;
            }
        }
        expect(found);
    }
}
```

- `bin/vo test main.vo`:
  - `FAIL dedupe keeps every value`, with a counterexample of 4 distinct small ints, such as `xs = [0, 1, 2, 3]` or something no larger;
  - exit 1.
- Run it 3 times: byte-identical output.
- `--seed 7` gives possibly different cases, still identical across runs.
- Remove `and len(out) < 3`: `PASS dedupe keeps every value (100 cases, Nms)`.

## 3. Contracts

Use the `isqrt` example from [contracts/language.md](contracts/language.md) with this `main`:

```vo
fn main() uses console {
    print(str(isqrt(10)));
    print(str(isqrt(-1)));
}
```

- `bin/vo run`:
  - prints `3`;
  - then R0012 ``Precondition `n >= 0` of `isqrt` does not hold.``, with the span at `isqrt(-1)` on main's line;
  - exit 70.
- Break the loop (`<` instead of `<=`): the run gives R0013 at `return r;`, quoting the `ensures` condition.
- `map([4, 9], isqrt)` still checks the contracts. A `requires` violation through `map` points at the `map(...)` call.
- A `requires` that calls `print` gives E0603.

## 4. Speed: no cost without contracts

```sh
make baseline-004                              # builds the spec 004 binary at 71375e5 into build-004/vo
python3 tests/speed.py --baseline build-004/vo  # each program within 5% of the 004 binary
```

## 5. Evaluation (SC-001, SC-002)

```sh
python3 tests/prop_eval.py
```

This prints one row per task h01–h10: mutants, how many the visible cases catch (always 0), how many the properties catch, and the total.
- It exits 0 when the properties catch at least 70% overall and at least one per task.
- The reference solution must pass every property three times byte-identically.

## 6. Reference and catalog

```sh
python3 tests/ref_examples.py   # new examples run
python3 tests/catalog.py        # W0601, E0601–E0604, R0011–R0013 each have a case
```

The reference stays under 15,000 tokens. Its growth is at most 2,000 tokens and is stated in the change.

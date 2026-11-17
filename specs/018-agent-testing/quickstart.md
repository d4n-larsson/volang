# Quickstart: validating spec 018

Prerequisites: `make` (builds `bin/vo`), Python 3 for the test scripts. All commands run from the
repository root. Expected outputs are given by shape. The exact text is in
[contracts/cli.md](contracts/cli.md).

## 1. List and select (US1)

```sh
mkdir -p /tmp/q18 && cd /tmp/q18
cat > main.vo <<'EOF'
# A plain positive number.
@spec("FR-001")
test "parses" {
    expect_eq(parse_int("21"), ok(21));
}

test "parses negative" {
    expect_eq(parse_int("-2"), ok(-2));
}
EOF
vo test --list main.vo             # two entries: main.vo::parses (with its comment, spec FR-001) and main.vo::parses negative; "2 tests"
vo test --test 'main.vo::parses' main.vo     # exactly one PASS line
vo test --spec FR-001 main.vo                # the same test
vo test --test 'main.vo::parse' main.vo; echo $?   # T0011, "Closest: `main.vo::parses`", 64
git diff --no-index /dev/null main.vo | vo test --diff - main.vo   # the tests vo impact lists for that diff
```

`vo test --list --json` gives `test_entry` records; a test that writes a file leaves it unwritten.

## 2. Explained failures (US2)

Write a test with `expect(len(xs) < limit)` that fails, one with `expect_eq` on two lists of records
that differ in one field, and one that prints and fails:
- Text: operand lines, `first difference at [i].field`, and an `output:` block under the failure only.
- `--json`: `cause`, `failure.operands`, `failure.difference` and `output`, and every stdout line parses
  as JSON (`vo test --json main.vo | python3 -c 'import json,sys; [json.loads(l) for l in sys.stdin]'`).

Each cause has a golden case in `tests/cases/test/cause_*`.

## 3. Coverage (US3)

```sh
vo test --coverage tests/cases/coverage/kinds.vo
```

Expect one `not run:` line per known gap (function, else, if_false, case, loop) and
`coverage: X of Y functions, A of B branches`. The output is identical on a second run and on ARM64 and RISC-V:

```sh
make ARCH=aarch64 test
make ARCH=riscv64 test
```

## 4. Mutation testing (US4)

```sh
cat > adult.vo <<'EOF'
fn is_adult(age: int) -> bool {
    return age >= 18;
}

test "adults" {
    expect(is_adult(30));
}

test "children" {
    expect(not is_adult(10));
}
EOF
vo test --mutate adult.vo          # SURVIVED adult.vo:2:12 in is_adult: `age >= 18` -> `age > 18`; exit 0
```

Add `test "eighteen" { expect(is_adult(18)); }` and run again: that change is caught (`0 survived`
for it). A failing test makes `--mutate` stop with T0012 and exit 1.

Static warnings: `test "t" { print("x"); }` gives W0602, and `expect_eq(3, 3)` gives W0603, in `vo check`.

## 5. Scale and gates

```sh
python3 tests/test_scale.py      # SC-001: 200 tests / 20 files listed < 1 s, every id selects exactly one; SC-004 coverage overhead <= 1.5x
python3 tests/mutate_eval.py     # SC-005: each fixture's boundary change survives weak tests, is caught by the full ones; examples < 60 s each
make test && make gate           # everything, including 20 repeated runs of the new cases
make instrumented
make ARCH=aarch64 test
make ARCH=riscv64 test
```

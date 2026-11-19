# Quickstart: validating String Formatting

Prerequisites: `make` (builds `build/vo`), Python 3 for the differential test. Expected outputs for each
example are in [contracts/language.md](contracts/language.md); diagnostics in
[contracts/diagnostics.md](contracts/diagnostics.md).

## 1. Formatting works (Stories 1 and 4)

```sh
cat > /tmp/f.vo <<'EOF'
fn main() uses console {
    print(format("{:<12}|{:>8.2f}", "coffee", 4.5));
    print(format("{:.2f} {:.2f} {:.0f}", 2.675, -0.001, 2.5));
    print(format("{:,} {:,.2f} {:+} {:05} {:x} {:08b}", 1234567, 1234567.891, 5, -42, 255, 5));
    print(format("{{}} {} {}", 1, true));
}
EOF
bin/vo run /tmp/f.vo
```

Expected:

```text
coffee      |    4.50
2.67 -0.00 2
1,234,567 1,234,567.89 +5 -0042 ff 00000101
{} 1 true
```

## 2. Mistakes are check errors with fixes (Stories 2 and 3)

```sh
cat > /tmp/g.vo <<'EOF'
fn main() uses console {
    let n = 3;
    let p = 2.5;
    print(format("{} and {}", n));
    print(format("{:.2f}", n));
    print(format("%d items, %.2f each", n, p));
    print(format("{0} {1}", n, p));
    print(f"n is {n}");
}
EOF
bin/vo check --json /tmp/g.vo
```

Expected: five diagnostics in one run, exit 65: E0316 (no fix), E0318 (fix `float(n)`), E0316 (fix
`"{} items, {:.2f} each"`), E0317 (fix `"{} {}"`), E0120 (fix `format("n is {}", n)`). Applying the fixes
(`tests/apply_fixes.py` does this for the golden cases) leaves only the first, which needs a decision.

## 3. Suites

```sh
make quick AREA=format          # golden cases in tests/cases/format, fixes, catalog, reference
python3 tests/format_diff.py    # SC-001: 100,000 values against Python
make test                       # once, to complete the change
make ARCH=aarch64 && VO_BIN=build-aarch64/vo python3 tests/format_diff.py --cross   # and riscv64
```

## 4. Reference and docs

```sh
bin/vo doc format               # the builtin row: signature and notes
python3 tests/ref_examples.py   # the new example runs; budget ≤ 15,000 tokens, growth ≤ 350 (SC-005)
bin/vo explain E0317
```

## 5. Speed (SC-004)

1,000,000 iterations of `format("{} {}", a, b)` against `str(a) + " " + str(b)`, and of
`format("{:>10}", i)` against `align_right(str(i), 10)`: median of 5 runs each, `format` not slower.
`{:.2f}` has no equivalent today; it is timed for the record (R3's fast path is added only if needed).

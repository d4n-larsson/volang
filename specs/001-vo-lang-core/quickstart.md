# Quickstart: Build and Validate vo-lang v1

## Prerequisites (x86_64 Ubuntu host)

```sh
sudo apt install binutils-riscv64-linux-gnu qemu-user
python3 --version   # 3.12+, used only by the test scripts
```

## Build

```sh
make            # writes build/vo (static RV64 ELF)
bin/vo version  # expect: vo 0.1.0 schema 1
```

`bin/vo` runs `build/vo` through `qemu-riscv64` when the host is not RV64. All commands below go through it.

## Full validation

```sh
make test       # golden cases + reference examples + catalog coverage
python3 tests/fuzz.py --iterations 5000
tests/run.sh --repeat 100 run/      # determinism (SC-006)
```

Each of these exits 0 when it passes.

## Scenarios by user story

Exit statuses and output records are defined in [contracts/cli.md](contracts/cli.md) and [contracts/output-schema.md](contracts/output-schema.md).

### US1: Write and run

```sh
cat > /tmp/sum.vo <<'EOF'
fn main() -> result[void] {
    var total = 0;
    var line = read_line();
    while true {
        match line {
            case ok(text) {
                set total = total + try parse_int(trim(text));
            }
            case err(_) {
                break;
            }
        }
        set line = read_line();
    }
    print(str(total));
    return ok();
}
EOF
printf '1\n2\n39\n' | bin/vo run /tmp/sum.vo   # prints 42, exit 0
```

Golden cases: `tests/cases/run/` includes `int_overflow.vo` and `int_div.vo` (`MIN/-1`, `/0`), `exit_status.vo` (expects 3), `div_zero.vo` (expects status 70 and an R0002 `runtime_error` that includes a trace), and `args.vo`.

### US2: Diagnostics

```sh
bin/vo check --json tests/cases/check/three_errors.vo
```

Expect three `diagnostic` records in order, then a `summary` with `"errors":3`, and exit status 65. In `tests/cases/check/typo_name.vo`, the E0201 record's fix replaces `totl` with `total`. Applying it and checking again should exit 0.

### US3: Reference

```sh
bin/vo reference | wc -w        # ≤ ~11,500 words, which is ~15k tokens (SC-002)
python3 tests/ref_examples.py   # every vo/output example pair passes (FR-030)
bin/vo explain E0401            # explanation, example, fixed version
```

### US4: Tests

```sh
bin/vo test --json tests/cases/test/two_pass_one_fail.vo
```

Expect 2 `pass` records, 1 `fail` record whose `failure` has `expected` and `actual`, and exit status 1. Running `tests/cases/test/infinite.vo --timeout 1` should produce one `timeout` result while the remaining tests still run.

### US5: Formatting

```sh
cp tests/cases/fmt/messy.vo /tmp/m.vo
bin/vo fmt /tmp/m.vo && bin/vo fmt --check /tmp/m.vo   # second command exits 0
diff /tmp/m.vo tests/cases/fmt/messy.expected.vo        # no output
bin/vo fmt tests/cases/fmt/broken.vo                    # exit 65, file unchanged
```

### US6: Multiple files

```sh
bin/vo run tests/cases/multi/app/main.vo        # imports from geom.vo and text/util.vo
bin/vo check --json tests/cases/multi/cycle/a.vo   # E0206 listing the a → b → a chain
```

## Agent benchmark (SC-001, SC-008)

```sh
bench/run_agent.sh --lang vo     # attempts per task → bench/results/vo.jsonl
bench/run_agent.sh --lang python # baseline
```

Pass criteria: at least 70% of tasks succeed on the first attempt and at least 95% within three, and the mean number of attempts for vo is lower than for Python.

# Quickstart: checking std.decimal

```sh
make
make quick AREA=decimal           # golden cases, doc examples, tests/decimals.py
python3 tests/decimals.py          # groups arith, round, text, alloc, speed
python3 tests/ref_examples.py     # the reference: under 15,000 tokens
bin/vo test examples/expenses/report.vo
make test                         # once, at the end
```

By hand:

```text
import std.decimal.Rounding;
import std.decimal.add;
import std.decimal.decimal_text;
import std.decimal.grouped_text;
import std.decimal.mul;
import std.decimal.parse_decimal;
import std.decimal.round_to;
import std.decimal.split_evenly;

fn main() -> result[void] uses console {
    let net = try parse_decimal("1999.90");
    let vat = round_to(mul(net, try parse_decimal("0.25")), 2, Rounding.half_even);
    print(grouped_text(add(net, vat)));
    for part in try split_evenly(add(net, vat), 3) {
        print(decimal_text(part));
    }
    return ok();
}
```

Expected: `2,499.88`, then `833.30`, `833.29`, `833.29`.

Other architectures: `make ARCH=aarch64`, then
`VO_BIN=$PWD/build-aarch64/vo tests/run.sh tests/cases/run` and `VO_BIN=... python3 tests/decimals.py`;
the same for `riscv64`.

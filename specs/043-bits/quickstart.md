# Quickstart: checking bits

```sh
make
make quick AREA=bits              # golden cases and tests/bits.py
python3 tests/bits.py             # groups ops, algorithms, literals, speed
python3 tests/ref_examples.py     # the reference: at most 14,900 tokens
make test                         # once, at the end
```

By hand:

```text
import std.bits.bit_and;
import std.bits.bit_xor;
import std.bits.shift_right_logical;
import std.bits.wrapping_mul;
import std.bytes.from_text;

fn fnv1a(data: bytes) -> int {
    var h = 0xCBF29CE484222325;
    for i in 0..len(data) {
        set h = wrapping_mul(bit_xor(h, data[i]), 0x100000001B3);
    }
    return h;
}

fn main() uses console {
    print(format("{:x}", shift_right_logical(fnv1a(from_text("hello")), 32)));
    print(str(bit_and(0b1100, 0b1010)));
}
```

prints `a430d846` and `8`. Expected outcomes are the acceptance scenarios of [spec.md](spec.md); the
functions and literals are in [contracts/bits.md](contracts/bits.md).

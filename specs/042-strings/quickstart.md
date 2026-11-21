# Quickstart: checking the strings feature

```sh
make
make quick AREA=front            # lexer, parser, formatter cases
make quick AREA=text             # runtime text cases
python3 tests/strings.py         # case tables, exponent corpus, for-in over text
python3 tests/grammars.py        # the three grammars against the toolchain
python3 tests/ref_examples.py    # reference examples and the token budget
make test                        # once, at the end
```

By hand:

```text
fn main() uses console {
    let pattern = `\d+"x"`;
    print(str(len(pattern)));
    print(str("a\u{e5}\r" == "a" + chr(229) + chr(13)));
    for c in "héj" {
        print(c);
    }
    print(upper("åäö") + " " + str(1.5e3));
}
```

prints `7`, `true`, `h`, `é`, `j`, `ÅÄÖ 1500.0`. `vo fmt` leaves it as it is; with `` `plain` `` in
place of the pattern it rewrites that to `"plain"`, and `vo check` reports W0102 for it.

Expected outcomes are the acceptance scenarios of [spec.md](spec.md); the forms and codes are in
[contracts/strings.md](contracts/strings.md).

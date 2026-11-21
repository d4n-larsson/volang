# Quickstart: checking package download

```sh
make
make quick AREA=packages
python3 tests/package_fetch.py                  # all groups; --group corpus|hostile|fetch|restore|effects|quiet|fuzz
VO_BIN=$PWD/build-aarch64/vo python3 tests/package_fetch.py --group corpus   # after make ARCH=aarch64
make test                                       # once, at the end
```

By hand, with a real forge (needs the network and the machine's trust store):

```sh
mkdir try && cd try && printf 'fn main() {\n}\n' > main.vo
vo package main.vo demo https://github.com/OWNER/REPO/archive/refs/tags/v1.0.0.tar.gz
cat vo.packages; ls packages/demo
rm -r packages && vo package --restore main.vo
```

Expected outcomes are the acceptance scenarios of [spec.md](spec.md); the command's forms, output and
codes are in [contracts/package.md](contracts/package.md).

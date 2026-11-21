# Quickstart: checking streaming

```sh
make
make quick AREA=stream                 # golden cases and tests/stream.py
python3 tests/stream.py                # all groups; --group files|http|children|replay|sweep
VO_STREAM=small VO_BIN=$PWD/build-aarch64/vo python3 tests/stream.py   # after make ARCH=aarch64
VO_STREAM=small VO_BIN=$PWD/build-riscv64/vo python3 tests/stream.py   # after make ARCH=riscv64
make test                              # once, at the end
```

By hand, in an empty directory:

```sh
seq 1 5000000 > numbers.txt            # 39 MB, one number per line
```

```text
import std.fs.each_line;

record Total {
    sum: int;
}

fn main() uses console, file.read("./") {
    let total = new Total { sum: 0 };
    let done = each_line("numbers.txt", fn(line: str) -> result[bool] {
        set total.sum = total.sum + try parse_int(line);
        return ok(true);
    });
    match done {
        case ok() {
            print(str(total.sum));
        }
        case err(m) {
            print(m);
        }
    }
}
```

`vo run --max-memory 16 --mem-stats main.vo` prints `12500002500000` with a peak far below the file's
size. Expected outcomes are the acceptance scenarios of [spec.md](spec.md); signatures and errors are
in [contracts/streaming.md](contracts/streaming.md).

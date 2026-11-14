# Quickstart: Validating Spec 007

How to show that the library, `vo doc` and packages work end to end. Signatures are in [contracts/library.md](contracts/library.md); commands in [contracts/cli.md](contracts/cli.md); the manifest in [contracts/packages.md](contracts/packages.md).

## Prerequisites

```sh
make                 # build/vo, with the std/*.vo files embedded
make test            # now including tests/cases/{doc,json,time,text,random,http,packages}
make baseline        # build-base/vo from spec 006 (1ff166a)
```

## 1. Look an API up, then use it

```sh
bin/vo doc                       # builtins and modules, one line each
bin/vo doc std.json              # every item of the module with its signature
bin/vo doc std.json.text_field   # one item, description and example
bin/vo doc std.jsn               # T0010, "did you mean std.json"
```

```vo
import std.json.parse_json;
import std.json.text_field;

fn main() -> result[void] uses console, input {
    let doc = try parse_json(read_all());
    print(try text_field(doc, "name"));
    return ok();
}
```

`echo '{"name": "ana", "age": 3}' | bin/vo run main.vo` prints `ana`; `echo '{"name": 3}' | ...` prints `error: field \`name\` is not text` and exits 1.

## 2. JSON corpus and round trips

`python3 tests/json_corpus.py`: every valid document parses to the expected value and survives `write` then `parse`; every invalid one returns `err` with a line and column.

## 3. Time

`python3 tests/time_check.py`: 10,000 generated times agree with Python's `datetime` in both directions, years 1–9999.

## 4. HTTP against the test server

`python3 tests/steps.py tests/cases/http`: the step runner starts `tests/http_server.py` and runs programs that `get` and `post` to it (plain, chunked, 404, redirect, refused, slow, too large), one whose host is outside its `net` scope (R0010), and one recorded and then replayed with the server stopped.

## 5. Packages

```sh
mkdir -p shared/money && printf 'fn cents(n: int) -> str {\n    return str(n / 100) + "." + str(n % 100);\n}\n' > shared/money/format.vo
bin/vo package main.vo money shared/money       # added money shared/money sha256:...
# main.vo: import money.format.cents;
bin/vo run main.vo
echo '# changed' >> shared/money/format.vo
bin/vo check main.vo                            # E0701 with both hashes; nothing runs
bin/vo package main.vo money shared/money       # updated money: sha256:... -> sha256:...
```

## 6. Evidence

```sh
python3 tests/doc_conformance.py   # every documented item: as documented checks, each mistake is rejected
python3 tests/stdlib_eval.py       # the task programs written from vo doc pass their tests
python3 tests/speed.py --baseline build-base/vo   # programs without the library: within 5%
```

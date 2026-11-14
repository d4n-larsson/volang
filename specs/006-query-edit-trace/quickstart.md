# Quickstart: Validating Spec 006

How to show that queries, edits, traces, frame values and replay work end to end. Formats are in [contracts/reports.md](contracts/reports.md), [contracts/trace.md](contracts/trace.md) and [contracts/recording.md](contracts/recording.md); the command line in [contracts/cli.md](contracts/cli.md).

## Prerequisites

```sh
make              # build/vo
make test         # the full suite, now including tests/cases/{query,edit,trace,replay}
make baseline     # build-base/vo from spec 005 (99b5d3f), for the speed check
```

## 1. Queries on a multi-file program

Use `tests/cases/query/shop/` (three files: `main.vo` imports `parse_line` from `io.vo`; `report.vo` passes it to `map`; a local and a string are also called `parse_line`).

- `bin/vo query callers tests/cases/query/shop/main.vo parse_line`: three lines (two `call`, one `value`), each with its enclosing function; the local and the string do not appear; exit 0.
- `bin/vo query refs ... Item.name`: the field declaration, the record literals and the field accesses; not the local `name`.
- `bin/vo query type-at ... main.vo:12:9`: `total: int (declared at main.vo:11:9)`.
- `bin/vo query refs ... parse_lin`: T0004 with "did you mean `parse_line`", exit 64.
- `--json` on each: `ref` / `type_at` records and a `query` summary.

## 2. Impact of a diff

```sh
cp -r tests/cases/query/shop /tmp/shop && cd /tmp/shop
# change the body of tax() in pricing.vo, then:
diff -u ../shop-orig/pricing.vo pricing.vo > change.diff   # or: git diff > change.diff
bin/vo impact main.vo --diff change.diff
```

- Lists `changed tax`, `affected subtotal via tax`, `affected checkout via subtotal`, and exactly the tests that reach `tax`.
- Edit `pricing.vo` again without regenerating the diff: T0007, exit 64.

## 3. Structural edits

- `bin/vo edit rename --dry-run main.vo add_item add_product`: lists the definition, the import and every call; the string `"add_item"` is untouched; nothing written.
- Without `--dry-run`: files changed; `bin/vo check main.vo` is clean with no effects-record change; the program's output on its inputs is unchanged.
- `bin/vo edit rename main.vo count total` where `total` is a local in the same function: T0006 naming the existing `total`, exit 65; `git diff` shows nothing.
- `bin/vo edit add-field main.vo Item discount: int = 0`: the declaration gets `discount: int;`, every `new Item { ... }` ends with `, discount: 0 }`; check clean.
- `bin/vo edit add-field main.vo Item discount: int = "x"`: T0006 (E0301 at every literal), nothing written.

## 4. Traces

```vo
fn fact(n: int) -> int {
    if n == 0 {
        return 1;
    }
    return n * fact(n - 1);
}

fn main() uses console {
    print(str(fact(2)));
}
```

- `bin/vo run --trace t.jsonl main.vo`: prints `2`; `t.jsonl` has `call main`, `call fact n=2`, `call fact n=1`, `call fact n=0`, three `return`s with `1`, `1`, `2`, `return main`, `end` (contracts/trace.md). Run twice: `cmp` shows the files identical.
- `--trace-limit 3`: three records, then `end` with `omitted` > 0; the output is unchanged.
- `tests/speed.py --baseline build-base/vo`: every program within 5% (no `--trace`).

## 5. Values in runtime errors

```vo
fn last(xs: list[int]) -> int {
    let i = len(xs);
    return xs[i];
}

fn main() uses console {
    print(str(last([1, 2, 3])));
}
```

- `bin/vo run main.vo`: R0003, and under `at last (...)` the line `xs: list[int] = [1, 2, 3], i: int = 3`; exit 70.
- `--json`: each trace entry has `locals`.

## 6. Record and replay

```sh
echo 'a' > in.txt
LIMIT=3 bin/vo run --record r.jsonl main.vo -- in.txt < input.txt   # a program that reads env, args, a file, stdin and now_ms, then fails
rm in.txt; unset LIMIT
bin/vo replay r.jsonl          # same stdout, stderr and exit status as the recorded run
```

- Fix the bug and replay again: the note `the program differs from the recorded one`, then the fixed behaviour on the same inputs.
- Change the program to read a different file first: R0014 naming both paths, exit 70.

## 7. The refactor stand-in (SC-001, SC-002)

```sh
python3 tests/refactor_eval.py
```

Prints per task: edits applied by `vo edit` (all must check and keep every visible and hidden output), edits broken by text replacement, queries matching the hand-written lists (all must), and wrong text-search hits. Exit 1 if any `vo edit` result or query fails.

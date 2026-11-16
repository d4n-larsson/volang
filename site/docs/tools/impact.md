# vo impact

`vo impact` reads a unified diff of a program and lists the functions it changed, added or removed, every function that calls them, directly or not, and the tests to run again.

## Synopsis

```text
vo impact [--json] ENTRY --diff FILE
```

`ENTRY` is the program's entry file; only the files it imports are considered. `FILE` is a unified diff,
as written by `git diff` or `diff -u`, of the program's **current** files: every context line and added
line must match the files as they are now. `--diff -` reads the diff from stdin
(`git diff | vo impact main.vo --diff -`). Paths in the diff may carry git's `a/` and `b/` prefixes.

The answer follows calls, not text: a function is affected when it calls a changed function, or calls a
function that does, and a test when its body does. See the reference's
[Querying and editing programs](../reference/querying-and-editing-programs.md).

## Options

| Option | Meaning |
|---|---|
| `--json` | One `impact` record per function or test, then a `summary` record. |
| `--diff FILE` | The diff to read (required); `-` for stdin. |

`--diff` only works with `impact` ([T0001](../errors/T0001.md) with any other command).

## Output

Text, on stdout, one line per item, in this order: changed, added, removed, then affected functions and
tests in breadth-first order from the changes, then the counts. The line forms are:

```text
changed tax (pricing.vo:7)
added helper (main.vo:1)
removed old_helper
affected total via tax (pricing.vo:11)
test "total adds tax" via total (main.vo:11)
ignored notes.txt
1 changed, 1 added, 1 removed, 1 affected, 1 test
```

`via` names the function whose call brought the item in. A file in the diff that is not part of the
program is `ignored`. With `--json`, each line is an `impact` record (`name`, `role`: `changed`, `added`,
`removed`, `affected`, `test` or `ignored`, `file`, `start`, and `via`), and a `summary` counts them:

```json
{"v":1,"kind":"impact","name":"total","role":"affected","file":"pricing.vo","start":{"line":11,"col":1},"via":"tax"}
{"v":1,"kind":"summary","command":"impact","errors":0,"warnings":0,"truncated":0,"files":2,"changed":1,"added":0,"removed":0,"affected":2,"tests":1}
```

A removed function has no `file` or `start`; an ignored file has only its path as `name`.

## Exit statuses

| Status | When |
|---|---|
| 0 | Answered, even when the program has check errors. |
| 64 | Usage error, such as no `--diff` ([T0001](../errors/T0001.md)), or a diff that is not unified or does not match the current files ([T0007](../errors/T0007.md)). |
| 65 | A file has a syntax error that stops the checker. |
| 66 | The entry file or the diff cannot be read ([T0002](../errors/T0002.md)). |

When the diff does not match, make it again from the current files:
``error[T0007]: The diff does not match `pricing.vo` as it is now (line 7): make it again from the current files.``

## Example

The tax rate in `pricing.vo` changed from a quarter to a fifth. The program as it is now:

```vo
# file: pricing.vo
export record Item {
    name: str;
    price: int;
}

export fn tax(amount: int) -> int {
    return amount / 5;
}

export fn total(items: list[Item]) -> int {
    var sum = 0;
    for item in items {
        set sum = sum + item.price + tax(item.price);
    }
    return sum;
}
```

```vo
# file: main.vo
import pricing.Item;
import pricing.total;

fn main() uses console {
    let items = [new Item { name: "pen", price: 4 }, new Item { name: "book", price: 20 }];
    print("total " + str(total(items)));
}

@spec("FR-001")
test "total adds tax" {
    expect_eq(total([new Item { name: "pen", price: 4 }]), 5);
}
```

and the change, as `git diff -U1 > change.diff` wrote it (text before the first `---` line is ignored):

```vo
# file: change.diff
--- a/pricing.vo
+++ b/pricing.vo
@@ -7,3 +7,3 @@
 export fn tax(amount: int) -> int {
-    return amount / 4;
+    return amount / 5;
 }
```

```console
$ vo impact main.vo --diff change.diff
changed tax (pricing.vo:7)
affected total via tax (pricing.vo:11)
affected main via total (main.vo:5)
test "total adds tax" via total (main.vo:11)
1 changed, 0 added, 0 removed, 2 affected, 1 test
$ vo impact --json main.vo --diff change.diff
{"v":1,"kind":"impact","name":"tax","role":"changed","file":"pricing.vo","start":{"line":7,"col":1}}
{"v":1,"kind":"impact","name":"total","role":"affected","file":"pricing.vo","start":{"line":11,"col":1},"via":"tax"}
{"v":1,"kind":"impact","name":"main","role":"affected","file":"main.vo","start":{"line":5,"col":1},"via":"total"}
{"v":1,"kind":"impact","name":"test \"total adds tax\"","role":"test","file":"main.vo","start":{"line":11,"col":1},"via":"total"}
{"v":1,"kind":"summary","command":"impact","errors":0,"warnings":0,"truncated":0,"files":2,"changed":1,"added":0,"removed":0,"affected":2,"tests":1}
```

The test to run again is `total adds tax`: `vo test --filter "total adds tax" main.vo` (see [vo test](test.md)).
That test now fails, which is the point of running it. To list the references of a name rather than the
reach of a change, use [vo query](query.md).

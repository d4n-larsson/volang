# vo effects

`vo effects` shows what a program may touch, and with `--update` writes the program's effects record, `vo.effects`, printing exactly what changed for a reviewer to approve.

## Synopsis

```text
vo effects [--json] [--update] FILE
```

`FILE` is the program's entry file. The record `vo.effects` lists every function and test of the program
with its effects, one line each, sorted:

```text
# vo effects record: update with `vo effects --update`
main.vo:main uses console, file.write("out/")
main.vo:save uses file.write("out/")
main.vo:test "nothing to save" pure
```

It lives next to the entry file, or in an ancestor directory. [vo check](check.md) compares the program
with it: a function or test that gained an effect or a wider scope is [E0504](../errors/E0504.md), fewer
effects are [W0502](../errors/W0502.md), a rename with unchanged effects is [W0503](../errors/W0503.md), and a
program of several files without a record is [W0504](../errors/W0504.md). Only `vo effects --update` changes
the record. Package functions appear as `@NAME/path.vo:fn`. The rules are in the reference's
[Effects](../reference/effects.md) section and the guide's [Effects](../guide/effects.md) chapter.

## Options

| Option | Meaning |
|---|---|
| `--json` | Report one JSON object per line (see [JSON output](json-output.md)). |
| `--update` | Write `vo.effects` (create it next to `FILE` if there is none) and print the changes. |

## Output

On stdout. Without `--update`, text: the grant of `main` and of each test (`main uses console, clock`,
`test "name" pure`), a blank line, the record as `--update` would write it, then the summary line. With
`--json`, one `capabilities` record per entry point:

```json
{"v":1,"kind":"capabilities","entry":"main","file":"main.vo","uses":"console, file.write(\"out/\"), clock"}
{"v":1,"kind":"capabilities","entry":"test \"nothing to save\"","file":"main.vo","uses":""}
{"v":1,"kind":"summary","command":"effects","errors":0,"warnings":0,"truncated":0,"files":1}
```

With `--update`, text: one line per function that changed, `+ FILE:FUNCTION: EFFECTS` for effects it
gained and `- FILE:FUNCTION: EFFECTS` for effects it dropped or narrowed, then the summary line; nothing else when the record was already up to date. With `--json`, one `effect_change`
record per change (`change` is `added`, `removed`, `widened`, `narrowed` or `renamed`; `via` lists the
calls that brought an effect in):

```json
{"v":1,"kind":"effect_change","change":"added","file":"main.vo","function":"main","effects":"clock","before":"uses console, file.write(\"out/\")","after":"uses console, file.write(\"out/\"), clock","via":[]}
```

A program with check errors prints its diagnostics, as `vo check` does, and writes nothing.

## Exit statuses

| Status | When |
|---|---|
| 0 | Printed, or the record written. |
| 64 | Usage error: no file, or more than one ([T0001](../errors/T0001.md)). |
| 65 | The program has check errors; nothing was written. |
| 66 | The file cannot be read ([T0002](../errors/T0002.md)). |

## Example

The record was written when `label` was pure; since then it started reading the clock, and so did `main`,
which calls it:

```vo
# file: lib.vo
export fn label(n: int) -> str uses clock {
    return "item " + str(n) + " at " + str(now_ms());
}
```

```vo
# file: main.vo
import lib;

fn main() uses console, clock {
    print(lib.label(1));
}
```

```vo
# file: vo.effects
# vo effects record: update with `vo effects --update`
lib.vo:label pure
main.vo:main uses console
```

`vo check` refuses the new effect, with a note on the call that brought it into `main`, until it is
approved. The update prints the two lines a reviewer needs to read:

```console
$ vo check main.vo
main.vo:4:4: error[E0504]: `main` gains clock; approve with `vo effects --update`.
   4 | fn main() uses console, clock {
     |    ^^^^
  note: main.vo:5:11: `main` calls `label`, which uses clock.
lib.vo:2:11: error[E0504]: `label` gains clock; approve with `vo effects --update`.
   2 | export fn label(n: int) -> str uses clock {
     |           ^^^^^
2 errors, 0 warnings
$ vo effects main.vo
main uses console, clock

lib.vo:label uses clock
main.vo:main uses console, clock
0 errors, 0 warnings
$ vo effects --update --json main.vo
{"v":1,"kind":"effect_change","change":"added","file":"lib.vo","function":"label","effects":"clock","before":"pure","after":"uses clock","via":[]}
{"v":1,"kind":"effect_change","change":"added","file":"main.vo","function":"main","effects":"clock","before":"uses console","after":"uses console, clock","via":[{"function":"main","calls":"label","file":"main.vo","start":{"line":5,"col":11}}]}
{"v":1,"kind":"summary","command":"effects","errors":0,"warnings":0,"truncated":0,"files":1}
$ vo check main.vo
0 errors, 0 warnings
$ vo effects --update main.vo
0 errors, 0 warnings
```

In text, the update would have printed `+ lib.vo:label: clock` and `+ main.vo:main: clock`. In a code
review, the diff of `vo.effects` shows the same two changes.

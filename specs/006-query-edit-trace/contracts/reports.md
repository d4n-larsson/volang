# Contract: Reports

Text and JSON output of the new commands, and the changes to existing reports. All JSON records start with `{"v":1,"kind":...}`, use the existing position and span shapes (`start`/`end`, 1-based `line`/`col`), and stay in schema 1: only kinds and fields are added. Values are rendered in vo syntax, shortened to 200 bytes with a trailing `...`.

## `vo query callers` / `vo query refs`

One record per reference, in file load order, then by position:

```json
{"v":1,"kind":"ref","target":"parse_line","role":"call","file":"io.vo","start":{"line":7,"col":15},"end":{"line":7,"col":25},"text":"parse_line","in":"load"}
```

- `role`: `declaration`, `call`, `value` (a function used as a value), `type` (a type annotation), `import`, `use` (anything else: a read, a field access, a record-literal field, a case in a pattern or constructor).
- `in`: the enclosing function, `test "name"`, `prop "name"`, or absent at top level (declarations, imports, field types).
- `callers` lists `call` and `value` entries only; `refs` lists all, the declaration first.

Text: one line per reference, `FILE:LINE:COL ROLE in ITEM: SOURCE LINE (trimmed)`:

```text
main.vo:9:17 call in main: let n = parse_line(line);
io.vo:7:15 call in load: push(out, parse_line(l));
report.vo:4:20 value in summary: return map(lines, parse_line);
3 references
```

## `vo query type-at`

```json
{"v":1,"kind":"type_at","file":"main.vo","start":{"line":12,"col":9},"end":{"line":12,"col":14},"text":"total","type":"int","name":"total","declaration":{"file":"main.vo","start":{"line":11,"col":9},"end":{"line":11,"col":14}}}
```

`name` and `declaration` only when the position is on a name. Text: `main.vo:12:9 total: int (declared at main.vo:11:9)`, or `main.vo:12:9 a + b: int` for an expression.

## `vo impact`

```json
{"v":1,"kind":"impact","name":"tax","role":"changed","file":"pricing.vo","start":{"line":3,"col":1}}
{"v":1,"kind":"impact","name":"subtotal","role":"affected","file":"cart.vo","start":{"line":10,"col":1},"via":"tax"}
{"v":1,"kind":"impact","name":"test \"checkout total\"","role":"test","file":"main.vo","start":{"line":40,"col":1},"via":"checkout"}
```

- `role`: `changed`, `added`, `removed` (no `file`/`start`), `affected`, `test`; `ignored` for a diff file outside the program (`name` = the diff path).
- `via`: the function it calls that brought it in (for `affected` and `test`).
- Order: changed, added, removed, then affected and test in breadth-first order from the changes.

Text: `changed tax (pricing.vo:3)`, `affected subtotal via tax (cart.vo:10)`, `test "checkout total" via checkout (main.vo:40)` (a test or property line starts with its name, which already says what it is), `removed NAME`, `ignored PATH`, then `1 changed, 0 added, 0 removed, 2 affected, 1 test`.

## `vo edit`

One record per change, then the summary:

```json
{"v":1,"kind":"edit","file":"catalog.vo","start":{"line":3,"col":4},"end":{"line":3,"col":12},"text":"add_item","replacement":"add_product"}
```

`file`/`start`/`end`/`replacement` are the existing fix shape; `text` is the source replaced. Text output: `catalog.vo:3:4 add_item -> add_product` (an insertion: `catalog.vo:12:62 + , discount: 0`, newlines shown as `\n`), then `renamed add_item to add_product: 5 changes in 3 files` (or `... (dry run, nothing written)`). The changes are listed after the files are written.

A refused edit prints the diagnostics the edit would introduce (spans mapped to the current text, fixes dropped), then T0006 (`toolchain_error`) with the reason, e.g. ``Renaming `count` to `total` would not check (1 new error): nothing was written.`` A bad name, a clash found before checking, an unwritable file or a broken program whose errors would change are T0006 with their own message:

```json
{"v":1,"kind":"toolchain_error","code":"T0006","name":"edit_refused","message":"Renaming `count` to `total` would not check: `total` is already declared in `main`.","related":[{"file":"main.vo","start":{"line":4,"col":9},"end":{"line":4,"col":14}}]}
```

## Summaries

The last record of `query`, `impact` and `edit`:

```json
{"v":1,"kind":"summary","command":"query","errors":0,"warnings":0,"truncated":0,"files":3,"results":3}
{"v":1,"kind":"summary","command":"impact","errors":0,"warnings":0,"truncated":0,"files":3,"changed":1,"added":0,"removed":0,"affected":2,"tests":1}
{"v":1,"kind":"summary","command":"edit","errors":0,"warnings":0,"truncated":0,"files":3,"changes":5,"changed":3,"written":true}
```

The members of the existing summary come first (`errors`/`warnings` count the program's check diagnostics, FR-005), then the command's own; with errors, the text output ends with `note: the program has N errors; answers cover the parts that check`.

## Runtime errors (changed)

Every `trace` entry gains `locals`, the parameters and locals in scope at that frame's position, in declaration order:

```json
"trace":[{"function":"last","file":"main.vo","start":{"line":2,"col":12},"locals":[{"name":"xs","type":"list[int]","value":"[1, 2, 3]"},{"name":"i","type":"int","value":"3"}]},{"function":"main","file":"main.vo","start":{"line":6,"col":15},"locals":[]}]
```

Text, one line under each `at` line (none when there are no locals):

```text
trace:
  at last (main.vo:2:12)
      xs: list[int] = [1, 2, 3], i: int = 3
  at main (main.vo:6:15)
```

Function values show as `<fn(int) -> int>`. In `vo test`, the `failure` of an `error` test gains the same `trace` array, and the text report prints the frames under the error line (`    at f (loc)`, values indented by eight spaces). A timeout (R0009) shows no values (they depend on the machine's speed), and a `timeout` test's report is unchanged.

## `vo replay`

When the program differs from the recorded one, before running, on stderr: text `note: the program differs from the recorded one (recorded main.vo)`; JSON `{"v":1,"kind":"replay","program":"main.vo","changed":true}`. Otherwise output is exactly the recorded run's.

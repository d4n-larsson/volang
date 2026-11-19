# Contract: CLI Changes (spec 027)

## `vo check`

After a check with no errors, the structure rules run on the program's own files. Exit status is
unchanged: 0 with warnings.

Summary, text: `0 errors, 2 warnings` as before; with accepted warnings `0 errors, 2 warnings (1 accepted)`.
Accepted warnings are not counted among the warnings.

Summary, JSON: the `summary` record gains `"accepted": N` when N > 0 (absent otherwise, so existing
outputs are unchanged). Schema 1, a field added.

## `vo test PATH...`

Each `PATH` names entry files as before (a file, or every `*.vo` under a directory). New: the tests and
properties of every program file an entry loads run, not only the entry's: the entry, then the files it
imports, in the order they are loaded; std and packages never. A file loaded by several entries runs once.

Unchanged: test identifiers (`FILE::NAME`), `--filter`, `--test ID`, `--spec ID`, `--diff`, `--list`,
`--coverage`, `--mutate`; they apply to the whole set. T0011 (no such test) considers the whole set.

`vo run` and `vo test` do not show structure warnings.

## `@allow`

```text
@allow("W0703", "one scanner, read top to bottom")
fn parse_number(...) -> ... {
```

On the line right above a `fn`, `test` or `prop`, beside any `@spec` lines. Two string arguments: a code
W0701–W0705 and a non-empty reason. For W0702, it stands above the file's first `fn`, `test` or `prop`. `vo fmt`
keeps annotations as it does `@spec`.

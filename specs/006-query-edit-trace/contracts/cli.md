# Contract: Command Line

New commands and options. Every existing command and option keeps its meaning. `--json` works with every new command.

```sh
vo query callers  [--json] ENTRY NAME
vo query refs     [--json] ENTRY TARGET
vo query type-at  [--json] ENTRY FILE:LINE:COL
vo impact         [--json] ENTRY --diff DIFF          # DIFF is a file, or - for stdin
vo edit rename    [--json] [--dry-run] ENTRY TARGET NEW_NAME
vo edit add-field [--json] [--dry-run] ENTRY RECORD FIELD: TYPE = DEFAULT
vo run    ... [--trace FILE] [--trace-limit N] [--record FILE] FILE [-- ARG...]
vo test   ... [--trace FILE] [--trace-limit N] PATH...
vo replay [--json] [--trace FILE] [--trace-limit N] [--timeout SEC] [--max-memory MiB] [--mem-stats] RECORDING
```

## Targets

| Form | Means | Example |
|---|---|---|
| `name` | a top-level function, record or variant, in any file of the program | `parse_line` |
| `Record.field` | a record field | `Item.price` |
| `Variant.case` | a variant case | `Shape.circle` |
| `FILE:LINE:COL` | the declaration or reference at that position: anything above, or a parameter, local, type parameter | `main.vo:14:9` |

- `callers` takes a function name or a builtin name (or a position on one).
- `LINE` and `COL` are 1-based; `COL` counts characters, as in diagnostics.
- No match: T0004 (with the closest name, if any). Several matches: T0005 listing them.

## Rules

- `ENTRY` is the program's entry file (the file `vo run` would be given). Only files it imports, directly or not, are searched or edited.
- `add-field`: the words after `RECORD` are joined with single spaces and read as `FIELD: TYPE = DEFAULT`, so `discount: int = 0` needs no quotes.
- `--dry-run` (edit only): print the changes and the verdict, write nothing.
- `--trace FILE`: write the trace (contracts/trace.md). `--trace-limit N`: records written before the rest are only counted (default 100000, 1 to 100000000). `--trace-limit` without `--trace` is T0001.
- `--record FILE` (`run` only): write a recording (contracts/recording.md).
- `--trace`/`--record` naming one of the program's source files, or both naming the same file: T0001. Whether the program later writes that file itself is decided by its effect grant; the toolchain does not predict it.
- `vo replay`: `--seed`, `--clock-ms`, `--record` and `-- ARG...` are T0001 (they come from the recording).
- Option order is free, as for existing commands.

## Exit statuses

| Command | 0 | Other |
|---|---|---|
| `query`, `impact` | answered (even if the program has check errors) | 64 bad target / usage (T0001, T0004, T0005); 64 diff does not apply (T0007); 65 a file has a syntax error that stops checking; 66 unreadable input (T0002) |
| `edit` | edit applied (or would be, with `--dry-run`) | 64 bad target / usage; 65 refused (T0006); 66 unreadable input |
| `run`/`test` with `--trace`/`--record` | unchanged | unchanged, plus 74 when the trace or recording cannot be written (T0009) |
| `replay` | as `vo run` of the recorded program | 66 unreadable recording (T0002); 64 invalid recording (T0008); 70 replay mismatch (R0014); 74 trace cannot be written (T0009) |

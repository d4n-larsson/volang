# Contract: `vo` Command Line

```
vo check     [--json] PATH...
vo run       [--json] [--timeout SEC] FILE [-- ARG...]
vo test      [--json] [--timeout SEC] [--filter TEXT] PATH...
vo fmt       [--json] [--check] PATH...
vo explain   [--json] CODE
vo reference [--json]
vo version   [--json]
```

- `PATH` is a `.vo` file or a directory. A directory means every `*.vo` file under it, recursively, in byte-wise sorted path order.
- Each file is checked as the entry of its own program. Imports resolve relative to the importing file's directory (contracts/grammar.md §Imports).
- `--timeout SEC` takes a whole number of seconds. The default is 10 and `0` disables the limit. For `test` it applies to each test separately.
- `--filter TEXT` runs only tests whose name contains `TEXT`.
- `fmt` rewrites files in place. With `--check` it changes nothing and lists the files that are not in canonical form.
- `explain` accepts `E####`, `W####`, `R####` and `T####` codes. It prints that code's section of the error catalog.
- `reference` prints the embedded language reference unchanged.
- `version` prints `vo <semver> schema 1`.
- `--json` is accepted by every command (FR-018). `explain`, `reference` and `version` then emit a single record each (contracts/output-schema.md).
- **Toolchain errors**: usage errors (T0001) and unreadable inputs (T0002) are `toolchain_error` records. They are written as JSONL whenever `--json` appears anywhere in argv, even if the argument parse itself fails, and in human form otherwise. They go to the command's report stream: stdout, or stderr for `run`.

## Where output goes

| Command | Toolchain reports (diagnostics, results, summary) | Program output |
|---|---|---|
| check, test, fmt, explain, reference, version | stdout | (test: the program's own stdout/stderr pass through) |
| run | stderr | stdout/stderr belong to the program |

With `--json`, every toolchain report is one JSON object per line that starts with `{"v":1,` (contracts/output-schema.md). A program's own output is never wrapped.

## Exit statuses (FR-019)

| Status | Meaning |
|---|---|
| 0 | Success |
| 0–63 | `run` only: the status the program passed to `exit()`. A normal return from `main` is 0. `main` returning `err` is 1 |
| 1 | `test`: at least one test did not pass. `fmt --check`: at least one file is not in canonical form |
| 64 | Usage error: bad flag, unknown command, unknown code (T0001) |
| 65 | Check errors: at least one error-severity diagnostic |
| 66 | An input file or directory could not be read (T0002) |
| 70 | Runtime error (any R code except R0009) |
| 124 | Timeout (R0009) |
| 125 | Internal toolchain error (T0003), which is always a bug in `vo` |

Warnings never change the exit status.

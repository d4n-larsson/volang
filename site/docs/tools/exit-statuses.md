# Exit statuses

Every `vo` command ends with an exit status that says what happened, so scripts and agents can act on it without reading the output.

## The statuses

| Status | Meaning | Commands |
|---|---|---|
| 0 | Success. Warnings never change the status. | all |
| 0–63 | The code the program passed to `exit()`. A normal end of `main` is 0. | `run`, `replay` |
| 1 | `main` returned `err` (its message is printed on stderr as `error: ...`). | `run`, `replay` |
| 1 | A test failed, errored or timed out. | `test` |
| 1 | A file is not in canonical form. | `fmt --check` |
| 64 | Usage error ([T0001](../errors/T0001.md)): unknown command or option, a missing or bad option value, the wrong number of arguments, an option given to a command it does not belong to, an unknown code for `explain`. | all |
| 64 | A query or edit target that does not exist ([T0004](../errors/T0004.md)) or is ambiguous ([T0005](../errors/T0005.md)). | `query`, `impact`, `edit` |
| 64 | A diff that is not unified or does not match the current files ([T0007](../errors/T0007.md)). | `impact` |
| 64 | A file that is not a complete recording ([T0008](../errors/T0008.md)). | `replay` |
| 64 | Nothing documented under that name ([T0010](../errors/T0010.md)). | `doc` |
| 65 | Check errors: at least one error-severity diagnostic, including effects that grew ([E0504](../errors/E0504.md)) and changed packages ([E0701](../errors/E0701.md)). Nothing ran or was written. | `check`, `run`, `test`, `fmt` (syntax errors), `effects`, `replay`, `doc`, `package`; `query`, `impact` and `edit` when a syntax error stops the checker |
| 65 | An edit was refused ([T0006](../errors/T0006.md)); no file changed. | `edit` |
| 66 | An input file or directory cannot be read ([T0002](../errors/T0002.md)): a program, a diff, a recording. A built executable whose program part is damaged ([T0014](../errors/T0014.md)). | all that read files; executables from `build` |
| 70 | A runtime error: any R code except R0009, for example [R0002](../errors/R0002.md) division by zero, [R0008](../errors/R0008.md) an `exit()` code outside 0–63, [R0010](../errors/R0010.md) an effect outside the grant, [R0014](../errors/R0014.md) a replay mismatch. | `run`, `replay` |
| 74 | A trace, a recording or a built executable cannot be written ([T0009](../errors/T0009.md)). | `run`, `test`, `replay`, `build` |
| 124 | Timeout ([R0009](../errors/R0009.md)): the program ran past `--timeout`. | `run`, `replay` |
| 125 | Internal error in `vo` ([T0003](../errors/T0003.md)): always a bug in the toolchain, never in the program. | all |

A run stopped by a signal (Ctrl-C, `kill`) ends with the shell's usual status for that signal, 128 plus
the signal number, and leaves an incomplete recording. A server (`std.http.serve`) is the exception: it
stops on Ctrl-C or SIGTERM and `serve` returns `ok()`, so `main` can end normally.

In `vo test`, a runtime error or a timeout belongs to the test (`ERROR`, `TIMEOUT`) and the command's
status is 1, not 70 or 124: the other tests still run.

The statuses are listed in the reference's [Command line](../reference/command-line.md) section, and the
codes in [Error codes](../reference/error-codes.md).

## By command

| Command | 0 | Other |
|---|---|---|
| [check](check.md) | no errors | 64, 65, 66 |
| [run](run.md) | `main` ended | 0–63 from `exit()`, 1 `main` returned `err`, 64, 65, 66, 70, 74, 124 |
| [test](test.md) | all tests passed | 1, 64, 65, 66, 74 |
| [fmt](fmt.md) | formatted, or canonical with `--check` | 1 with `--check`, 64, 65, 66 |
| [effects](effects.md) | printed or written | 64, 65, 66 |
| [explain](explain.md) | printed | 64 |
| [doc](doc.md) | printed | 64, 65 |
| [reference](reference.md), [version](version.md) | printed | 64 |
| [query](query.md) | answered, even with check errors | 64, 65, 66 |
| [impact](impact.md) | answered, even with check errors | 64, 65, 66 |
| [edit](edit.md) | written, or would be with `--dry-run` | 64, 65, 66 |
| [replay](replay.md) | as the replayed run | 64, 65, 66, 70, 74, 124, and the run's own status |
| [package](package.md) | manifest written | 64, 65 |

## Example

A program that chooses its own status:

```vo
fn main() uses console {
    let missing = 2;
    if missing > 0 {
        print(str(missing) + " files missing");
        exit(3);
    }
    print("all files present");
}
```

```output
2 files missing
```

`vo run main.vo` prints that line and exits with status 3; in a shell, `echo $?` shows it. `exit(64)`
would not be passed on: it is [R0008](../errors/R0008.md), status 70, so a program's own codes never
collide with the statuses `vo` uses from 64 up.

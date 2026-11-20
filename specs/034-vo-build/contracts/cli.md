# Contract: Command Line (spec 034)

## `vo build`

```sh
vo build [--json] [-o OUTPUT] [--base TOOLCHAIN] FILE
```

- Checks FILE's program as `vo check` does, including `vo.effects` (E0504 refuses). Errors: the
  diagnostics as `vo check` prints them (exit 65), nothing written.
- OUTPUT defaults to FILE's name without `.vo`, in the current directory. It is replaced if it exists.
- Success, on stdout: `built OUTPUT: N files, SIZE bytes` (exit 0); with `--json` one record
  `{"v":1,"kind":"build","output":"app","files":N,"bytes":SIZE}`.
- Refusals (T0001, exit 64, nothing written):
  - `--base` file unreadable, not an ELF executable, not this toolchain's version, or already a built
    program;
  - OUTPUT names one of the program's source files or the toolchain itself.
- FILE with no `main`: the check error `vo run` gives (exit 65), nothing written.
- An OUTPUT that cannot be written: T0009 `cannot_write_output` (exit 74; its catalog entry gains the
  build case), nothing left behind.

## A built executable

```sh
./app [--vo-OPTION [VALUE]]... [ARG]...
```

| Option | Meaning |
|---|---|
| `--vo-seed N`, `--vo-clock-ms N`, `--vo-max-memory MiB`, `--vo-mem-stats`, `--vo-trace FILE`, `--vo-trace-limit N`, `--vo-record FILE`, `--vo-ca-file FILE`, `--vo-workers N`, `--vo-json` | as the same `vo run` option |
| `--vo-timeout SEC` | as `vo run --timeout`; without it there is no timeout |
| `--vo-effects` | print the `main` line of `vo effects` (`--vo-json`: its `capabilities` record); run nothing |
| `--vo-replay FILE` | replay a recording with the embedded program, as `vo replay` |
| `--vo-version` | print the toolchain version, as `vo version` |

- Every argument not starting with `--vo-` reaches the program through `args()`, in order (`--`,
  `--help` and `--seed` included).
- An unknown `--vo-` option or a bad value: T0001 (exit 64) listing the options; the program does not
  run.
- Exit statuses, stdout and stderr as `vo run`: the program's status, 1 when `main` returns `err`, 70
  for a runtime error.
- A damaged program part: T0014 (exit 66), nothing runs.

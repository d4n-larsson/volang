# vo build

`vo build` turns a program into one executable file that runs on any Linux machine of its architecture, with no toolchain and no source files, and that states what it may touch.

## Synopsis

```text
vo build [--json] [-o OUTPUT] [--base TOOLCHAIN] FILE
```

`FILE` is the program's entry file, the one with `main`. The program is checked first, exactly as
[vo check](check.md) checks it, including its effects record: a program with errors, or one whose effects
grew since `vo.effects` was approved ([E0504](../errors/E0504.md)), is not built, so what was reviewed is
what ships. The output holds the toolchain's runtime, the standard library and every file the program
imports (its own files and its packages). Building the same program twice gives the same bytes.

```text
$ vo build main.vo -o notes
built notes: 3 files, 10331204 bytes
```

## Options

| Option | Meaning |
|---|---|
| `-o OUTPUT` | The file to write (default: the entry's name without `.vo`, in the current directory). An existing file is replaced. |
| `--base TOOLCHAIN` | Build for another architecture: `TOOLCHAIN` is the `vo` binary of the same version for that architecture. |
| `--json` | Diagnostics as JSON lines; on success one `build` record: `{"v":1,"kind":"build","output":"notes","files":3,"bytes":10331204}`. |

## Running the executable

```text
./notes [--vo-OPTION [VALUE]]... [ARG]...
```

The executable behaves as `vo run main.vo -- ARG...` does: the same output, exit statuses and runtime
reports, and the same enforcement of effects. Every argument that does not start with `--vo-` reaches the
program through `args()`, in order, `--help` and `--` included. Relative paths the program opens are
relative to the directory it is started in.

| Option | Meaning |
|---|---|
| `--vo-effects` | Print what `main` declares, for example `main uses console, file.read("data/")`, and run nothing. With `--vo-json`: a `capabilities` record. |
| `--vo-record FILE` | Record the run, as `vo run --record`. |
| `--vo-replay FILE` | Replay a recording with the program inside the executable, as [vo replay](replay.md); a recording made by `vo run` of the same sources works too. |
| `--vo-seed N`, `--vo-clock-ms N`, `--vo-max-memory MiB`, `--vo-mem-stats`, `--vo-trace FILE`, `--vo-trace-limit N`, `--vo-ca-file FILE`, `--vo-workers N`, `--vo-json` | As the same option of [vo run](run.md). |
| `--vo-timeout SEC` | A time limit. Unlike `vo run`, an executable has none by default. |
| `--vo-version` | Print the toolchain version it was built with. |

No option widens what the program may do: the grant is the one `main` declares.

## What the executable may touch

Whoever receives the file can ask it before running it:

```text
$ ./notes --vo-effects
main uses console, file.read("data/"), file.write("data/"), serve("localhost:8080")
```

The line is not documentation: a read outside `data/` stops the program with
[R0010](../errors/R0010.md), as under `vo run`.

## Other architectures

The executable is made from a toolchain binary, by default the one that runs the build. To build for
ARM64 or RISC-V on an x86-64 machine, give that architecture's `vo` of the same version:

```text
$ vo build main.vo -o notes-arm64 --base vo-aarch64
```

## A container

The executable is static and needs no files of its own, so an image can hold nothing else:

```dockerfile
FROM scratch
COPY notes /notes
ENTRYPOINT ["/notes"]
```

The executable reads its program from its own file through `/proc/self/exe`, which container runtimes
provide. HTTPS needs certificate authorities: copy a bundle into the image and name it with
`--vo-ca-file`.

## Limits

- The executable is the whole toolchain plus the program, about 10 MB; the program is checked again each
  time it starts, which takes milliseconds.
- An executable is not a toolchain: it has no `check` or `test`.
- A file whose program part was changed after the build does not start
  ([T0014](../errors/T0014.md)); one whose end was cut off has lost the part and starts as the plain
  toolchain, with a usage error.

## Exit statuses

`vo build`: 0 built; 64 a usage error ([T0001](../errors/T0001.md): a bad `--base`, an output that
is a source file); 65 check errors; 66 an entry file that cannot be read; 74 an output that cannot be
written ([T0009](../errors/T0009.md)). The executable: as [vo run](run.md), and 66 for T0014.

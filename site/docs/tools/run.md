# vo run

`vo run` checks a program and, if it has no errors, runs its `main` with the effects `main` declares as the grant.

## Synopsis

```text
vo run [--json] [--timeout SEC] [--seed N] [--clock-ms N] [--max-memory MiB] [--mem-stats]
       [--trace FILE] [--trace-limit N] [--record FILE] [--ca-file FILE] [--workers N] FILE [-- ARG...]
```

`FILE` is the program's entry file, the one that declares `main`. The words after `--` are the program's
arguments, returned by `args()`; options after `--` belong to the program, not to `vo`.

`run` checks first. Errors stop it before anything runs (and are printed with any warnings); warnings
alone are not printed and do not stop it (use [vo check](check.md) to see them). `main`'s `uses` clause is the root grant: any file, variable, host or
address outside it stops the program with [R0010](../errors/R0010.md) (see [Effects](../guide/effects.md)).

## Options

| Option | Meaning |
|---|---|
| `--json` | Write `vo`'s own reports (diagnostics, the runtime error, the memory report) as JSON lines. The program's output is never wrapped. |
| `--timeout SEC` | Stop the program after `SEC` whole seconds with [R0009](../errors/R0009.md), exit status 124. Default 10; `0` disables the limit. |
| `--seed N` | Fix `random_int` (and `std.random`): the same seed gives the same numbers. Without it, each run uses a new seed. |
| `--clock-ms N` | Make `now_ms` return `N`, `N+1`, `N+2`, ... instead of the wall clock. |
| `--max-memory MiB` | The limit on live data, 1 to 65536 MiB (default 512). Holding more is [R0006](../errors/R0006.md). |
| `--mem-stats` | After the run, report the peak live size, the total allocated and the number of collections (on stderr; a `memory` record with `--json`). |
| `--trace FILE` | Write a trace of every vo function call and return to `FILE` as JSON lines. |
| `--trace-limit N` | With `--trace`: write at most `N` records (1 to 100000000, default 100000) and only count the rest. |
| `--ca-file FILE` | Trust the certificate authorities in `FILE` (PEM) for HTTPS, in addition to the machine's trust store, for this run; repeatable. Read before the program starts: unreadable is [T0002](../errors/T0002.md), no certificate is [T0013](../errors/T0013.md). Nothing turns certificate verification off. |
| `--record FILE` | Save everything the program receives to `FILE`, so [vo replay](replay.md) can run it again exactly. |
| `--workers N` | Serve with `N` worker processes (1 to 64) in every `std.http.serve_workers` call, whatever the program declares. A program that serves with `serve` keeps answering one request at a time, with a note. |

`--seed` and `--clock-ms` take whole numbers from 0. A trace or recording path cannot be one of the
program's source files, and `--trace` and `--record` cannot name the same file
([T0001](../errors/T0001.md)). `--trace-limit` without `--trace` is T0001 too.

## Output

The program owns stdout and stderr. `vo`'s own reports go to **stderr**, so the program's stdout stays clean:

- If the program does not check, its diagnostics and a summary, as [vo check](check.md) prints them
  (`"command":"run"` in the JSON summary), and nothing runs.
- A runtime error prints its code, place, message and the call stack, innermost first, with each frame's
  parameters and the locals declared before the failing statement:

```text
main.vo:2:12: runtime error[R0002]: Division by zero.
   2 |     return total / count;
     |            ^^^^^^^^^^^^^
trace:
  at average (main.vo:2:12)
      total: int = 10, count: int = 0
  at main (main.vo:7:15)
```

With `--json` the same error is one `runtime_error` record:

```json
{"v":1,"kind":"runtime_error","code":"R0002","name":"division_by_zero","message":"Division by zero.","file":"main.vo","start":{"line":2,"col":12},"end":{"line":2,"col":25},"trace":[{"function":"average","file":"main.vo","start":{"line":2,"col":12},"locals":[{"name":"total","type":"int","value":"10"},{"name":"count","type":"int","value":"0"}]},{"function":"main","file":"main.vo","start":{"line":7,"col":15},"locals":[]}]}
```

That report comes from this program:

```vo
fn average(total: int, count: int) -> int {
    return total / count;
}

fn main() uses console {
    print("start");
    print(str(average(10, 0)));
}
```

```error R0002
```

- A timeout ([R0009](../errors/R0009.md)) shows the place and the frames but no values, since they depend
  on the machine's speed: `main.vo:4:5: runtime error[R0009]: Time limit of 1 s exceeded.`
- `main` returning `err(e)` prints `error: ` and the message on stderr, and the exit status is 1. This
  line is text even with `--json`.
- `--mem-stats` adds one line after the run (normal or not), or a `memory` record with `--json`. For the
  example program below:

```text
memory: peak live 432 B, allocated 432 B, 0 collections (limit 512 MiB)
```

```json
{"v":1,"kind":"memory","command":"run","peak_live":432,"allocated":432,"collections":0,"limit":536870912}
```

### Traces

`--trace FILE` writes one JSON line per event: `call` (the function, its call site, the depth and the
arguments by name), `return` (the value), `error` when a runtime error ends the run, and `end` (records
written, records omitted after `--trace-limit`, exit status). Builtins are not traced, but vo functions
they call are. Values are shortened to 200 bytes. The same program, input and options give the same
bytes. The record kinds are listed in [JSON output](json-output.md#traces); the rules are in the
reference's [Traces and replay](../reference/traces-and-replay.md).

### Recordings

`--record FILE` saves the arguments, standard input, environment variables read, file reads, the results
of file writes, HTTP requests and responses, the clock, the seed and the bytes `random_bytes` returned,
as JSON lines. The recording holds
the contents of the files and variables the program read: treat it like them. See [vo replay](replay.md).

## Exit statuses

| Status | When |
|---|---|
| 0 | `main` returned normally (or returned `ok`). |
| 0–63 | The program called `exit(code)`. A code outside 0–63 is [R0008](../errors/R0008.md). |
| 1 | `main` returned `err`. |
| 64 | Usage error ([T0001](../errors/T0001.md)): a bad option value, more than one file, no file. |
| 65 | The program has check errors; nothing ran. |
| 66 | The file cannot be read ([T0002](../errors/T0002.md)). |
| 70 | A runtime error (any R code except R0009). |
| 74 | The trace or recording cannot be written ([T0009](../errors/T0009.md)). |
| 124 | Timeout ([R0009](../errors/R0009.md)). |

## Example

```vo
fn roll(sides: int) -> int uses random {
    return random_int(1, sides);
}

fn main() uses console, clock, random {
    print("hello " + join(args(), " and "));
    print("rolled " + str(roll(6)));
    let start = now_ms();
    print("took " + str(now_ms() - start) + " ms");
}
```

With `--seed` and `--clock-ms` the run is the same every time; `--trace` and `--record` change nothing on
stdout:

```console
$ vo run --seed 7 --clock-ms 1000 main.vo -- Ada Grace
hello Ada and Grace
rolled 1
took 1 ms
$ vo run --seed 7 --clock-ms 1000 --trace trace.jsonl main.vo -- Ada Grace
hello Ada and Grace
rolled 1
took 1 ms
$ vo run --seed 7 --clock-ms 1000 --record session.jsonl main.vo -- Ada Grace
hello Ada and Grace
rolled 1
took 1 ms
$ vo replay session.jsonl
hello Ada and Grace
rolled 1
took 1 ms
$ vo run --seed 7 --clock-ms 1000 --mem-stats --json main.vo -- Ada
hello Ada
rolled 1
took 1 ms
```

(The `memory` record of the last command went to stderr.) `trace.jsonl` holds:

```json
{"v":1,"kind":"call","depth":1,"function":"main","args":[]}
{"v":1,"kind":"call","depth":2,"function":"roll","file":"main.vo","start":{"line":7,"col":27},"args":[{"name":"sides","value":"6"}]}
{"v":1,"kind":"return","depth":2,"function":"roll","value":"1"}
{"v":1,"kind":"return","depth":1,"function":"main"}
{"v":1,"kind":"end","records":4,"omitted":0,"status":0}
```

and `session.jsonl`:

```json
{"v":1,"kind":"recording","program":"main.vo","fingerprint":"e4cc7936e2fa8c75","seed":7,"clock_ms":1000,"args":["Ada","Grace"]}
{"v":1,"kind":"now_ms","value":1000}
{"v":1,"kind":"now_ms","value":1001}
{"v":1,"kind":"end","status":0}
```

The full command line, with every option and default, is in the reference's
[Command line](../reference/command-line.md).

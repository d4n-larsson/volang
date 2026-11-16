# vo replay

`vo replay` runs a program again on exactly what a recorded run received, its arguments, input, files, variables, HTTP traffic, clock and seed, without reading or writing anything outside.

## Synopsis

```text
vo replay [--json] [--timeout SEC] [--max-memory MiB] [--mem-stats] [--trace FILE] [--trace-limit N] RECORDING
```

`RECORDING` is a file written by `vo run --record FILE` (see [vo run](run.md)). It names the program's
entry file, relative to the directory `vo run` ran in, so replay from that same directory. The program is
loaded from its **current** source: that is the point, a fixed program can be replayed on the input that
made the old one fail.

- An unchanged program gives the same stdout, stderr and exit status as the recorded run (unless that run
  timed out: then the replay times out too, but how far it got depends on the machine's speed).
- A changed program runs with a note on stderr: `note: the program differs from the recorded one (recorded main.vo)`.
- Each input is taken from the recording in order. A program that asks for a different input (another
  file, variable or HTTP request: host, port, method or path) stops with [R0014](../errors/R0014.md).
  Reading less than was recorded is fine.
- Writes are not performed: `write_file` returns the recorded result. A server opens no port: it gets the
  recorded requests again, and each response that changed is a note.
- Effects are still checked against the grant; a read the recorded run was denied is denied again
  ([R0010](../errors/R0010.md)).

The rules are in the reference's [Traces and replay](../reference/traces-and-replay.md); the guide's
[Determinism](../guide/determinism.md) chapter puts them to use.

## Options

| Option | Meaning |
|---|---|
| `--json` | Write `vo`'s reports (the `replay` note, a runtime error, the memory report) as JSON lines on stderr. |
| `--timeout SEC` | The time limit in whole seconds (default 10, `0`: none), as for `vo run`. |
| `--max-memory MiB` | The live-data limit, 1 to 65536 MiB (default 512). |
| `--mem-stats` | Report memory use after the run, as for `vo run`. |
| `--trace FILE` | Write a trace of the replayed run (see [vo run](run.md#traces)). |
| `--trace-limit N` | With `--trace`: at most `N` records (default 100000). |

`--seed`, `--clock-ms` and program arguments after `--` come from the recording; giving them is
[T0001](../errors/T0001.md). `--record` only works with `vo run`.

## Output

The program's own stdout and stderr, as in the recorded run. `vo`'s reports go to stderr, as with
`vo run`. A changed program adds, with `--json`, a `replay` record; a changed server response a
`response_changed` record:

```json
{"v":1,"kind":"replay","program":"main.vo","changed":true}
{"v":1,"kind":"response_changed","request":"GET /x","recorded_status":200,"status":200,"recorded_body":"hi /x","body":"hello /x"}
```

A mismatch stops the program like any runtime error:

```text
note: the program differs from the recorded one (recorded main.vo)
main.vo:3:20: runtime error[R0014]: The program reads the file `other.txt`, but the recording has a read of `greeting.txt` next: replay the program that was recorded, or record this one.
   3 |     let text = try read_file("other.txt");
     |                    ^^^^^^^^^^^^^^^^^^^^^^
trace:
  at main (main.vo:3:20)
      name: str = "Ada"
```

The recording format, one JSON line per input, is in [JSON output](json-output.md#recordings).

## Exit statuses

| Status | When |
|---|---|
| any | The replayed run's own status, as for [vo run](run.md): 0–63, 1, 70, 124. |
| 64 | Usage error ([T0001](../errors/T0001.md)), or the file is not a complete recording ([T0008](../errors/T0008.md)), for example from a run killed by a signal. |
| 65 | The current program has check errors. |
| 66 | The recording cannot be read ([T0002](../errors/T0002.md)). |
| 70 | The program asked for an input that is not next in the recording ([R0014](../errors/R0014.md)), or another runtime error. |
| 74 | The trace cannot be written ([T0009](../errors/T0009.md)). |

## Example

```vo
fn main() uses console, clock, random {
    let names = args();
    let pick = random_int(0, len(names) - 1);
    print("started at " + str(now_ms()));
    print("today's winner: " + names[pick]);
}
```

Record a run, then replay it: no arguments, seed or clock are given to `replay`, and the output is the same:

```console
$ vo run --seed 3 --clock-ms 1000 --record session.jsonl main.vo -- Ada Grace Edsger
started at 1000
today's winner: Grace
$ vo replay session.jsonl
started at 1000
today's winner: Grace
$ vo replay --json --trace trace.jsonl session.jsonl
started at 1000
today's winner: Grace
```

`--seed` and `--clock-ms` are given here only so that this page shows the same output every time.
Without them, the recording keeps the seed and the clock readings the run actually used, and the replay
gives them back. A run with no `--seed` recorded:

```json
{"v":1,"kind":"recording","program":"main.vo","fingerprint":"d42389b00ce9ea09","seed":147710802127042,"clock_ms":null,"args":["Ada","Grace","Edsger"]}
{"v":1,"kind":"now_ms","value":1794834775649}
{"v":1,"kind":"end","status":0}
```

A recording contains the files and environment variables the program read: keep it as private as they are.

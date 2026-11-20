# Determinism

A vo program given the same inputs produces the same output every time, and the toolchain can fix the inputs that would change (the seed, the clock), record all the others, replay a run exactly, trace every call, and bound a run's time and memory.

## Why runs are deterministic

A bug that happens once and never again is the most expensive kind to fix. vo removes the usual sources
of variation, so that a run can be repeated:

- **There is no hidden input.** Everything a program receives from outside goes through an effect:
  arguments, standard input, files, environment variables, the network, the clock and random numbers.
  Pure code cannot reach any of them (see [Effects](effects.md)). SQL is held to the same rule:
  `CURRENT_TIMESTAMP`, `'now'` and `random()` are refused in `std.sqlite` statements, so a query gives
  the same rows on every run and platform.
- **The language has no unspecified order.** Maps keep insertion order, `sort` and `sort_by` are
  stable, `for` visits elements in order, and there are no threads.
- **Values print the same everywhere.** `str` of a float always gives the same text, and integers have
  a fixed size with overflow as an error ([R0001](../errors/R0001.md)), not a silent wrap.

That leaves the effects. Two of them, the clock and randomness, can be fixed with a command-line option.
The others can be recorded in one run and played back in another.

The rules are in [Traces and replay](../reference/traces-and-replay.md) and
[Command line](../reference/command-line.md).

## Fixing the seed and the clock

`random_int(lo, hi)` (with `uses random`) returns a whole number from `lo` to `hi`, both included.
`now_ms()` (with `uses clock`) returns the wall-clock time in milliseconds since 1970. By default each
run of `vo run` uses a new seed and the real clock. Two options fix them:

- `--seed N` fixes every random choice: `random_int`, and `std.random`'s `unit`, `pick` and `shuffle`.
  The same program with the same seed makes the same choices.
- `--clock-ms N` makes the clock start at `N` and advance by exactly 1 on each reading: `now_ms`
  returns `N`, then `N+1`, then `N+2`, and `std.time.now` reads the same clock.

```vo
import std.time.format_time;

# Simulate a queue of support calls: each caller waits a random number of minutes.
fn main() uses console, clock, random {
    let start = now_ms();
    var total = 0;
    for caller in ["ana", "bo", "cy"] {
        let wait = random_int(1, 10);
        set total = total + wait;
        print(caller + " waits " + str(wait) + " min");
    }
    print("average " + str(total / 3) + " min");
    print(
        "started " + format_time(start) + ", next reading " + str(now_ms() - start) + " ms later"
    );
}
```

```console
$ vo run --seed 1 --clock-ms 1767225600000 main.vo
ana waits 3 min
bo waits 9 min
cy waits 10 min
average 7 min
started 2026-01-01T00:00:00Z, next reading 1 ms later
$ vo run --seed 2 --clock-ms 1767225600000 main.vo
ana waits 8 min
bo waits 4 min
cy waits 7 min
average 6 min
started 2026-01-01T00:00:00Z, next reading 1 ms later
```

`random_int` with `lo > hi` is runtime error [R0007](../errors/R0007.md).

`vo test` fixes both for you: every test, and every case of a property, starts with seed 1 and a clock
at 0, so tests that use time or randomness are repeatable without any setup. `vo test --seed N` and
`--clock-ms N` change the starting values; for a property, `--seed` also picks the generated inputs
(see [Properties](properties.md)).

## Recording and replaying a run

The seed and the clock cover two effects. The rest, what a user typed, which files and variables were
there, what a server answered, cannot be fixed by an option. `vo run --record FILE` saves all of it while
the program runs, and `vo replay FILE` runs the program again on exactly what was saved:

```vo
import std.random.shuffle;

# Draw the prize winner among the names given on the command line.
fn main() -> result[void] uses console, random {
    let names = args();
    if len(names) == 0 {
        return err("usage: vo run main.vo -- NAME...");
    }
    let order = shuffle(names);
    print("first prize: " + order[0]);
    print("ticket: " + str(random_int(1000, 9999)));
    return ok();
}
```

```console
$ vo run --seed 42 --record raffle.jsonl main.vo -- ana bo cy di
first prize: di
ticket: 5805
$ vo replay raffle.jsonl
first prize: di
ticket: 5805
```

The replay needs no arguments, seed or clock: they come from the recording, and `vo replay` refuses
`--seed`, `--clock-ms` and program arguments ([T0001](../errors/T0001.md)). Without `--seed` the
recording stores the seed the run chose, so an ordinary run can be replayed too.

### What a recording contains

A recording is a file of JSON lines. The first line identifies the run: the program, a fingerprint of
its code, the seed, the fixed clock (or `null`) and the arguments. Then comes one line for each thing
the program received, in order, and a last line with the exit status. Here is a run that reads an
environment variable, a line of input and a file, and reads the clock twice:

```json
{"v":1,"kind":"recording","program":"main.vo","fingerprint":"d97b8fa55a9f39cc","seed":147643588019103,"clock_ms":null,"args":[]}
{"v":1,"kind":"now_ms","value":1794834708435}
{"v":1,"kind":"env","name":"GREETING","result":"ok","value":"hello"}
{"v":1,"kind":"read_line","result":"ok","value":"ana"}
{"v":1,"kind":"read_file","path":"data/notes.txt","result":"ok","value":"remember the milk\u000a"}
{"v":1,"kind":"now_ms","value":1794834708435}
{"v":1,"kind":"end","status":0}
```

Recorded inputs are arguments, standard input, environment variables, file reads, the results of file
writes, HTTP responses (and, for a server, the requests it received), what each `std.sqlite` call on a
database returned (never the database file itself), the clock and the seed. The
program's output is not recorded: replaying produces it again.

A recording holds the contents of every file and the value of every variable the program read. Treat it
as you would treat those files and variables: a recording of a program that reads a secret contains
the secret.

### What replay guarantees

`vo replay FILE` runs the current program and answers every effect from the recording. It reads no
file, no variable and no input of its own, writes nothing outside, and opens no network connection.
The same program gives the same output and the same exit status as the recorded run, unless the
recorded run timed out, since that depends on the machine's speed.

The program may have changed since the recording. That is the point: record a failure in production,
then replay it while you fix the code. A changed program is run with a note on stderr,
`note: the program differs from the recorded one`. As long as it asks for the same inputs in the same
order, it runs on the recorded ones. When it asks for something else, another file, another variable,
an HTTP request to another host, port, method or path, or another SQL statement or parameters, it stops
with [R0014](../errors/R0014.md):

```text
main.vo:5:21: runtime error[R0014]: The program reads the file `data/other.txt`, but the recording has a read of `data/notes.txt` next: replay the program that was recorded, or record this one.
```

A server is replayed by giving it the recorded requests again, without opening a port; each response
that differs from the recorded one is reported as a note. The [status-board](../examples/status-board.md)
and [notes-api](../examples/notes-api.md) examples include recordings.

`vo replay` also takes `--trace FILE`, `--timeout` and `--max-memory`, so a recorded failure can be
replayed with a full trace of every call.

## Traces

`vo run --trace FILE` writes a trace of the run: one JSON line per event.

- `call` for every call of a vo function: the function, the call site, the depth, and the arguments by
  name;
- `return` with the returned value;
- `error` when a runtime error ends the function;
- `end` with the number of records written, the number omitted, and the exit status.

`vo test --trace FILE` writes the same, with a `test` record before each test.

```vo
fn fib(n: int) -> int {
    if n < 2 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() uses console {
    print(str(fib(2)));
}
```

```console
$ vo run --trace trace.jsonl main.vo
1
```

`trace.jsonl` then holds:

```json
{"v":1,"kind":"call","depth":1,"function":"main","args":[]}
{"v":1,"kind":"call","depth":2,"function":"fib","file":"main.vo","start":{"line":9,"col":15},"args":[{"name":"n","value":"2"}]}
{"v":1,"kind":"call","depth":3,"function":"fib","file":"main.vo","start":{"line":5,"col":12},"args":[{"name":"n","value":"1"}]}
{"v":1,"kind":"return","depth":3,"function":"fib","value":"1"}
{"v":1,"kind":"call","depth":3,"function":"fib","file":"main.vo","start":{"line":5,"col":25},"args":[{"name":"n","value":"0"}]}
{"v":1,"kind":"return","depth":3,"function":"fib","value":"0"}
{"v":1,"kind":"return","depth":2,"function":"fib","value":"1"}
{"v":1,"kind":"return","depth":1,"function":"main"}
{"v":1,"kind":"end","records":8,"omitted":0,"status":0}
```

Values in a trace are written in vo syntax and shortened to 200 bytes. A long run can produce many
records: after `--trace-limit N` records (100000 by default), the rest are only counted, in `omitted`.
A trace that cannot be written stops the command with [T0009](../errors/T0009.md) (exit status 74).

Traces complement the report of a runtime error, which already lists every frame with its parameters
and the locals declared before the failing statement. The trace adds the history: every call that led
there, with what each one returned.

## Time and memory limits

A run that does not end, or that grows without bound, is stopped with a runtime error rather than left
to hang a machine.

**Time.** `--timeout SEC` limits a run to a whole number of seconds, 10 by default; `0` disables the
limit. A run that exceeds it stops with [R0009](../errors/R0009.md) and exit status 124. In `vo test`,
the limit applies to each test, and to a property as a whole. For a server, it applies to each call of
the handler.

```text
$ vo run --timeout 1 main.vo
main.vo:3:5: runtime error[R0009]: Time limit of 1 s exceeded.
   3 |     while n >= 0 {
     |     ^^^^^^^^^^^^^^
trace:
  at main (main.vo:3:5)
```

A timeout's report shows no values: where a run is when it times out depends on the speed of the
machine, and that is the one thing a replay cannot reproduce.

**Memory.** Values that can no longer be reached are reclaimed automatically. `--max-memory MiB` limits
the live data, what the program still holds, to between 1 and 65536 MiB, 512 by default. Holding more is
[R0006](../errors/R0006.md):

```vo
fn main() uses console {
    let rows = new list[str];
    var line = "x";
    for _ in 0..30 {
        set line = line + line;
        push(rows, line);
    }
    print(str(len(rows)));
}
```

```error R0006
```

```text
main.vo:5:20: runtime error[R0006]: The program holds 512.9 MiB of live data, over the 512 MiB limit.
```

`--mem-stats` reports, after the run (or after each test), the peak live size, the total allocated and
the number of collections, on stderr (a `memory` record with `--json`):

```text
memory: peak live 2 GiB, allocated 2 GiB, 9 collections (limit 4 GiB)
```

Both limits make a run's failure predictable: a runaway program ends with a known code and exit status,
and a test that loops forever is reported as `TIMEOUT` while the other tests still run. All the options
are on the [vo run](../tools/run.md), [vo test](../tools/test.md) and [vo replay](../tools/replay.md)
pages.

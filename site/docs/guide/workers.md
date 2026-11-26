# Workers

`concurrent_map` starts work, waits for all of it and returns. A *worker* is work the program does
not wait for: a function that runs in a process of its own, beside the program, and exchanges
messages with it. Use one for something that lasts (a loop that takes jobs, a periodic check, a reader
of a long-lived connection), for a call that needs a time limit, and for computing on several cores.

The functions are in `std.workers`.

## A worker beside the program

`start_worker(f)` starts `f` in its own process and returns at once with a handle. The function gets a
`Link`, its end of the conversation. Both ends are typed: a `Link[str, str]` belongs to a worker that
takes texts and gives texts.

```vo
import std.workers;

# Runs in a process of its own: takes texts, gives them back in capitals.
fn shout(link: workers.Link[str, str]) uses console {
    while true {
        match workers.next_message(link, workers.Wait.forever) {
            case ok(text) {
                print("worker: got " + text);
                if workers.emit(link, upper(text) + "!") != ok() {
                    return;
                }
            }
            case err(_) {
                return;
            }
        }
    }
}

fn main() uses console {
    match workers.start_worker(shout) {
        case ok(w) {
            for word in ["one", "two"] {
                if workers.tell(w, word) == ok() {
                    match workers.receive(w, workers.Wait.forever) {
                        case ok(answer) {
                            print("program: " + answer);
                        }
                        case err(_) {
                            print("program: no answer");
                        }
                    }
                }
            }
        }
        case err(_) {
            print("no worker");
        }
    }
}
```

```output
worker: got one
program: ONE!
worker: got two
program: TWO!
```

| The program | The worker |
|---|---|
| `tell(w, message)` sends | `next_message(link, wait)` takes the next one |
| `receive(w, wait)` takes the next one | `emit(link, message)` sends |

A wait is `Wait.forever` or `Wait.at_most(ms)`; `Wait.at_most(0)` asks without waiting. Messages
arrive in the order sent, each once. When the program ends, its workers are ended with it: the loop in
`shout` needs no way out.

Effects work as for any function the program calls: `main` declares `console` because `shout` prints,
with the same scopes, and the worker runs under the program's grant. Starting a worker is not an
effect of its own.

## A worker has its own copy

A worker works on a copy of everything its function was given or captured, as a task of
`concurrent_map` does. What it changes, it changes for itself; the only way back is a message. A
message is copied too, so a message type cannot hold a function
([R0015](../errors/R0015.md) when the worker is started), and a worker cannot read standard input: the
program reads it and sends what the worker needs.

## Failures are values

A worker that stops with a runtime error does not stop the program. The program learns of it where it
receives: after the messages the worker sent before failing comes `err(failed(text))`, with the error
as it would have been printed (below, [R0002](../errors/R0002.md)).

```vo
import std.workers;

fn divides(link: workers.Link[int, int]) {
    for d in [2, 1, 0] {
        if workers.emit(link, 10 / d) != ok() {
            return;
        }
    }
}

fn describe(r: result[int, workers.WorkerError]) -> str {
    match r {
        case ok(n) {
            return str(n);
        }
        case err(e) {
            match e {
                case failed(text) {
                    return "failed, division by zero: " + str(find(text, "R0002") >= 0);
                }
                case ended {
                    return "ended";
                }
                case timed_out {
                    return "timeout";
                }
                case full {
                    return "full";
                }
                case too_large {
                    return "too large";
                }
                case too_many {
                    return "too many";
                }
            }
        }
    }
}

fn main() uses console {
    match workers.start_worker(divides) {
        case ok(w) {
            for i in 0..4 {
                print(str(i) + ": " + describe(workers.receive(w, workers.Wait.forever)));
            }
            print("the program goes on");
        }
        case err(_) {
            print("no worker");
        }
    }
}
```

```output
0: 5
1: 10
2: failed, division by zero: true
3: ended
the program goes on
```

A worker whose function returns gives `ended`; one that calls `exit` with another status than 0 gives
`failed("exit status 3")`. A failure is given once, and `ended` from then on.

A failure that no `receive` returned is not lost: when the program ends, it is written to standard
error as the runtime error it was, and an exit status of 0 becomes 70. In `vo test`, it fails the test
that started the worker.

`tell` answers from what the program has received, never from timing: it gives `err(ended)` only once
a `receive` has given the worker's end or the worker was stopped. Before that it accepts the message,
which is dropped if the worker is gone.

## A time limit for one call

`with_deadline(ms, f)` runs `f` in a worker of its own and gives its result, or `err(timed_out)` when the
time has passed; the work is then stopped.

```vo
import std.workers;

# Minutes of work.
fn slow() -> int {
    var n = 0;
    for i in 0..4000000000 {
        set n = n + i % 7;
    }
    return n;
}

fn main() uses console {
    print(str(workers.with_deadline(1000, fn() => 6 * 7) == ok(42)));
    print(str(workers.with_deadline(100, slow) == err(workers.WorkerError.timed_out)));
}
```

```output
true
true
```

`stop_worker(w)` does the same for a worker you started: it ends the worker whatever it is doing
(computing, sleeping, waiting for the network) and returns when the process is gone.

A stopped function does not finish what it was doing. A file keeps what was written to it up to that
moment; an open SQLite transaction is rolled back. And since `f` works on a copy, what it changed in
the program's values is not seen by the caller: only its result comes back.

## Several workers, whichever speaks first

`receive_first(workers, wait)` waits for the first message from any worker of a list and says which
one sent it: an `Arrival` with the worker's `index` in the list and its `message`. A worker's end or
failure arrives once, as its message, and the worker is skipped from then on; `err(ended)` means every
worker of the list has been reported.

```vo
import std.workers;

fn counts_to(n: int) -> fn(workers.Link[int, int]) {
    return fn(link) {
        for i in 1..n + 1 {
            if workers.emit(link, i) != ok() {
                return;
            }
        }
    };
}

fn main() uses console {
    let ws = new list[workers.Worker[int, int]];
    for n in [3, 5, 2] {
        match workers.start_worker(counts_to(n)) {
            case ok(w) {
                push(ws, w);
            }
            case err(_) {
                print("no worker");
            }
        }
    }
    var messages = 0;
    var total = 0;
    var open = true;
    while open {
        match workers.receive_first(ws, workers.Wait.forever) {
            case ok(arrival) {
                match arrival.message {
                    case ok(n) {
                        set messages = messages + 1;
                        set total = total + n;
                    }
                    case err(_) {
                    }
                }
            }
            case err(_) {
                set open = false;
            }
        }
    }
    print(str(messages) + " messages, total " + str(total));
}
```

```output
10 messages, total 24
```

When several workers have a message waiting, the earliest in the list is chosen. With a time limit
(`Wait.at_most(ms)`), `err(timed_out)` means nothing arrived in time: that is how a program gives each
job a deadline while the other workers go on (the
[job-queue example](https://github.com/d4n-larsson/volang/tree/main/examples/job-queue) does).

## Computing on several cores

Workers that stay alive make computation on several cores pay: each is started once, and what goes
back and forth is a small description of the work and a small result.

```vo
import std.workers;

record Range {
    lo: int;
    hi: int;
}

fn sum_of_squares(link: workers.Link[Range, int]) {
    while true {
        match workers.next_message(link, workers.Wait.forever) {
            case ok(r) {
                var total = 0;
                for i in r.lo..r.hi {
                    set total = total + i * i;
                }
                if workers.emit(link, total) != ok() {
                    return;
                }
            }
            case err(_) {
                return;
            }
        }
    }
}

fn main() uses console {
    let pool = new list[workers.Worker[Range, int]];
    for i in 0..4 {
        match workers.start_worker(sum_of_squares) {
            case ok(w) {
                if workers.tell(w, new Range { lo: i * 250, hi: (i + 1) * 250 }) == ok() {
                    push(pool, w);
                }
            }
            case err(_) {
                print("no worker");
            }
        }
    }
    var total = 0;
    for w in pool {
        match workers.receive(w, workers.Wait.forever) {
            case ok(part) {
                set total = total + part;
            }
            case err(_) {
                print("a worker failed");
            }
        }
    }
    print(str(total));
}
```

```output
332833500
```

A message costs about 10 microseconds each way, so hand out work in pieces that take a millisecond or
more. Starting a worker does not copy the program's data: the copy is made of what the worker
changes.

## Workers or `concurrent_map`

| | `concurrent_map` | a worker |
|---|---|---|
| the program | waits for every task | goes on |
| results | the list, in the order of the input | messages, as the program asks for them |
| a failure | stops the program | is a value: `failed` |
| stopping the work | no | `stop_worker`, `with_deadline` |

For "do these N things and give me the results", `concurrent_map` is one line and stays the tool.

## Output and determinism

What a worker prints is written by the program, in order with that worker's messages: just before the
next message the program receives from it. What is left is written when the worker is stopped or when
the program ends, worker by worker in the order they were started. So a worker's lines do not appear
the moment they are printed; a worker that must log as it goes appends to a file.

That is what makes a program with workers give the same output on every run, as every vo program
does: when each `receive` names its worker and has no time limit, nothing depends on which process was
faster. Timing enters only where the program asks for it by name: `Wait.at_most`, `receive_first`,
`stop_worker`, and ending while workers still run.

For those, there is the recording (see [Determinism](determinism.md)). `vo run --record` writes down
what the program got from its workers: every message, end, failure and timeout, and what they
printed. `vo replay` starts no worker; it gives the program the same answers in the same order, and
the run prints and ends as the recorded one did. A replay cannot step into a worker's code.

Each worker has its own random sequence, fixed by `--seed` and its number among the workers started.
`--max-memory` applies to each worker separately. `--trace` writes a `worker` record with the
worker's number before its calls. `--timeout` is the program's: when it passes, the program ends and
its workers with it.

## Limits

- At most 64 workers at a time; `start_worker` gives `err(too_many)` for one more. A worker's seat is
  free again when the program has received its end or stopped it.
- A message is at most 16 MiB (`err(too_large)`).
- 1 MiB can be on its way in each direction, a worker's output included; then the sender waits for
  the reader. The program never waits to send while its worker waits to be read: it takes in what the
  worker sends meanwhile, up to 64 MiB, beyond which `tell` gives `err(full)` and the worker is
  stopped.
- Workers are started by the program's own process: `start_worker` (and so `with_deadline`) in a
  worker, a `concurrent_map` task or a `serve_workers` handler stops with
  [R0007](../errors/R0007.md). `concurrent_map` works inside a worker.
- No worker outlives the program, however it ends.

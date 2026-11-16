# Concurrency

A program that fetches ten URLs one after another waits for each answer in turn. `concurrent_map`
waits for all of them at once: it reads like `map`, and the whole call takes about as long as the
slowest answer. It is meant for waiting (HTTP requests), not for splitting up computation.

## Running tasks

`concurrent_map(xs, limit, f)` calls `f` on every element of `xs`, each call as a separate *task*, at
most `limit` tasks at a time (1 to 64; anything else is [R0007](../errors/R0007.md)). It returns the
results in the order of `xs`, whatever order the tasks finish in:

```vo
import std.http.get;

# The HTTP status of each URL, checked at the same time.
fn status(url: str) -> str uses net("localhost") {
    match get(url) {
        case ok(r) {
            return url + " " + str(r.status);
        }
        case err(e) {
            return url + " " + e;
        }
    }
}

fn main() uses console, net("localhost") {
    let urls = ["http://localhost:8000/health", "http://localhost:8001/health"];
    for line in concurrent_map(urls, 8, status) {
        print(line);
    }
}
```

```console
$ vo effects main.vo
main uses console, net("localhost")

main.vo:main uses console, net("localhost")
main.vo:status uses net("localhost")
0 errors, 0 warnings
```

The sequential version is the same program with `map(urls, status)`. The limit is required: think of
it as how many requests at once the servers you call should get.

Effects work as with `map`: the caller declares what `f` does, with the same scopes, and every task
runs with the caller's grant (a host outside `net("localhost")` is still [R0010](../errors/R0010.md)).
Starting tasks is not an effect of its own, so making a loop concurrent changes nothing in
`vo.effects` (see [Effects](effects.md)).

## Tasks share nothing

Each task works on its own copy of everything it is given or captures. A task that changes a list or a
record changes its copy; the caller sees only what the task returns:

```vo
record Tally {
    n: int;
}

fn main() uses console {
    let tally = new Tally { n: 0 };
    let lists = [[1], [2, 2]];
    let sizes = concurrent_map(lists, 2, fn(xs: list[int]) -> int {
        push(xs, 0);
        set tally.n = tally.n + 1;
        return len(xs);
    });
    print("sizes " + str(sizes[0]) + " " + str(sizes[1]));
    print("lists " + str(len(lists[0])) + " " + str(len(lists[1])) + ", tally " + str(tally.n));
}
```

```output
sizes 2 3
lists 1 2, tally 0
```

So there are no data races to get wrong: the only way back is the result. What cannot come back is
refused: a result that holds a function is [E0314](../errors/E0314.md), a collection created with its
own comparison function is [R0015](../errors/R0015.md). Tasks cannot read stdin either
([E0511](../errors/E0511.md)): read it first and pass the text in.

## Output, failures and determinism

What tasks print appears whole and in the order of `xs`: all of task 0's output, then all of task 1's,
whatever order they finish in.

```vo
fn work(n: int) -> int uses console {
    print("task " + str(n) + " starts");
    print("task " + str(n) + " ends");
    return n * 10;
}

fn main() uses console {
    let rs = concurrent_map([1, 2, 3], 3, work);
    print("results " + str(rs[0] + rs[1] + rs[2]));
}
```

```output
task 1 starts
task 1 ends
task 2 starts
task 2 ends
task 3 starts
task 3 ends
results 60
```

If tasks stop with a runtime error or call `exit`, the first one in the order of `xs` decides: the
other tasks are stopped and the program ends with that task's report, exactly as `map` would have
reported it. A task that returns an `err` result is not a failure; the caller gets every result and
decides. `--timeout` covers the whole program, tasks included, and no task outlives the call that
started it.

The rest of [Determinism](determinism.md) holds too: each task has its own random sequence, fixed by
`--seed`; `--trace` writes a `task` record before each task's calls; and a run recorded with
`--record` replays exactly, with no network.

Inside a task, `concurrent_map` runs one task at a time. Each task has its own `--max-memory` limit.

## Servers

`std.http.serve` reads many requests at once, so a slow or stalled client no longer holds up the
others. It still calls the handler for one request at a time, in the order the requests arrive
complete, so a handler that keeps state between requests (a counter in a record it captures) works
without locks. See [Standard library](standard-library.md).

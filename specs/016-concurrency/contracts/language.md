# Contract: Language (spec 016)

## The builtin

```
concurrent_map(xs: list[T], limit: int, f: fn(T) -> U) -> list[U]
```

- Runs `f` on each element of `xs` as a separate task, at most `limit` at a time, and returns the results
  in the order of `xs` (research R2). `xs` empty: `[]`-equivalent empty list at once, no task started.
- `limit` outside 1..64: R0007 at the call, before any task starts.
- Effects: as `map`. `f`'s effects (with scopes) become the caller's; E0501/E0502/E0503 as for `map`;
  `concurrent_map` adds no effect of its own (research R15).
- `f` whose effects include `input`: E0511 (research R8).
- `U` containing a function type anywhere: E0314 (research R4).
- Like every builtin, `concurrent_map` is not a value (wrap it to pass it).

## Semantics the reference states

1. **Copies.** Each task works on its own copy of `xs`'s element, of `f`, and of everything `f`
   captures. Changes a task makes stay in that task; only its result comes back. After the call, the
   caller's values are as before the call.
2. **Order.** Results are in the order of `xs`. Each task's printed output (stdout and stderr) appears
   whole, in the order of `xs`, whatever order the tasks finish in.
3. **Failure.** If tasks stop with a runtime error or call `exit`, the one first in `xs` order decides:
   the other tasks are stopped, and the program stops with that task's report (the same report and
   exit status as `map`), or exits with its code. An `err` result is not a failure.
4. **Results that cannot come back.** A collection created with its own order (`ordered_map_by` etc.) in
   a result: R0015 in that task.
5. **Nesting.** Inside a task, `concurrent_map` runs one task at a time.
6. **Seeds and memory.** Each task has its own random sequence, fixed by `--seed`, and its own
   `--max-memory`. `--timeout` covers the whole program.
7. **Server.** `std.http.serve` reads many requests at once and calls `handle` for one request at a
   time, in the order the requests arrive complete.

## Reference text (as shipped: +286 tokens, budget 400)

Row added to the table in "Functions as values":

```
| concurrent_map | `(xs: list[T], limit: int, f: fn(T) -> U) -> list[U]` | like `map`, at most `limit` (1 to 64, else R0007) tasks at a time |
```

Paragraph after that section's example (followed by a checked example: three tasks that print, then
the sum of their results):

> **Tasks**: `concurrent_map` runs `f` on each element as a separate task, for waiting on several things
> at once (HTTP requests). Results, and each task's whole output, come in the order of `xs`, whatever
> order the tasks finish in. Each task works on its own copy of everything it gets or captures: changes
> stay in the task, only its result comes back. The first task in `xs` order that stops with a runtime
> error or `exit` stops the others and the program, as `map` would. Results cannot hold functions
> (E0314) or a collection with its own order (R0015); `f` cannot read stdin (E0511). Inside a task,
> tasks run one at a time. Each task has its own random sequence (fixed by `--seed`) and its own
> `--max-memory`.

Also changed: the `serve` bullet ("reads many requests at once and calls `handle(request) -> Response`
for one at a time, in the order they arrive complete"), the trace bullet (`task` (`index`) before each
task's records), and `--mem-stats` ("without its tasks").

## Example (as in examples/url-check; the reference carries a pure one)

```vo
import std.http.get;

fn status(url: str) -> str uses net("127.0.0.1") {
    match get(url) {
        case ok(r) { return url + " " + str(r.status); }
        case err(e) { return url + " " + e; }
    }
}

fn main() uses console, net("127.0.0.1") {
    let urls = ["http://127.0.0.1:8080/a", "http://127.0.0.1:8080/b"];
    for line in concurrent_map(urls, 8, status) {
        print(line);
    }
}
```

The sequential version differs in one call: `map(urls, status)`.

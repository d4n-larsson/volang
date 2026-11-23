# Contract: `std.workers`

Types: [data-model.md](../data-model.md). `In`: what the worker takes; `Out`: what it gives; `E`: the
effects of its function.

## The program's side

```vo
export fn start_worker[In, Out, E: effects](body: fn(Link[In, Out]) uses E) -> result[Worker[In, Out], WorkerError] uses E
export fn tell[In, Out](w: Worker[In, Out], message: In) -> result[void, WorkerError]
export fn receive[In, Out](w: Worker[In, Out], wait: Wait) -> result[Out, WorkerError]
export fn receive_first[In, Out](ws: list[Worker[In, Out]], wait: Wait) -> result[Arrival[Out], WorkerError]
export fn stop_worker[In, Out](w: Worker[In, Out])
export fn with_deadline[T, E: effects](ms: int, f: fn() -> T uses E) -> result[T, WorkerError] uses E
```

- `start_worker` returns at once. `In` or `Out` holding a function or a collection with its own
  order: R0015. Called in a worker, a `concurrent_map` task or a `serve_workers` worker: R0007.
- `receive` gives the worker's messages in the order it emitted them, then its end. What the worker
  printed before a message is written just before `receive` returns that message.
- `tell` gives `err(ended)` only after a receive or a stop has given that worker's end; before that
  it returns `ok`, and the message is dropped if the worker is gone. A failure is given once, by a
  receive; every operation after it gives `err(ended)`.
- `receive_first`: the first message from any worker of the list; with several waiting, the earliest
  in the list. `index` is the position in `ws`. A worker's end or failure is given once (as the
  `message` of an arrival); the worker is then skipped. `err(timeout)` when the limit passes;
  `err(ended)` when every worker of the list has been reported.
- `stop_worker` returns when the process is gone, after writing the output that had arrived from
  it; messages not received are dropped. On a worker that ended, nothing.
- `with_deadline` runs `f` in a worker of its own: its result, `err(timeout)` with the worker
  stopped, or `err(failed(text))`. `f` works on a copy of the program's values.

## The worker's side

```vo
export fn next_message[In, Out](link: Link[In, Out], wait: Wait) -> result[In, WorkerError]
export fn emit[In, Out](link: Link[In, Out], message: Out) -> result[void, WorkerError]
```

`next_message` gives `err(timeout)` or a message; a worker never sees the program end (it is ended
with it). `emit` waits while the program is not reading and 1 MiB is in flight.

## At the program's end

Every worker is ended and the output that had arrived is written, worker by worker in the order
they were started. A failure no operation returned is written to standard error as the runtime
error it was, and an exit status of 0 becomes 70.

## Stops

| Situation | Result |
|---|---|
| a message type that cannot cross | R0015 at `start_worker` |
| `start_worker` outside the program's own process | R0007 |
| a negative time limit | R0007 |
| a worker reads standard input | R0007 in the worker |
| a handle not made by `start_worker`, or used with other message types | R0007 |

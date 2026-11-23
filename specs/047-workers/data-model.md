# Data model: Workers (spec 047)

Signatures: [contracts/workers.md](contracts/workers.md).

| Type | Fields or cases | Rules |
|---|---|---|
| `Worker[In, Out]` (record) | `id: int` | the handle in the program; slot and generation; made only by `start_worker` |
| `Link[In, Out]` (record) | `id: int` | the worker's end; passed to its function |
| `Wait` (variant) | `forever`, `at_most(int)` | milliseconds; 0 asks without waiting; negative is R0007 |
| `WorkerError` (variant) | `timeout`, `ended`, `failed(str)`, `full`, `too_large`, `too_many` | see below |
| `Arrival[Out]` (record) | `index: int`, `message: result[Out, WorkerError]` | what `receive_first` gives |

## Worker errors

| Case | From | Meaning |
|---|---|---|
| `timeout` | `receive`, `receive_first`, `next_message`, `with_deadline` | nothing within the limit |
| `ended` | `receive`, `receive_first`, `tell` | the worker's function returned, it called `exit(0)`, or it was stopped; from `tell` only after a receive or a stop gave it |
| `failed(text)` | `receive`, `receive_first`, `tell`, `with_deadline` | a runtime error (its code, message and trace as text), `exit` with another status, or a signal |
| `full` | `tell` | more than 64 MiB taken in from the worker while waiting to send; the worker is stopped (part of the message may be on its way) |
| `too_large` | `tell`, `emit` | a message over 16 MiB encoded |
| `too_many` | `start_worker`, `with_deadline` | 64 workers are running |

## A worker's life

```text
started ── running ──┬─ function returned ───────────── ended
                     ├─ runtime error, exit(n≠0), signal ─ failed(text)
                     └─ stop_worker, or the program ends ─ ended (killed)
```

After the end: messages that arrived before it are still received, in order; then one `receive`
gives `ended` or `failed`, and every operation after that gives `ended` (before that a `tell` is
accepted and the message dropped). The worker's slot is free again from that moment. A failure is "delivered" when a `receive`, `receive_first` or `with_deadline`
returned it; an undelivered one is reported at the program's end.

## Limits

| What | Limit |
|---|---|
| workers running at a time | 64 |
| one message, encoded | 16 MiB |
| in flight in each direction before the sender waits | 1 MiB (the pipe); a worker's output counts |
| taken in by the program while it waits to send | 64 MiB per worker |

## In a recording

One event (`worker`) per `receive`, `receive_first` and `stop_worker`, per `tell` that gave `full`, and
per worker ended with the program: its name is the operation, its value the frames the operation
passed (output, the message, the end or failure, or a mark for a time limit that passed). `tell` and
`start_worker` are not recorded: their answers follow from what was received. Nothing from inside a
worker.

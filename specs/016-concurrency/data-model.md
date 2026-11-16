# Data Model: Concurrency (spec 016)

Runtime state added by this spec. Nothing here is visible to vo programs except through
`concurrent_map`'s results, output order and error reports.

## Task group (parent side, one per `concurrent_map` call)

Lives on the C stack of the call (`src/task.c`); its heap references are rooted (research R4 of spec 003).

| Field | Meaning |
|---|---|
| `xs`, `fn`, `call` | the input list, the function value, the call node (for errors and the result type) |
| `limit` | 1..64 as given; 1 inside a task (research R7) and in replay (R10) |
| `next` | position of the next task to start |
| `head` | lowest position not yet finished; its pipes are the ones drained (R5) |
| `slots[64]` | running or exited-but-undrained children: position, pid, stdout/stderr/control fds, exit status |
| `results` | `list[U]` being filled in position order |
| `failed` | lowest failing position so far, or -1 (R6) |

**States of one task**: `waiting` (not started) → `running` (forked) → `exited` (POLLHUP seen, reaped;
pipes may still hold data) → `drained` (head reached it, all pipes at end of file, result read) →
`done`. From `running`, a task above `failed` goes to `killed` (SIGKILL, pipes closed unread).

**Invariants**:
- At most `limit` tasks are `running`.
- Output is copied only from the head; positions below `head` are `done`.
- The call returns only when every started task is `done` or `killed` (FR-003).
- If `failed >= 0`, no task above `failed` is started, and the call ends by re-raising the failure of
  the lowest failing position once every task below it is `done`.

## Task (child side)

A forked copy of the parent at the moment the task starts, plus:

| State | Meaning |
|---|---|
| `task_depth` | > 0 in a task (nested calls run with limit 1) |
| `task_ctl` | control fd |
| own recovery point | runtime errors and `exit` end the task with a control message, never resume the parent's code |
| seed | run seed mixed with the position path (R12) |
| recording writer | writes to the control pipe (R10) |
| trace writer | writes to the control pipe (R11) |

## Control messages (child → parent, on the control pipe)

Each message: a kind byte, a length word, the payload. Sent in this order; `result`, `error` and `exit`
are mutually exclusive and last.

| Kind | Payload |
|---|---|
| `trace` | trace JSON lines (already formatted), and finally the count of omitted records |
| `record` | recording event lines (already formatted) |
| `replay_pos` | the next-event index after the task (replay only) |
| `result` | the result value in transfer form (below) |
| `error` | code (catalog string), message bytes, node pointer, extra JSON or empty, text trace, JSON trace |
| `exit` | the exit code passed to `exit` |

## Transfer form of a value (research R4)

Driven by the concrete static type; written depth-first.

| Type | Encoding |
|---|---|
| `int`, `float`, `bool` | one word |
| `void` | nothing |
| reference types (`str`, list, map, record, variant, result, collection) | a back-reference word (index of an object already sent) or a "new" marker followed by the object |
| `str` | length, bytes |
| `list[T]` | length, elements as `T` |
| `map[K, V]` | count, entries in insertion order as `K`, `V` |
| record | its fields in declaration order, by their concrete types |
| variant, `result[T]` | tag, payload by the case's type |
| `ordered_map`, `ordered_set`, `priority_queue`, `deque` | count, items in iteration order (queue: pop order, so ties keep their order); R0015 if the collection has its own comparison function |
| function type | never (E0314 at check time) |

The parent allocates each "new" object before decoding its contents, so cycles resolve to the same
object.

## Server connection (`src/serve.c`, research R14)

| Field | Meaning |
|---|---|
| `fd` | the accepted socket (non-blocking) |
| `buf`, `frame` | bytes received so far and the `HttpFrame` state |
| `deadline` | accept time + 10 s (408 after it) |
| `out`, `sent` | queued response text and how much of it is written |
| `state` | `reading` → `ready` (complete request, waiting for the handler) → `handling` → `writing` → closed |

**Invariants**: at most 64 connections open; at most one `handling`; `ready` connections are handed to
the handler in the order they became ready; refusals (400, 408, 413, 431) are queued as `writing`
without reaching the handler.

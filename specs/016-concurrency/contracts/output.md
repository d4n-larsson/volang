# Contract: Output, Traces, Recordings and the Server (spec 016)

## stdout, stderr, JSON records

For a `concurrent_map` call: everything the caller wrote before the call, then the whole stdout of task 0,
task 1, ... in order (stderr the same, independently), then what the caller writes after. A failing task
(research R6): output of tasks up to and including it, then the report from the parent. Output of tasks
above it never appears. `--json` records written by tasks follow the same rule.

## Traces (`--trace`)

New record kind, schema v1 (a field added, Principle II):

```json
{"v":1,"kind":"task","index":3}
```

Written before the records of the task at that position; the task's `call` records continue the depth of
the `concurrent_map` call. Nested tasks write their own `task` records inside. `--trace-limit` counts
records in this merged order; `end` reports `records` and `omitted` for the whole run.

## Recordings (`--record`, `vo replay`)

Format unchanged (`specs/006-query-edit-trace/contracts/recording.md`, spec 011's server events): the
events of the tasks of one call appear in task order, after the caller's earlier events, exactly as a
sequential `map` would produce them. A recording made on one platform replays on the others. During
replay tasks run one at a time; R0014 and change notes are unchanged.

## `--mem-stats`

Reports the parent process only (research R13). Unchanged format.

## The server (`std.http.serve`)

- Up to 64 connections are read at the same time; each has the existing limits (10 s to send the whole
  request, 408 after; 413; 431; 400). More connections wait in the accept backlog.
- The handler is called for one request at a time, in the order requests became complete.
- Responses are written without holding the next handler call; each response has 10 s to be written,
  then its connection is closed. `Connection: close` as before.
- Recordings: `request` and `response` events in handling order (unchanged); replay feeds them in that
  order and opens no port.

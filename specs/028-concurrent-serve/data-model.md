# Data Model: Concurrent Request Handling (spec 028)

## Pool (in the parent)

| Field | Meaning |
|---|---|
| workers | W, 1 to 64: the declared count, or `--workers N` |
| slots | one per worker: pid, request pipe (to the worker), response pipe (from it), state, the request it holds |
| waiting | complete requests not yet given to a worker, in arrival order (each with its connection) |
| next_number | the arrival number of the next request (R8, R9) |
| stopping | 0 serving; 1 shutting down (no new requests, finish the others); 2 stopped at once |

**Worker states**: `starting` (running the setup) → `ready` → `busy` (one request) → `ready` ...;
`starting` → `failed` (setup `err` or a runtime error: the server stops, research R5); any state →
`gone` (its pipe closed unexpectedly: replaced, its request answered 500).

## Messages

Parent → worker: `request` {number, bytes}.

Worker → parent, per request, in this order: `output` {stdout bytes, stderr bytes} (zero or more),
`events` {recording lines} (when recording), `response` {status of the call: ok or failed, response text}.
At start: `ready` (after the setup ran, with its output and events) or `setup_failed` {the `err` text}.

## Per-request inputs

Each handler call's inputs are its own: the request, the setup's result (the worker's, unchanged by other
requests), and the effects it performs (file reads, database results, HTTP responses, clock readings),
recorded in its request block; its random sequence starts from (seed, number).

## Limits (unchanged, now per worker)

| Limit | Where |
|---|---|
| 64 connections, 64 KiB head, 100 headers, 16 MiB body, 10 s to receive | the parent |
| `--timeout` | each handler call and each setup, in its worker |
| `--max-memory` | each worker |

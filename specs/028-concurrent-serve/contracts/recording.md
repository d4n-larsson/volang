# Contract: recordings of servers with workers (spec 028)

A recording of a `serve_workers` run is a recording of a `serve` run with two additions; schema 1 adds
kinds, removes none.

```text
{"v":1,"kind":"listen","address":"8080","result":"ok","value":3}
{"v":1,"kind":"setup","worker":0}
... the setup's events (file reads, database results, ...)
{"v":1,"kind":"setup_end"}
{"v":1,"kind":"setup","worker":1}
...
{"v":1,"kind":"setup_end"}
{"v":1,"kind":"request","value":"GET /notes HTTP/1.1\r\n..."}
... the events of that request's handler call
{"v":1,"kind":"response","value":"HTTP/1.1 200 OK\r\n..."}
{"v":1,"kind":"request", ...}
...
```

- One `setup` block per worker started (replacements included), written when the worker is ready.
- A request whose worker ended without answering (killed from outside) keeps the events the worker sent,
  then `{"v":1,"kind":"worker_ended"}` and the 500 `response`; replay passes that block over without
  running the handler (its inputs were never recorded).
- Request blocks (`request` ... `response`) in arrival order, each contiguous, whatever order the workers
  finished them in.
- Replay runs in one process: the setup once, with the first `setup` block's events; the other `setup`
  blocks are skipped; each request with its own block; a response that differs is a note, as today.
- Randomness: each request's random sequence is derived from the recording's seed and the request's
  number, so replay reproduces it.

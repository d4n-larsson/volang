# Status board

A monitoring client for a service on `localhost`: it fetches `/status.json` and `/incidents.json` over
plain HTTP, prints which services are up, the slowest one and the open incidents, and exits with status
2 when a service is down, so a script or cron job can act on it.

The service is a vo program too, in `service/`: it answers `/status.json` and `/incidents.json` from
the files in `service/site/` with `std.http.serve` (see [The service](#the-service)).

```sh
cd examples/status-board
(cd service && ../../../bin/vo run main.vo) &   # the service, on port 8000 of this machine
../../bin/vo run main.vo                    # the board (the port defaults to 8000; pass another after --)
../../bin/vo test main.vo                   # tests of the parsing and the rendering, no network needed
kill %1                                     # stop the service (it exits with status 0)
```

```text
4 of 5 services up

  DOWN  search
  up    api        42 ms
  up    billing   260 ms
  up    mail       75 ms
  up    web       118 ms

slowest: billing (260 ms)

open incidents:
  search: index rebuild stalled
  billing: slow invoice export
```

## Record and replay

`vo run --record FILE` saves everything the program received: its arguments and every HTTP response.
`vo replay FILE` runs the program again on exactly that, without the network: the server can be gone.

```sh
../../bin/vo run --record recordings/mine.jsonl main.vo   # while the service runs
kill %1                                                   # stop the service
../../bin/vo run main.vo                                  # error: connection refused by localhost:8000 (exit 1)
../../bin/vo replay recordings/mine.jsonl                 # the same board as before, exit 2
```

`recordings/demo.jsonl` is such a recording, committed: `../../bin/vo replay recordings/demo.jsonl`
works on any machine. Replay is how you debug a run you cannot repeat (the service has moved on, or
the bug only showed against production data): change the program, replay the recording, and see the
fix on the very input that broke it. Each request is matched to the recording by host, port, method and
path: if the changed program asks for something else (`GET /v2/status` instead of `GET /status`), or
makes more requests than were recorded, replay stops with R0014 instead of guessing.

## What it shows

- **The network as a scoped effect.** `main` and `fetch` declare `uses net("localhost")`. A request to
  any other host, even one built at run time, stops the program with R0010. `std.http.get` runs with its
  caller's grant, so the library cannot reach further than the program allows.
- **A reviewable record.** `vo.effects` shows that only `main` and `fetch` use the network and that
  everything in `board.vo` is pure; an edit that adds a network call anywhere else fails `vo check`.
- **`std.http` with `std.json`.** `get` returns the status, headers and body; a status other than 200 and
  a body that is not the expected JSON are `err` values with a message (``no field `latency_ms` ``),
  passed up with `try`.
- **Exit statuses for scripts.** 0 when everything is up, 2 when a service is down (`exit(2)`), 1 when
  the board cannot be built (`main` returned `err`), such as a refused connection.

## Files

| File | Contents |
|---|---|
| `main.vo` | `fetch`, `main` and the tests |
| `board.vo` | the `Service` and `Incident` records, parsing and rendering (pure) |
| `service/` | the service: `main.vo` (handler, tests), `site/` (the two JSON documents), its `vo.effects` and a recording |
| `recordings/demo.jsonl` | a recorded run, replayed by `make test` |
| `vo.effects` | the effects record, updated with `vo effects --update main.vo` |

## The service

`service/main.vo` serves the two files with `std.http.serve`: a handler function takes each `Request`
and returns a `Response`; `serve("8000", answer)` runs it until Ctrl-C or SIGTERM.

- **Its grant is visible**: `main` declares `serve("8000")` (port 8000 of this machine only; the network
  would need `serve("0.0.0.0:8000")`) and `file.read("site/")`, which the handler needs. `vo.effects`
  records both.
- **The handler is tested without a network**: its tests build `Request` values and check the answers
  (`../../../bin/vo test main.vo` in `service/`).
- **A failing handler does not stop the service**: that request gets `500`, the runtime error report goes
  to stderr, and the next request is answered.
- **Record and replay work for servers too**: `service/recordings/demo.jsonl` holds the two requests the
  board made, the files read and the responses sent. `../../../bin/vo replay recordings/demo.jsonl` in
  `service/` answers them again without opening a port, and notes every response that differs.

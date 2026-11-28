# Status board

A monitoring client for a service on `localhost`: it fetches `/status.json` and `/incidents.json` over
plain HTTP, prints which services are up, the slowest one and the open incidents, and exits with status
2 when a service is down, so a script or cron job can act on it.

The service is a vo program too, in `service/`: it answers `/status.json` and `/incidents.json` from
the files in `service/site/` with `std.http.serve` (see [The service](#the-service)).

![The status board and its service. main.vo, whose fetch declares net("localhost"), asks
service/src/main.vo, which declares serve("8000"), for /status.json and /incidents.json; the service reads
them from site/ with file.read("site/") and answers with JSON. vo run --record saves every response
the board received in a recording; vo replay gives the board those responses again, with no
network.](diagram.svg)

## Run it

```sh
cd examples/status-board
(cd service && ../../../bin/vo run src/main.vo) &   # the service, on port 8000 of this machine
../../bin/vo run src/main.vo                    # the board (the port defaults to 8000; pass another after --)
../../bin/vo test src/main.vo                   # tests of the parsing and the rendering, no network needed
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

## How it works

### The network is a scoped effect

`main` and `fetch` declare `uses net("localhost")`. A request to any other host, even one built at run
time, stops the program with R0010. `std.http.get` runs with its caller's grant, so the library cannot
reach further than the program allows.

<!-- code: src/main.vo fn fetch -->

### A record a reviewer can read

`vo.effects` shows that only `main` and `fetch` use the network and that everything in `board.vo` is
pure; an edit that adds a network call anywhere else fails `vo check`.

<!-- code: src/vo.effects -->

### JSON that may be wrong

`std.http`'s `get` returns the status, headers and body; a status other than 200 and a body that is not
the expected JSON are `err` values with a message (``no field `latency_ms` ``), passed up with `try`.

<!-- code: src/board.vo fn services_from -->

### Exit statuses for scripts

0 when everything is up, 2 when a service is down (`exit(2)`), 1 when the board cannot be built (`main`
returned `err`), such as a refused connection.

<!-- code: src/main.vo fn main -->

## Record and replay

`vo run --record FILE` saves everything the program received: its arguments and every HTTP response.
`vo replay FILE` runs the program again on exactly that, without the network: the server can be gone.

```sh
../../bin/vo run --record recordings/mine.jsonl src/main.vo   # while the service runs
kill %1                                                   # stop the service
../../bin/vo run src/main.vo                                  # error: connection refused by localhost:8000 (exit 1)
../../bin/vo replay recordings/mine.jsonl                 # the same board as before, exit 2
```

`recordings/demo.jsonl` is such a recording, committed: `../../bin/vo replay recordings/demo.jsonl`
works on any machine. Replay is how you debug a run you cannot repeat (the service has moved on, or
the bug only showed against production data): change the program, replay the recording, and see the
fix on the very input that broke it. Each request is matched to the recording by host, port, method and
path: if the changed program asks for something else (`GET /v2/status` instead of `GET /status`), or
makes more requests than were recorded, replay stops with R0014 instead of guessing.

## The service

`service/src/main.vo` serves the two files with `std.http.serve`: a handler function takes each `Request`
and returns a `Response`; `serve("8000", answer)` runs it until Ctrl-C or SIGTERM.

<!-- code: service/src/main.vo fn answer -->

- **Its grant is visible**: `main` declares `serve("8000")` (port 8000 of this machine only; the network
  would need `serve("0.0.0.0:8000")`) and `file.read("site/")`, which the handler needs. `vo.effects`
  records both.
- **The handler is tested without a network**: its tests build `Request` values and check the answers
  (`../../../bin/vo test src/main.vo` in `service/`).
- **A failing handler does not stop the service**: that request gets `500`, the runtime error report goes
  to stderr, and the next request is answered.
- **Record and replay work for servers too**: `service/recordings/demo.jsonl` holds the two requests the
  board made, the files read and the responses sent. `../../../bin/vo replay recordings/demo.jsonl` in
  `service/` answers them again without opening a port, and notes every response that differs.

The service's own requirements, proved by its tests in `service/src/main.vo`:

| ID | Requirement |
|---|---|
| SV-001 | `GET /status.json` and `/incidents.json` answer the files in `site/` as JSON. |
| SV-002 | Other paths answer 404; other methods 405. |

## Requirements

| ID | Requirement |
|---|---|
| FR-001 | The board reads each service's name, state and latency from `/status.json`. |
| FR-002 | A document with a missing field is an error naming the field. |
| FR-003 | Down services are listed first, and the slowest service that is up is named. |
| FR-004 | Only open incidents are listed. |
| FR-005 | The exit status is 2 when a service is down. |
| FR-006 | With every service up and no open incidents, the board has no incident section. |

Each test names the requirements it proves with `@spec("FR-...")` on the line above it; `vo query specs src/main.vo` lists them, and `vo test` ends with a line per requirement.

## Files

| File | Contents |
|---|---|
| `src/main.vo` | `fetch` and `main`: only the effects |
| `src/board.vo` | the `Service` and `Incident` records, parsing and rendering (pure), and their tests |
| `service/` | the service: `src/main.vo` (handler, tests), `site/` (the two JSON documents), its `vo.effects` and a recording |
| `recordings/demo.jsonl` | a recorded run, replayed by `make test` |
| `src/vo.effects` | the effects record, updated with `vo effects --update src/main.vo` |

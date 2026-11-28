# Notes API

A small JSON service built with `std.web`: list, add, read, complete and delete notes, kept in the SQLite
database `data/notes.db`. It answers on port 8080 of this machine, up to four requests at once, until
Ctrl-C.

![The notes API. Requests reach notes_app(), the routes with their inputs and problems, in two ways:
from serve_app on port 8080 with four workers, and from handle_request in the tests, with no network.
notes_app() calls notes.vo, which is written for any database (Notes[D]): the SQLite file data/notes.db
that each worker opens, or a new database in memory in each test.](diagram.svg)

## Run it

```sh
cd examples/notes-api
../../bin/vo run src/main.vo        # serve on http://localhost:8080 (Ctrl-C stops it, exit status 0)
../../bin/vo test src/main.vo       # nine tests and a property: every request through the app, a database in memory
```

## Try it

In another terminal (`std.web` takes JSON bodies with `Content-Type: application/json`, as Spring does;
`curl -d` alone sends a form, answered 415):

```text
$ J='Content-Type: application/json'
$ curl -H "$J" -d '{"text": "water the plants"}' localhost:8080/notes        # 201, Location: /notes/1
{"id":1,"text":"water the plants","done":false}
$ curl -H "$J" -d '{"text": "buy bread"}' localhost:8080/notes               # 201, Location: /notes/2
{"id":2,"text":"buy bread","done":false}
$ curl localhost:8080/notes
[{"id":1,"text":"water the plants","done":false},{"id":2,"text":"buy bread","done":false}]
$ curl -X POST localhost:8080/notes/2/done
{"id":2,"text":"buy bread","done":true}
$ curl -X DELETE localhost:8080/notes/1                                       # 204
$ curl -H "$J" -d '{"txt": "typo"}' localhost:8080/notes                      # 400
{"type":"about:blank","title":"Bad Request","status":400,"errors":[{"field":"txt","reason":"unknown field"},{"field":"text","reason":"missing"}]}
$ curl localhost:8080/nope                                                    # 404
{"type":"about:blank","title":"Not Found","status":404,"detail":"no route for /nope"}
$ curl localhost:8080/health
{"status":"up"}
```

| Request | Answer |
|---|---|
| `GET /notes` | 200, every note |
| `POST /notes` with `{"text": "..."}` | 201 with the new note and its `Location`; 400 for a missing text or an unknown field, 422 for an empty, blank or too long one |
| `GET /notes/ID` | 200 with the note; 404 when there is none |
| `POST /notes/ID/done` | 200 with the note, now done |
| `DELETE /notes/ID` | 204 |
| `GET /health`, `GET /health/ready` | 200 `{"status":"up"}`, or 503 naming the failing check |
| `GET /health/live` | 200 `{"status":"up"}` while the process answers (no check runs) |
| `GET /metrics` | the metrics in the Prometheus text format |
| anything else | 404 or 405 |

Every failure is an RFC 9457 problem (`application/problem+json`) with the status, a `detail` or the
`errors` of each field.

## Operate it

Each request gets an id (`X-Request-Id`, kept when the client sends one) and one log record on stderr, a
line of JSON; records written while it is answered, such as `note created`, carry the same id:

```text
{"time":"2026-11-24T19:05:43.221Z","level":"info","message":"note created","request_id":"21983af349ee109f","id":2}
{"time":"2026-11-24T19:05:43.221Z","level":"info","message":"request","request_id":"21983af349ee109f","method":"POST","route":"/notes","status":201,"ms":8}
```

- `GET /metrics` answers the request counts and durations by route pattern (`/notes/{id}`, not each
  path), the requests in flight and `notes_created_total`, totalled over the four workers.
- `GET /health/live` and `GET /health/ready` are for an orchestrator: restart when live fails, send no
  traffic while ready fails (the database check).
- `--log-level warn` writes only warnings and errors (default `info`; probes of `/health/...` and
  `/metrics` are `debug`).
- On SIGTERM the service stops accepting, answers what it has read for up to `--drain` seconds (default
  30) and ends with a `stopped` record saying how many requests were drained and cut off.

The steps and routes are a few lines of `notes_app`: `request_ids(random_id)`, `request_logs()`,
`request_metrics()`, `metrics_route([notes_created()])` and `health_routes(database)`.

## How it works

### The whole API is a value

`notes_app()` in `api.vo` is the whole API as a value: a route table (`on_get("/notes/{id}", show)`,
...), the input of each operation as a record (`NoteId { id: int }` from the path,
`NewNote { body: NoteText }` from the JSON body, with `@size(1, 200)` on the text), and a health route.
Routing, 404/405, reading and checking the input, JSON answers and problem bodies come from `std.web`;
there is no code for them here.

<!-- code: src/api.vo fn notes_app -->

### Every way to fail has a status

The operations fail with a `NoteError` (`not_found(id)`, `blank`, `database(message)`), and one
function, `note_problem`, gives each case its status, so the checker sees that every way to fail has
one; `typed(op, note_problem)` makes each operation one the routes take.

<!-- code: src/api.vo fn note_problem -->

### Workers and a database

`main` calls `serve_app("8080", 4, open_notes, notes_app())`: four worker processes, each opening the
database once (`open_notes`). Notes live in SQLite, so requests answered at once never lose each
other's changes.

<!-- code: src/main.vo fn main -->

### Tests without a network

The tests send requests to `notes_app()` with `handle_request` and an in-memory database; the
operations and `notes.vo` are generic over the database's effects (`Notes[D]`), so the same code runs
on the file in `main` and in memory in the tests.

<!-- code: src/api.vo test "POST /notes adds a note with the next id" -->

### Effects are the service's permissions

`main` declares `serve("8080")` (this machine only; the network would need `serve("0.0.0.0:8080")`) and
`Storage`, the effect set `api.vo` names once: `file.read("data/")` and `file.write("data/")`, which
`open_notes` needs. `vo.effects` lists those effects written out; it shows that only `open_notes`
touches files and that the operations use only the database they are given.

<!-- code: src/api.vo fn open_notes -->

### Record and replay

`recordings/demo.jsonl` is the session above: each worker's setup, then every request with its database
calls and its response. `../../bin/vo replay recordings/demo.jsonl` answers the same requests again, one
at a time, without opening a port or touching `data/`.

## Requirements

| ID | Requirement |
|---|---|
| FR-001 | `GET /notes` lists every note as JSON. |
| FR-002 | `POST /notes` adds a note with the next free id, answered with 201 and its `Location`. |
| FR-003 | A missing, empty or too long text is refused (400 or 422) with the reason. |
| FR-004 | A note can be read, marked done and deleted. |
| FR-005 | Unknown notes and paths are 404; other methods on a resource are 405. |
| FR-006 | The notes are kept in `data/notes.db` between requests, and requests answered at once lose no change. |
| FR-007 | Each request is logged with its id, and a created note is logged with the same id. |
| FR-010 | `/metrics` counts requests by route pattern and the notes created. |
| FR-015 | `/health/live` and `/health/ready` answer apart from `/health`. |

Each test names the requirements it proves with `@spec("FR-...")` on the line above it; `vo query specs src/main.vo` lists them, and `vo test` ends with a line per requirement.

`vo query specs src/main.vo` lists FR-001 to FR-007, FR-010 and FR-015; comparing the query's IDs with this table is how a
reviewer, or a tool, finds requirements without a test.

## Files

| File | Contents |
|---|---|
| `src/main.vo` | `main` (serving): only the effects |
| `src/api.vo` | `notes_app` (the routes and operations), `open_notes`, and the tests of every request |
| `src/notes.vo` | the `Note` record and its storage: listing, finding, adding, completing and removing notes |
| `data/notes.db` | the notes (created on the first run) |
| `recordings/demo.jsonl` | the recorded session, replayed by `make test` |
| `src/vo.effects` | the effects record, updated with `vo effects --update src/main.vo` |

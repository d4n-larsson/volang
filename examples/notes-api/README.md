# Notes API

A small JSON service over plain HTTP: list, add, read, complete and delete notes, kept in the SQLite
database `data/notes.db`. It answers on port 8080 of this machine, up to four requests at once, until
Ctrl-C.

```sh
cd examples/notes-api
../../bin/vo run main.vo        # serve on http://localhost:8080 (Ctrl-C stops it, exit status 0)
../../bin/vo test main.vo       # six tests and a property: the routes, and the database in memory
```

In another terminal:

```text
$ curl -d '{"text": "water the plants"}' localhost:8080/notes               # 201, Location: /notes/1
{"id":1,"text":"water the plants","done":false}
$ curl -d '{"text": "buy bread"}' localhost:8080/notes                      # 201, Location: /notes/2
{"id":2,"text":"buy bread","done":false}
$ curl localhost:8080/notes
[{"id":1,"text":"water the plants","done":false},{"id":2,"text":"buy bread","done":false}]
$ curl -X POST localhost:8080/notes/2/done
{"id":2,"text":"buy bread","done":true}
$ curl -X DELETE localhost:8080/notes/1                                    # 204
$ curl -d '{"txt": "typo"}' localhost:8080/notes                           # 400
{"error":"the body needs a \"text\" field"}
$ curl localhost:8080/nope                                                 # 404
{"error":"no such resource: /nope"}
```

| Request | Answer |
|---|---|
| `GET /notes` | 200, every note |
| `POST /notes` with `{"text": "..."}` | 201 with the new note and its `Location`; 400 for a missing, empty or too long text |
| `GET /notes/ID` | 200 with the note; 404 when there is none |
| `POST /notes/ID/done` | 200 with the note, now done |
| `DELETE /notes/ID` | 204 |
| anything else | 404 or 405, with `{"error": "..."}` |

## Requirements

| ID | Requirement |
|---|---|
| FR-001 | `GET /notes` lists every note as JSON. |
| FR-002 | `POST /notes` adds a note with the next free id, answered with 201 and its `Location`. |
| FR-003 | A missing, empty or too long text is 400 with the reason. |
| FR-004 | A note can be read, marked done and deleted. |
| FR-005 | Unknown notes and paths are 404; other methods on a resource are 405. |
| FR-006 | The notes are kept in `data/notes.db` between requests, and requests answered at once lose no change. |

Each test names the requirements it proves with `@spec("FR-...")` on the line above it; `vo query specs main.vo` lists them, and `vo test` ends with a line per requirement.

`vo query specs main.vo` lists FR-001 to FR-006; comparing the query's IDs with this table is how a
reviewer, or a tool, finds requirements without a test.

## What it shows

- **`std.http.serve_workers`**: `main` calls `serve_workers("8080", 4, open_notes, handle)`: four
  worker processes, each opening the database once (`open_notes`) and then answering one request at a
  time with `handle(db, request)`, so a slow request does not hold up the others (`vo run --workers N`
  changes the count). Ctrl-C or SIGTERM lets the requests already read finish, then `main` prints
  `stopped`.
- **State in a database, changed in transactions.** Workers share no memory, so the notes live in
  SQLite; `handle` loads them, routes and writes what changed inside one `transaction`, so two requests
  at once never lose each other's change (reading a file, changing it and writing it back would).
- **A pure core, a thin shell.** `route(request, notes)` in `api.vo` decides every answer and returns
  the notes after it; it reads and writes nothing, so its tests, next to it in `api.vo`, call it directly with
  `Request` values. `handle`, `load` and `save` work on any database (`Database[D]`), so their test uses
  one in memory.
- **Effects as the service's permissions.** `main` declares `serve("8080")` (this machine only; the
  network would need `serve("0.0.0.0:8080")`), `file.read("data/")` and `file.write("data/")`, which
  `open_notes` needs. `vo.effects` shows that only `open_notes` touches files, that `handle`, `load` and
  `save` use only the database they are given, and that `notes.vo` and `route` are pure.
- **A failing request does not stop the service**: a runtime error in a handler answers 500, its report
  goes to stderr, and the next request is answered. `--timeout` (default 10 s) limits each request, not
  the service.
- **Record and replay.** `recordings/demo.jsonl` is the session above: each worker's setup, then every
  request with its database calls and its response. `../../bin/vo replay recordings/demo.jsonl` answers
  the same requests again, one at a time, without opening a port or touching `data/` (the recorded
  database results are used); after a change to the code, each response that differs is a note on
  stderr:

  ```text
  note: the response to `GET /nope` changed: recorded 404 (36 bytes), now 404 (29 bytes)
  ```

## Files

| File | Contents |
|---|---|
| `main.vo` | `main` (serving): only the effects |
| `api.vo` | `route` (pure), `open_notes`, `handle`, `load`, `save`, and their tests |
| `notes.vo` | the `Note` record, its JSON form, adding, finding, completing and removing notes (pure), and the property of `add` |
| `data/notes.db` | the notes (created on the first run) |
| `recordings/demo.jsonl` | the recorded session, replayed by `make test` |
| `vo.effects` | the effects record, updated with `vo effects --update main.vo` |

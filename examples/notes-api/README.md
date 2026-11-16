# Notes API

A small JSON service over plain HTTP: list, add, read, complete and delete notes, kept in
`data/notes.json`. It answers on port 8080 of this machine until Ctrl-C.

```sh
cd examples/notes-api
../../bin/vo run main.vo        # serve on http://localhost:8080 (Ctrl-C stops it, exit status 0)
../../bin/vo test main.vo       # five tests and a property: the routes, without a network or files
```

In another terminal:

```text
$ curl localhost:8080/notes
[{"id":1,"text":"water the plants","done":false},{"id":2,"text":"renew the library card","done":true}]
$ curl -d '{"text": "buy bread"}' localhost:8080/notes                      # 201, Location: /notes/3
{"id":3,"text":"buy bread","done":false}
$ curl -X POST localhost:8080/notes/3/done
{"id":3,"text":"buy bread","done":true}
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

## What it shows

- **`std.http.serve`**: `main` calls `serve("8080", handle)`; `handle` is an ordinary function from a
  `Request` to a `Response`. Requests are answered one at a time; Ctrl-C or SIGTERM lets the request in
  progress finish, then `serve` returns and `main` prints `stopped`.
- **A pure core, a thin shell.** `route(request, notes)` in `api.vo` decides every answer and returns
  the notes after it; it reads and writes nothing, so the tests in `main.vo` call it directly with
  `Request` values. Only `handle` touches `data/notes.json` (load, route, save when changed).
- **Effects as the service's permissions.** `main` declares `serve("8080")` (this machine only; the
  network would need `serve("0.0.0.0:8080")`), `file.read("data/")` and `file.write("data/")`, which
  `handle` needs. `vo.effects` shows that only `handle`, `load` and `save` touch files, and that
  `notes.vo` and `route` are pure.
- **A failing request does not stop the service**: a runtime error in a handler answers 500, its report
  goes to stderr, and the next request is answered. `--timeout` (default 10 s) limits each request, not
  the service.
- **Record and replay.** `recordings/demo.jsonl` is the session above: every request, every read and
  write of `data/notes.json` and every response. `../../bin/vo replay recordings/demo.jsonl` answers the
  same requests again without opening a port or touching `data/` (the recorded file contents are used);
  after a change to the code, each response that differs is a note on stderr:

  ```text
  note: the response to `GET /nope` changed: recorded 404 (36 bytes), now 404 (29 bytes)
  ```

## Files

| File | Contents |
|---|---|
| `main.vo` | `main` (serving) and the tests |
| `api.vo` | `route` (pure), `handle`, `load`, `save` |
| `notes.vo` | the `Note` record, its JSON form, adding, finding, completing and removing notes (pure) |
| `data/notes.json` | the notes |
| `recordings/demo.jsonl` | the recorded session, replayed by `make test` |
| `vo.effects` | the effects record, updated with `vo effects --update main.vo` |

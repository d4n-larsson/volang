# Contract: std.web (spec 029)

```text
# ---- applications ----
export record App[C, E: effects] {
    routes: list[Route[C, E]];      # the operations (on_get, ..., health, info)
    steps: list[Step[C, E]];        # around every request, in order
    errors: list[ErrorStatus];      # `kind: detail` errors -> status, beside the defaults
}
export fn serve_app[C, E: effects, F: effects](address: str, workers: int, setup: fn() -> result[C] uses F,
    app: App[C, E]) -> result[void] uses serve, E, F
export fn handle_request[C, E: effects](app: App[C, E], ctx: C, r: Request) -> Response uses E
export fn web_app[C, E: effects](routes: list[Route[C, E]]) -> App[C, E]   # no steps, default errors

# ---- routes ----
export record Route[C, E: effects] { ... }     # built by these:
export fn on_get[C, I: json, E: effects](pattern: str, op: fn(C, I) -> result[Response] uses E) -> Route[C, E]
export fn on_post / on_put / on_patch / on_delete  (the same)
export record NoInput {}                        # the input of an operation that takes none

# ---- answers ----
export fn json_reply[T: json](status: int, v: T) -> Response
export fn created[T: json](location: str, v: T) -> Response  # 201 + Location
export fn no_content() -> Response                           # 204

# ---- errors ----
export record ErrorStatus { kind: str; status: int; }
export fn error_status(kind: str, status: int) -> ErrorStatus
# defaults: bad_request 400, unauthorized 401, forbidden 403, not_found 404, conflict 409, gone 410,
# unprocessable 422; an operation's err("not_found: no note 7") answers 404 with detail "no note 7".

# ---- steps ----
export record Step[C, E: effects] { prefix: str; run: fn(C, Request, fn(Request) -> Response uses E) -> Response uses E; }
export fn request_ids[C, E: effects](new_id: fn() -> str uses E) -> Step[C, E]   # request_ids(random_id)
export fn random_id() -> str uses random
export fn log_requests[C, E: effects](now: fn() -> int uses E, log: fn(str) uses E) -> Step[C, E]
export fn clock_ms() -> int uses clock                      # log_requests(clock_ms, to_stderr)
export fn to_stderr(line: str) uses console
export fn cors[C, E: effects](origins: list[str]) -> Step[C, E]
export fn bearer_auth[C, E: effects](prefix: str, check: fn(C, str) -> result[str] uses E) -> Step[C, E]
export fn api_key_auth[C, E: effects](prefix: str, header: str, check: fn(C, str) -> result[str] uses E) -> Step[C, E]

# ---- settings, health, info ----
export fn load_settings[T: json](file: str, prefix: str) -> result[T] uses env, file.read
export record HealthCheck[C, E: effects] { name: str; check: fn(C) -> result[void] uses E; }
export fn health_route[C, E: effects](checks: list[HealthCheck[C, E]]) -> Route[C, E]   # GET /health
export fn info_route[C, E: effects](name: str, version: str) -> Route[C, E]              # GET /info

# ---- tests ----
export fn request(method: str, path: str, body: str) -> Request
export fn with_header(r: Request, name: str, value: str) -> Request
```

The ready steps take their effects as functions (`request_ids(random_id)`, `log_requests(clock_ms,
to_stderr)`), so a step's type is the application's `Step[C, E]` and the effects show where the app is
assembled: an app with these steps names `random, console, clock` in its route table's type. (A type's
effect argument cannot combine a concrete effect with an effect parameter; `vo.effects` shows the rest.)
`health_route`/`info_route` are named so they do not clash with `std.fs.info`.

## Input binding (an operation's `I`)

| Field | Source | Types |
|---|---|---|
| named like a pattern variable | that path segment (percent-decoded) | `str`, `int`, `float`, `bool`, enum-like variant |
| named `body` | the JSON body (`Content-Type: application/json`) | any `json` type |
| with `@header("Name")` | that request header | `str`, `int`, `float`, `bool` |
| any other | the query parameter of that name | as path; a `list` of those takes every value |

## Answers produced by std.web

| Situation | Status |
|---|---|
| no route for the path | 404 |
| route for the path, not the method | 405, `Allow` |
| `HEAD` / `OPTIONS` | as GET without body / 204 with `Allow` |
| input missing, malformed, wrong type, unknown field | 400 |
| a well-formed body breaking only limits | 422 |
| a body on an operation without `body`; not JSON content type | 400; 415 |
| `err("kind: detail")` with a mapped kind | that status, detail |
| any other `err`, a runtime error | 500, no detail (logged to stderr) |

Problem body (`application/problem+json`):

```json
{"type":"about:blank","title":"Bad Request","status":400,"errors":[{"field":"text","reason":"missing"}]}
```

## Example

```vo
record Notes {
    db: Database[uses file.read, file.write];
}

record NoteId {
    id: int;
}

record NewNote {
    body: NoteText;
}

record NoteText {
    @size(1, 200)
    text: str;
}

fn routes() -> list[Route[Notes, uses file.read, file.write]] {
    return [
        on_get("/notes", list_notes),
        on_post("/notes", add_note),
        on_get("/notes/{id}", show_note),
        on_delete("/notes/{id}", remove_note),
        health_route([new HealthCheck { name: "database", check: db_alive }]),
    ];
}

fn show_note(app: Notes, input: NoteId) -> result[Response] uses file.read, file.write {
    let note = try find_note(app.db, input.id);
    return ok(json_reply(200, note));
}

fn main() -> result[void] uses console, file.read("data/"), file.write("data/"), serve("8080") {
    return serve_app("8080", 4, open_notes, web_app(routes()));
}
```

`find_note` returns `err("not_found: no note 7")` for a missing note: the answer is 404 with that detail.
(An app with `log_requests()` among its steps names `console, clock` in its route table's type too.)

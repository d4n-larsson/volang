# Contract: std.http workers (spec 028)

```text
# Serves requests on address with `workers` processes (1 to 64; `vo run --workers N` overrides it), each
# running setup once and then handle for one request at a time, until Ctrl-C or SIGTERM: requests already
# read are answered, then it returns ok(). A handler that stops with a runtime error answers 500.
export fn serve_workers[S, E: effects, F: effects](address: str, workers: int, setup: fn() -> result[S] uses F,
    handle: fn(S, Request) -> Response uses E) -> result[void] uses serve, E, F

export fn serve_bytes_workers[S, E: effects, F: effects](address: str, workers: int,
    setup: fn() -> result[S] uses F, handle: fn(S, BytesRequest) -> BytesResponse uses E)
    -> result[void] uses serve, E, F
```

Results:

| Situation | Result |
|---|---|
| address invalid, or not allowed | as `serve` (err, or R0010) |
| `workers` outside 1..64 (and no `--workers`) | `err("workers must be 1 to 64, got N")` |
| a setup returns `err(e)` while starting | the other workers are stopped; `err(e)` |
| a replacement worker's setup returns `err(e)` | the server shuts down as on SIGTERM; `err(e)` |
| stopped by SIGINT/SIGTERM | `ok()` after the requests already read are answered |

The setup's result stays in its worker process; each handler call of that worker receives it. Nothing
else is shared between requests.

Example (the guide's):

```vo
fn open_db() -> result[Database[uses file.read, file.write]] uses file.read("data/"), file.write("data/") {
    return open("data/app.db");
}

fn handle(db: Database[uses file.read, file.write], r: Request) -> Response uses file.read("data/"), file.write("data/") {
    ...
}

fn main() -> result[void] uses console, file.read("data/"), file.write("data/"), serve("8080") {
    return serve_workers("8080", 4, open_db, handle);
}
```

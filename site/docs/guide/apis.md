# Building APIs

`std.web` turns an HTTP API into a value: a table of routes, one function per operation, and a record
per operation for its input. Routing, 404 and 405, reading and checking the input, JSON answers and
error bodies come with it, so an operation holds only its own logic. It runs on
[`serve_workers`](concurrency.md) and is tested without a network.

## Routes and operations

An operation takes the application's context (here a shop's name, in a real API a database) and its
input, and returns a `Response` or an error:

```vo
import std.http.Response;
import std.web.App;
import std.web.ErrorStatus;
import std.web.Route;
import std.web.Step;
import std.web.handle_request;
import std.web.json_reply;
import std.web.on_get;
import std.web.request;

record Shop {
    name: str;
}

record ItemId {
    id: int;
}

record Item {
    id: int;
    name: str;
}

fn show(shop: Shop, input: ItemId) -> result[Response] {
    if input.id > 3 {
        return err("not_found: no item " + str(input.id));
    }
    return ok(json_reply(200, new Item { id: input.id, name: shop.name + " item" }));
}

fn routes() -> list[Route[Shop]] {
    return [on_get("/items/{id}", show)];
}

fn main() uses console {
    let app = new App { routes: routes(), steps: new list[Step[Shop]], errors: new list[ErrorStatus] };
    for path in ["/items/2", "/items/9", "/items/x", "/users"] {
        let r = handle_request(app, new Shop { name: "corner" }, request("GET", path, ""));
        print(str(r.status) + " " + r.body);
    }
}
```

```output
200 {"id":2,"name":"corner item"}
404 {"type":"about:blank","title":"Not Found","status":404,"detail":"no item 9"}
400 {"type":"about:blank","title":"Bad Request","status":400,"errors":[{"field":"id","reason":"expected integer, got \"x\""}]}
404 {"type":"about:blank","title":"Not Found","status":404,"detail":"no route for /users"}
```

The pattern `/items/{id}` is a string literal that `vo check` checks: a variable the input lacks is
[E0330](../errors/E0330.md), two routes matching the same paths [E0331](../errors/E0331.md). Where two
patterns differ, a literal segment wins over a variable (`/items/latest` before `/items/{id}`).

## Input

The input record's fields say where each value comes from: a field named like a path variable takes it, a
field named `body` takes the JSON body, a field with `@header("Name")` takes that header, and every other
field the query parameter of its name. Annotations above a field limit it (`@size`, `@range`, `@pattern`)
or make it optional (`@default`). Input that does not fit never reaches the operation: the client gets
400 (or 422 when only limits failed) naming every field. The body is converted with `from_json`, so it can
be any record, list or variant:

```vo
import std.http.Response;
import std.web.App;
import std.web.ErrorStatus;
import std.web.Route;
import std.web.Step;
import std.web.created;
import std.web.handle_request;
import std.web.on_post;
import std.web.request;

record NewItem {
    @size(1, 30)
    name: str;
    @range(0, 1000)
    @default(1)
    count: int;
}

record AddItem {
    body: NewItem;
}

fn add(n: int, input: AddItem) -> result[Response] {
    return ok(created("/items/7", input.body));
}

fn main() uses console {
    let routes: list[Route[int]] = [on_post("/items", add)];
    let app = new App { routes: routes, steps: new list[Step[int]], errors: new list[ErrorStatus] };
    for body in ["{\"name\": \"cup\"}", "{\"name\": \"\", \"count\": 5000}", "{\"nam\": \"cup\"}"] {
        let r = handle_request(app, 0, request("POST", "/items", body));
        print(str(r.status) + " " + r.body);
    }
}
```

```output
201 {"name":"cup","count":1}
422 {"type":"about:blank","title":"Unprocessable Content","status":422,"errors":[{"field":"name","reason":"at least 1 character, got 0"},{"field":"count","reason":"at most 1000"}]}
400 {"type":"about:blank","title":"Bad Request","status":400,"errors":[{"field":"nam","reason":"unknown field"},{"field":"name","reason":"missing"}]}
```

## Errors and answers

`json_reply(status, v)` answers with any record, list or value as JSON; `created(location, v)` 201;
`no_content()` 204; a `Response` built by hand passes through. An error is `err("kind: detail")`, from
anywhere below the operation with `try`: `bad_request` 400, `unauthorized` 401, `forbidden` 403,
`not_found` 404, `conflict` 409, `gone` 410, `unprocessable` 422, plus the application's own
`error_status("quota", 429)` in `App.errors`. Any other error, and a runtime error, answers 500 with no
details for the client. Every failure is an RFC 9457 problem (`application/problem+json`).

## Steps, settings and health

Steps run around requests in list order: `request_ids(random_id)`, `log_requests(clock_ms, to_stderr)`,
`cors(origins)`, `bearer_auth(prefix, check)` and `api_key_auth(prefix, header, check)` (the checked
identity reaches operations as the header `X-Identity`), or your own `Step`. The ready steps take their
effects as functions, so the application's type names them all: an app with `log_requests` declares
`console, clock` in its route table's type. `load_settings("settings.json", "app")` reads a record of
settings from a JSON file and `APP_FIELD` environment variables, with defaults and limits from its
annotations. `health_route(checks)` and `info_route(name, version)` add `GET /health` and `GET /info`.

## Serving and testing

`serve_app("8080", 4, open_db, app)` serves the application with four worker processes, each with the
context `open_db` returns (a database connection per worker). Tests call `handle_request(app, ctx,
request(method, path, body))` with an in-memory database: the same steps, routes and checks, no port.
The [notes-api](../examples/notes-api.md) example is a complete API with SQLite.

# Building APIs

`std.web` turns an HTTP API into a value: a table of routes, one function per operation, and a record
per operation for its input. Routing, 404 and 405, reading and checking the input, JSON answers and
error bodies come with it, so an operation holds only its own logic. It runs on
[`serve_workers`](concurrency.md) and is tested without a network.

## Routes and operations

An operation takes the application's context (here a shop's name, in a real API a database) and its
input, and returns a `Response` or an error:

```vo
import std.http;
import std.web;

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

fn show(shop: Shop, input: ItemId) -> result[http.Response] {
    if input.id > 3 {
        return err("not_found: no item " + str(input.id));
    }
    return ok(web.json_reply(200, new Item { id: input.id, name: shop.name + " item" }));
}

fn routes() -> list[web.Route[Shop]] {
    return [web.on_get("/items/{id}", show)];
}

fn main() uses console {
    let app = new web.App {
        routes: routes(),
        steps: new list[web.Step[Shop]],
        errors: new list[web.ErrorStatus]
    };
    for path in ["/items/2", "/items/9", "/items/x", "/users"] {
        let r = web.handle_request(app, new Shop { name: "corner" }, web.request("GET", path, ""));
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
import std.http;
import std.web;

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

fn add(n: int, input: AddItem) -> result[http.Response] {
    return ok(web.created("/items/7", input.body));
}

fn main() uses console {
    let routes: list[web.Route[int]] = [web.on_post("/items", add)];
    let app = new web.App { routes: routes, steps: new list[web.Step[int]], errors: new list[web.ErrorStatus] };
    for body in [`{"name": "cup"}`, `{"name": "", "count": 5000}`, `{"nam": "cup"}`] {
        let r = web.handle_request(app, 0, web.request("POST", "/items", body));
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

An operation can instead fail with its own error type. Wrap it with `typed(op, answer)`, where `answer`
maps each error to a `Problem { status, detail }`. Because `answer` is one function with a `match`, the
checker sees that every way to fail has a status:

```vo
import std.http;
import std.web;

variant NoteError {
    not_found(int);
}

record ById {
    id: int;
}

fn note_problem(e: NoteError) -> web.Problem {
    match e {
        case not_found(id) {
            return new web.Problem { status: 404, detail: "no note " + str(id) };
        }
    }
}

fn show(ctx: int, input: ById) -> result[http.Response, NoteError] {
    if input.id == 1 {
        return ok(web.json_reply(200, "first"));
    }
    return err(NoteError.not_found(input.id));
}

fn main() uses console {
    let app = web.web_app([web.on_get("/notes/{id}", web.typed(show, note_problem))]);
    let r = web.handle_request(app, 0, web.request("GET", "/notes/5", ""));
    print(str(r.status) + " " + r.body);
}
```

```output
404 {"type":"about:blank","title":"Not Found","status":404,"detail":"no note 5"}
```

## Steps, settings and health

Steps run around requests in list order: `request_ids(random_id)`, `log_requests(fn() -> int { return now_ms(); }, to_stderr)`,
`cors(origins)`, `bearer_auth(prefix, check)` and `api_key_auth(prefix, header, check)` (the checked
identity reaches operations as the header `X-Identity`), or your own `Step`. The ready steps take their
effects as functions, so the application's type names them all: an app with `log_requests` declares
`console, clock` in its route table's type. `load_settings("settings.json", "app")` reads a record of
settings from a JSON file and `APP_FIELD` environment variables, with defaults and limits from its
annotations. `health_route(checks)` and `info_route(name, version)` add `GET /health` and `GET /info`.

## Access control

`std.security` puts one table in front of the application: who may call what. `secure(policy)` is a
step; the first rule that matches a request's method and path decides, and a request no rule matches
is refused. A rule's path is a prefix of whole segments: `/admin` covers `/admin/users/{id}`, not
`/administrator`. A refused request is answered 401 when nobody is signed in and 403 when the user is
known and may not, and its operation does not run.

Programs sign in with tokens, keys or Basic credentials (below); people in a browser sign in with a
form and a session cookie, the `browser` way (Signing in browsers, below), under the same rules.

```vo
import std.http;
import std.security;
import std.web;

record Caller {
    @header("X-Identity")
    @default("")
    identity: str;
}

fn up(n: int, input: web.NoInput) -> result[http.Response] {
    return ok(web.json_reply(200, "up"));
}

fn hello(n: int, input: Caller) -> result[http.Response] {
    let user = try security.require_user(input.identity);
    return ok(web.json_reply(200, "hello " + user.name));
}

fn rules() -> list[security.Rule] {
    return [
        security.rule("/health", security.Access.anyone),
        security.rule_for(["GET"], "/notes", security.Access.signed_in),
        security.rule("/admin", security.Access.any_role(["admin"]))
    ];
}

fn main() uses console {
    let routes: list[web.Route[int]] = [web.on_get("/health", up), web.on_get("/notes", hello), web.on_get("/admin/users", hello), web.on_get("/reports", hello)];
    let ways: list[security.Way[int]] = [security.test_users()];
    let p = security.policy(rules(), ways, fn() -> int { return 0; });
    let app = new web.App { routes: routes, steps: [security.secure(p)], errors: new list[web.ErrorStatus] };
    let bo = new security.User { name: "bo", roles: ["user"] };
    for rt in routes {
        let r = web.request("GET", rt.pattern, "");
        let nobody = web.handle_request(app, 0, r).status;
        let as_bo = web.handle_request(app, 0, security.as_user(r, bo));
        print(format("{}: nobody {}, bo {}", rt.pattern, nobody, as_bo.status));
    }
    print(security.describe(app, rules()));
    for finding in security.review(app, p) {
        print("review: " + finding);
    }
}
```

```output
/health: nobody 200, bo 200
/notes: nobody 401, bo 200
/admin/users: nobody 401, bo 403
/reports: nobody 401, bo 403
GET /health: anyone
GET /notes: signed in
GET /admin/users: roles admin
GET /reports: nobody (no rule)
review: no rule for GET /reports
review: test_users is accepted
```

**The user in an operation.** The step passes the signed-in user on in the request header
`X-Identity`, which a client cannot set. An operation declares the field as `Caller` does above and
calls `require_user(identity)` (401 when nobody is signed in), `require_role(identity, roles)` (403
without one of them) or `current_user(identity)` (an `Option[User]`, for routes open to anyone).
Rules by route cannot say "only the owner": that is an `if` in the operation, with `has_role(user,
"admin")` for the exception, answered with `err("forbidden: not your note")`.

**Ways of signing in** are the second argument of `policy`:

| Way | The caller sends | Checked by |
|---|---|---|
| `tokens([key])` | `Authorization: Bearer TOKEN` | the module: a signed token (JWT, HMAC-SHA256) with the name, the roles and an expiry |
| `basic(find_account)` | `Authorization: Basic ...` | the module, against the `Account` your function finds for the name |
| `header_key("X-Api-Key", check)` | the key in that header | your function, which returns a `User` |
| `test_users()` | what `as_user(request, user)` adds | nothing: for tests only |

`sign_in_route("/sign-in", policy, find_account, key, 3600)` is a ready route that exchanges
`{"name": ..., "password": ...}` for `{"token": ..., "expires": ...}`; give it a rule that opens it to
anyone. The key is 32 bytes or more that only the application knows (`std.crypto.random_bytes(32)`,
kept in its settings); to change it, list the new key first and keep the old one until its tokens
have expired. Passwords are stored as `std.crypto.hash_password` makes them. An unknown name, a wrong
password and a disabled account get the same answer after the same work.

- Basic checks the password on every request, which takes about half a second on purpose. Use it for
  occasional calls, and the sign-in route with tokens for everything else.
- Credentials that are present and wrong are answered 401 on every route, the open ones included; so
  is a request with credentials for two ways at once. A client that sends an expired token along with
  its sign-in request is refused before the sign-in runs.
- Put `cors` before `secure` in `App.steps`, so that preflight requests (which carry no credentials)
  are answered; put `safe_headers(0)` first, so that refusals also get `X-Content-Type-Options`,
  `X-Frame-Options`, `Referrer-Policy` and `Cache-Control: no-store`.
- `new Policy { rules: ..., ways: ..., now: fn() -> int { return now_ms(); }, on_event: log }` names a function that is told of
  every refused credential, forbidden request and sign-in (an `Event`: kind, reason, name, method,
  path, time), never with a password, a key or a token in it.
- A password hash made with a lower cost than `hash_password` uses now is reported as the event
  `outdated_hash`. To replace such hashes, write the sign-in operation yourself: `password_user(ctx,
  find_account, name, password)` gives the user, `std.crypto.password_outdated(hash)` says whether to
  store `hash_password(password)`, and `issue_token(key, user, now_ms, lifetime_s)` makes the token.

**Reviewing and testing.** `decide(rules, method, path, caller)` is the step's decision as a pure
function, for tests and properties. `describe(app, rules)` is the table above: keep its text in a
test, and a change of access shows up in a diff. `review(app, policy)` lists what deserves a look:
routes no rule matches, rules that decide for no route (misspelt, or hidden behind an earlier rule),
rules that decide for only some requests of a route, and `test_users` in the policy. Tests add
`test_users()` to the ways and send `as_user(request, user)`; an application that does not list it
ignores what `as_user` adds. The [secure-notes](../examples/secure-notes.md) example is a complete
service with these tests.

`bearer_auth` and `api_key_auth` (above) remain the short form when one prefix is guarded by one
check and there are no roles.

### Signing in browsers

A service behind a frontend signs people in with the frontend's form and a session cookie. In
`std.security` that is one more way of signing in beside tokens and Basic: `browser(b)`, listed in the
policy, with the same rules and the same `require_user` in operations. The service serves no pages:
the frontend shows its sign-in page at the sign-in path and posts `name`, `password` and `next` there.
The application says in one `Browser` record how it keeps accounts and sessions (functions over its
context), the two paths and how long a session lasts; `browser_routes(b, p)` adds the two routes.

```text
fn browsers[E: effects](random: fn(int) -> bytes uses E) -> Browser[Store, E] {
    return new Browser {
        find_account: find_account,
        keep: fn(store: Store, s: Session) -> result[void] { set store.sessions[s.key] = s; return ok(); },
        find: fn(store: Store, key: str) -> result[Option[Session]] { ... },
        end: fn(store: Store, key: str) -> result[void] { remove(store.sessions, key); return ok(); },
        random: random,                     # std.crypto.random_bytes
        sign_in: "/sign-in",
        sign_out: "/sign-out",
        lifetime_ms: 8 * 3600 * 1000,
        origin: ""
    };
}
```

- **Signing in**: a right name and password answer 303 to `next` with the session cookie; a wrong one
  answers 303 to `/sign-in?failed=1&next=...` with no cookie, the same for an unknown name or a
  disabled account and after the same password work, so the frontend's page says one message and the
  address tells nothing. A script posts the same form encoding (`URLSearchParams`) and sees where it is
  sent. Programs keep the token route (`sign_in_route`).
- **The cookie** is `__Host-session`, `HttpOnly`, `Secure` (HTTPS only, and `http://localhost` in
  Chrome and Firefox) and `SameSite=Lax`. Its value is 16 random bytes made at each sign-in; storage
  gets only its SHA-256, so a copy of the database signs nobody in.
- **Pages and APIs**: a browser `GET` that accepts HTML and needs a user is sent to the sign-in path
  with the path to return to; the same request from a program still gets 401. The return path is
  followed only when it is a path of the application.
- **Other sites' forms**: a request signed in by the cookie that changes something (any method but
  `GET`, `HEAD`, `OPTIONS`), and every post of the sign-in form, must come from the application's own
  pages (the browser's `Sec-Fetch-Site`, else `Origin`, compared with the host the request names), or
  it is refused with 403 before the operation runs. Behind a proxy, set `origin` to the address people
  use, such as `"https://notes.example"`.
- **Ending sessions**: sign-out ends the session in storage, so a copied cookie stops working at once;
  a session also ends when its lifetime is over, and the account is looked up on every request.
- **Events**: signing in, refusals (`wrong password`, `expired`, `cross-site`, ...) and signing out go to
  the policy's `on_event`, never with a password or a cookie value. Nothing limits repeated guessing;
  count the `refused` events by name to slow or lock an account.

`review` names the browser way with its settings, and reports a sign-in path the rules close to posts
and an `origin` starting with `http://` (Safari keeps `Secure` cookies only over HTTPS, so developing
there needs `origin: "http://localhost:8080"`). A recording of a signed-in service holds the passwords
posted and the cookies: keep it as secret as the database. Cookies of the application's own go through
`std.web`: `read_cookie(header, name)` on a field `@header("Cookie") @default("") cookies: str;`, and
`with_cookie(answer, cookie(name, value))`, with the same safe flags. The
[secure-notes](../examples/secure-notes.md) example signs in programs and browsers under one table.

## Serving and testing

`serve_app("8080", 4, open_db, app)` serves the application with four worker processes, each with the
context `open_db` returns (a database connection per worker). Tests call `handle_request(app, ctx,
request(method, path, body))` with an in-memory database: the same steps, routes and checks, no port.
The [notes-api](../examples/notes-api.md) example is a complete API with SQLite. Operations read HTML form bodies (`application/x-www-form-urlencoded`) as they read query parameters (a form leaves out an unchecked box: a missing `bool` is `false`, and `on` is `true`); the pages themselves are the frontend's.

# Secure notes

A JSON service whose access rules are one table, built with `std.security` on `std.web`: anyone may
ask for its health and sign in, signed-in users keep notes of their own, and an admin lists the users
and may delete any note. A route the table does not mention is refused.

![Secure notes. Every request, with or without a token, passes rules(), which has one line per route.
A request with no valid token on a route that needs one is answered 401 Unauthorized; a signed-in
caller the rule does not allow is answered 403 Forbidden; otherwise the operation runs with the
caller's identity.](diagram.svg)

## Run it

```sh
cd examples/secure-notes
../../bin/vo run main.vo        # serve on http://localhost:8080 (Ctrl-C stops it)
../../bin/vo test main.vo       # six tests: every route as nobody, a user and an admin; no network
```

## The rules

The table is `rules()` in `notes.vo`:

<!-- code: notes.vo fn rules -->

It prints as:

```text
GET /health: anyone
POST /token: anyone
POST /sign-in: anyone
POST /sign-out: anyone
GET /notes: signed in
POST /notes: signed in
DELETE /notes/{id}: signed in
GET /admin/users: roles admin
```

That text is what `describe` prints, and a test compares it, so a change of access shows in a diff.
Another test asks `review` for routes without a rule and rules that never apply, and expects none.

## Try it

In another terminal (the accounts are `ada`, an admin, with the password `correct horse`, and `bo`
with `battery staple`):

```text
$ J='Content-Type: application/json'
$ curl localhost:8080/notes                                                   # 401, WWW-Authenticate: Bearer
{"type":"about:blank","title":"Unauthorized","status":401,"detail":"sign in"}
$ curl -H "$J" -d '{"name": "bo", "password": "battery staple"}' localhost:8080/token
{"token":"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...","expires":1795262400}
$ T=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
$ curl -H "$J" -H "Authorization: Bearer $T" -d '{"text": "buy bread"}' localhost:8080/notes
{"id":1,"owner":"bo","text":"buy bread"}
$ curl -H "Authorization: Bearer $T" localhost:8080/admin/users               # 403: bo is no admin
{"type":"about:blank","title":"Forbidden","status":403,"detail":"not allowed"}
$ curl -H 'X-Identity: {"name":"ada","roles":["admin"]}' localhost:8080/admin/users   # 401: not believed
{"type":"about:blank","title":"Unauthorized","status":401,"detail":"sign in"}
```

## How it works

### The operations do not parse headers

Each declares the field `identity` and calls `require_user`; "only the owner or an admin deletes a
note" is an `if` in `erase`.

<!-- code: notes.vo fn erase -->

### Tests sign in without passwords

The test application adds `test_users()` to the ways of signing in, and `as_user(request, user)` makes a
request as that user. The served application does not list it, so the header it uses means nothing
there.

<!-- code: notes.vo fn test_app -->
<!-- code: notes.vo fn call -->
<!-- code: notes.vo test "who may call what" -->

### The policy is built once

`notes_policy` builds it, for the step and for the token route.

<!-- code: notes.vo fn notes_policy -->

### Browsers sign in with a session cookie

Programs exchange a password for a token at `/token`. Browsers sign in with the form of a frontend:
this service serves no pages. The frontend shows its sign-in page at `/sign-in` and posts `name`,
`password` and `next` there; the answer is a 303 back to `next` with a session cookie, or to
`/sign-in?failed=1&next=...` when the name or password is wrong. `/sign-out` ends the session. Both
are `browser_routes`, and `browser(...)` is the second way of signing in in the same policy, so a
cookie reaches the same operations under the same rules as a token. `browsers` in `store.vo` says
where the sessions are kept (in the store, as the notes) and for how long:

<!-- code: store.vo fn browsers -->

The library does the rest: the cookie is `HttpOnly`, `Secure` and `SameSite=Lax` and its value is new
at every sign-in (the store keeps only its hash), a cookie-signed request that changes something must
come from the service's own origin (else 403), and a browser asking for a page that needs a user is
sent to `/sign-in`. The APIs guide, under "Access control", lists the threats and their answers.

### What it leaves out

Notes and sessions are kept in memory by one worker (`store.vo`), and the key that signs the tokens
is made at start, so a restart forgets the notes and signs everybody out. A real service keeps both in its storage and
settings (see [notes-api](../notes-api/) for SQLite).

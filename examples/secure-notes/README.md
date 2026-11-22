# Secure notes

A JSON service whose access rules are one table, built with `std.security` on `std.web`: anyone may
ask for its health and sign in, signed-in users keep notes of their own, and an admin lists the users
and may delete any note. A route the table does not mention is refused.

```sh
cd examples/secure-notes
../../bin/vo run main.vo        # serve on http://localhost:8080 (Ctrl-C stops it)
../../bin/vo test main.vo       # six tests: every route as nobody, a user and an admin; no network
```

The table is `rules()` in `notes.vo`:

```text
GET /health: anyone
POST /sign-in: anyone
GET /notes: signed in
POST /notes: signed in
DELETE /notes/{id}: signed in
GET /admin/users: roles admin
```

That text is what `describe` prints, and a test compares it, so a change of access shows in a diff.
Another test asks `review` for routes without a rule and rules that never apply, and expects none.

In another terminal (the accounts are `ada`, an admin, with the password `correct horse`, and `bo`
with `battery staple`):

```text
$ J='Content-Type: application/json'
$ curl localhost:8080/notes                                                   # 401, WWW-Authenticate: Bearer
{"type":"about:blank","title":"Unauthorized","status":401,"detail":"sign in"}
$ curl -H "$J" -d '{"name": "bo", "password": "battery staple"}' localhost:8080/sign-in
{"token":"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...","expires":1795262400}
$ T=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
$ curl -H "$J" -H "Authorization: Bearer $T" -d '{"text": "buy bread"}' localhost:8080/notes
{"id":1,"owner":"bo","text":"buy bread"}
$ curl -H "Authorization: Bearer $T" localhost:8080/admin/users               # 403: bo is no admin
{"type":"about:blank","title":"Forbidden","status":403,"detail":"not allowed"}
$ curl -H 'X-Identity: {"name":"ada","roles":["admin"]}' localhost:8080/admin/users   # 401: not believed
{"type":"about:blank","title":"Unauthorized","status":401,"detail":"sign in"}
```

What to look at:

- **The operations do not parse headers.** Each declares the field `identity` and calls
  `require_user`; "only the owner or an admin deletes a note" is an `if` in `erase`.
- **Tests sign in without passwords.** The test application adds `test_users()` to the ways of signing
  in, and `as_user(request, user)` makes a request as that user. The served application does not list
  it, so the header it uses means nothing there.
- **The policy is built once**, by `notes_policy`, for the step and for the sign-in route.
- Notes are kept in memory by one worker, and the key that signs the tokens is made at start, so a
  restart forgets the notes and signs everybody out. A real service keeps both in its storage and
  settings (see [notes-api](../notes-api/) for SQLite).

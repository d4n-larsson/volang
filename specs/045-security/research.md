# Research: Security (spec 045)

## R1: Speed, and no primitive

**Decision**: the module is written in vo. Nothing is added to the C side beyond registering it.

**Rationale**: a prototype (a step that finds the `Authorization: Bearer` header, checks a JWT with
`hmac_sha256`, `from_base64` and `parse_json`, and passes the user on as JSON) was measured through
`handle_request` on x86-64: 10,000 requests take about 70 ms without the step and 170 ms with it,
so the step costs about 10 microseconds a request against SC-005's bound of 100. The token it issued
verifies with Python's `hmac` and `base64`.

URL-safe base64 is the standard form with `+` and `/` replaced by `-` and `_` and the padding
removed; it is written with `split` and `join` on the text of `to_base64`/`from_base64`.

**Alternatives considered**: a `__base64url` builtin (not needed at this cost).

## R2: A way of signing in is one record

**Decision**: every way of signing in is a value of one record: the request header it reads, the
scheme its value starts with (`"Basic "`, `"Bearer "` or `""`), a name for events, and a function from
the context, the credential text and the time to a user.

- `basic(find)`: `Authorization`, `Basic `; decodes `name:password`, checks as R4.
- `tokens(keys)`: `Authorization`, `Bearer `; checks as R5.
- `header_key(header, check)`: the named header, no scheme; the application's function.
- `test_users()`: header `X-Test-User`, no scheme; the value is a user as JSON. `as_user(request,
  user)` adds it.

The function's errors follow `std.web`'s kinds: `err("unauthorized: REASON")` is a refused credential
(401, the reason goes to the event only); any other error is a failure of the application (500).

**Rationale**: the step is then one loop for all four, "two ways at once" is a count, and the test way
is visibly one more item of the list (FR-018). A variant with one case per way would need a `match`
in the step and cannot hold the application's own check without a function anyway.

A request is examined so: each accepted way whose header is present with its scheme is a candidate.
No candidate and no `Authorization` header: nobody. An `Authorization` header that is no candidate
(another scheme): 401. Two candidates: 401. One: its function decides.

## R3: How the user reaches the operation

**Decision**: the step puts the user, as JSON (`{"name":"ada","roles":["admin"]}`), in the request
header `X-Identity`, which `handle_request` already removes from every incoming request. An operation
declares `@header("X-Identity") @default("") identity: str;` and calls `current_user(identity)`
(an `Option[User]`), `require_user(identity)` (a result: `unauthorized` without a user) or
`require_role(identity, roles)` (a result: `forbidden` without one of the roles).

**Rationale**: no change to `std.web`'s binding, and FR-014's "removed whether or not the step runs"
is already true. A field of type `Option[User]` bound straight from the header was tried: JSON
deriving has no form for an absent `Option` field, and header values bind as text, so it would need
new binding rules in `std.web` for one header. Operations written for `bearer_auth` use the same
field; there the header holds a bare name, which `current_user` reads as a user with no roles.

**Alternatives considered**: a second context type wrapping the application's `C` with the user
(changes the type of every operation).

## R4: Passwords

**Decision**: one private function checks a name and password against the application's lookup
`fn(C, str) -> result[Option[Account]]`, exported as `password_user` for applications that write
their own sign-in operation:

1. look the name up; an error of the lookup is passed on (500);
2. `verify_password(password, hash)`, where `hash` is the account's, or for an unknown name a constant
   hash at the current cost, so the work is the same;
3. unknown, wrong and disabled all give `err("unauthorized: invalid credentials")`; the event's
   reason tells them apart.

`std.crypto` gains `password_outdated(stored) -> bool`: true when the text is a hash of a lower cost
than `hash_password` makes now. The ready sign-in route reports it as an event (`outdated_hash`); an
application that wants to replace hashes writes its own sign-in operation with `password_user`,
`password_outdated`, `hash_password` and `issue_token` (the guide shows it: about ten lines). The
module itself never makes a hash, so it needs no `entropy` (FR-013).

**Measured**: one `verify_password` at the current cost (600,000 rounds) takes about 510 ms on the
development machine. Basic therefore costs half a second a request by design; tests use hashes of
cost 1 except where the timing is the subject (SC-004, a handful of checks).

**Alternatives considered**: a callback receiving the password or a new hash (the first hands out
passwords, the second needs `entropy` inside a step whose effects are the application's `E`).

## R5: Tokens

**Decision**: compact JWT. Header `{"alg":"HS256","typ":"JWT"}`; claims `sub`, `roles`, `iat`, `exp`
in seconds. Checking, in this order: three parts; the signature equals `hmac_sha256(key, head.body)`
for one of the keys (`same_bytes`, every key tried); the header's `alg` is `HS256`; `sub` is a text,
`roles` a list of texts, `exp` an integer greater than now; `nbf`, if present, an integer not greater
than now. Other claims are ignored (read with `std.json`'s field functions, not by deriving a record,
which refuses unknown fields). Every failure is `unauthorized` with its own reason.

Key length (32 bytes or more) and lifetime (positive) are `requires` clauses on `tokens`,
`issue_token` and `sign_in_route`: R0012 at the call, when the policy is built.

The time is milliseconds, as `now_ms` and `std.web.log_requests` use; tokens carry seconds.

## R6: Matching, the decision, review and description

**Decision**: a rule's path is segments, each a literal or `{variable}`; it matches a request path
whose first segments match them (a literal equals the segment as sent, a variable matches any
non-empty segment). `/` matches everything. `HEAD` counts as `GET`, as in the router. Literals are
compared undecoded, as the router compares them.

`decide(rules, method, path, caller)` is pure and takes only the rules, so it needs no type
parameters. The step calls it after the caller is known.

`describe(app, rules)` and `review(app, policy)` call `decide` with each route's own pattern as the
path (a `{variable}` segment matches a rule's variable, and no rule literal). The review's findings
are lines of text:

- `no rule for METHOD PATTERN`;
- `rule N (PATH) decides for no route`: misspelt or hidden behind an earlier rule;
- `rule N (PATH) decides for only some requests of METHOD PATTERN`: a rule literal where the route
  has a variable, the one case in which the description of a route is not the whole truth;
- `rule N (PATH) needs a user, and no way of signing in is accepted`;
- `test_users is accepted`.

**Rationale**: prefix matching only (no "exactly this path" form) keeps one rule form; what a prefix
opens is visible per route in the description.

## R7: Answers

**Decision**: `std.web` exports `problem_reply(status, detail)`, its existing private `problem`
without the list of errors; the step answers 401 (`sign in` or `invalid credentials`), 403 (`not
allowed`) and 500 with it. 401 answers carry one `WWW-Authenticate` per accepted scheme: `Basic
realm="api"`, `Bearer`.

**Rationale**: reuse instead of a second copy of the problem format. It is a new export of an
existing module: no existing behaviour changes.

## R8: Tests

- Golden cases in `tests/cases/web_security/` (the directory `tests/cases/security` belongs to the
  security review): `policy` (story 1), `sign_in` (story 2), `user` (story 3), `review` (story 4),
  `headers` (story 5), `short_key` (R0012).
- `tests/web_security.py` (in `make test`), groups: `tokens` (SC-002: 1,000 each way against Python's
  `hmac`, `hashlib`, `base64` and `json`; SC-003: 10,000 damaged tokens through one vo driver reading
  a file), `timing` (SC-004) and `speed` (SC-005); the last two are skipped when `CI` is set or under
  emulation.
- SC-001 and SC-007 are tests of the example (`vo test`): a loop over routes and callers comparing
  `handle_request` with `decide`, and the description compared with the text in the test.
- Area `web_security` in `tools/quick.sh`.

## R9: The example

**Decision**: a new example, `examples/secure-notes/`: notes with an owner; open health route, a
sign-in route, signed-in users read and write their notes, an admin lists users; Basic is not
accepted (tokens and the test way in tests only). `notes-api` stays as it is: it shows routing by
hand on `std.http`.

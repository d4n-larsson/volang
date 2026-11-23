# Research: Sessions and Sign-In Forms

Decisions for spec 052, from reading `std/security.vo` (spec 045), `std/web.vo` (specs 029, 051), the
notes-pages example and a server recording. Q1 and Q2 of the spec were answered by the user; the rest
is decided here.

## R1. A browser is one more `Way`, configured by one record

**Decision**: `std.security` gets a record `Browser[C, E]` that says everything about signing in
browsers (the application's account and session functions, the sign-in page, the two paths, the
lifetime, the origin), and two functions over it: `browser(b)`, the `Way` listed in the policy beside
`tokens` and `basic`, and `browser_routes(b, p)`, the three routes (show the sign-in page, take the
sign-in form, sign out) that the application appends to its own.

**Rationale**: the spec's FR-004 (one table of rules, the same `X-Identity`) holds by construction: the
way's `check` returns a `User` like every other way, and `secure` does the rest. One record keeps the
application's part to one `new Browser { ... }` and two calls (SC-004). Routes made by the library,
like `sign_in_route` today, mean the application writes no cookie, origin or session code.

**Alternatives considered**: a separate `sessions` step in front of `secure` (a second access-control
path: rejected by Principle I); building the routes in the application from smaller helpers (each
application would set cookies and check origins itself, which is what the spec exists to avoid).

## R2. `Way` gains two fields; the cookie is read by `secure`

**Decision**: `Way` gets `sign_in_page: str` and `origin: str`, both `""` for the existing ways. A way
whose `header` is `Cookie` takes its credential from the cookie named by its `scheme` (for `browser`,
`__Host-session`). `secure` uses the two fields for the page redirect (R6) and the cross-site check
(R5). The way's `name` carries what `review` prints (R9).

**Rationale**: `secure` is the only place that sees the request, the way and the decision together;
the way's `check` sees only the credential. The doc comment of `Way` already says it is made by the
library's functions; no program in the repository builds one with `new` (searched), so adding fields
breaks nothing in practice; the changelog says so.

**Alternatives considered**: a field `browser: Option[...]` (a second record for two strings); adding
the settings to `Policy` (programs do write `new Policy { ..., on_event: f }`, as the doc of `policy`
tells them to, so a new field would break them).

## R3. Sessions kept by the application, storage holds a hash (Q1)

**Decision**: the cookie's value is 16 random bytes from `b.random` (the application passes
`std.crypto.random_bytes`) as 32 hex characters. Storage gets a `Session { key, name, expires_ms }`
whose `key` is the hex SHA-256 of the value; `keep`, `find` (by key) and `end` (by key) are the
application's functions, as `find_account` is. On each request the way finds the session, refuses it
when expired (and ends it), then calls `find_account` for the name, refusing an unknown or disabled
account; the user's roles come from the account, so a changed role or a disabled account applies at
the next request. The lifetime is fixed from sign-in (no sliding renewal). `random` is called with
16 and its result is refused (a 500, as other failures of the application's functions) when it is not
16 bytes.

**Rationale**: the user chose stored sessions so sign-out works at once. Hashing the key means a copy
of the sessions table signs nobody in, and session values do not appear in recorded SQL (`vo run
--record` records statements and parameters). Randomness is passed in, as `request_ids(new_id)` and
`policy(..., now)` take theirs, so the library's functions stay generic over the application's effect
set `E` and `vo test` stays deterministic (`random_bytes` gives the same bytes on every test run).

**Alternatives considered**: the cookie as a signed token (rejected by Q1); storing the raw value
(a database leak becomes a session leak); sliding expiry (more writes per request, and nothing in the
spec asks for it; a later change if people want it).

## R4. Cookies in `std.web`

**Decision**: `record Cookie { name, value, max_age_s, http_only, secure, same_site, path }`;
`cookie(name, value)` gives one with `HttpOnly`, `Secure`, `SameSite=Lax`, `Path=/`, kept until the
browser closes (`max_age_s` -1); `with_cookie(resp, c) -> result[Response]` adds its `Set-Cookie`;
`read_cookie(header, name) -> Option[str]` reads a `Cookie` header's text. Turning a default off is
`set c.http_only = false;` on the record, written at the call (FR-002). Removing is `max_age_s` 0.
`with_cookie` refuses a name that is not an RFC 6265 token, a value with bytes outside cookie-octets,
`same_site` other than `Lax`, `Strict`, `None`, `None` without `secure`, and a `__Host-` name without
`secure` or with a path other than `/`. `read_cookie` takes the header's text because operations get
their input record, not the request (`@header("Cookie") @default("") cookies: str;`).

**Rationale**: the smallest surface that covers User Story 4; the record makes every flag visible and
the defaults safe; errors as values (FR-003).

**Alternatives considered**: a `@cookie("name")` annotation on input fields (checker change and
reference text for something one function does); options as a list of flags (a second spelling of
record fields).

## R5. Cross-site protection by origin

**Decision**: for a request signed in by the browser way with a method other than `GET`, `HEAD`,
`OPTIONS`, and for every post to the sign-in route (login forgery), the request is allowed when
`Sec-Fetch-Site` is `same-origin` (or `none`, a user typing or bookmarking), or, without it, when the
`Origin` header names the application's origin; anything else, including a request with neither
header, is refused with 403 and a `refused` event with reason `cross-site`. The application's origin
is `b.origin` when set, else the host of the request's `Host` header (compared with `Origin`'s
host and port; the scheme is not compared, since behind a TLS proxy the application sees plain HTTP).
The rule is one pure function, tested on its own with a table.

**Rationale**: the order and the headers are those of Go 1.25's `CrossOriginProtection` and the
OWASP cheat sheet's "Fetch Metadata" defence; every current browser sends `Origin` on cross-origin
and same-origin `POST`s and `Sec-Fetch-Site` on all requests. Unlike Go, a request with neither header
is refused when it carries the session cookie: only a browser has that cookie, and a browser that sends
it says where the request came from. Nothing goes into forms (FR-012), so the fragments of spec 051
need nothing either. `SameSite=Lax` is a second layer.

**Alternatives considered**: a synchronizer token in every form (every template must carry it; the
swap script must too); double-submit cookie (the same plus a script-readable cookie).

## R6. Pages are sent to sign-in, APIs get 401

**Decision**: when the decision is "sign in needed", the method is `GET` or `HEAD`, the `Accept` header
contains `text/html`, and a way of the policy has a `sign_in_page`, `secure` answers `303` with
`Location: SIGN_IN?next=PATH` (path and query, percent-encoded); otherwise 401 as today. A browser
cookie that is refused (expired, ended, unknown, account gone) counts as no credential, so public
pages, the sign-in page among them, still work, and the answer also removes the stale cookie.

**Rationale**: browsers send `Accept: text/html,...` for navigations and programs rarely do; no route
needs marking (spec assumption). Treating a refused cookie as none avoids locking a person out of the
sign-in page with an old cookie, which today's rule (a refused credential is a 401 everywhere) would
do.

**Alternatives considered**: marking page routes (route-table change and reference text); 401 with a
page body (browsers would show it, but the person would not come back to where they were).

## R7. Return path

**Decision**: `next` is followed only when it starts with `/`, its second character is not `/`, and it
contains no `\` and no control character; otherwise `/`. The same check is applied before `next` is
put into the sign-in page.

**Rationale**: browsers treat `\` as `/` and drop tabs and newlines in URLs, so `/\evil.example` and
`/\t/evil.example` are protocol-relative; refusing every `\` and control character closes both.

## R8. The sign-in routes reuse `on_get`/`on_post` binding

**Decision**: the routes are `on_get(sign_in, ...)` with input `{ next }`, `on_post(sign_in, ...)` with
input `{ name, password, next, and the Origin, Sec-Fetch-Site, Host and Cookie headers }`, and
`on_post(sign_out, ...)` with the headers. Name and password have `@size` limits. The form's failure
answer is the application's page (`b.page(message, next)`) with status 401 and no
`WWW-Authenticate`, so the browser shows it; the message is fixed by the library ("The name or the
password is wrong."). Success is `303` to `next` with the new cookie; an old session cookie in the
request is ended first (rotation, fixation). Sign-out ends the session found by the cookie, removes the
cookie and answers `303` to the sign-in page; without a cookie it only answers the redirect.

**Rationale**: form decoding, limits and error answers come from spec 051's binding, with no parser in
`std.security`.

## R9. Review

**Decision**: `browser`'s `Way.name` is `browser (sign-in /sign-in, sign-out /sign-out, 3600 s, origin
from Host)` (or the origin given), so `describe`/`review` callers and events see it. `review` adds:
the sign-in page when the rules do not allow it for anyone ("nobody could sign
in"), and an origin starting with `http://` ("the session cookie is sent over plain HTTP"). With such an
origin the cookie is not `Secure` and its name is `session` (the `__Host-` prefix requires `Secure`).

**Rationale**: the review lists what to look at; the way's settings travel in its name with no new
field.

## R10. Recordings, events, traces (Q2)

**Decision**: recordings unchanged: they hold sign-in form bodies and session cookies, and the reference,
the guide and docs/security.md say that a recording of a signed-in application is a secret. Events
never carry a password, a cookie value or a key (the existing rule of spec 045, extended to the new
kinds `signed_out` and the reason `cross-site`). Runtime error reports and traces show parameters and
locals; the library keeps session values in locals only inside the functions that need them, as it
does for passwords today, and SC-008 is checked by searching the outputs of the tests for the values.

## R11. Browsers and `http://localhost`

Chrome and Firefox treat `http://localhost` as secure for cookies, so the `Secure`, `__Host-` session
cookie works when developing on localhost. Safari does not keep `Secure` cookies from
`http://localhost`; the guide says to use another browser or set `origin: "http://localhost:8080"`
while developing (which `review` reports). Checked by hand in a browser during implementation (SC-002).

## R12. Reference cost

The new text is about 55 words (~70 tokens): the `browser`/`browser_routes`/`Browser` names in the
`std.security` row, one sentence for the cross-site rule and the page redirect, the three cookie names
in the `std.web` bullet, and "recordings of signed-in apps hold credentials". The reference has 2 tokens
left, so the same amount is cut from other text in the same change; the change lists the cuts.

## R13. Tests

- Golden cases in `tests/cases/web_security/`: `cookies` (parsing and `Set-Cookie` text, every refusal),
  `browser` (the whole flow through `handle_request` with the cookie carried by hand), `cross_site`
  (the table of method × cookie × headers), `sign_out`, `return_path`, `stale_cookie`, `review_browser`.
- `tests/web_security.py` gains groups: `cookies` (10,000 random `Cookie` headers parsed by
  `read_cookie` and by Python's `http.cookies` where it is defined), `origin` (the cross-site function
  against a Python model over generated header combinations), `timing` extended to the form route
  (SC-006), `secrets` extended to the browser case (SC-008).
- The example's tests (FR-020) and a run in a real browser (SC-002, SC-003, R11).
- SC-005 (each protection removed makes a test fail): done during implementation by deleting each
  protection in turn in `std/security.vo`, rebuilding and running the cases; the list goes in
  `results.md`.

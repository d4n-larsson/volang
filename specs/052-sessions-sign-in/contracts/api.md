# Contract: cookies, the browser way and its routes

Names and types are the contract; doc comments in the library are the documentation. All functions
are standard-library vo; there is no new syntax, builtin or diagnostic code (precondition failures
are the existing R0012).

## `std.web`

```vo
# A cookie to set with with_cookie. cookie(name, value) gives the safe defaults; change a field with
# `set c.FIELD = ...;` at the call.
export record Cookie {
    name: str;        # an RFC 6265 token
    value: str;       # cookie-octets only (no space, quote, comma, semicolon, backslash, controls)
    max_age_s: int;   # -1: until the browser closes; 0: removes the cookie; > 0: seconds
    http_only: bool;  # true: scripts cannot read it
    secure: bool;     # true: sent over HTTPS only (and http://localhost in most browsers)
    same_site: str;   # "Lax", "Strict" or "None" (None needs secure)
    path: str;        # "/" by default
}

export fn cookie(name: str, value: str) -> Cookie
export fn with_cookie(resp: Response, c: Cookie) -> result[Response]
export fn read_cookie(header: str, name: str) -> Option[str]
```

- `with_cookie` appends `Set-Cookie: NAME=VALUE; Path=P[; Max-Age=N][; Secure][; HttpOnly];
  SameSite=S` (attributes in this order; `Max-Age` only when `max_age_s >= 0`). Errors, as
  `err("bad_cookie: REASON")`: name not a token; value outside cookie-octets; `same_site` not one of
  the three; `None` without `secure`; a `__Host-` name without `secure` or with a path other than `/`.
- `read_cookie` splits on `;`, trims spaces, takes the first pair whose name is exactly `name`, and
  returns its value with surrounding double quotes removed. Pairs without `=` are skipped. A header
  that cannot be read gives `Option.none`, never an error.

## `std.security`

```vo
# A browser's session as the application keeps it.
export record Session {
    key: str;          # hex SHA-256 of the cookie's value; the value itself is never stored
    name: str;         # the account's name
    expires_ms: int;   # from the policy's clock
}

# Signing in browsers: the application's functions and settings.
export record Browser[C, E: effects] {
    find_account: fn(C, str) -> result[Option[Account]] uses E;
    keep: fn(C, Session) -> result[void] uses E;
    find: fn(C, str) -> result[Option[Session]] uses E;   # by key
    end: fn(C, str) -> result[void] uses E;               # by key; ending an unknown key is ok
    random: fn(int) -> bytes uses E;                      # std.crypto.random_bytes
    page: fn(str, str) -> Html;                           # the sign-in page: message ("" or why), return path
    sign_in: str;                                         # path of the sign-in page and form, e.g. "/sign-in"
    sign_out: str;                                        # e.g. "/sign-out"
    lifetime_s: int;                                      # > 0
    origin: str;                                          # "" (the request's Host), or "https://notes.example"
}

export fn browser[C, E: effects](b: Browser[C, E]) -> Way[C, E]
export fn browser_routes[C, E: effects](b: Browser[C, E], p: Policy[C, E]) -> list[Route[C, E]]
```

`Way` gains two fields (empty for `basic`, `tokens`, `header_key`, `test_users`):

```vo
    sign_in_page: str;   # where a page that needs a user sends the browser; "" for other ways
    origin: str;         # the origin state-changing requests must come from ("" : the request's Host)
```

and its `scheme` doc gains: "for the header `Cookie`, the name of the cookie".

### Behaviour

| Request | Answer |
|---|---|
| `GET b.sign_in?next=P` | 200, `html_reply(b.page("", safe(P)))` |
| `POST b.sign_in` form `name`, `password`, `next`, same origin, right password | 303 `Location: safe(next)`, `Set-Cookie: __Host-session=V; Path=/; Max-Age=LIFETIME; Secure; HttpOnly; SameSite=Lax`; old session (cookie in request) ended; event `signed_in` (and `outdated_hash` when so) |
| the same, wrong name/password/disabled | 401, `html_reply(b.page("The name or the password is wrong.", safe(next)))`, no cookie; event `refused` with the reason |
| `POST b.sign_in` not same origin | 403 problem; event `refused`, reason `cross-site` |
| `POST b.sign_out` | session of the cookie ended; 303 `Location: b.sign_in`; cookie removed (`Max-Age=0`); event `signed_out` when there was a session |
| a page (`GET`/`HEAD`, `Accept` has `text/html`) that needs a user, no valid cookie | 303 `Location: b.sign_in?next=ENCODED(path[?query])`; a stale cookie is removed |
| an API request that needs a user, no credential | 401 problem, as today |
| `POST`/`PUT`/`PATCH`/`DELETE` signed in by the cookie, not same origin | 403 problem; event `refused`, reason `cross-site`; operation not run |
| the cookie, session expired / ended / account unknown or disabled | as no credential (above); event `refused` with `expired` / `unknown session` / `unknown account` / `disabled account`; expired sessions are ended |
| cookie and another credential | refused as two credentials today |

`safe(P)`: `P` when it starts with `/`, its second character is not `/`, and it has no `\` and no
control character; else `/`.

Same origin (pure, one function): method `GET`/`HEAD`/`OPTIONS` → yes. Else `Sec-Fetch-Site` present →
yes iff `same-origin` or `none`. Else `Origin` present → yes iff its host and port equal the origin's
(`b.origin`'s, else the `Host` header's). Else no.

With `b.origin` starting `http://`, the cookie is named `session` and is not `Secure`.

### Review

`browser`'s `Way.name`: `browser (sign-in S, sign-out O, N s, origin from Host)` or `..., origin X)`.
`review` adds: `the sign-in page S needs a user, so nobody could sign in`; `the session cookie is sent
over plain HTTP (origin X)`. (A sign-out path that needs a user is harmless: whoever has a session
may use it, and whoever has none is sent to sign-in.)

### Events

`kind` gains `signed_out`. Reasons for `refused` gain `cross-site`, `expired`, `unknown session`,
`unknown account`. No event carries a password, a cookie value, a session key or a key.

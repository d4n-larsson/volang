# Data Model: Sessions and Sign-In Forms

Types are in [contracts/api.md](contracts/api.md); this file gives their rules and lifecycle.

## Cookie (`std.web`)

- **Fields**: name, value, max_age_s, http_only, secure, same_site, path.
- **Validation** (in `with_cookie`): name is a token (letters, digits and ``!#$%&'*+-.^_`|~``), not
  empty; value only bytes 0x21, 0x23–0x2B, 0x2D–0x3A, 0x3C–0x5B, 0x5D–0x7E; `same_site` is `Lax`,
  `Strict` or `None`; `None` needs `secure`; `__Host-` names need `secure` and path `/`; path starts with
  `/` and has no `;` or control character.
- **Defaults** (`cookie(name, value)`): max_age_s -1, http_only true, secure true, same_site `Lax`,
  path `/`.

## Session (`std.security`, kept by the application)

- **Fields**: key (64 hex characters, SHA-256 of the cookie value), name, expires_ms.
- **Created** at a successful form sign-in: value = 16 bytes from `random` as 32 hex characters;
  expires_ms = now + lifetime_s × 1000.
- **Valid** while now < expires_ms and `find_account(name)` gives an enabled account.
- **Ended** by sign-out, by a new sign-in from the same browser (the old key), or when found expired.
- **States**: none → active (sign-in) → ended (sign-out, replaced, expired). An ended session is gone
  from storage; a cookie naming it signs nobody in.

```text
            sign-in (form, same origin, right password)
   none ──────────────────────────────────────────────▶ active
                                                          │
         sign-out │ new sign-in in this browser │ found expired
                  ▼                              ▼
                             ended (deleted)
```

## Browser (`std.security`, written by the application)

- **Rules**: sign_in and sign_out are paths (`requires` they start with `/`); lifetime_s > 0; origin is
  `""` or `SCHEME://HOST[:PORT]` with no path.
- **Relations**: `browser(b)` makes the `Way` (header `Cookie`, scheme the cookie name, sign_in_page
  b.sign_in, origin b.origin); `browser_routes(b, p)` makes three routes using `p.now` and
  `p.on_event`.

## Way (changed)

- **New fields**: sign_in_page (`""` except for browser ways), origin (`""` except as set by
  `browser`).
- The `scheme` of a `Cookie` way is the cookie's name.

## Event (changed)

- **kind**: adds `signed_out`.
- **reason** for `refused`: adds `cross-site`, `expired`, `unknown session`, `unknown account`
  (`disabled account` exists).

## Example storage (notes-pages, over `std.sqlite`)

- `users(name text primary key, password_hash text not null, roles text not null default '')`.
- `sessions(key text primary key, name text not null, expires_ms integer not null)`.
- `notes` gains `owner text not null`; every query filters by the signed-in user's name.
- Users are added with `vo run main.vo add-user NAME` (password from standard input), since sign-up is
  out of scope.

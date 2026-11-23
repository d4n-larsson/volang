# Data Model: Remove HTML Rendering from the Core

## Browser (std.security), after

Fields: `find_account`, `keep`, `find`, `end`, `random`, `sign_in`, `sign_out`, `lifetime_s`, `origin`.
`page` is removed. Rules unchanged (paths start with `/` and have no `{`; lifetime > 0; origin `""` or
`SCHEME://HOST[:PORT]`).

## Sign-in input (private record in std.security), after

`name` (`@size(1, 200)`), `password` (`@size(1, 1000)`), `next` (`@default("")`), and the headers
`Origin`, `Sec-Fetch-Site`, `Host`, `Cookie`, `Content-Type` (each `@default("")`). A `Content-Type`
starting with `application/json` selects JSON answers.

## Retired code (docs/errors.md)

Heading, one-line description and `Fixable:` as before; then `Retired: DATE, spec NNN. REASON.`; no
Example, Fixed or golden case. Never emitted: no `"CODE"` string in `src/` or `std/`.

## secure-notes Store, after

`notes`, `next_id`, `accounts` as before, plus `sessions: map[str, Session]` (key: the session's hashed
key). One worker, so the map is the only copy.

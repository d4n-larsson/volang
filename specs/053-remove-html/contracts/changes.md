# Contract: what changes for programs and tools

## Removed

| Item | Was | After |
|---|---|---|
| `std.html` (`Html`, `html`, `html_text`, `no_html`) | spec 051 | not a module: `import std.html.X;` is the ordinary unknown-module diagnostic |
| `std.web.html_reply` | spec 051 | not an item |
| `Browser.page` | spec 052 | not a field |
| `GET Browser.sign_in` route from `browser_routes` | spec 052 | not added: the frontend serves the sign-in page |
| E0335–E0340 | spec 051 | retired: in the catalog, never emitted |

## Unchanged

Form-body binding, the 415 message, `Cookie`/`cookie`/`with_cookie`/`read_cookie`, `Session`,
`browser`, `same_origin`, `local_path`, `secure`'s cookie handling, page redirect and cross-site refusal,
sign-out, events, `review`, and the parser fix of 052.

## `browser_routes(b, p)` after the change

Two routes: `POST b.sign_in`, `POST b.sign_out`.

| Request to `POST b.sign_in` | Answer |
|---|---|
| any, not from the application's origin | 403 problem `cross-site request`; event `refused` `cross-site` |
| form, right password | 303 `Location: local_path(next)`, session cookie; events `signed_in` (and `outdated_hash`) |
| form, wrong password / unknown name / disabled | 303 `Location: SIGN_IN?failed=1&next=ESCAPED(local_path(next))`, no cookie; event `refused` with the reason |
| a JSON body | 400 problem, as any operation without a `body` field (JSON sign-in was dropped: see spec, US3 scenario 4) |
| missing or oversized `name`/`password` | 400/422 problem, as any input binding |

`POST b.sign_out`: unchanged (303 to `b.sign_in`, cookie removed, session ended).

## Retired catalog entry

```text
## E0335 html_template_not_literal

The template of `html` is not a string literal or a constant.

Fixable: no

Retired: 2026-11-23, spec 053. `std.html` was removed from the core: vo is for backend services and
renders no HTML. No version of vo reports this code, and no other code will use it.
```

`vo explain E0335` prints this entry (text and `--json`), exit 0. The catalog check accepts it without
Example, Fixed or golden case, and fails if `"E0335"` appears in `src/` or `std/`.

# Data model: Security (spec 045)

All in `std/security.vo`. Signatures: [contracts/security.md](contracts/security.md).

| Type | Fields | Rules |
|---|---|---|
| `User` (record) | `name: str`, `roles: list[str]` | roles compared exactly; may be empty |
| `Account` (record) | `name: str`, `password_hash: str`, `roles: list[str]`, `enabled: bool` | the hash is what `std.crypto.hash_password` makes |
| `Access` (variant) | `anyone`, `signed_in`, `any_role(list[str])`, `nobody` | |
| `Rule` (record) | `methods: list[str]`, `path: str`, `access: Access` | empty `methods` is every method; `path` starts with `/` |
| `Decision` (variant) | `allowed`, `sign_in_needed`, `forbidden` | |
| `Way[C, E]` (record) | `name: str`, `header: str`, `scheme: str`, `check: fn(C, str, int) -> result[User] uses E` | `check(ctx, credential, now_ms)`; `unauthorized: REASON` is a refusal, another error a failure |
| `Policy[C, E]` (record) | `rules: list[Rule]`, `ways: list[Way[C, E]]`, `now: fn() -> int uses E`, `on_event: fn(C, Event) uses E` | `now` in milliseconds |
| `Event` (record) | `kind: str`, `reason: str`, `name: str`, `method: str`, `path: str`, `at_ms: int` | kinds: `signed_in`, `refused`, `forbidden`, `outdated_hash`; `name` is `""` when unknown |

## The decision

| Deciding rule's access | nobody | a user |
|---|---|---|
| `anyone` | allowed | allowed |
| `signed_in` | sign-in needed | allowed |
| `any_role(rs)` | sign-in needed | allowed with one of `rs`, else forbidden |
| `nobody` | sign-in needed | forbidden |
| no rule matches | sign-in needed | forbidden |

## A request through the step

| Credentials | Result |
|---|---|
| none | `decide` with nobody: the operation, or 401 `sign in` |
| one accepted way, its check gives a user | `decide` with the user: the operation (user in `X-Identity`), or 403 and event `forbidden` |
| one accepted way, `unauthorized: REASON` | 401 `invalid credentials`, event `refused` with the reason |
| one accepted way, another error | 500 |
| two accepted ways, or `Authorization` in a scheme not accepted | 401 `invalid credentials`, event `refused` |

## Token

`base64url(header) . base64url(claims) . base64url(HMAC-SHA256(key, header.claims))`, no padding.
Header `{"alg":"HS256","typ":"JWT"}`. Claims `{"sub": name, "roles": [...], "iat": s, "exp": s}`.
Refusal reasons: `not a token`, `signature`, `signing method`, `content`, `expired`, `not yet valid`.

## Headers of `safe_headers`

`X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `Referrer-Policy: no-referrer`,
`Cache-Control: no-store`, and `Strict-Transport-Security: max-age=N` when N > 0; each only when the
answer has no header of that name.

# Contract: `std.security`

Types: [data-model.md](../data-model.md). `C` is the application's context, `E` its effects.

## Policy

```vo
export fn rule(path: str, access: Access) -> Rule                      # every method
export fn rule_for(methods: list[str], path: str, access: Access) -> Rule
export fn policy[C, E: effects](rules: list[Rule], ways: list[Way[C, E]], now: fn() -> int uses E) -> Policy[C, E]   # no event function
export fn secure[C, E: effects](p: Policy[C, E]) -> Step[C, E]        # prefix "/"
export fn decide(rules: list[Rule], method: str, path: str, caller: Option[User]) -> Decision   # pure
```

## Ways of signing in

```vo
export fn basic[C, E: effects](find: fn(C, str) -> result[Option[Account]] uses E) -> Way[C, E]
export fn tokens[C, E: effects](keys: list[bytes]) -> Way[C, E]       # requires: at least one key, each of 32 bytes or more
export fn header_key[C, E: effects](header: str, check: fn(C, str) -> result[User] uses E) -> Way[C, E]
export fn test_users[C, E: effects]() -> Way[C, E]                    # header X-Test-User
export fn as_user(r: Request, user: User) -> Request                  # for tests, with test_users
```

## Passwords and tokens

```vo
export fn password_user[C, E: effects](ctx: C, find: fn(C, str) -> result[Option[Account]] uses E, name: str, password: str) -> result[User] uses E
export fn issue_token(key: bytes, user: User, now_ms: int, lifetime_s: int) -> result[str]   # requires: key of 32 bytes or more, lifetime_s > 0
export fn check_token(keys: list[bytes], now_ms: int, token: str) -> result[User]
export fn sign_in_route[C, E: effects](pattern: str, p: Policy[C, E], find: fn(C, str) -> result[Option[Account]] uses E, key: bytes, lifetime_s: int) -> Route[C, E]
```

- `sign_in_route`: `POST pattern` with body `{"name": ..., "password": ...}`; 200 with
  `{"token": ..., "expires": SECONDS}`; 401 `invalid credentials`. Events `signed_in`, `refused`,
  `outdated_hash` through `p.on_event`, time from `p.now`. The policy needs an `anyone` rule for it.
- `std.crypto` gains `export fn password_outdated(stored: str) -> bool`.

## In operations

```vo
export fn current_user(identity: str) -> Option[User]
export fn require_user(identity: str) -> result[User]                  # err("unauthorized: sign in")
export fn require_role(identity: str, roles: list[str]) -> result[User]   # err("forbidden: not allowed") without one of them
export fn has_role(user: User, role: str) -> bool
```

`identity` is the input field `@header("X-Identity") @default("") identity: str;`.

## Review

```vo
export fn describe[C, E: effects](app: App[C, E], rules: list[Rule]) -> str
export fn review[C, E: effects](app: App[C, E], p: Policy[C, E]) -> list[str]
```

`describe`: one line per route in the application's order, `METHOD PATTERN: WHO`, where WHO is
`anyone`, `signed in`, `roles admin, editor`, `nobody` or `nobody (no rule)`. `review`: the findings
of research.md R6, in the order: routes, rules, ways.

## Headers

```vo
export fn safe_headers[C, E: effects](hsts_seconds: int) -> Step[C, E]   # 0: no Strict-Transport-Security
```

## `std.web`

```vo
export fn problem_reply(status: int, detail: str) -> Response
```

## Errors and stops

| Situation | Result |
|---|---|
| key shorter than 32 bytes, no key, lifetime ≤ 0 | R0012 at the call |
| refused credential | `err("unauthorized: REASON")` from `check_token`, `password_user`; 401 from the step |
| the application's lookup fails | its error; 500 from the step |

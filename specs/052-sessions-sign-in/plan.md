# Implementation Plan: Sessions and Sign-In Forms

**Branch**: `052-sessions-sign-in` (work on `main`, as for earlier specs) | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/052-sessions-sign-in/spec.md`

## Summary

All of it is standard-library vo; the toolchain does not change. `std.web` gets cookies: a `Cookie`
record whose defaults are safe, `with_cookie` to set one, `read_cookie` to read one (R4).
`std.security` gets a `Browser` record (the application's account and session functions, the sign-in
page, the paths, the lifetime, the origin) and two functions over it: `browser(b)`, a `Way` listed in
the policy beside `tokens` and `basic`, and `browser_routes(b, p)`, the sign-in page, the sign-in form
and sign-out (R1, R8). Sessions are kept by the application, which stores only a hash of the cookie's
value (R3, Q1). `secure` learns three things: reading a credential from a cookie, refusing a
state-changing cookie-signed request that does not come from the application's own origin (R5), and
sending a page that needs a user to the sign-in page instead of answering 401 (R6). `Way` gains two
fields for this (R2). The notes-pages example gets users. The reference text is paid for by shortening
other text (R12).

| Part | File | What |
|---|---|---|
| Cookies | `std/web.vo` | `Cookie`, `cookie`, `with_cookie`, `read_cookie` |
| Browser sign-in | `std/security.vo` | `Session`, `Browser`, `browser`, `browser_routes`; `Way` fields; `secure` (cookie credential, same-origin rule, page redirect, stale cookie); `review`; events |
| Docs | `docs/reference.md`, `docs/security.md`, `site/docs/guide/pages.md` (section "Signing in"), `site/docs/guide/apis.md` (security section: link), roadmap, changelog | R10, R12 |
| Example | `examples/notes-pages/` | users, sessions, notes per user, `add-user`, sign-in page, sign-out, tests |
| Tests | `tests/cases/web_security/` (new cases), `tests/web_security.py` (groups `cookies`, `origin`; `timing`, `secrets` extended) | R13 |

## Technical Context

**Language/Version**: vo for `std.web`, `std.security` and the example; Python 3 for test drivers.

**Primary Dependencies**: none new. `std.crypto` (`sha256`, `random_bytes`, the password functions),
`std.html` (the sign-in page is `Html`), `std.web` binding (form and headers).

**Storage**: the application's own (the example uses `std.sqlite`: tables `users`, `sessions`, and an
`owner` column on `notes`).

**Testing**: golden cases in `tests/cases/web_security/`; `tests/web_security.py` groups; the example's
`vo test`; `make quick AREA=web_security` between edits; `make test` once at the end.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64 (no platform code changes).

**Project Type**: compiler toolchain with its standard library (only the library changes).

**Performance Goals**: a cookie-signed request costs two lookups through the application's functions
plus one SHA-256 of 32 bytes; no target beyond spec 045's (tokens: at most 100 µs over an unguarded
request), which the browser way is not held to because its cost is the application's storage.

**Constraints**: reference ≤ 15,000 tokens with 2 left (R12); `std/security.vo` is one file (one
module per import path) and already carries `@allow("W0702")` where needed; no new diagnostic codes;
zero warnings.

**Scale/Scope**: about 250 lines of vo in `std/security.vo`, 80 in `std/web.vo`, 120 changed in the
example, 10 golden cases, 200 lines of Python.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One way to sign in a browser (one `Way` in the same policy); one way to set a cookie (a record with safe defaults, fields changed with `set`); no new syntax. | Pass |
| II. Machine-readable feedback | No new codes: misuse is a precondition (R0012) or an error value (`bad_cookie: ...`); refusals are problem answers and events as in spec 045. | Pass |
| III. Determinism | Randomness and time come through the application's `random` and the policy's `now` (fixed in `vo test`, recorded in runs). | Pass |
| IV. Test-first | The cases of each story and the Python groups before the library code. | Pass |
| V. Single source of truth, minimal surface | Library only; reference growth paid for by cuts listed in the change (R12); `ponytail:` comments on the fixed lifetime (no sliding renewal) and on `Way` carrying browser settings. | Pass, with R12 a task of its own |
| Implementation constraints | vo only. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/052-sessions-sign-in/
├── plan.md
├── research.md          # R1–R13
├── data-model.md        # Cookie, Session, Browser, Way and Event changes, the example's tables
├── quickstart.md
├── contracts/
│   └── api.md           # std.web cookies, std.security browser way and routes, behaviour table
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
std/web.vo                       # cookies
std/security.vo                  # Browser, Session, browser, browser_routes, secure, review
examples/notes-pages/            # main.vo (add-user), pages.vo (sign-in page, owners), notes.vo, users.vo (new: accounts, sessions)
tests/cases/web_security/        # cookies, browser, cross_site, sign_out, return_path, stale_cookie, review_browser
tests/web_security.py            # groups cookies, origin; timing and secrets extended
docs/reference.md  docs/security.md  site/docs/guide/pages.md  site/docs/guide/apis.md
```

**Structure Decision**: no new module; cookies belong to `std.web` (HTTP), sessions to `std.security`
(who is signed in).

## Complexity Tracking

No violations.

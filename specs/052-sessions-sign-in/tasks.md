---

description: "Task list for vo-lang spec 052: Sessions and Sign-In Forms"
---

# Tasks: Sessions and Sign-In Forms

**Input**: Design documents from `specs/052-sessions-sign-in/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R13), data-model.md, contracts/api.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases land first and fail before its code.

**Organization**:
- Setup: results file, quick area, room in the reference (R12) before any new reference text.
- Foundational: cookies in `std.web` (every story sets or reads the session cookie), the new `Way`
  fields, and the pure rules (same origin, return path) with their table tests.
- US1 (P1) sign-in through a form and staying signed in; US2 (P1) cross-site refusal; US3 (P1)
  sign-out; US4 (P2) application cookies; US5 (P2) tests and review.
- Polish: the example, the Python groups, docs, three architectures, threat removal check, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Everything a program sees** (names, fields, answers, events, review lines): contracts/api.md.
  Lifecycle and validation rules: data-model.md. Decisions: research.md.
- **Golden cases**: `tests/cases/web_security/NAME.vo` with `.code` and `.out` (or `.err`), run by
  `tests/run.sh tests/cases/web_security`. Write the case, see it fail, write the code; `--accept` only
  after reading the actual output. Cases drive the app through `handle_request` with headers added by
  `with_header` (`Accept`, `Origin`, `Sec-Fetch-Site`, `Host`, `Cookie`) and an in-memory store kept in
  the context (a `map[str, Session]` behind a record is enough for cases).
- **std code**: `std/web.vo` and `std/security.vo` stay one file each; every new export has a doc comment
  with a checked example (`vo doc`, `tests/doc_conformance.py`); `ponytail:` comments on the fixed
  lifetime and on `Way` carrying browser settings.
- **Secrets**: no event, error text or problem detail contains a password, a cookie value or a session
  key (FR-013); session values live in locals only.
- **Between edits**: `make quick AREA=web_security`; `make test` once per change; the gate only when
  asked. Never two suites at once.

## Phase 1: Setup

- [X] T001 Create `specs/052-sessions-sign-in/results.md` with headings SC-001 to SC-008, "Reference room", "Threat removal (SC-005)" and "Browser check"; add `tests/cases/web_security` scripts to area `web_security` in `tools/quick.sh` if missing (`"web_security.py --group cookies"`, `"web_security.py --group origin"`) and `examples/notes-pages` to its example tests
- [X] T002 Make room in the reference (research R12): shorten `docs/reference.md` by at least 80 tokens without losing a rule (merge repeated explanations; cut wording in the std.web bullet and module rows that `vo doc` already gives); run `python3 tests/ref_examples.py` and `python3 tests/doc_conformance.py`; record the counts before and after and each cut in results.md under "Reference room"

## Phase 2: Foundational

- [X] T003 [P] Golden case `tests/cases/web_security/cookies.vo`: `cookie("a", "1")` through `with_cookie` gives exactly `a=1; Path=/; Secure; HttpOnly; SameSite=Lax` (attribute order of contracts/api.md); `max_age_s` 0 and 600 give `Max-Age=0`/`Max-Age=600`; turning off `http_only` and `secure` with `set`; every `bad_cookie` refusal of contracts/api.md (name not a token, value with space, `;`, `"`, `,`, `\`, a control byte; `same_site` `"lax"`; `None` without secure; `__Host-x` without secure or with path `/a`); `read_cookie` on `a=1; b=two`, `b="q"`, a repeated name (first wins), a pair without `=`, an empty header, a name that is a prefix of another (`ab=1; a=2` for `a`)
- [X] T004 In `std/web.vo`: `export record Cookie` with the fields and doc comments of contracts/api.md, `export fn cookie(name, value) -> Cookie` (max_age_s -1, http_only true, secure true, same_site "Lax", path "/"), `export fn with_cookie(resp: Response, c: Cookie) -> result[Response]` (validation exactly as data-model.md "Cookie": name a token of letters, digits and ``!#$%&'*+-.^_`|~``, not empty; value only bytes 0x21, 0x23–0x2B, 0x2D–0x3A, 0x3C–0x5B, 0x5D–0x7E; path starts with `/` with no `;` or control character; errors `err("bad_cookie: REASON")`), `export fn read_cookie(header: str, name: str) -> Option[str]`; until T003 passes
- [X] T005 [P] Golden case `tests/cases/web_security/origin_rule.vo` over the pure same-origin function (exported for tests as `same_origin(method, sec_fetch_site, origin_header, host_header, origin_setting) -> bool`, or reached through `secure` if kept private): the full table of contracts/api.md "Same origin" (safe methods; `Sec-Fetch-Site` same-origin, none, same-site, cross-site; `Origin` equal to `Host`, with another port, another host, `null`; `origin_setting` set and `Host` different; neither header)
- [X] T006 [P] Golden case `tests/cases/web_security/return_path.vo` for the return-path rule of research R7: `/notes`, `/notes?x=1` kept; `//evil.example`, `/\evil.example`, `/\t/evil.example` (a tab), `https://evil.example`, `evil`, ``, `/a\b` become `/`
- [X] T007 In `std/security.vo`: the same-origin function (research R5, contracts/api.md), the return-path function (research R7, named `safe_path` or private), and `Way`'s new fields `sign_in_page: str` and `origin: str` with doc comments, set to `""` in `basic`, `tokens`, `header_key`, `test_users`; `Way.scheme`'s doc gains "for the header Cookie, the name of the cookie"; `secure` reads a `Cookie` way's credential with `read_cookie` (absent cookie: no credential); until T005, T006 and every existing `tests/cases/web_security` case pass

**Checkpoint**: cookies work, the pure rules pass their tables, existing security cases unchanged.

## Phase 3: User Story 1 - Signing in through a form and staying signed in (P1) 🎯 MVP

**Goal**: a browser is sent to sign-in, signs in with the form, comes back signed in, and operations get
the user through `require_user` as with tokens.

**Independent Test**: `tests/run.sh tests/cases/web_security` passes `browser`, `stale_cookie`.

- [X] T008 [P] [US1] Golden case `tests/cases/web_security/browser.vo`: an app with rules `/sign-in` and `/sign-out` anyone, `/notes` signed in, `/api/notes` signed in; a `Browser` over an in-memory store and one account (hash made with a low-cost `pbkdf2-sha256$1$...` text, as `password_user`'s example does); walk: `GET /notes` with `Accept: text/html` → 303 `Location: /sign-in?next=%2Fnotes`; `GET /sign-in?next=/notes` → 200 with the page; `POST /sign-in` form `name=ada&password=...&next=/notes` with `Sec-Fetch-Site: same-origin` → 303 to `/notes`, the exact `Set-Cookie` of contracts/api.md, events `signed_in`; `GET /notes` with the cookie → the operation's `require_user` name; wrong password, unknown name, disabled account → 401 page with "The name or the password is wrong.", no `Set-Cookie`, `refused` events with the three reasons; `GET /api/notes` without credentials → 401 problem; an account with an outdated hash → `outdated_hash` event; signing in again with the old cookie in the request ends the old session (the old cookie signs nobody in afterwards)
- [X] T009 [P] [US1] Golden case `tests/cases/web_security/stale_cookie.vo`: a cookie whose session expired (clock moved past `expires_ms`), was ended, names an unknown account or a disabled one: on `/sign-in` the page is shown (200) and the answer removes the cookie (`Max-Age=0`); on `/notes` with `Accept: text/html` → 303 to sign-in; events `refused` with `expired`, `unknown session`, `unknown account`, `disabled account`; an expired session is ended in the store
- [X] T010 [US1] In `std/security.vo`: `export record Session` and `export record Browser[C, E: effects]` with the fields and doc comments of contracts/api.md and data-model.md (`requires` sign_in and sign_out start with `/`, lifetime_s > 0, origin `""` or `SCHEME://HOST[:PORT]` without a path); `export fn browser(b) -> Way` (header `Cookie`, scheme the cookie name: `__Host-session`, or `session` when origin starts with `http://`; name as research R9; `check` hashes the value with `sha256` to the hex key, `find`s it, ends and refuses expired sessions, looks the account up with `find_account`, refuses unknown and disabled accounts, gives the account's roles)
- [X] T011 [US1] In `std/security.vo`: `export fn browser_routes(b, p) -> list[Route]` (research R8): `on_get(b.sign_in)` with input `{ @default("") next: str; }` answering `html_reply(200, b.page("", safe(next)))`; `on_post(b.sign_in)` with input `name` (`@size(1, 200)`), `password` (`@size(1, 1000)`), `@default("") next`, and the headers `Origin`, `Sec-Fetch-Site`, `Host`, `Cookie` (`@header`, `@default("")`): same-origin check (research R5, login forgery), password check through the existing `checked` (same events as `sign_in_route`), on success end the old session of the cookie, make the value from `b.random(16)` (refuse other lengths as a failure), `keep` the `Session` (hex SHA-256 key, name, now + lifetime_s × 1000), answer 303 to `safe(next)` with the cookie (`Max-Age` lifetime_s); on failure 401 `html_reply(b.page("The name or the password is wrong.", safe(next)))`
- [X] T012 [US1] In `secure` (`std/security.vo`): a refused browser cookie counts as no credential (event `refused` with its reason, the answer removes the cookie); when the decision is "sign in needed", the method `GET`/`HEAD`, `Accept` contains `text/html` and a way has a `sign_in_page`, answer 303 to `SIGN_IN?next=` + the percent-encoded path and query (research R6), else today's 401; until T008 and T009 pass and every older case is unchanged

**Checkpoint**: US1 works alone: a browser can sign in and use pages.

## Phase 4: User Story 2 - Other sites cannot act for the signed-in user (P1)

**Goal**: state-changing cookie-signed requests from another origin are refused before the operation
runs; nothing is written into forms.

**Independent Test**: `cross_site` passes.

- [X] T013 [P] [US2] Golden case `tests/cases/web_security/cross_site.vo`: with the cookie, `POST /notes/1/done` with `Sec-Fetch-Site: cross-site` → 403, operation not run (it records a call in the store), event `refused` reason `cross-site`; with `same-origin` → runs; with `Origin` of another host or port → 403; equal to `Host` → runs; with neither header → 403; `GET` with `cross-site` → runs; the same cross-site post with a token (`tokens` way) → runs; `Browser.origin` `https://notes.example` with `Host: 127.0.0.1:8080` and `Origin: https://notes.example` → runs; a cross-site `POST /sign-in` → 403 and no cookie
- [X] T014 [US2] In `secure` (`std/security.vo`): when the credential came from a way with a `sign_in_page` and the method is not `GET`/`HEAD`/`OPTIONS`, apply the same-origin function with the way's `origin` before deciding; refuse with `problem_reply(403, "cross-site request")` and the event; until T013 passes

## Phase 5: User Story 3 - Signing out ends the session (P1)

**Goal**: sign-out ends the session on the server; a copied cookie stops working at once.

**Independent Test**: `sign_out` passes.

- [X] T015 [P] [US3] Golden case `tests/cases/web_security/sign_out.vo`: sign in, keep the cookie, `POST /sign-out` same-origin → 303 `Location: /sign-in`, `Set-Cookie` with `Max-Age=0`, event `signed_out` with the name; the kept cookie on `/notes` → 303 to sign-in (or 401 for an API request); sign-out without a cookie → 303 and no event; a cross-site sign-out → 403
- [X] T016 [US3] In `browser_routes` (`std/security.vo`): `on_post(b.sign_out)` with the `Cookie` header: find the session of the cookie, `end` it, event `signed_out` with its name; answer 303 to `b.sign_in` removing the cookie (same name, path and flags, `Max-Age=0`); add `signed_out` to `Event.kind`'s doc and `cross-site`, `expired`, `unknown session`, `unknown account` to the reasons; until T015 passes

## Phase 6: User Story 4 - Cookies in std.web (P2)

**Goal**: applications read and set their own cookies with the same defaults.

**Independent Test**: `app_cookie` passes.

- [X] T017 [US4] Golden case `tests/cases/web_security/app_cookie.vo`: an operation with `@header("Cookie") @default("") cookies: str;` reads a preference with `read_cookie` and answers it; another sets it with `with_cookie(resp, cookie("show_done", "1"))` and one removes it with `max_age_s` 0; a value a cookie cannot hold gives the `bad_cookie` error as a 500 through the operation (as other errors); passes with T004 alone (fix `std/web.vo` if not)

## Phase 7: User Story 5 - Tested and reviewed without a browser (P2)

**Goal**: deterministic tests through `handle_request`; `review` shows the browser settings and the
choices that weaken them.

**Independent Test**: `review_browser` passes; the example's tests pass 20 times identically.

- [X] T018 [P] [US5] Golden case `tests/cases/web_security/review_browser.vo`: `review` of an app whose policy lists `browser(b)`: the way's name of research R9; a rule making `/sign-in` signed-in only → "the sign-in page /sign-in needs a user, so nobody could sign in"; origin `http://localhost:8080` → "the session cookie is sent over plain HTTP (origin http://localhost:8080)"; `test_users` still reported
- [X] T019 [US5] In `review` (`std/security.vo`): for each way with a `sign_in_page`, the sign-in page line when `decide(p.rules, "GET", page, Option.none)` is not allowed, and the plain-HTTP line when its `origin` starts with `http://`; until T018 passes
- [X] T020 [US5] Check determinism: run `tests/run.sh tests/cases/web_security` 20 times and compare outputs; record in results.md under SC-001

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T021 Example `examples/notes-pages/` (FR-020, data-model.md "Example storage"): new `users.vo` with tables `users(name text primary key, password_hash text not null, roles text not null default '')` and `sessions(key text primary key, name text not null, expires_ms integer not null)`, `find_account`, `keep`, `find`, `end` over `std.sqlite` (at most 30 lines for the session functions, SC-004), and `add_user`; `notes` gains `owner text not null` and every query in `notes.vo` filters by owner; `pages.vo` gets the sign-in page template (form posting `name`, `password` and a hidden `next` to `/sign-in`), a sign-out button in the layout, the policy (`/sign-in`, `/sign-out`, `/app.js` anyone; `/` signed in), `browser(...)`, `browser_routes(...)` and operations taking `@header("X-Identity") @default("") identity: str;` with `require_user`; `main.vo` gains `add-user NAME` reading the password from standard input (`hash_password`) and the effects `entropy`, `clock`, `input`; update `vo.effects`, `README.md`, and re-record `recordings/demo.jsonl` with a sign-in, two notes, done, sign-out (the recording holds the password: use a throwaway one and say so in the README)
- [X] T022 Example tests in `examples/notes-pages/pages.vo` (FR-020): sign-in and the redirect back; sign-out; a cross-site post refused; ada does not see bo's notes and cannot mark them done (404); the done button with and without `X-Swap`; the sign-in, sign-out and policy lines counted for SC-004 and recorded in results.md
- [X] T023 [P] `tests/web_security.py`: group `cookies` (10,000 random `Cookie` headers from a grammar of names, values, quotes, spaces and junk; `read_cookie` through one vo program against Python's `http.cookies.SimpleCookie` where it parses the header, else against a direct model of contracts/api.md); group `origin` (every combination of method, `Sec-Fetch-Site`, `Origin`, `Host`, setting from small sets, through `same_origin` against a Python model); extend `timing` to the form route (unknown name vs wrong password medians within 10%, SC-006) and `secrets` to the browser case (search the case's outputs and events for the password, the cookie value and the session key, SC-008); add the new groups to `make test`'s call
- [X] T024 [P] Docs: `docs/reference.md` (research R12: `Browser`, `browser`, `browser_routes` in the std.security row; one sentence for the cross-site rule and the page redirect; `cookie`, `with_cookie`, `read_cookie` in the std.web bullet; recordings of signed-in apps hold credentials) within 15,000 tokens; `docs/security.md` (the threat list of spec.md Edge Cases with their answers, and recordings as secrets); `site/docs/guide/pages.md` section "Signing in" (the `Browser` record, the routes, the policy, the storage over `std.sqlite`, the threats, Safari and `http://localhost` from research R11, brute force through events); a link from the security section of `site/docs/guide/apis.md`; `docs/roadmap.md` (052 section and the Status table rows for 049–052, with 050 before 051); `CHANGELOG.md` (including the two new `Way` fields)
- [X] T025 Threat removal (SC-005): for each protection (HttpOnly default, Secure default, SameSite default, `__Host-` name, same-origin check in `secure`, same-origin check on the sign-in form, return-path check, session rotation at sign-in, expiry, end at sign-out, hash in storage, unknown-name timing) remove it in `std/security.vo`/`std/web.vo`, rebuild, run `tests/run.sh tests/cases/web_security` and the example's tests, confirm at least one fails, restore; list each with the failing test in results.md
- [X] T026 Three architectures: run `tests/cases/web_security` and the example's tests under the ARM64 and RISC-V builds as earlier specs do; record in results.md
- [X] T027 Browser check (SC-002, SC-003, research R11) following quickstart.md part 3 in Chrome or Firefox, or, if no browser can be driven here, write in results.md that it is left for the user with the exact steps
- [X] T028 `make test` once; the instrumented build on `tests/cases/web_security` and the example; fill results.md (SC-001 to SC-008, reference counts); set spec.md Status to Implemented

## Dependencies & Execution Order

- Setup (T001, T002) → Foundational (T003–T007) → stories.
- US1 (T008–T012) needs Foundational. US2 (T013, T014) and US3 (T015, T016) need US1's routes and
  `secure` changes. US4 (T017) needs only T004. US5 (T018–T020) needs US1 and US3.
- Polish: T021–T022 need US1–US3; T023 needs T007; T024 after the API is final; T025–T028 last.

## Parallel Opportunities

- T003, T005, T006 (different case files) together; T004 and T007 are in different files and can go
  side by side once their cases exist.
- T008 and T009 together; T013, T015, T017, T018 are separate case files.
- T023 and T024 together.

## Implementation Strategy

1. MVP: Setup, Foundational, US1: a browser can sign in and stay signed in.
2. US2 and US3 before anything is published: without them cookie sign-in is unsafe.
3. US4, US5, then the example, docs and checks.

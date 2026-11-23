# Feature Specification: Sessions and Sign-In Forms

**Feature Branch**: `052-sessions-sign-in`

**Created**: 2026-11-23

**Status**: Implemented ([results](results.md))

**Input**: User description: "Specify the sessions and sign-in feature." The next step named in
docs/roadmap.md under spec 051 ("cookies, sessions and sign-in forms with cross-site protection, the
next step for a full web app").

Background: since spec 051 a vo service can answer HTML pages and read HTML forms, but it cannot know
who is using them. `std.security` (spec 045) signs in programs only: a credential in a request header
(`Authorization: Basic`, `Authorization: Bearer` with a signed token, an API key), checked on every
request by `secure(policy)`, which enforces one table of rules and hands the user to operations. A
browser does not send those headers by itself; it sends cookies. `std.web` cannot read a request's
cookies or set one except by writing the header by hand, and an agent writing sessions by hand gets the
details wrong in ways that are security holes: cookie flags (a cookie readable by scripts, sent over
plain HTTP, sent with requests from other sites), a session value that survives sign-in (fixation), a
"sign out" that only forgets the cookie in this browser, a sign-in page that sends the user on to any
address it is given (open redirect), and forms that any other site can make the browser post
(cross-site request forgery).

From an agent's point of view:
- **A browser signs in the way a program does.** The same rules, the same user in operations
  (`require_user`, `require_role`), the same security events: one more way of signing in, not a second
  access-control system.
- **Safety is the default, not a step.** Cookie flags, cross-site protection and the new session
  value at sign-in happen without the agent writing them; turning one off is written out and shows in
  the review.
- **Pages and APIs differ only where a person is involved.** A page that needs a signed-in user sends
  the browser to the sign-in page; an API still answers 401.

What this spec must keep:
- **Principle I**: one table of rules, one way to say who is signed in in operations, one way to sign
  in a browser.
- **Principle III**: deterministic tests (`vo test` has a fixed clock; random values come from
  `random_bytes` under the `entropy` effect); record and replay of every program unchanged.
- **Principle V**: the reference is at about 14,998 of 15,000 tokens. Everything new is paid for by
  shortening other text, and the change states what.
- **Security**: the properties of docs/security.md; passwords, keys and session values never appear in
  events, logs or traces; the threats listed under Edge Cases each have a stated answer.
- **No new syntax or builtin** where standard-library code written in vo suffices (`std.security` and
  `std.web` are written in vo).

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents that write vo services from the reference and `vo doc`, the people who
review those services, and the people who use the pages in a browser.

### User Story 1 - Signing in through a form and staying signed in (Priority: P1)

An agent adds users to the notes pages. A visitor opens `/notes`, is sent to `/sign-in?next=/notes`,
fills in name and password, and comes back to `/notes` signed in. Every following page knows who they
are until the session ends. A wrong password shows the sign-in page again with a message.

**Why this priority**: it is the feature; without it a browser application has no users.

**Independent Test**: `handle_request` cases that walk the flow with the cookies a browser would keep:
the redirect to sign-in, the posted form, the cookie set, the next request carrying it and reaching the
operation with the user; the failure page; golden cases of the exact `Set-Cookie` headers.

**Acceptance Scenarios**:

1. **Given** a policy with a rule that `/notes` needs a signed-in user and the browser way of signing
   in, **When** a browser asks for `GET /notes` without a session, **Then** it is answered `303` to the
   sign-in page with the path it asked for as where to return.
2. **Given** the sign-in route, **When** a form posts the right name and password, **Then** it answers
   `303` to the return path and sets a session cookie that is `HttpOnly`, `Secure`, `SameSite=Lax`,
   `Path=/`, with no `Domain`, and the policy is told of a `signed_in` event.
3. **Given** that cookie, **When** the browser asks for `/notes`, **Then** the operation runs with the
   user, through the same `require_user` and `require_role` as for tokens.
4. **Given** a wrong password, an unknown name or a disabled account, **When** the form is posted,
   **Then** no cookie is set, the policy is told of a `refused` event with the reason, and the
   application's sign-in page is answered (status 200 or 401 as the plan decides) with a message that
   does not say which of the three it was; the time taken does not depend on whether the name exists.
5. **Given** an API route under the same policy, **When** a request without credentials asks for it,
   **Then** it is still answered `401` in the problem format, as today.
6. **Given** a password whose stored hash is weaker than `hash_password` makes now, **When** it is
   right, **Then** the `outdated_hash` event is reported, as for the JSON sign-in route.

---

### User Story 2 - Other sites cannot act for the signed-in user (Priority: P1)

A signed-in user visits another site that contains a hidden form posting to the notes app's
`/notes/3/done`. The browser sends the post; the notes app refuses it. The agent wrote nothing for
this.

**Why this priority**: cookie sign-in without this protection is a hole in every form; it must be on
from the first version.

**Independent Test**: `handle_request` cases with every combination of method, cookie, origin
information present, absent and foreign; golden cases of the refusal.

**Acceptance Scenarios**:

1. **Given** a request signed in by the session cookie with a method other than `GET`, `HEAD` or
   `OPTIONS`, **When** the browser says it comes from another site, **Then** it is refused with `403`
   before the operation runs, and the policy is told of a `refused` event naming cross-site.
2. **Given** the same request from a page of the application itself, **When** it arrives, **Then** it
   is allowed, with no value the agent had to put into the form.
3. **Given** a state-changing request signed in by the cookie that carries no information about where
   it comes from, **When** it arrives, **Then** it is refused (a browser that sends cookies also says
   where a request comes from; anything else is not a browser this protection can trust).
4. **Given** a request signed in by a token, a key or Basic credentials, **When** it arrives from
   anywhere, **Then** the protection does not apply (those credentials are not sent by a browser on
   its own).
5. **Given** an application served behind a proxy under another host name, **When** its origin differs
   from the `Host` the application sees, **Then** the application can name its public origin once in the
   policy, and that is what requests are compared with.

---

### User Story 3 - Signing out ends the session (Priority: P1)

The visitor presses "Sign out". The browser forgets the cookie and goes to the sign-in page. A copy of
the old cookie, taken before, no longer signs anybody in.

**Why this priority**: a stolen cookie is the main threat to a session; ending it on the server is the
answer people expect from "sign out".

**Independent Test**: `handle_request` cases: sign in, keep the cookie, sign out, replay the kept
cookie.

**Acceptance Scenarios**:

1. **Given** a signed-in browser, **When** it posts to the sign-out route, **Then** it is answered `303`
   to the sign-in page with a `Set-Cookie` that removes the cookie, and the policy is told of a
   `signed_out` event.
2. **Given** a copy of the cookie from before sign-out, **When** it is sent again, **Then** it signs
   nobody in, from that moment on (sessions are kept by the application, Q1).
3. **Given** a session older than its lifetime, **When** its cookie is sent, **Then** it signs nobody
   in and the policy is told of a `refused` event with the reason `expired`.
4. **Given** a sign-out posted from another site, **When** it arrives, **Then** it is refused as in
   User Story 2 (forced sign-out is a nuisance, not a hole, but one rule for every state change is
   simpler to review).

---

### User Story 4 - Cookies in std.web (Priority: P2)

Apart from the session, an application reads and sets its own cookies (a preference such as "show done
notes"), with the same safe defaults.

**Why this priority**: the session needs cookie reading and writing anyway; exposing it once avoids
every application parsing the `Cookie` header itself.

**Independent Test**: golden cases of cookie parsing (several cookies, spaces, quoted values, repeated
names, invalid bytes) and of the `Set-Cookie` text for every option.

**Acceptance Scenarios**:

1. **Given** a request with `Cookie: a=1; b=two`, **When** the application asks for cookie `b`, **Then**
   it gets `two`; for a cookie that is not there, nothing.
2. **Given** an answer and a cookie name and value, **When** the application sets the cookie with no
   options, **Then** the `Set-Cookie` header has `HttpOnly`, `Secure`, `SameSite=Lax` and `Path=/`.
3. **Given** an application that must turn one of those off (a cookie a script reads, a development
   server without HTTPS), **When** it says so, **Then** it does so with a named option written at that
   call, never by a setting elsewhere.
4. **Given** a name or value with bytes a cookie cannot hold, **When** set, **Then** it is an error
   value, not a broken header.
5. **Given** a cookie to remove, **When** the application removes it, **Then** the answer carries a
   `Set-Cookie` with the same name, path and flags and an expiry in the past.

---

### User Story 5 - Tested and reviewed without a browser (Priority: P2)

An agent tests the pages with `vo test`: as today, `as_user` gives a request a user under
`test_users()`; a test can also walk the real sign-in flow with `handle_request`, taking the cookie from
one answer and sending it with the next request. A reviewer reads `review` and sees which routes are
pages, which way signs in browsers, and every safe default that was turned off.

**Why this priority**: agents fix code by running tests; reviewers need to see the security choices in
one place.

**Acceptance Scenarios**:

1. **Given** the same program and requests, **When** tests run 20 times and on the three
   architectures, **Then** the session values, cookies and answers are byte-identical (fixed clock;
   random values under `entropy` are deterministic in `vo test`).
2. **Given** an application with the browser way, **When** `review` is run, **Then** it names the way,
   the session lifetime, the sign-in and sign-out routes, the origin the cross-site check compares
   with, and each cookie default turned off.
3. **Given** a recorded run of the application, **When** it is replayed, **Then** it gives the same
   answers; the recording holds the sign-in form bodies and session cookies as they arrived (Q2), and
   the guide says that such a recording is a secret, kept like the database.

### Edge Cases

Threats, each with its answer:

- **Stolen cookie**: `HttpOnly` keeps scripts from reading it; `Secure` keeps it off plain HTTP; the
  session has a lifetime; sign-out ends it (User Story 3). Binding a session to an address or a browser
  fingerprint is not done (it breaks on mobile networks and is easily faked).
- **Session fixation**: a session value is created only by a successful sign-in, never accepted from
  the browser beforehand, and a new one replaces any old one at sign-in.
- **Cross-site request forgery**: User Story 2; `SameSite=Lax` as a second layer.
- **Cross-site scripting reading the cookie**: `HttpOnly`, and `std.html`'s escaping and content
  security policy (spec 051) against the scripting itself.
- **Open redirect through the return path**: only a path of the same application is followed (it
  starts with one `/`, not `//` or `/\`, and has no scheme or host); anything else returns to `/`.
- **Timing of the name lookup**: an unknown name costs the same password check as a known one, as in
  spec 045.
- **Brute force**: not limited by this spec (rate limiting is out of scope); every refusal is an event
  with the name, so an application can count and lock out, and the guide shows how.
- **Guessing a session value**: at least 128 random bits from the system (`entropy`).
- **A copy of the session storage** (a leaked database or backup): storage keeps a one-way hash of each
  session value, never the value, so the copy signs nobody in.
- **Cookie tossing from a sibling subdomain**: the cookie has no `Domain` attribute; a `__Host-`
  prefixed name is used where the browser supports it.

Other edge cases:

- **Two credentials in one request** (a cookie and a token): refused, as two credentials are today.
- **A session for an account that was disabled or lost a role after signing in**: the change applies
  from the next request, since the session is looked up on each request (Q1).
- **A sign-in page that is itself behind a rule needing a user**: `review` reports it, since nobody could
  sign in.
- **`GET` requests that change things**: not protected by User Story 2; the guide says that a change is
  a `POST`, and `review` lists `GET` routes that the application marks as changing nothing.
- **Plain HTTP during development**: browsers keep `Secure` cookies for `http://localhost`; another
  plain-HTTP host needs the `Secure` default turned off, which `review` shows.
- **Many cookies, long `Cookie` header**: within the existing header size limits; a malformed `Cookie`
  header signs nobody in and is not an error of the request.
- **Clock**: expiry uses the policy's clock, so `vo test` and replay see the same times.

## Requirements *(mandatory)*

### Functional Requirements

**Cookies (`std.web`)**

- **FR-001**: `std.web` MUST provide reading one cookie of a request by name, and setting and removing
  a cookie on an answer.
- **FR-002**: A cookie set with no options MUST be `HttpOnly`, `Secure`, `SameSite=Lax`, `Path=/`,
  without `Domain`; each of these MUST be turned off only by an option named at the call.
- **FR-003**: Cookie names and values MUST be checked; one that a `Set-Cookie` header cannot hold is
  an error value.

**Browser sign-in (`std.security`)**

- **FR-004**: `std.security` MUST provide a way of signing in by session cookie that is listed in a
  policy beside the existing ways, gives operations the user exactly as they do (`X-Identity`,
  `current_user`, `require_user`, `require_role`), and is governed by the same rules.
- **FR-005**: `std.security` MUST provide a sign-in route for HTML forms: it reads name and password
  from a form, checks them as the JSON sign-in route does (same account lookup function, same
  unknown-name timing, same events), on success creates a new session, sets its cookie and answers
  `303` to the return path; on failure answers the application's sign-in page with a message and sets
  no cookie.
- **FR-006**: `std.security` MUST provide a sign-out route that ends the session, removes the cookie and
  answers `303` to the sign-in page.
- **FR-007**: Where a rule needs a user and nobody is signed in, a request from a browser for a page
  (a `GET` or `HEAD` that accepts HTML) MUST be answered `303` to the sign-in page with the requested
  path as the return path; every other such request MUST keep today's `401`.
- **FR-008**: Sessions MUST be kept by the application, in its own storage, through functions it gives
  the policy (as it gives `find_account` today): one that keeps a new session, one that finds a session
  with its user, one that ends it. The cookie holds a random value; storage holds only a one-way hash of
  it, the user's name and the expiry. A session MUST have a lifetime set by the application; an expired
  or ended session signs nobody in, and the user's account is looked up again on each request, so a
  disabled account or a changed role applies at once. The example and the guide show the functions over
  `std.sqlite`.
- **FR-009**: The return path MUST be followed only when it is a path of the same application (FR-007,
  Edge Cases); otherwise the browser returns to `/`.

**Cross-site protection**

- **FR-010**: A request signed in by the session cookie with a method other than `GET`, `HEAD` or
  `OPTIONS` MUST be refused with `403` before its operation runs unless the browser says it comes from
  the application's own origin; a request that says nothing about its origin MUST be refused.
- **FR-011**: The application's origin MUST default to the one the request was sent to, and MUST be
  settable once in the policy for applications behind a proxy.
- **FR-012**: The protection MUST need nothing written into forms or pages by the application.

**Events, review and secrets**

- **FR-013**: Sign-in, refusal (with reasons including `expired` and cross-site), sign-out and
  outdated hashes MUST be reported to the policy's event function; no event carries a password, a key
  or a session value.
- **FR-014**: `review` MUST show the browser way, the session lifetime, the sign-in and sign-out routes,
  the origin compared with, each cookie default turned off, and a sign-in page that needs a user.
- **FR-015**: Session values and passwords MUST NOT appear in traces or runtime error reports in readable
  form. Recordings (`vo run --record`) keep requests as they arrived, so a recording of a signed-in
  application holds passwords from sign-in forms and session cookies, as it already holds
  `Authorization` headers; replay is unchanged. The reference and the guide MUST say that such a
  recording is a secret.

**Fit**

- **FR-016**: Everything MUST be standard-library code written in vo, with no new syntax or builtin,
  unless the plan shows that one is needed and states why.
- **FR-017**: Tests MUST be deterministic and byte-identical on x86-64, ARM64 and RISC-V; record and
  replay of programs that do not use sessions MUST be unchanged.
- **FR-018**: The reference MUST describe the browser way, cookies and the cross-site rule in the
  fewest words, and MUST NOT exceed 15,000 tokens; what is shortened to make room is stated in the
  change.
- **FR-019**: `vo doc` MUST document every new item with a checked example; any new diagnostic MUST
  have a stable code and a catalog entry.
- **FR-020**: The notes-pages example MUST have users: notes belong to the user who wrote them, a
  sign-in page, sign-out, and the done button working with and without its script; its tests MUST cover
  sign-in, sign-out, a refused cross-site post and one user not seeing another's notes.
- **FR-021**: The site's guide MUST explain the browser way and list the threats with their answers.

### Key Entities

- **Session**: that a browser is signed in as a user, from sign-in until sign-out or its lifetime ends;
  identified by a value in a cookie that cannot be guessed or forged.
- **Cookie**: a name and value the browser keeps for the application and sends back, with flags that
  say who may read it and when it is sent.
- **Browser way**: the way of signing in that reads the session cookie, beside basic, tokens and keys.
- **Return path**: the page to go back to after signing in; a path of the same application only.
- **Origin**: the scheme, host and port the application is reached at; what cross-site checks compare
  with.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every acceptance scenario is a golden or `handle_request` case, byte-identical on x86-64,
  ARM64 and RISC-V and over 20 runs.
- **SC-002**: A person can sign in to the example in a browser, use the pages, sign out, and see the
  sign-in page again; after sign-out, the old cookie sent again signs nobody in.
- **SC-003**: A form on another origin posting to every state-changing route of the example is refused
  in every case (as `handle_request` cases for every route, and once in a real browser).
- **SC-004**: An agent adding sign-in to a page application writes no cookie flags, no cross-site
  check and no session value handling: the example's sign-in, sign-out and policy together are at most
  25 lines, plus the session storage functions (at most 30 lines over `std.sqlite`).
- **SC-005**: Every threat under Edge Cases has a test that fails when its protection is removed
  (each protection deleted in turn makes at least one test fail).
- **SC-006**: Signing in costs the same time for unknown and known names within 10% (median of 20).
- **SC-007**: The reference stays at or under 15,000 tokens.
- **SC-008**: No event, trace or runtime error report from the example's tests contains a password or a
  session value (searched for the values used in the tests).

## Assumptions

- **One more way, not a second system**: browsers sign in through `std.security`'s policy beside the
  existing ways; the rules, events and operation-side functions do not change.
- **Cross-site protection by origin, not by form tokens** (decided): browsers say where a request comes
  from (`Origin` on every state-changing request, `Sec-Fetch-Site` in current browsers); comparing it
  with the application's origin protects every form with nothing written into it, which a hidden token
  field would not. `SameSite=Lax` is a second layer. Rejected: a synchronizer token (needs every form
  to carry it, and the fragments of spec 051 to fetch it), a double-submit cookie (the same, plus a
  readable cookie).
- **The session carries only the user** (decided): no flash messages or other data. The sign-in
  failure message is shown by answering the sign-in page directly, not through a redirect, so nothing
  must survive a request; application data belongs in the application's database.
- **Pages are told apart from APIs by the request** (a `GET`/`HEAD` that accepts HTML), not by marking
  routes, so no route table changes; the plan may refine the test.
- **Tests use `as_user` and `test_users()` as today**; a session needs no new test helper beyond taking
  a cookie from one answer and sending it with the next.
- Out of scope: sign-in with other providers (OAuth, OpenID Connect, passkeys), sign-up, password reset
  and email, "remember me" beyond the session lifetime, rate limiting and lock-out (events make them
  possible), serving over HTTPS (a proxy in front, as today), multi-factor authentication, binding
  sessions to addresses or devices, cookies for other paths or domains than the whole application.

## Clarifications

### Session 2026-11-23

- Q: Where are sessions kept: signed cookies with nothing on the server, or sessions kept by the
  application? → A: Kept by the application, through functions it gives the policy; sign-out and
  disabling an account take effect at once (FR-008).
- Q: Recordings store requests as they arrived, with passwords from sign-in forms and session cookies.
  Keep them, or leave them out and replay the check's outcome? → A: Keep them; recordings of a
  signed-in application are secrets, and the documentation says so; record and replay do not change
  (FR-015).

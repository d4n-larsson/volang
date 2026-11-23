# Feature Specification: Remove HTML Rendering from the Core

**Feature Branch**: `053-remove-html`

**Created**: 2026-11-23

**Status**: Implemented ([results](results.md))

**Input**: User description: "Specify the removal" (of HTML rendering). Decided 2026-11-23 with the
user: vo's target is **backend network services written by agents and reviewed by people** (APIs,
integrations, services behind a frontend). Rendering pages is the frontend's job and out of scope for
the core. Spec 051 (`std.html`, pushed in d4b377c) is undone; spec 052 (sessions and sign-in, pushed in
e007729) keeps everything that is HTTP and security and loses the one part that draws a page.

Background: spec 051 added `std.html` (templates with `{}` holes, checked by `vo check` in C, codes
E0335–E0340, an opaque `Html` type), `std.web`'s `html_reply`, a site guide "Web pages" and the
notes-pages example. Spec 052 built browser sign-in on top: its sign-in routes show the application's
sign-in page as `Html`. Neither has been released: both are under "Unreleased" in the changelog.

What stays, because it is the backend's part of talking to browsers:
- **Form bodies**: `application/x-www-form-urlencoded` bound into input records (spec 051); a frontend
  posts forms to a backend whatever renders them.
- **Cookies, sessions and browser sign-in** (spec 052): the cookie functions, the `browser` way, the
  sessions the application keeps, the cross-site refusal, the redirect of pages that need a user, sign
  in and sign out.
- **The parser fix** found during 052 (effect parameters after written effects in type arguments).

What this spec must keep:
- **Principle II**: codes are never reused or redefined; E0335–E0340 stay in the catalog, marked
  retired.
- **Principle V**: the reference shrinks; the tokens freed are reported.
- Every remaining test passes, on x86-64, ARM64 and RISC-V; programs that do not use `std.html` check
  and run exactly as before.

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents writing vo services, the people who review them, and the people who
maintain vo.

### User Story 1 - The core has no HTML rendering (Priority: P1)

An agent reading the reference, `vo doc` or the site finds no way to render HTML in vo, and so does not
try: pages are made by whatever frontend the service sits behind. The toolchain carries no template
checker.

**Why this priority**: it is the decision; everything else follows from it.

**Independent Test**: `vo doc` lists no `std.html`; the reference and the site guides do not mention
it; a program importing it gets the ordinary diagnostic for an unknown module; the full test suite
passes without the removed cases.

**Acceptance Scenarios**:

1. **Given** the toolchain after the change, **When** `vo doc` lists the library, **Then** there is no
   `std.html` and `std.web` has no `html_reply`.
2. **Given** a program with `import std.html.html;`, **When** checked, **Then** it gets the same
   diagnostic as any import of a module that does not exist, with no special case.
3. **Given** the reference, the site's guides and the examples, **When** searched, **Then** nothing
   tells an agent to render HTML with vo.
4. **Given** every program in the repository that does not use `std.html` and its golden output,
   **When** checked and run, **Then** the output is byte-identical to before.

---

### User Story 2 - Retired codes stay documented (Priority: P1)

An agent or a person who meets E0335–E0340 in an old log or conversation looks them up and learns they
were retired and why; no new diagnostic ever uses those codes.

**Why this priority**: Principle II makes codes permanent; this is the first time codes are retired,
so the form of a retired entry is set here.

**Independent Test**: `vo explain E0337` and the catalog page answer with "retired" and the reason; the
catalog check accepts retired entries without a golden case and refuses a retired code that the
toolchain still emits.

**Acceptance Scenarios**:

1. **Given** `vo explain E0335` (each of E0335–E0340), **When** run, **Then** it says the code is
   retired, since when, and why, and exits like a lookup of an existing code.
2. **Given** the catalog check, **When** run, **Then** a retired code needs no triggering case, and a
   retired code that appears in the toolchain's source as an emitted code is a failure.

---

### User Story 3 - Browser sign-in without pages (Priority: P1)

An agent builds a service whose frontend (a static page, a JavaScript application, another server)
shows the sign-in page. The service signs browsers in with the session cookie as spec 052 does, but
draws nothing: a failed sign-in sends the browser back to the frontend's sign-in page with the reason
in the address, or answers a JavaScript client with a problem.

**Why this priority**: spec 052's security work stays usable; only the page goes.

**Independent Test**: the 052 cases, changed to the new answers, pass; the threat-removal check of 052
still finds a failing test for every protection.

**Acceptance Scenarios**:

1. **Given** the browser way's settings, **When** written, **Then** they name the sign-in path and no
   page; the library adds no route that answers a page.
2. **Given** a form post of a right name and password, **When** it arrives from the application's own
   origin, **Then** the answer is 303 to the return path with the session cookie, as in 052.
3. **Given** a form post of a wrong name or password, an unknown name or a disabled account, **When**
   it arrives, **Then** the answer is 303 to the sign-in path with a failure marker and the return
   path in the query (`/sign-in?failed=1&next=%2Fnotes`), no cookie, the same `refused` event, and the
   same time taken whether or not the name exists.
4. ~~JSON sign-in~~ dropped while implementing: `std.web` binds a JSON body only to a field named
   `body`, so one input record cannot take a form and JSON. A frontend script posts the same form
   encoding (`URLSearchParams`) and sees where it is sent.
5. **Given** a page request that needs a user, a stale cookie, a cross-site post, sign-out: **Then**
   the answers are exactly those of spec 052.

---

### User Story 4 - The examples show a backend (Priority: P2)

An agent looking for how to sign browsers in finds it in the example that already shows access
control: the secure-notes service signs programs in with tokens and browsers in with a session cookie,
under one table of rules.

**Why this priority**: the notes-pages example goes with `std.html`; its sign-in part must not go with
it.

**Independent Test**: the secure-notes example's tests cover both ways of signing in; the examples
index has no notes-pages row.

**Acceptance Scenarios**:

1. **Given** the secure-notes example, **When** its tests run, **Then** a browser signs in with the
   form, uses a note route with the cookie, is refused a cross-site post, and signs out; a program
   still signs in with a token; both reach the same operations under the same rules.
2. **Given** the examples index and the site, **When** read, **Then** notes-pages is gone and nothing
   links to it.

---

### User Story 5 - The positioning is written down (Priority: P2)

A person or agent reading the README learns what vo is for: backend network services written by
agents and reviewed by people. The roadmap orders its open gaps by that.

**Why this priority**: the removal is the first decision taken from this positioning; it should be
written where the next decision will be judged.

**Acceptance Scenarios**:

1. **Given** the README, **When** read, **Then** its first paragraph names backend network services
   written by agents and reviewed by people as vo's target.
2. **Given** the roadmap, **When** read, **Then** spec 051 is marked removed with the reason, 052 is
   described without pages, and the open gaps are ordered by "does it make an agent-written network
   service easier to write, review or operate?".

### Edge Cases

- **A golden case or test outside the removed ones that used `std.html`**: none is expected outside
  `tests/cases/html` and notes-pages; any found is changed to not need it, or removed with the rest.
- **Documents that link to the removed guide or example** (the site's navigation, `apis.md`, the
  examples index, the review document's Templates row): updated, and the site build's link check
  passes.
- **Spec 051's own documents** stay in `specs/` as the record of the decision, marked removed with a
  pointer to this spec.
- **The two `std.web` cases whose expected output carries line numbers of `std/web.vo`** change again
  when `html_reply` and its imports go; they are re-accepted after reading the diff.
- **Recordings**: none outside notes-pages uses `std.html`; the secure-notes recording, if any, is made
  again after its change.

## Requirements *(mandatory)*

### Functional Requirements

**Removal**

- **FR-001**: The toolchain MUST NOT contain `std.html`, its template checker, its builtins or
  `html_reply`; `Html` MUST NOT be a type anywhere in the library.
- **FR-002**: A program importing `std.html` MUST get the ordinary unknown-module diagnostic.
- **FR-003**: Every test that exercised only `std.html` (its golden cases, the safety and speed tests,
  the example) MUST be removed with it; no other test may lose coverage it had.
- **FR-004**: Programs that do not use `std.html` MUST check, run, test and replay byte-identically to
  before, on the three architectures.

**Retired codes**

- **FR-005**: E0335–E0340 MUST stay in the catalog marked retired, with the date, the spec that retired
  them and the reason; `vo explain` MUST say so.
- **FR-006**: The catalog check MUST accept a retired code without a triggering case and MUST fail if
  the toolchain can still emit a retired code.

**Browser sign-in without pages**

- **FR-007**: The browser way's settings MUST NOT include a page; the library MUST NOT add a route that
  answers the sign-in page.
- **FR-008**: The sign-in route MUST accept the name, password and return path as a form and answer
  303 to the return path (success, with the cookie) or to the sign-in path with `failed=1` and the return
  path (failure, no cookie). (JSON was in the first version of this requirement: dropped, see User
  Story 3, scenario 4.)
- **FR-009**: Everything else of spec 052 (cookies, sessions kept by the application, the cross-site
  refusal, the page redirect, stale cookies, sign-out, events, timing, review) MUST behave as before.

**Examples and documents**

- **FR-010**: The secure-notes example MUST show browser sign-in with sessions beside tokens, with tests
  for sign-in, a cookie-signed request, a refused cross-site post and sign-out; notes-pages MUST be
  removed.
- **FR-011**: The reference, `docs/security.md`, the site guides (the "Signing in" section moves to the
  access-control part of the APIs guide), the changelog (the Unreleased entries of 051 and 052 rewritten
  so that nothing of `std.html` appears as a change), the roadmap and spec 051's status MUST be updated.
- **FR-012**: The README's first paragraph MUST state vo's target, and the roadmap's open gaps MUST be
  ordered by it.

### Key Entities

- **Retired code**: a diagnostic code no longer emitted, kept in the catalog with when and why.
- **Browser way settings**: the sign-in and sign-out paths, the lifetime, the origin and the
  application's account and session functions; no page.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The full test suite passes on x86-64, and the security, web and generic cases on ARM64
  and RISC-V.
- **SC-002**: Searching the reference, the site's guides, `vo doc` output and the examples for
  `std.html`, `html(`, `html_reply` and `Html` finds nothing (spec records and the changelog's
  historical lines aside).
- **SC-003**: Every golden case outside the removed ones gives the same output as before the change,
  except the cases of spec 052's sign-in answers and the two line-number cases, each listed with its
  reason.
- **SC-004**: Each of the 12 protections of spec 052, removed in turn, still makes at least one test
  fail.
- **SC-005**: The reference shrinks; the number of tokens freed is reported.
- **SC-006**: The toolchain's C source shrinks by the template checker and its wiring; lines removed
  are reported.

## Assumptions

- **Nothing was released**: no published version contains `std.html`, so no deprecation period or
  removal notice in the diagnostic is needed (FR-002); the changelog simply never lists it.
- **No JSON sign-in on the browser route** (decided while implementing; the first version said the
  binding read both into one record, which it does not): programs keep the token route
  (`sign_in_route`), scripts post the form encoding.
- **The failure marker is `failed=1`** (decided), with no reason text in the address: the frontend shows
  one message for every failure, as the library's page did, so the address does not say whether a name
  exists.
- **secure-notes keeps its notes and accounts in memory**; sessions are kept in the same store (one
  worker), so it needs no database.
- **The constitution** gets the positioning in a separate `/speckit-constitution` step; this spec only
  writes it in the README and the roadmap.
- **Form binding stays in `std.web`**, and so does the content-type rule naming both encodings.
- Out of scope: an HTML package outside the core (possibly later, as a builder API that is safe by
  construction), serving static files, any other `std.web` change.

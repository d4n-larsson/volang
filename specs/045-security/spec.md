# Feature Specification: Security (who may call what, for `std.web` applications)

**Feature Branch**: `045-security`

**Created**: 2026-11-22

**Status**: Implemented ([results](results.md))

**Input**: User description: "a new vo framework inspired by spring security"

Background: a `std.web` application today has two steps for access control, `bearer_auth` and
`api_key_auth`. Each guards one path prefix, hands the application's own function a token, and passes
the name it returns to the operation as a header. Everything else is written by hand in every service:
who may call which route, roles, checking a password, issuing and checking tokens, the difference
between "not signed in" (401) and "not allowed" (403), response headers, a log of refusals. A route
added without a step in front of it is open, and nothing says so. Hand-written access control is
where a reviewer most needs help and gets least.

Spring Security's answer, in the Java world, is the model this feature borrows: every request is
refused unless a rule allows it; the rules for the whole application are one table; ways of signing in
are plugged into that table; the signed-in user with roles is available to the code; passwords and
tokens are handled by the framework, not the application. What vo adds is that the table is a plain
value: it can be printed, tested as a pure function, and checked against the application's routes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - One table says who may call what, and nothing is open by accident (Priority: P1)

A developer writes the access rules of a service as one ordered list: the health route is open, `GET`
under `/notes` needs a signed-in user, everything under `/admin` needs the role `admin`. A route no
rule mentions is refused.

**Why this priority**: it is the whole point. The failure it removes (a new route that is open because
nobody put a guard in front of it) is the most common access-control bug.

**Independent Test**: an application with five routes and a three-rule policy answers every
combination of route and caller (nobody, a user, an admin) as the table says; a sixth route added
without a rule answers 401 to nobody and 403 to everybody else.

**Acceptance Scenarios**:

1. **Given** a rule "open" for `/health`, **When** a request without credentials arrives, **Then** the
   operation runs.
2. **Given** a rule "signed in" for `GET /notes`, **When** a request without credentials arrives,
   **Then** the answer is 401 with a `WWW-Authenticate` header naming the ways to sign in that the
   policy accepts, and the operation does not run.
3. **Given** a rule "role `admin`" for `/admin`, **When** a signed-in user without that role calls it,
   **Then** the answer is 403 and the operation does not run; with the role, it runs.
4. **Given** a request that no rule matches, **When** it arrives, **Then** it is refused: 401 without
   credentials, 403 with valid ones. This holds for paths the application has no route for: an
   unauthenticated caller cannot learn which paths exist.
5. **Given** two rules that both match a request, **When** it arrives, **Then** the first in the list
   decides.
6. **Given** a rule limited to some methods (`GET`), **When** another method (`DELETE`) is used on the
   same path, **Then** that rule does not apply and the following rules are tried.
7. **Given** a request with credentials that are present but wrong (bad password, tampered token),
   **When** it arrives, **Then** the answer is 401 whatever the rule, the open one included.

---

### User Story 2 - Sign in with a password, a signed token or a key (Priority: P1)

A developer chooses the ways callers prove who they are: a name and password on each request (HTTP
Basic), a signed token that carries the name, the roles and an expiry, or a key that the application
looks up itself. A caller exchanges a password for a token at a sign-in route the module provides.

**Why this priority**: rules about users need users. Password checking and token signing are the code
that must not be written by hand.

**Independent Test**: tokens made by the module are accepted by an independent implementation of the
token standard and the reverse; every tampered, expired or differently signed token is refused.

**Acceptance Scenarios**:

1. **Given** a policy accepting Basic and an application function that finds a user by name (stored
   password hash, roles, enabled or not), **When** a request carries the right name and password,
   **Then** it is handled as that user with those roles.
2. **Given** a wrong password, an unknown name or a disabled user, **When** the request arrives,
   **Then** the answer is 401 with the same body in all three cases, and for the first two after the
   same amount of password work, so that names cannot be told apart by the answer or its timing.
3. **Given** a signing key, a user and a lifetime, **When** a token is issued, **Then** it is a
   standard signed web token (JWT, HMAC-SHA256) carrying the name, the roles, the time of issue and
   the expiry.
4. **Given** a token whose signature does not verify under any accepted key, that has expired, that is
   not yet valid, that names another signing method (including "none"), or that is not a token at all,
   **When** it is presented, **Then** the answer is 401 and the reason is in the security event
   (story 4), not in a form that helps the caller forge one.
5. **Given** two accepted keys (rotation), **When** tokens signed by either are presented, **Then**
   both are accepted; new tokens are signed with the first.
6. **Given** the sign-in route of the module in the application, **When** a name and password are
   posted to it, **Then** the answer carries a token and its expiry; a wrong pair is answered as in
   scenario 2.
7. **Given** a policy accepting keys or opaque tokens checked by the application's own function (as
   `bearer_auth` and `api_key_auth` do today), **When** the function returns a user, **Then** the
   request is handled as that user with the roles the function gave.
8. **Given** a stored password hash made with a lower cost than the current one, **When** the password
   is checked successfully, **Then** the application can learn that the hash should be replaced and
   get the replacement.

---

### User Story 3 - The operation knows the user (Priority: P1)

An operation receives the signed-in user (name and roles) as typed input, and can refuse on its own
grounds: "only the owner of the note or an admin may delete it".

**Why this priority**: rules by route cannot express ownership; without this every operation parses a
header by hand.

**Independent Test**: an operation that returns the caller's name and roles returns exactly those of
the credentials; a request that carries a forged user in the place the module uses is handled as the
real user or refused, never as the forged one.

**Acceptance Scenarios**:

1. **Given** an operation that takes the current user, **When** it is called by a signed-in user,
   **Then** it gets the name and the roles.
2. **Given** an open route called without credentials, **When** the operation asks for the user,
   **Then** it gets "nobody" in a form it must handle (an option or an error, not an empty name).
   The operation declares one input field for the identity and gets the user from it with one call;
   it never parses a header itself.
3. **Given** a client that sends, in its request, the data the module uses to pass the user on,
   **When** the request arrives, **Then** that data is dropped before anything reads it.
4. **Given** an operation that requires a role or a condition of its own, **When** the condition is
   false, **Then** one call gives the 403 answer ("forbidden") in the form `std.web` already maps.

---

### User Story 4 - The policy can be reviewed and tested without a server (Priority: P1)

A reviewer, or a test, asks the policy itself: who may call this route? Which routes does no rule
cover? Which rules can never apply? A test calls the application as a given user without a password
or a token.

**Why this priority**: this is what vo adds to the model. A policy that can only be understood by
sending requests is the situation the feature exists to end.

**Independent Test**: the decision function, called for every route and every kind of caller of the
example service, agrees with the answers of the running application in every case; a policy with a
misspelt path is reported.

**Acceptance Scenarios**:

1. **Given** a policy, a method, a path and a caller (nobody or a user), **When** the decision is
   asked for, **Then** it is one of: allowed, sign-in needed, forbidden; the function is pure (no
   effects), so it can be used in tests, properties and contracts.
2. **Given** an application and a policy, **When** the review is asked for, **Then** it lists every
   route that no rule matches (refused by default: intended or forgotten?) and every rule that is
   never the deciding rule for any route (a misspelt path, or a rule hidden behind an earlier one).
3. **Given** a policy, **When** its description is asked for, **Then** it is a table, one line per
   route of the application: method, path, and who may call it; stable enough to keep as a golden
   file, so that a change of access shows up in a diff.
4. **Given** a test, **When** it sends a request "as user ada with role admin" through
   `handle_request`, **Then** the request is handled as that user without credentials. The way this
   is done is an explicit part of the policy value the test builds: a served application that does not
   write it into its policy does not have it.
5. **Given** a function the application supplies for security events, **When** a sign-in succeeds, a
   sign-in is refused, or access is forbidden, **Then** the function is called with the kind, the
   reason, the name if known, the method and the path, and never with a password, a key or a token.

---

### User Story 5 - Safe response headers by default (Priority: P2)

A developer adds one step and every answer carries the headers a security scan asks for.

**Why this priority**: small, but it is asked of every service and always copied from somewhere.

**Acceptance Scenarios**:

1. **Given** the step, **When** any answer leaves, **Then** it carries `X-Content-Type-Options:
   nosniff`, `X-Frame-Options: DENY`, `Referrer-Policy: no-referrer` and `Cache-Control: no-store`.
2. **Given** an operation that set one of these headers itself, **When** the answer leaves, **Then**
   the operation's value is kept.
3. **Given** the step made with a transport-security lifetime, **When** any answer leaves, **Then** it
   carries `Strict-Transport-Security` with that lifetime; without one, the header is not sent.

---

### User Story 6 - Documented with a secured example, within budget (Priority: P2)

The guide explains the model on one page with a complete secured service, the examples directory has
that service, and the reference grows by one library row.

**Acceptance Scenarios**:

1. **Given** the example service, **When** its tests run, **Then** they cover every route for nobody,
   a user and an admin, and its policy description is compared with a text checked in
   with them.
2. **Given** the guide's page on APIs, **When** read, **Then** it shows the policy, a sign-in, an
   operation using the user, a test as a user, and the review; `bearer_auth` and `api_key_auth` are
   described as the short form for a single guarded prefix.
3. **Given** the reference, **When** counted, **Then** it is under 15,000 tokens.

### Edge Cases

- A policy with no rules refuses everything.
- A policy that accepts no way of signing in and has a rule other than "open" or "refused" is a
  mistake the review reports (nobody can ever satisfy it).
- A request with credentials for two accepted ways at once (a Basic header and a key header) is
  refused with 401: which one counts must not depend on order.
- A Basic header that is not valid base64, has no `:`, or has an empty name is 401, not 400 or 500.
- A password containing `:` works (the name ends at the first `:`).
- A token with a valid signature whose content is not the expected shape (no name, roles not a list
  of texts, expiry not a number) is refused.
- A token without an expiry is refused: the module never issues one, and does not accept one.
- A signing key shorter than 32 bytes is refused when the policy is made, not when a token arrives.
- A token lifetime of zero or less is refused when a token is issued.
- A role name is compared exactly (case-sensitive); a user with no roles is still signed in.
- Path matching is by whole segments: a rule for `/admin` covers `/admin` and `/admin/users`, not
  `/administrator`. A path with `..`, `//` or an encoded `/` is matched as the application's router
  would see it, so a rule and the route it guards cannot disagree.
- The application's function for finding a user fails (the database is down): the answer is 500 as for
  any failing operation, not 401, and nothing is allowed.
- The function for security events returns nothing, so it cannot change the answer.
- An `Authorization` header in a scheme the policy does not accept counts as wrong credentials (401).
- Preflight requests of `cors` (OPTIONS without credentials) are answered by the `cors` step when it
  is placed before the policy; the guide says in which order to put the steps.
- Time comes from the clock the application gives (as `log_requests` does), so tests of expiry are
  deterministic.

## Requirements *(mandatory)*

### Functional Requirements

**The policy**

- **FR-001**: The standard library MUST provide a module `std.security` that turns a policy value
  into a step of a `std.web` application. No new syntax and no change to how applications without it
  behave.
- **FR-002**: A policy MUST hold an ordered list of rules. A rule has a path, optionally a
  set of methods, and one access: open to anyone, any signed-in user, a user with at least one of
  the named roles, or refused. A rule's path is always a prefix of whole segments, each a literal or
  a `{variable}` that stands for any one segment: there is one matching form.
- **FR-003**: For each request the first rule that matches its method and path MUST decide. A request
  that no rule matches MUST be refused.
- **FR-004**: A refused request MUST be answered 401 when it carries no credentials and 403 when it
  carries valid ones; the operation MUST NOT run. The 401 answer MUST carry `WWW-Authenticate` for the
  accepted ways that have a standard scheme. Both answers MUST be in the problem format `std.web`
  uses for its other errors.
- **FR-005**: Credentials that are present and not valid MUST be answered 401 on every route.
  Credentials for more than one accepted way in one request MUST be answered 401.
- **FR-006**: The decision MUST be available as a pure function of policy, method, path and caller
  (nobody or a user) with three outcomes: allowed, sign-in needed, forbidden. The step MUST answer as
  this function says.

**Signing in**

- **FR-007**: A user MUST be a record of a name and a list of role names.
- **FR-008**: A policy MUST accept zero or more ways of signing in, each explicit in the policy
  value: HTTP Basic, signed tokens, and a credential read from a named header and checked by a
  function of the application that returns a user.
- **FR-009**: For Basic, the application supplies a function from a name to an account (name, stored
  password hash as `std.crypto.hash_password` makes, roles, enabled). An unknown name MUST cost the
  same password work as a wrong password; unknown name, wrong password and disabled account MUST give
  the same answer.
- **FR-010**: Signed tokens MUST be JWTs signed with HMAC-SHA256, carrying subject (the name), roles,
  issued-at and expiry. Checking MUST refuse any other signing method, a signature that verifies
  under none of the accepted keys (compared in constant time), a missing or passed expiry, a
  not-before in the future, and content of the wrong shape. Keys shorter than 32 bytes MUST be refused
  when the policy or the token is made.
- **FR-011**: The module MUST provide issuing a token for a user with a lifetime in seconds, and a
  ready sign-in route that exchanges a name and password (checked as in FR-009) for a token and its
  expiry.
- **FR-012**: After a successful password check the application MUST be able to learn that the stored
  hash was made with a lower cost than the current one, and obtain a new hash to store.
- **FR-013**: Time MUST come from a clock function the application supplies; the module MUST NOT read
  the clock or random source otherwise, so that its behaviour under `vo test`, `--seed` and
  record/replay is deterministic and its effects are the ones the application's functions declare.

**The user in the operation**

- **FR-014**: An operation MUST be able to get the current user as a typed value (name and roles)
  from one declared input field and one call, as an option that is empty on open routes called
  without credentials. Anything a client sends in the place the module
  uses to pass the user on MUST be removed before the application sees the request, whether or not the
  policy step runs for that path.
- **FR-015**: The module MUST provide checks for use inside operations (has this role, has any of
  these roles) and one call that produces the `forbidden` error `std.web` maps to 403.

**Review and tests**

- **FR-016**: The module MUST provide a review of a policy against an application: the routes no rule
  matches, the rules that decide for no route, the rules that decide for only some requests of a
  route (a literal in the rule where the route has a variable), and rules no caller can satisfy because the policy
  accepts no way of signing in.
- **FR-017**: The module MUST provide a description of a policy against an application: one line per
  route with method, pattern and who may call it, in the order of the application's routes, the same
  text for the same input.
- **FR-018**: The module MUST provide a way of signing in meant for tests: a request marked "as this
  user" is handled as that user. It MUST be one of the explicit ways in the policy value (FR-008), so
  it exists only in an application whose source lists it, and the review (FR-016) MUST report its
  presence.
- **FR-019**: A policy MAY carry a function for security events. It MUST be called for each
  successful sign-in at the sign-in route, each refused credential and each forbidden request, with
  kind, reason, name if known, method, path and time. Passwords, keys and tokens MUST NOT appear in
  an event, an answer or an error text of the module.

**Headers**

- **FR-020**: The module MUST provide a step that adds the headers of story 5 to answers that do not
  already have them, with `Strict-Transport-Security` only when a lifetime is given.

**Keeping what exists**

- **FR-021**: `bearer_auth` and `api_key_auth` of `std.web` MUST keep working unchanged. No existing
  golden case may change except the listing of modules, cases that list the exports of a module that
  gains one, and the fingerprint line of recordings whose programs import a changed module.
- **FR-022**: The feature MUST add no syntax and no diagnostic code. It SHOULD add no builtin: what it
  needs beyond `std.crypto`, `std.bytes` and `std.json` (the URL-safe form of base64) is written in
  vo unless the plan measures that to be too slow.
- **FR-023**: Every exported item MUST have a doc comment with examples that run; the guide on the
  site MUST document the module with the example service; the site MUST NOT mention specs or other
  development material; the reference MUST stay under 15,000 tokens (14,873 now: one library row).
- **FR-024**: The examples directory MUST gain a secured service (or an existing one secured) whose
  tests cover every route for each kind of caller and whose policy description is checked in with those
  tests.

### Key Entities

- **Policy**: the ordered rules, the accepted ways of signing in, the clock, optionally the function
  for security events. One value per application; the step is made from it.
- **Rule**: a path, optionally methods, and an access (open, signed in, any of these roles, refused).
- **User**: a name and role names. What an operation receives.
- **Account**: what the application returns for a name when a password is to be checked: name, stored
  password hash, roles, enabled.
- **Way of signing in**: Basic with an account lookup; signed tokens with their keys; a named header
  with a checking function; the test way.
- **Token**: a JWT (HMAC-SHA256) with subject, roles, issued-at, expiry.
- **Decision**: allowed, sign-in needed, or forbidden.
- **Security event**: kind (signed in, refused, forbidden), reason, name if known, method, path, time.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In the example service, for every route and every kind of caller (nobody, wrong
  credentials, each role, each way of signing in), the answer of the application equals the pure
  decision: 100% of the combinations, and no operation runs on a refused request.
- **SC-002**: 1,000 random tokens issued by the module verify in an independent implementation of
  JWT HMAC-SHA256 (written with Python's standard library), and 1,000 issued there are accepted by
  the module with the same name, roles and expiry.
- **SC-003**: 100% of 10,000 damaged tokens are refused: any one character changed, signing method
  replaced (including "none"), another key, expired, not yet valid, parts removed or added.
- **SC-004**: With Basic, the median time to refuse an unknown name is within 10% of the median time
  to refuse a wrong password for a known name, and the two answers are byte-identical.
- **SC-005**: A request authenticated by a token costs at most 100 microseconds more than the same
  request with no policy step, measured through `handle_request` on x86-64 (10,000 requests in under
  one second added).
- **SC-006**: A policy with a misspelt path, a route without a rule, a hidden rule and the test way of
  signing in gets exactly those four findings from the review; the example service gets none.
- **SC-007**: Securing the example service takes at most 15 lines of policy for its routes, and no
  operation of it parses a header to find the user.
- **SC-008**: The reference is under 15,000 tokens; every doc example of the module passes; the full
  test suite passes on x86-64, and the module's cases pass on ARM64 and RISC-V.

## Assumptions

- **A standard-library module, `std.security`**, as `std.web` is: "framework" is read as a module
  that plugs into `std.web` applications, not a separate package to download.
- **Stateless only in this version**: Basic, signed tokens and application-checked keys. A served
  application runs as several worker processes that share no memory, so sessions kept on the server
  need a shared store; that, cookies, and the cross-site request forgery protection cookies require
  are left for a later version. Token revocation before expiry is the application's business (a short
  lifetime, or a check in its own function).
- **Users are the application's**: the module asks a function for an account; it does not store
  users, register them, reset passwords or lock accounts. Counting failures and locking is done by the
  application from security events and the `enabled` field. Events are per worker process.
- **Replacing an outdated password hash** (FR-012) is for an application that writes its own
  sign-in operation from the module's parts; the ready sign-in route only reports the outdated hash
  as a security event, because the module never makes a hash and never hands out a password.
- **Basic costs a password check on every request** (deliberately slow work). The guide recommends
  the sign-in route and tokens for anything called often.
- **Roles are plain texts**, no hierarchy ("admin implies user" is written as two roles in the rule)
  and no permissions per object: ownership checks are code in the operation (story 3).
- **First match decides**, as in Spring Security; the review makes hidden rules visible instead of a
  cleverer matching order.
- **Wrong credentials are refused on open routes too**: a client with a broken token learns it at
  once.
- **No clock leeway** for token expiry in this version.
- **Only HMAC-SHA256 tokens**: public-key signatures (RS256, ES256) need primitives `std.crypto` does
  not have.
- Out of scope: OAuth2 and OpenID Connect sign-in, SAML, LDAP, sessions and cookies, CSRF,
  remember-me, multi-factor, rate limiting, role hierarchies, per-object access lists, public-key
  tokens, encrypting tokens, a Content-Security-Policy builder, changes to `bearer_auth` and
  `api_key_auth`.

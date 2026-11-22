---

description: "Task list for vo-lang spec 045: Security (who may call what, for std.web applications)"
---

# Tasks: Security (who may call what, for `std.web` applications)

**Input**: Design documents from `specs/045-security/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/security.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden case and its group in
`tests/web_security.py` land first and fail before its code.

**Organization**:
- Setup: the module registered, `problem_reply`, the test script, the area wiring.
- Foundational: the types.
- US1 (P1) the policy and the step; US3 (P1) the user in the operation (it needs only US1, and US2's
  cases use it, so it comes second); US2 (P1) passwords, tokens, the sign-in route, events;
  US4 (P1) description and review; US5 (P2) headers; US6 (P2) the example and documentation.
- Polish: three architectures, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Types, signatures, error texts**: contracts/security.md and data-model.md (the decision table, the
  request table, the token form and refusal reasons, the headers). Algorithms: research.md R2 (ways
  and candidates), R3 (`X-Identity`), R4 (passwords), R5 (tokens), R6 (matching, review), R7 (answers).
- **Golden cases** live in `tests/cases/web_security/` (not `tests/cases/security`, which belongs to
  the security review). A case needs a `.code` file to be discovered; run one directory per call of
  `tests/run.sh`; accept with `--accept` and review with `git diff`/`git status tests/cases`. Each
  case prints `label -> status [headers] body` lines on the pattern of `tests/cases/web/ready_steps.vo`.
- **No existing golden case may change**, except `tests/cases/doc/all.out` and `all_json.out` (the
  module listing) and any case that lists the exports of `std.web` or `std.crypto` (one new line each).
  If recordings' fingerprints change because `std/web.vo` or `std/crypto.vo` changed, update the one
  fingerprint line in each `record_replay` step case and rerun `python3 tests/steps.py` on them.
- **Doc comments**: every exported item has one with examples in the form `#     EXPR is VALUE`;
  private functions have none (comments go inside the body). No spec numbers in any doc comment or on
  the site.
- **Password hashes in tests** have cost 1 (`pbkdf2-sha256$1$c2FsdA==$VawEblbjCJ/sFpHCJUS2BflBhSFt3gRl5oudV8INrLw=`
  is "passwd"), except where timing is the subject.
- **Secrets**: no password, key or token in any answer, event or error text.
- `make quick AREA=web_security` between edits; `make test` once, in T021; the gate is not run.

## Phase 1: Setup

- [X] T001 Create `std/security.vo` with only its header comment (what the module is, how an
  operation gets the user; "Part of the vo standard library."); add it to `STD_FILES` in `Makefile`
  (after `std/web.vo` and `std/crypto.vo`) and `"security"` to `doc_modules` in `src/doc.c`; add
  `web_security.py` to the test list in `Makefile`; add area `web_security` to `tools/quick.sh`
  (golden areas `web_security web doc crypto`; scripts `ref_examples.py doc_conformance.py
  web_security.py`; file mapping for `std/security.vo`, `tests/web_security.py`); `make` builds.
- [X] T002 In `std/web.vo`: `export fn problem_reply(status: int, detail: str) -> Response` calling the
  private `problem` with no errors, with a doc comment and a run example. `make quick AREA=web` passes.
- [X] T003 Create `tests/web_security.py` on the pattern of `tests/decimals.py` (`--group`, `case()`,
  temp directory under `build/tmp`, emulation and `CI` detection) with groups `tokens`, `timing`,
  `speed` as in research.md R8; a Python reference for JWT HMAC-SHA256 from `hmac`, `hashlib`,
  `base64`, `json`. The `tokens` group also sends validly signed tokens of the wrong shape (no
  `sub`, `roles` not a list of texts, no `exp`, `exp` not an integer, claims not an object): each
  refused with reason `content`. It also runs the `sign_in` golden case's program and asserts that
  neither the password, the key nor any token it issued appears in an event or in an answer other
  than the sign-in route's own token (FR-019). All groups fail now.

## Phase 2: Foundational

- [X] T004 In `std/security.vo`: the records and variants of data-model.md (`User`, `Account`,
  `Access`, `Rule`, `Decision`, `Way[C, E]`, `Policy[C, E]`, `Event`), each with a doc comment and a
  comment per field or case; `rule`, `rule_for`, `policy` (its `on_event` does nothing).

## Phase 3: US1 - One table says who may call what (P1)

**Goal**: rules decide; nothing is open by accident. **Independent test**: `tests/cases/web_security/policy`.

- [X] T005 [US1] Golden case `tests/cases/web_security/policy.vo` (+`.code`, `.out` by hand from the
  spec): five routes, the rules of story 1, callers nobody / user / admin through `test_users` and
  `as_user`; scenarios 1 to 7 (wrong credentials on the open route through a `header_key` way whose
  check refuses), a sixth route without a rule, an unknown path, `HEAD`, a method-limited rule, a
  policy with no rules, two ways at once, `Authorization: Digest x`, `/administrator` against a rule
  for `/admin`, a `header_key` check that fails with another error (500), and `decide` printed for
  the same matrix; the paths `/admin/../health`, `//admin` and `/admin%2Fusers` answered by the
  policy and, with a user allowed everything, by the router, to show a rule and its route see the
  same segments. The application's effect set is empty (`App[int]`), so the case compiles only if
  the module has no effects of its own (FR-013). Fails now.
- [X] T006 [US1] `decide` in `std/security.vo` (R6: segments, prefix match, `{variable}`, `HEAD` as
  `GET`, first match, the decision table of data-model.md), pure.
- [X] T007 [US1] First check that `test_users()` and `tokens(keys)` get their `C` and `E` from an
  annotated list (`let ways: list[Way[int]] = [...]`); if not, record it and give them a parameter
  that fixes the types. `secure`, `header_key`, `test_users`, `as_user` in `std/security.vo`: the request
  table of data-model.md (candidates as R2), answers with `problem_reply` and `WWW-Authenticate` per
  accepted scheme (R7), the user as JSON in `X-Identity` (R3), events `refused` and `forbidden`
  through `on_event` with the time from `now`.

## Phase 4: US3 - The operation knows the user (P1)

**Goal**: name and roles in the operation. **Independent test**: `tests/cases/web_security/user`.

- [X] T008 [US3] Golden case `tests/cases/web_security/user.vo`: an operation returning the caller;
  an open route without credentials (`current_user` is none, `require_user` is 401); a forged
  `X-Identity` and a forged `X-Test-User` without `test_users` in the policy; `require_role` giving
  403; an owner check with `has_role`; a bare name in `X-Identity` (as `bearer_auth` sets) read as a
  user without roles. Fails now.
- [X] T009 [US3] `current_user`, `require_user`, `require_role`, `has_role` in `std/security.vo`.

## Phase 5: US2 - Sign in with a password, a token or a key (P1)

**Goal**: Basic, JWT, the sign-in route. **Independent test**: `tests/web_security.py --group tokens`
and `tests/cases/web_security/sign_in`.

- [X] T010 [US2] Golden cases: `tests/cases/web_security/sign_in.vo` (story 2 scenarios 1 to 8 with a
  fixed clock: Basic right / wrong password / unknown name / disabled with identical bodies, bad
  base64, no `:`, empty name, a password containing `:`; a token issued and used, expired, not yet
  valid, two keys; the sign-in route right and wrong; a lookup that fails is 500; the events printed,
  including `outdated_hash`; `password_outdated` on a cost-1 hash and on a text that is no hash) and
  `tests/cases/web_security/short_key` (R0012 from `tokens` with a 31-byte key). Fail now.
- [X] T011 [US2] `issue_token`, `check_token`, `tokens` in `std/security.vo` as R5 (URL-safe base64
  by `split`/`join`; claims read with `std.json` field functions; every key tried; the refusal
  reasons of data-model.md; `requires` for key length and lifetime). `tests/web_security.py --group
  tokens` passes: 1,000 tokens each way, 10,000 damaged ones all refused (SC-002, SC-003).
- [X] T012 [US2] `password_outdated` in `std/crypto.vo` (true for a well-formed hash of a cost below
  the current one; false otherwise), with doc examples.
- [X] T013 [US2] `password_user` and `basic` in `std/security.vo` as R4: a constant hash at the
  current cost for unknown names (a doc example or test states `password_outdated` of it is false);
  one error text for unknown, wrong and disabled; the reason only in the event.
- [X] T014 [US2] `sign_in_route` in `std/security.vo` (body `name`, `password`; answer `token`,
  `expires`; events `signed_in`, `refused`, `outdated_hash`). `tests/web_security.py --group timing`
  (SC-004: medians of 7 refusals each, unknown name against wrong password, within 10%, bodies
  identical; the spread of the samples goes into the results) and `--group speed` (SC-005) pass.

## Phase 6: US4 - Review and description (P1)

**Goal**: the policy answers questions without a server. **Independent test**:
`tests/cases/web_security/review`.

- [X] T015 [US4] Golden case `tests/cases/web_security/review.vo`: `describe` of the application of
  T005; `review` of a policy with a misspelt path, a route without a rule, a hidden rule and
  `test_users` (exactly those four findings, SC-006), of a policy with a rule literal on a route
  variable, of one with rules needing a user and no way, and of a clean one (no findings). Fails now.
- [X] T016 [US4] `describe` and `review` in `std/security.vo` as R6 and the forms of
  contracts/security.md.

## Phase 7: US5 - Safe response headers (P2)

- [X] T017 [US5] Golden case `tests/cases/web_security/headers.vo` (the four headers; an operation's
  own `Cache-Control` kept; with and without a transport-security lifetime; on a 401 of the policy
  when the step is first; a `cors` preflight answered when `cors` is before the policy and 401 when
  it is after), failing; then `safe_headers` in `std/security.vo`.

## Phase 8: US6 - The example and documentation (P2)

- [X] T018 [US6] Doc comments with run examples on every exported item of `std/security.vo`;
  `python3 tests/doc_conformance.py` passes; accept the module listing in `tests/cases/doc/all.out`
  and `all_json.out` after reading the diff.
- [X] T019 [P] [US6] `examples/secure-notes/` as R9 (`main.vo`, `api.vo`, `README.md`, `vo.effects`
  if the other examples have one): at most 15 lines of rules; tests: every route for nobody, a user
  and an admin with `handle_request` equal to `decide`, and a caller with a wrong token (401 on
  every route) (SC-001), the review empty, the description
  equal to the text in the test (SC-006, SC-007), an owner check, a token from the sign-in route
  used on a route. A row in `examples/README.md`. `bin/vo test` there and
  `python3 tests/build_examples.py` pass.
- [X] T020 [P] [US6] `docs/reference.md`: one `std.security` row in the library table (and
  `problem_reply`, `password_outdated` only if they fit; `python3 tests/ref_examples.py` stays under
  15,000). `site/docs/guide/apis.md`: a section on access control with the policy, the sign-in route,
  an operation using the user, a test as a user, the review, the order of steps with `cors`, the cost
  of Basic, that an expired token sent along with a sign-in request is refused before the sign-in
  runs, a sign-in operation of one's own that replaces outdated hashes, and `bearer_auth` /
  `api_key_auth` as the short form; no development material. `python3 tests/site_examples.py` and
  `make site-check` pass. `CHANGELOG.md`, `docs/roadmap.md` (row and section "045: security").

## Phase 9: Polish

- [X] T021 Three architectures: `make ARCH=aarch64` and `make ARCH=riscv64`; with `VO_BIN` set, run
  `tests/run.sh tests/cases/web_security` and `python3 tests/web_security.py` on each. Then
  `python3 tools/progress.py run test` once; read `build/progress/test.log`; fix and rerun only on
  failure.
- [X] T022 Write `specs/045-security/results.md` (SC-001 to SC-008 with figures, changed cases,
  deviations, what was not verified); set the spec's status to Implemented.

## Dependencies

- T001 → T002, T003, T004 → everything.
- US1 (T005–T007) → US3 (T008, T009) → US2 (T010–T014) → US4 (T015, T016). US5 (T017) needs only
  T004. US6 needs all functions.
- T019 and T020 touch different files and can run in parallel after T018.

## Implementation strategy

MVP is US1 with US3: a policy, the test way and the user in the operation already replace hand-written
guards. US2 adds the real ways of signing in. Stop after any phase with
`make quick AREA=web_security` green.

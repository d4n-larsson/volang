---

description: "Task list for vo-lang spec 053: Remove HTML Rendering from the Core"
---

# Tasks: Remove HTML Rendering from the Core

**Input**: Design documents from `specs/053-remove-html/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R7), data-model.md, contracts/changes.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Changed answers land as case changes that fail before the code.

**Organization**:
- Setup: results file and the "before" measurements.
- US3 (P1) comes first although it is the third story: `std/security.vo` imports `std.html`, so the
  page must leave sign-in before the module can be deleted.
- US1 (P1) the removal; US2 (P1) retired codes; US4 (P2) secure-notes; US5 (P2) positioning.
- Polish: docs, measurements, threat check, architectures, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **What changes for programs**: contracts/changes.md. File list: research.md R1.
- **Cases**: change the case, run `tests/run.sh DIR`, see it fail, change the code; `--accept` only after
  reading the actual output.
- **Between edits**: `make quick AREA=web_security,web`; `make test` once at the end; never two suites at
  once.
- **C**: zero warnings; `make` after each C edit.

## Phase 1: Setup

- [X] T001 Create `specs/053-remove-html/results.md` with headings SC-001 to SC-006 and "Before"; record before the change: `python3 tests/ref_examples.py` tokens, `wc -l src/*.c std/*.vo | tail -1`, and `tests/run.sh tests/cases` totals (all passing at e007729)

## Phase 3: User Story 3 - Browser sign-in without pages (P1) — first, see Organization

**Goal**: `Browser` has no page, `browser_routes` adds no page route, the sign-in route answers forms
with redirects and JSON with 204/401 (contracts/changes.md).

**Independent Test**: `tests/run.sh tests/cases/web_security` passes with the new answers.

- [X] T002 [US3] Change the 052 cases in `tests/cases/web_security/`: remove `page:` from every `Browser` (`browser.vo`, `cross_site.vo`, `sign_out.vo`, `stale_cookie.vo`, `review_browser.vo`) and the `std.html` imports and `sign_in_page`; in `browser.vo` replace the two `GET /sign-in` scenarios with: a form sign-in failure for each of wrong password, unknown name, disabled account answering `303` with `Location: /sign-in?failed=1&next=%2Fnotes` and no `Set-Cookie`; a foreign `next` on failure giving `next=%2F`; a JSON sign-in (`Content-Type: application/json`, body `{"name": "ada", "password": "passwd"}`) answering `204` with the cookie; a JSON failure answering `401` problem `invalid credentials` without a cookie; in `stale_cookie.vo` replace the "expired, on the sign-in page" request with a public route of the case's app (add one, `rule("/health", Access.anyone)`) so it still shows a stale cookie removed on an answered request; update `FORM_TIMING` in `tests/web_security.py` (no `page:`; its two refusals now answer `303`, so `measure` compares status and body as they are); run the cases and see them fail
- [X] T003 [US3] In `std/security.vo`: remove `import std.html.Html;`, `Browser.page` (field and doc), `SignInQuery` and the GET route; `browser_routes` returns `[at_path(on_post("/", take), b.sign_in), at_path(on_post("/", leave), b.sign_out)]` and its doc says the frontend serves the page at `sign_in`; `SignInForm` gains `@header("Content-Type") @default("") content_type: str;`; in `sign_in_form` a content type starting with `application/json` answers `204` with the cookie on success and `problem_reply(401, "invalid credentials")` on failure, otherwise `303` to `local_path(next)` with the cookie or `303` to `b.sign_in + "?failed=1&next=" + query_escape(local_path(f.next))`; update the `Browser`, `browser` and `browser_routes` doc examples (no `page:`, routes count 2); `make`; until T002's cases and `python3 tests/web_security.py --group timing` pass and `python3 tests/doc_conformance.py` reports 0 failures

**Checkpoint**: `std/security.vo` no longer imports `std.html`.

## Phase 4: User Story 1 - The core has no HTML rendering (P1)

**Goal**: no `std.html`, no template checker, no `html_reply`; programs without it unchanged.

**Independent Test**: `tests/run.sh tests/cases` passes; the quickstart's `git grep` finds nothing.

- [X] T004 [P] [US1] Golden case `tests/cases/check/std_html_gone.vo` (`.cmd` `check --json {}`, `.code`, `.out`): `import std.html.html;` and an empty `main`; its expected output is the diagnostic any unknown standard module gets today (write it from `import std.nosuch.x;`'s output with the name changed); see it fail (today it checks)
- [X] T005 [US1] In `std/web.vo`: remove `html_reply`, its doc comment and section header, and the imports `std.html.Html`, `std.html.html_text`, `std.html.no_html`; keep form-body binding, the 415 message and cookies; in `tests/cases/web/forms.vo` remove the `html_reply` scenario and the `std.html` imports, keep every form-binding scenario, and update its header comment; `make`; re-accept `tests/cases/web/forms`, `tests/cases/web/errors` and `tests/cases/web/steps` after reading that only the removed scenario and `std/web.vo` line numbers changed
- [X] T006 [US1] Delete `std/html.vo` and `src/check_html.c`; remove from `src/vo.h` `B_HTML`, `B_HTML_PIECE`, their prototypes (`is_html_call`, `check_html_call`, `ty_is_html`, `html_piece`, ...) and set `B_LAST` back to 117; from `src/check_expr.c` the call hook, `B_COUNT` back to 117 with the two names removed, the E0340 checks in `check_field` and `check_new_record`, `M_HTML_OPAQUE`; from `src/compile.c` `chtml` and its dispatch; from `src/rt_call.c` `BU_HTML`, `BU_HTML_PIECE`, their `_Static_assert` entries and the `html_piece` dispatch; from `src/check_generic.c` `json_why`'s `Html` line; `"html"` from `doc_modules` in `src/doc.c`; `std/html.vo` from `STD_FILES` and `"html_safety.py --quick"` from the test list in `Makefile`; area `html` and its file patterns from `tools/quick.sh`; `make` with zero warnings; T004 passes
- [X] T007 [US1] Delete `tests/cases/html/`, `tests/html_safety.py`, `tests/html_speed.py`; in `tests/doc_conformance.py` remove the `Html` sample, `no_html` from `TYPE_PATHS` and the `no_html` import allowance; re-accept `tests/cases/doc/all` and `tests/cases/doc/all_json` after reading that only `std.html` and `html_reply` left the listing; run `tests/run.sh tests/cases` (all pass)

## Phase 5: User Story 2 - Retired codes stay documented (P1)

**Goal**: E0335–E0340 in the catalog as retired (contracts/changes.md "Retired catalog entry").

**Independent Test**: `python3 tests/catalog.py` 0 problems; `vo explain E0337` shows the retired entry.

- [X] T008 [US2] In `tests/catalog.py`: a section with a `Retired:` line needs no Example, Fixed or golden case; every retired code is searched for as `"CODE"` in `src/*.c` and `std/*.vo` and found is a problem ("retired but still emitted"); docstring updated; check the new rule fails by running it once with a scratch copy of `src/check_html.c` from git placed back temporarily (or by asserting on a fake source string in a quick inline run), then remove the scratch copy
- [X] T009 [US2] In `docs/errors.md`: E0335–E0340 keep heading, description and `Fixable:`; their explanation, Example and Fixed are replaced by the `Retired:` paragraph of contracts/changes.md; in `specs/051-html-templates/contracts/codes.md` add under the table "Retired by spec 053 (2026-11-23): `std.html` was removed."; `make`; `bin/vo explain E0337` and `bin/vo explain --json E0337` print the entry with exit 0; `python3 tests/catalog.py` 0 problems

## Phase 6: User Story 4 - The examples show a backend (P2)

**Goal**: secure-notes signs programs in with tokens and browsers with sessions; notes-pages gone.

**Independent Test**: `(cd examples/secure-notes && ../../bin/vo test main.vo)` passes with the new tests.

- [X] T010 [US4] Tests in `examples/secure-notes/notes.vo` (research R4): a browser signs in by form (303, cookie) and by JSON (204, cookie); a note route (`GET /notes`, `POST /notes`) works with the cookie and `Sec-Fetch-Site: same-origin`; a cross-site `POST /notes` with the cookie is 403 and adds nothing; sign-out ends the session and the old cookie is refused (401 for a JSON request); a token still works on the same route; the review test expects the browser line; see them fail
- [X] T011 [US4] In `examples/secure-notes/notes.vo`: `Store` gains `sessions: map[str, Session]` (`new_store` makes it empty); `notes_policy` and `notes_app` gain `random: fn(int) -> bytes uses E`; the policy lists `browser(...)` after `tokens(...)` with `find_account` over the store's accounts, `keep`/`find`/`end` over `store.sessions`, `sign_in: "/sign-in"`, `sign_out: "/sign-out"`, `lifetime_s: 8 * 3600`, `origin: ""`; the rules open `/sign-in` and `/sign-out` to anyone; `notes_app` adds `browser_routes(...)`; `main.vo` passes `random_bytes`; `vo effects --update`; `vo fmt`; the README gains a section "Browsers" (the frontend serves the sign-in page, posts the form or JSON, gets the cookie; the protections, one line each, linking the APIs guide); until T010 passes; if a recording exists, make it again
- [X] T012 [US4] Delete `examples/notes-pages/`; remove its row from `examples/README.md` and any link to it (`git grep notes-pages`)

## Phase 7: User Story 5 - The positioning is written down (P2)

- [X] T013 [P] [US5] `README.md`: the first paragraph says vo is a language for backend network services (APIs, integrations, services behind a frontend) written by AI coding agents and reviewed by people, then what the toolchain is
- [X] T014 [US5] `docs/roadmap.md`: Status row 051 → "**Removed** by [053](../specs/053-remove-html/): vo renders no HTML"; section 051 starts with "Removed by spec 053 (2026-11-23): ..." and keeps its text; 052's row and section without pages and with the form and JSON answers; a row and section for 053; the open-gap table ordered by "does it make an agent-written network service easier to write, review or operate?" with that question stated above it (recorded tests, events, operating a service, schema migrations, serving HTTPS, rate limiting first; macOS, debugger, tuples after)

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T015 [P] Docs: `docs/reference.md` (remove the `std.html` row, the template bullet, `html_reply`; the browser sentence says a browser `GET` that needs a user is sent to sign-in and `browser_routes` signs in and out); `docs/security.md` (drop "and std.html's escaping and content security policy" from the cross-site-scripting line; nothing else); `site/docs/guide/pages.md` deleted, its "Signing in" section rewritten without the page function and with the form and JSON answers as a subsection of "Access control" in `site/docs/guide/apis.md`, and the link to pages.md removed; `tools/site.py` `GUIDE` without `pages`; `docs/review-2026-11/ecosystem.md` Templates row: "removed by spec 053: vo renders no HTML; form bodies, cookies and browser sessions stay"; `CHANGELOG.md` Unreleased: remove the std.html entry, keep form binding as its own entry, the 052 entry without the page and notes-pages, plus the parser-fix entry as it is; `specs/051-html-templates/spec.md` status "Removed by [spec 053](../053-remove-html/spec.md)"; `specs/052-sessions-sign-in/results.md` a note that 053 removed the page parts; run `python3 tests/ref_examples.py`, `python3 tests/site_examples.py`, `python3 tools/site.py`
- [X] T016 SC-002, SC-005, SC-006: run the quickstart's `git grep` (record what it finds; expected nothing outside `specs/`); record the reference tokens and `wc -l src/*.c std/*.vo` after, against T001
- [X] T017 SC-004: rerun 052's threat-removal script (`/tmp/claude-1000/-home-dan-dev-priv-volang/1093dedb-f538-4359-94d3-8bfc0dcea8f7/scratchpad/threats.py`, adjusted for the example now being secure-notes) and record which tests fail for each protection
- [X] T018 Three architectures and the instrumented build: `tests/cases/web`, `web_security`, `generic`, `check` and secure-notes' tests on `build-aarch64/vo`, `build-riscv64/vo` and `build-instrumented/vo`
- [X] T019 `make test` once; fill results.md (SC-001 to SC-006); spec.md Status Implemented

## Dependencies & Execution Order

- T001 → US3 (T002, T003) → US1 (T004–T007) → US2 (T008, T009) → US4 (T010–T012) → US5 (T013, T014) →
  Polish (T015–T019).
- US2 needs US1 (the codes must no longer be emitted before the catalog check forbids it). US4 needs
  US3 (the page-free `Browser`). T013 is independent.

## Parallel Opportunities

- T004 beside T005 (different files). T013 any time. T015's site and changelog edits beside T016.

## Implementation Strategy

1. US3 then US1: the module is gone and sign-in still works — the core decision is done.
2. US2: the catalog is consistent again.
3. US4, US5, docs, checks.

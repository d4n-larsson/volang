# Research: Remove HTML Rendering from the Core

From reading the commits of specs 051 (d4b377c) and 052 (e007729), `tests/catalog.py`, `docs/errors.md`
and the secure-notes example.

## R1. Remove by hand, not by reverting

**Decision**: delete the HTML parts file by file; do not `git revert d4b377c`.

**Rationale**: d4b377c also holds what stays (form-body binding in `std/web.vo` and
`src/json_derive.c`, the 415 message, `tests/cases/web/forms`, the reference cuts that made room), and
e007729 is built on top of it (`std.security` imports `std.html.Html`; cookies sit next to
`html_reply`). A revert would conflict and remove what stays.

What goes (from `git grep`, 66 files outside `specs/`):

| Area | Files |
|---|---|
| C | `src/check_html.c` (599 lines, deleted); in `src/vo.h` `B_HTML`, `B_HTML_PIECE`, `B_LAST` back to 117, the prototypes; in `src/check_expr.c` the call hook, the two names, `B_COUNT`, the E0340 checks in `check_field`/`check_new_record`, `M_HTML_OPAQUE`; `chtml` and its dispatch in `src/compile.c`; `BU_HTML`, `BU_HTML_PIECE` and `html_piece` dispatch in `src/rt_call.c`; `json_why`'s `Html` line in `src/check_generic.c`; `"html"` in `src/doc.c` |
| Library | `std/html.vo` (deleted); `html_reply` and the `std.html` imports in `std/web.vo`; in `std/security.vo` the `std.html` import, `Browser.page`, the GET route |
| Build and tools | `Makefile` (`STD_FILES`, `html_safety.py` in the test list); `tools/quick.sh` (area `html`, its file patterns); `tools/site.py` (`GUIDE` entry `pages`) |
| Tests | `tests/cases/html/` (23 cases), `tests/html_safety.py`, `tests/html_speed.py`; `tests/doc_conformance.py` (the `Html` sample and `no_html` lines); `tests/catalog.py` (retired codes, R2); `tests/cases/web/forms` (drop its `html_reply` part, keep the form binding); `tests/cases/doc/all*.out` (module list); the 052 cases that pass a page function (`browser`, `cross_site`, `sign_out`, `stale_cookie`, `review_browser`); `tests/web_security.py` (`FORM_TIMING`'s `page:`) |
| Docs | `docs/reference.md`, `docs/errors.md` (R2), `docs/security.md`, `docs/roadmap.md`, `docs/review-2026-11/ecosystem.md` (Templates row), `CHANGELOG.md`, `README.md`, `site/docs/guide/pages.md` (deleted, its "Signing in" section moved to `apis.md`), `site/docs/guide/apis.md`, `examples/README.md` |
| Example | `examples/notes-pages/` (deleted); `examples/secure-notes/` gains browser sign-in (R4) |

The changes to `std/web.vo` move its line numbers, so `tests/cases/web/errors.err` and `steps.err`
are accepted again after reading their diff (as in 052).

## R2. The form of a retired code

**Decision**: a retired entry keeps its heading (`## E0335 html_template_not_literal`), its one-line
description and its `Fixable:` line, and replaces its explanation, Example and Fixed with:

```text
Retired: 2026-11-23, spec 053. `std.html` was removed from the core: vo is for backend services and
renders no HTML. No version of vo reports this code, and no other code will use it.
```

`vo explain E0335` prints that section, as it prints any entry (it reads the embedded
`docs/errors.md`), so it needs no change. `tests/catalog.py`:

- reads `Retired:` lines; a retired section needs no Example, Fixed or golden case;
- fails when a retired code appears in `src/*.c` or `std/*.vo` as a string (`"E0335"`), so the
  toolchain cannot emit it again;
- keeps reading `specs/051-html-templates/contracts/codes.md`, whose table gains a note that the codes
  are retired by spec 053 (the names stay the contract of the catalog headings).

**Rationale**: Principle II says retired codes stay in the catalog, marked retired; this is the first
retirement, and the smallest form that `vo explain`, the catalog check and the site's error pages all
show without new code paths. The old examples go because they import a module that no longer exists
and would mislead an agent that copies them.

**Alternatives considered**: deleting the entries (breaks Principle II); keeping the examples under a
"was" heading (they cannot run, and the catalog check would need a second exception).

## R3. The sign-in route without a page

**Corrected while implementing**: the JSON half below was dropped. `std.web` binds a JSON body only
to a field named `body`, so the premise ("the binding already reads both encodings into the record")
was wrong; the route takes forms only and needs no `Content-Type` field.

**Decision**: `Browser` loses `page`; `browser_routes(b, p)` returns two routes (POST sign-in, POST
sign-out). The sign-in input record gains `@header("Content-Type") @default("") content_type: str;`.
A body sent as JSON (`application/json`) gets JSON answers: 204 with the cookie, or a 401 problem
("invalid credentials") and no cookie. Any other (a form) gets redirects: 303 to `local_path(next)`
with the cookie, or 303 to `b.sign_in + "?failed=1&next=" + escape(local_path(next))` with no cookie.
The cross-site check, the events, the password work and rotation are unchanged and come first in the
same order. `next` is `@default("")` and is ignored for JSON answers.

**Rationale**: the binding already reads both encodings into the record, so JSON costs one header
field and one branch; the content type is what the client chose and decides the kind of answer it
can use. `failed=1` with no reason keeps the address from saying whether a name exists (spec
assumption). The failure redirect carries `next` so the frontend can post it again.

**Alternatives considered**: form only (a frontend script would have to fake a form); an `Accept`
check (clients send `*/*`; the body's type is unambiguous).

## R4. Browser sign-in in secure-notes

**Decision**: `Store` gains `sessions: map[str, Session]`; `notes_policy` lists `browser(...)` after
`tokens(...)` with `find_account` (the store's accounts), `keep`/`find`/`end` over the map, the
lifetime 8 hours, origin `""`; `notes_app` adds `browser_routes(...)` and a parameter
`random: fn(int) -> bytes uses E` beside `now` (main passes `random_bytes`, tests a fixed function).
Rules gain `/sign-in` and `/sign-out` for anyone. Tests: browser sign-in by form and by JSON, a note
route with the cookie, a cross-site post refused, sign-out and the old cookie refused, a token still
working on the same route, and `review` showing the browser line. The README gains a section; the
diagram stays (it shows the rules, which are the same for both ways).

**Rationale**: secure-notes is the access-control example and keeps everything in memory with one
worker, so sessions fit in its store; the example then shows one rules table for programs and
browsers. No database is needed.

## R5. Builtin numbers

`B_HTML` (118) and `B_HTML_PIECE` (119) were the last two builtins; removing them puts `B_LAST` back to
117 and leaves no gap. Builtin numbers are not stored anywhere outside a running toolchain (bytecode is
compiled at each start; `vo build` attaches sources), so nothing persisted changes.

## R6. Reference and docs

- Reference: the `std.html` row, the template bullet, `html_reply` in the std.web bullet, and "a page
  needing a user redirects to sign-in" kept as "a browser `GET` needing a user"; `browser_routes(b, p)`
  described as sign in and out. Tokens freed are measured with `tests/ref_examples.py`.
- `docs/security.md`: the browser-session list keeps every answer; "scripts reading the cookie" loses
  "and std.html's escaping and content security policy" (the frontend's concern now).
- Site: `pages.md` deleted; its "Signing in" section, without the page function and with the new
  failure answers, becomes a subsection of `apis.md`'s "Access control"; `tools/site.py` loses `pages`.
- Changelog: the Unreleased entries for std.html and notes-pages go; the 052 entry loses the example
  and the page; the form-binding sentence stays as its own entry.
- Roadmap: Status row 051 "Removed by 053"; section 051 keeps its text with a first line "Removed by
  spec 053 (2026-11-23): ..."; 052 adjusted; 053 row and section; the gap list ordered by the
  positioning question.
- README: first paragraph states the target.
- `specs/051-html-templates/spec.md` status "Removed by [spec 053](../053-remove-html/spec.md)".

## R7. Checks

`make test` once; the instrumented build on the web, web_security and generic cases and secure-notes;
the ARM64 and RISC-V builds on the same; the threat-removal script of 052 rerun (with the page lines
gone) for SC-004; `git grep` for SC-002; `wc -l` and `tests/ref_examples.py` before and after for SC-005
and SC-006.

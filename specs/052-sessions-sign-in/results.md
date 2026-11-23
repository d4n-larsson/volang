# Results: Sessions and Sign-In Forms

**Since spec 053**: the page parts are removed (`Browser.page`, the GET sign-in route, the notes-pages
example; failures answer 303 to `SIGN_IN?failed=1&next=...`). The figures below are those of 052 as
built; 053's results give the threat check again.

`make test` passes (exit 0). The instrumented build passes `tests/cases/web_security`, `web` and
`generic` (58 cases), the example's 7 tests and its replay. The toolchain changed in one place, a parser
fix found on the way (below); everything else is standard-library vo.

## SC-001 Acceptance scenarios as cases, identical on three architectures and over 20 runs

Met. `tests/cases/web_security/` has 8 new cases (cookies, origin_rule, return_path, browser,
stale_cookie, cross_site, sign_out, app_cookie, review_browser: 9 with the review) beside the 6 of spec
045. All 15 passed 20 runs in a row on x86-64 (each run compares with the golden output), and the 27
web_security and web cases and the example's 7 tests pass on `build-aarch64/vo` and `build-riscv64/vo`.

## SC-002 Sign in, use, sign out in a browser; the old cookie signs nobody in

Met without a browser, open in one: the example's tests and a run over real HTTP (curl, recorded in
`recordings/demo.jsonl`) sign in, use the pages, sign out, and send the old cookie again, which is
sent to the sign-in page. A browser run is left to the user (see "Browser check").

## SC-003 Cross-site posts refused

Met for every state-changing route of the example through `handle_request` (`cross_site` case: every
header combination; the example's test; `web_security.py --group origin`: 3,360 combinations against a
model) and once over HTTP (`Origin: https://evil.example`, 403). Not yet tried from a real page of
another origin in a browser.

## SC-004 Lines an application writes

In examples/notes-pages: `notes_browser` is 22 lines, of which 12 are the four adapters to the storage
functions; `notes_policy` is 12; `pages_app` adds 2 (the routes and the step). Sign-in, sign-out and the
policy: 10 + 12 + 2 = 24 lines (target at most 25): met. No cookie flag, origin check or session value
appears in the application. Session storage: 23 lines in users.vo (`keep_session`, `find_session`,
`end_session`) plus the 12 adapter lines, 35 (target at most 30): not met by 5 lines; the adapters exist
because `Browser`'s functions take the context (`Pages`) and users.vo's take the database.

## SC-005 Each protection removed makes a test fail

Met: each of 12 protections removed in turn made at least one test fail (table under "Threat removal").

## SC-006 Sign-in timing, unknown name vs wrong password

`tests/web_security.py --group timing`, the sign-in form (7 samples each, x86-64): a wrong password
504 ms (498-518), an unknown name 499 ms (498-691): within 1%. Met. (Basic, unchanged: 501 and 501.)

## SC-007 Reference within 15,000 tokens

~14,981 tokens (`tests/ref_examples.py`). Met.

## SC-008 No password or session value in events and reports

`tests/web_security.py --group tokens` runs the browser case and searches everything it prints except
the `Set-Cookie` answers for the passwords used, every session cookie value and the SHA-256 key of each:
none found, with 13 events printed. Met. Events carry the user's name and a reason only; session values
live in locals of `sign_in_form`, `sign_out_form` and `session_user`. (Recordings keep requests as they
arrived, by decision Q2: see the example's README and docs/security.md.)

## Reference room

Before: 11,537 words, ~14,998 tokens. After the cuts (T002): 11,473 words, ~14,914 tokens (84 freed).
Cuts, each a detail `vo doc` or the guides already give:
- https: "valid now ... return an `err` saying why" shortened; "`--clock-ms` does not affect certificate
  dates" and "A recording keeps requests and responses, never keys" removed (the second is no longer
  true of signed-in applications; R10 replaces it).
- serve: "(the report goes to stderr)" removed; serve_workers: "changed in a `transaction`" and "replay
  runs the requests one at a time" removed.
- std.web bullet: "checked with the field annotations before it runs (... naming each field)" shortened;
  std.web row: names the bullet already gives removed.
- std.process: "(arguments passed as they are)", "waits for it" removed; packages: "unpacked into
  `packages/NAME`" removed.

After the new text (T024: `browser`, `browser_routes` in the std.security row; cookies, the page
redirect, the cross-site rule and recordings in the std.web bullet): ~14,981 tokens, 19 left.

## Threat removal (SC-005)

Each protection was removed from `std/web.vo` or `std/security.vo` by a one-line change, the toolchain
rebuilt, and `tests/cases/web_security`, the example's tests and (for timing) `web_security.py --group
timing` run; then the file was restored.

| Protection removed | Golden cases failing | Example tests failing |
|---|---|---|
| HttpOnly default | app_cookie, browser, cookies, sign_out, stale_cookie | 0 |
| Secure default | app_cookie, cookies | 0 |
| SameSite=Lax default (made Strict) | app_cookie, browser, cookies, sign_out, stale_cookie | 0 |
| `__Host-` cookie name | browser, cross_site, sign_out, stale_cookie | 5 |
| same-origin check in `secure` | cross_site, sign_out | 1 (a form another site posts) |
| same-origin check on the sign-in form | cross_site | 0 |
| return-path check (`local_path`) | browser, return_path, stale_cookie | 0 |
| session rotation at sign-in | browser | 0 |
| expiry | stale_cookie | 0 |
| end at sign-out | sign_out | 1 (signing out ends the session) |
| hash in storage (value stored as the key) | browser, stale_cookie | 0 |
| unknown-name timing (password work skipped for an unknown name) | none | 0; `web_security.py --group timing` fails (503 ms against 0) |

## Browser check

Not done here: no browser can be driven on this machine without taking over the user's own. Over real
HTTP with curl (quickstart part 2), against `vo run main.vo` with recording on: the redirect to sign
in, signing in (303 and the `__Host-session` cookie with `HttpOnly`, `Secure`, `SameSite=Lax`), adding
notes, the list with the note escaped, the swap, an empty note (422), a post with `Origin:
https://evil.example` (403), an API request (401), sign-out (303, the cookie removed), and the old
cookie afterwards (303 to sign in) all answered as specified; the recording replays. Left for the user:
quickstart part 3 in Chrome or Firefox (steps 1 to 5).

## Found on the way

- **Parser (fixed)**: in a type's effect argument, `uses D, clock` parsed but `uses clock, D` (and
  `uses clock, Storage`, a named set) did not: the effect list stopped at the first capitalized name
  (`src/parse.c`). `vo fmt` writes effect parameters and sets last, so formatting the example made it
  fail to check (E0327), which `tests/effect_sets_records.py` (it formats a rewritten copy) found in
  `make test`. Now a capitalized name after the written effects belongs to the list when a `,` or `]`
  follows it, and the checker reports a type written there (`Two[uses console, Note]`) as E0327, as
  before (`effects_named` in `src/check_generic.c`). New case `tests/cases/generic/effect_args_order`
  (fails before the fix); `effect_args_syntax` gained two misplaced-type lines.
- **Checker (not fixed)**: an effect parameter is not inferred from a mixed effect argument: passing a
  `Job[uses D, clock]` to `fn both[D2: effects](j: Job[uses clock, D2])` from another generic function
  reports "Expected `Job[uses clock, D]`, found `Job[uses clock, D]`". The example does not need it.
- The example's files had not been run through `vo fmt`; they are now, and the recording was made again
  against the formatted program.

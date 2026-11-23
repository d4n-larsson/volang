# Results: Remove HTML Rendering from the Core

## Before

At e007729: reference ~14,981 tokens; `src/*.c` 44,714 lines, `src/*.c` and `std/*.vo` 52,304;
`tests/run.sh tests/cases` 1,192 passed, 0 failed.

## SC-001 Tests pass on three architectures

Met. `make test` passes (exit 0). The web, web_security, generic and check cases (129) and secure-notes'
8 tests pass on `build-aarch64/vo`, `build-riscv64/vo` and `build-instrumented/vo`.

## SC-002 Nothing tells an agent to render HTML

Met. `git grep -i 'std\.html|html_reply|html_text|no_html|notes-pages|check_html|\bHtml\b'` outside
`specs/` finds only the six retired catalog entries, the roadmap's record of 051 and 053, the review
row saying it was removed, and the changelog's line that vo renders no HTML. `vo doc` lists no
`std.html`; importing it is the ordinary E0205 (`tests/cases/check/std_html_gone`).

## SC-003 Unchanged outputs

Met. `tests/run.sh tests/cases`: 1,170 passed (1,192 before, minus the 23 html cases, plus
`check/std_html_gone`). Expected outputs changed, each read before accepting: `web/forms` (its
`html_reply` scenario became `json_reply`; every binding result unchanged), `web/errors` and
`web/steps` (line numbers of `std/web.vo`), `doc/all` and `doc/all_json` (the `std.html` line),
`web_security/browser`, `stale_cookie` and `review_browser` (the page answers, the `WWW-Authenticate`
fix, the review's POST line).

## SC-004 Each protection of 052 still guarded by a test

Met: 052's threat script, with secure-notes as the example, finds a failing test for each of the 12
protections (golden cases as in 052; the example's browser tests now catch the `__Host-` name, the
cross-site check in `secure` and the end at sign-out; the timing group catches the unknown-name work).

## SC-005 Reference tokens freed

Met: ~14,981 → ~14,833 tokens, 148 freed (the `std.html` row, the template bullet, `html_reply`).

## SC-006 C lines removed

Met: `src/*.c` 44,714 → 44,062 lines (652 removed: `src/check_html.c` and its wiring); `src/*.c` and
`std/*.vo` together 52,304 → 51,586.

## Deviations and findings

- **No JSON sign-in** (spec FR-008 as first written): `std.web` binds a JSON body only to a field named
  `body`, so one input record cannot take a form and JSON; the premise in research R3 was wrong. The
  route takes forms; scripts post the same encoding. Spec, contract and research updated.
- **Bug fixed (from 052)**: the 401 challenge listed every way with a scheme as an authentication
  scheme, so a policy with the browser way answered `WWW-Authenticate: __Host-session`. Cookie ways are
  skipped now; `tests/cases/web_security/browser` prints the header (found by secure-notes' header
  test).
- **Review**: the sign-in line now asks whether anyone may *post* to the sign-in path (the backend
  serves no page, so a `GET` rule is the frontend's business).
- **secure-notes**: its token route moved from `POST /sign-in` to `POST /token` so browsers get
  `/sign-in`; the store, accounts and browser settings moved into a new `store.vo` (formatting the
  grown `notes.vo` passed the 400-line limit, W0702).
- **Retired codes**: the first ones; `tests/catalog.py` checked once against a temporary copy of the
  old checker to see the "still named in the source" rule fail.

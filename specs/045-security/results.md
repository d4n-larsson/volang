# Results: Security (spec 045)

Status: implemented. `make test` passed (one run). The gate was not run.

`std/security.vo` is written in vo: no builtin, opcode, syntax or diagnostic code was added. The C
change is the module's name in `src/doc.c`. Two small exports elsewhere: `std.web.problem_reply` and
`std.crypto.password_outdated`.

## SC-001: the application answers as the rules decide

`examples/secure-notes`, test "every route answers each caller as the rules decide": seven requests
(the six routes and a path with no route) as nobody, a user and an admin: the status is 401 where
`decide` says sign-in needed, 403 where it says forbidden, and the operation's own answer where it
says allowed; with a wrong token, 401 on all seven. The golden case `web_security/policy` prints the
same comparison for eleven requests and four callers.

## SC-002, SC-003: tokens

`python3 tests/web_security.py --group tokens`, against a JWT (HMAC-SHA256) written with Python's
`hmac`, `hashlib`, `base64` and `json`:

- 1,000 tokens issued by the module verify there with the expected claims; 1,000 issued there (extra
  claims, another key order, spaces in the JSON, `nbf` in the past) are accepted with the same name and
  roles.
- 10,000 damaged tokens are all refused: one character changed (4,000), another signing method,
  signed or not (1,000), another key, expired, not yet valid, a part removed, a part added (1,000
  each); where the reason is determined it is the expected one.
- 13 validly signed tokens of the wrong shape and one whose claims are not JSON: reason `content`.

Two deliberate faults were put in to see that the script notices: without the check that a
signature's base64 is canonical, 1 of the 10,000 was accepted (its last character changed in bits the
decoder ignores); without the check of the signing method, 443 were. Both were removed.

## SC-004: timing

Basic, 7 refusals each, full-cost hashes: a wrong password 503 ms (samples 500 to 618), an unknown
name 500 ms (samples 497 to 504); the answers are byte-identical. One earlier run on a busy machine
gave medians of 627 and 541 ms (samples 498 to 868) for the same work, so the script repeats the
measurement up to three times before failing. Skipped on CI and under emulation.

## SC-005: speed

A token-authenticated request costs 22.5 microseconds more than an unguarded one through
`handle_request` on x86-64 (bound 100; the plan's prototype, without rule matching and with a shorter
path, measured 10). Emulated, not bounded: 191 (ARM64), 177 (RISC-V).

## SC-006: review

`web_security/review`: the faulty policy gets exactly four findings (no rule for a route, a hidden
rule, a misspelt rule, `test_users`); the clean one none. The example's served policy gets none.

## SC-007: the example

`rules()` is 4 rules (8 lines). No operation parses a header: each declares the `identity` field and
calls `require_user`. Run for real on port 8080 and called with curl: sign-in, a note added with the
token, 403 for a non-admin, and 401 for a request carrying forged `X-Identity` and `X-Test-User`
headers.

## SC-008: budget, docs, architectures

- Reference: 14,905 tokens of 15,000 (one library row).
- `tests/doc_conformance.py`: every example of the module runs. `tests/site_examples.py` and
  `make site-check` pass; the generated pages of the module, the example and the guide mention no
  spec.
- `tests/cases/web_security` (6) and `tests/web_security.py` pass with `build-aarch64/vo` and
  `build-riscv64/vo` under qemu (100 tokens each way and 1,000 damaged there).

## Existing files changed outside the plan's list

- `tests/cases/crypto/doc.out`: the listing of `std.crypto` gains `password_outdated` (it is placed
  after `verify_password`, so the line numbers in `crypto/timeout` did not move).
- `tests/doc_conformance.py`: a generic type with its arguments given (`Option[User]`) gets a sample
  value, and examples of `std.security` may call functions of `std.web` and `std.option`.
- No recording fingerprint changed: no recorded program imports `std.web` or `std.crypto`.

## Deviations

- **`sign_in_route` builds its `Route` by hand**: `on_post` takes only a literal pattern. The body is
  read with `std.json`; a body without `name` and `password` is 400.
- **A scheme is matched as written or in lower case** (`Basic `, `basic `), not in any mix of cases:
  the match is `find(...) == 0`, which cannot cut a header value inside a character.
- **`Event.name` is empty for a credential refused by a way**; for Basic the reason carries the name
  ("wrong password for ada"). The sign-in route and `forbidden` events set the name.
- **`outdated_hash` is reported by the sign-in route only**, not by Basic.
- **The user reaches the operation through one declared field and one call** (FR-014 as amended in the
  plan), not as an input field of type `Option[User]`.
- **Type parameters of `tokens` and `test_users`** are inferred from an annotated list or a
  parameter's type; `tokens[int](...)` is not vo syntax.
- The module is 965 lines against 500 planned (doc comments and the `match` forms).
- The example keeps notes in memory in one worker and makes its key at start; its README says so.

## Not verified

- The gate.
- Real ARM64 or RISC-V hardware (qemu only).
- A real browser or third-party JWT library against the tokens (Python's standard library only).
- The R0012 golden case holds a line of the test file, not of the module; `crypto/timeout` holds a
  line of `std/crypto.vo` and changes if lines above `verify_password` move.

## Noticed, not changed

- `examples/README.md` describes notes-api as built on `std.http.serve`; it uses `std.web`.
- The second line of the header comment of older std modules still names their spec.

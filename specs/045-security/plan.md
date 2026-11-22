# Implementation Plan: Security (who may call what, for `std.web` applications)

**Branch**: `045-security` (work on `main`, as for specs 001–044) | **Date**: 2026-11-22 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/045-security/spec.md`

## Summary

One new library module, `std/security.vo`, written in vo on top of `std.web`, `std.crypto`,
`std.json` and `std.bytes`. No syntax, builtin, opcode or diagnostic code.

- **No primitive**: a prototype of token checking in a step costs about 10 microseconds a request
  (bound 100), and its token verifies in Python (R1).
- **A way of signing in is one record** (header, scheme, checking function), so the step is one loop
  and the way for tests is a visible item of the policy (R2).
- **The user travels in `X-Identity`** as JSON; `handle_request` already drops that header from
  clients. Operations declare one field and call `current_user`, `require_user` or `require_role`
  (R3). Binding an `Option[User]` field directly was tried and needs new binding rules; not done.
- **Passwords**: one function with equal work for unknown names; `std.crypto` gains
  `password_outdated`. The module never hashes, so it has no effects of its own (R4).
- **Tokens**: JWT, HMAC-SHA256, read with `std.json` field functions; key length and lifetime are
  `requires` clauses (R5).
- **`decide` is pure and takes only the rules**; `describe` and `review` run it over the
  application's routes (R6).
- **`std.web` exports `problem_reply`**, so the 401 and 403 answers are its problem format without a
  copy (R7).

| Part | File | What |
|---|---|---|
| Library | `std/security.vo` (new) | the eight types and twenty-one functions of contracts/security.md |
| Library | `std/web.vo` | `problem_reply` exported |
| Library | `std/crypto.vo` | `password_outdated` |
| Registration | `Makefile` (`STD_FILES`, test list), `src/doc.c` (`doc_modules`), `tools/quick.sh` (area `web_security`) | |
| Example | `examples/secure-notes/` (new), `examples/README.md` | R9 |
| Docs | `docs/reference.md` (one row), `site/docs/guide/apis.md` (section), `CHANGELOG.md`, `docs/roadmap.md` | nothing about specs on the site |
| Tests | `tests/web_security.py` (new, in `make test`), `tests/cases/web_security/` (new), `tests/cases/doc/all*.out` (module listing) | R8 |

## Technical Context

**Language/Version**: vo for the module; C only for the registration line in `src/doc.c`.

**Primary Dependencies**: none new.

**Storage**: none; users are the application's.

**Testing**: golden cases for the stories, edge cases and the contract stop; `tests/web_security.py`
for token interoperation with Python's standard library, damaged tokens, timing and speed; the
example's own tests for the decision matrix, the review and the description; doc examples under
`tests/doc_conformance.py`. `make quick AREA=web_security` between edits, `make test` once; the gate
only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: at most 100 microseconds added per token-authenticated request (SC-005);
prototype: about 10.

**Constraints**: reference under 15,000 tokens (14,873 now; one row of about 50 tokens); no secret in
any answer, event or error text; deterministic (time only from the application's clock function).

**Scale/Scope**: about 500 lines of vo, 250 of test script, 6 golden cases, an example of about 200
lines, 80 lines of guide.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. One form per construct | No syntax. One rule form (prefix by segments), one record for every way of signing in, one way to get the user in an operation. `bearer_auth` and `api_key_auth` remain as the short form for one prefix: the guide says when to use which. | Pass |
| II. Machine-readable feedback | No new code: R0012 for a short key or a non-positive lifetime; refusals are `unauthorized`/`forbidden` errors and problem answers; review findings are one line each. | Pass |
| III. Determinism and defined behaviour | The module reads no clock and no random source: time comes from the policy's function, so `vo test`, `--clock-ms` and record/replay behave as for any step. | Pass |
| IV. Test-first | Golden cases and `tests/web_security.py` before the functions they cover. | Pass |
| V. Minimal surface, reference budget | No builtin; two small exports in existing modules; one reference row. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/045-security/
├── plan.md
├── research.md          # R1–R9
├── data-model.md
├── quickstart.md
├── contracts/
│   └── security.md      # the types, the functions, the errors
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
std/security.vo (new), std/web.vo, std/crypto.vo, Makefile, src/doc.c, tools/quick.sh
examples/secure-notes/ (new), examples/README.md
docs/reference.md, site/docs/guide/apis.md, CHANGELOG.md, docs/roadmap.md
tests/web_security.py (new), tests/cases/web_security/ (new), tests/cases/doc/
```

**Structure Decision**: one vo file; no C file is added or changed in substance.

## Order of work

1. Register an empty module; `problem_reply`; golden cases and `tests/web_security.py` written and
   failing.
2. Types, `rule`, `rule_for`, `decide`; `policy`, `secure` with `header_key` and `test_users`,
   `as_user` (story 1).
3. `current_user`, `require_user`, `require_role`, `has_role` (story 3).
4. `issue_token`, `check_token`, `tokens`; `password_outdated`, `password_user`, `basic`,
   `sign_in_route`; events (story 2).
5. `describe`, `review` (story 4); `safe_headers` (story 5).
6. The example; docs; three architectures; `make test`; results.

## Complexity Tracking

Nothing beyond the constitution's principles.

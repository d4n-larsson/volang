# Implementation Plan: Remove HTML Rendering from the Core

**Branch**: `053-remove-html` (work on `main`, as for earlier specs) | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/053-remove-html/spec.md`

## Summary

Delete `std.html`, its C template checker (`src/check_html.c`, 599 lines) and its wiring, `html_reply`,
the html tests, the "Web pages" guide and the notes-pages example, by hand rather than by revert, since
the same commits hold what stays (R1). E0335–E0340 stay in `docs/errors.md` as the first retired codes,
in a form `vo explain` already prints; `tests/catalog.py` learns to accept them and to fail if one is
emitted again (R2). `std.security`'s browser sign-in loses its page: two routes instead of three, and
the sign-in route answers a form with redirects and JSON with 204 or 401, chosen by the request's
content type (R3). Browser sign-in moves into the secure-notes example beside tokens (R4). Docs, the
changelog and the roadmap are rewritten, and the README states the target (R6).

| Part | Files | What |
|---|---|---|
| Toolchain | `src/check_html.c` (deleted), `src/vo.h`, `src/check_expr.c`, `src/compile.c`, `src/rt_call.c`, `src/check_generic.c`, `src/doc.c`, `Makefile` | R1, R5 |
| Library | `std/html.vo` (deleted), `std/web.vo`, `std/security.vo` | R1, R3 |
| Catalog | `docs/errors.md`, `tests/catalog.py`, `specs/051-html-templates/contracts/codes.md` | R2 |
| Tests | `tests/cases/html/` (deleted), `tests/html_safety.py`, `tests/html_speed.py` (deleted), `tests/cases/web/forms`, the 052 cases, `tests/web_security.py`, `tests/doc_conformance.py`, `tests/cases/doc/all*.out`, `tools/quick.sh` | R1, R3 |
| Example | `examples/notes-pages/` (deleted), `examples/secure-notes/` | R4 |
| Docs | reference, security, roadmap, review row, changelog, README, site guides, `tools/site.py`, examples index, spec 051 status | R6 |

## Technical Context

**Language/Version**: C11 freestanding and vo; Python 3 for tests.

**Primary Dependencies**: none.

**Storage**: none (secure-notes keeps sessions in memory).

**Testing**: `make quick AREA=web_security,web` between edits; `make test` once; the instrumented,
ARM64 and RISC-V builds on the web, web_security and generic cases and secure-notes; 052's threat
script.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: none; nothing in a hot path changes.

**Constraints**: codes never reused (Principle II); programs not using `std.html` unchanged byte for
byte; zero C warnings.

**Scale/Scope**: about 700 lines of C and vo removed, 30 lines changed in `std/security.vo`, 80 added to
secure-notes, 20 to `tests/catalog.py`.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One fewer way to make output; nothing new to learn. | Pass |
| II. Machine-readable feedback | Codes retired, not deleted or reused; `vo explain` still answers for them. | Pass |
| III. Determinism | Unaffected. | Pass |
| IV. Test-first | The changed sign-in answers land as case changes before the code; the catalog's retired rule gets a failing check first (a retired code still emitted). | Pass |
| V. Single source of truth, minimal surface | The reference and the toolchain shrink. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/053-remove-html/
├── plan.md
├── research.md          # R1–R7
├── data-model.md        # Browser and the sign-in input after, retired codes, the secure-notes store
├── quickstart.md
├── contracts/
│   └── changes.md       # removed items, the sign-in answers, the retired entry
└── tasks.md             # /speckit-tasks
```

**Structure Decision**: no new files outside `specs/`, except the secure-notes tests and README
section inside existing files.

## Complexity Tracking

No violations.

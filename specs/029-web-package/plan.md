# Implementation Plan: Web Package for Building APIs

**Branch**: `029-web-package` (work on `main`, as for specs 001–028) | **Date**: 2026-11-20 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/029-web-package/spec.md`

## Summary

`std.web` gives vo APIs Spring's conveniences as values and functions: a route table
(`on_get("/notes/{id}", show_note)`, ...) checked by `vo check`, operations `fn(ctx, input) ->
result[Response]` whose input record is bound from the path, query, headers and JSON body before they run
(400/415/422 problems otherwise), answer helpers, errors `err("not_found: ...")` mapped to statuses in one
table, steps around requests (request ids, logging, CORS, bearer auth), typed settings, health and info
routes, and `handle_request` for tests without a network, served with `serve_workers` (research R1). The
toolchain gains derived conversion between values and JSON (`std.json.to_json`/`from_json` with a `json`
constraint, R2), limits on record fields as annotations (R3), checks of route literals (R6), effect
arguments from the expected type (R10) and `new Name {}` (R7).

| Part | File | What |
|---|---|---|
| Derived JSON | `src/json_derive.c` (new), `std/json.vo`, `src/check_generic.c` | the `json` constraint (E0328), the encode/decode primitive over concrete types, limits |
| Field annotations | `src/parse.c`, `src/check.c` | annotations on record fields, E0329 |
| Route checks | `src/check_web.c` (new), `src/check_expr.c` | patterns and route lists (E0330, E0331) |
| Inference | `src/check_generic.c` | effect parameters bound from the expected type first (R10) |
| Library | `std/web.vo` (new) | everything in contracts/web.md |
| Docs | `docs/reference.md`, `docs/errors.md`, `site/docs/guide/`, `vo doc` | R16 |
| Example | `examples/notes-api/` | rewritten with std.web (SC-001) |
| Tests | `tests/cases/{json_derive,annotations,web}/`, `tests/cases/web_steps/`, `tests/web_fuzz.py`, `tests/web_speed.py`, `bench/web/` | R18 |

## Technical Context

**Language/Version**: C (C11, freestanding) for the toolchain; vo for `std/web.vo`, `std/json.vo` and the
example.

**Primary Dependencies**: none new; `std.http` (spec 011/028), `std.json`, `std.regex` (patterns),
`std.sqlite` (the example).

**Storage**: none (settings read from a JSON file and the environment).

**Testing**: golden cases, step cases, `tests/web_fuzz.py` (SC-003), `tests/web_speed.py` (SC-006, `make
gate`), the agent benchmark (SC-004, manual, results in results.md); `make quick` between edits, `make
test` once per change.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain and runtime (CLI) with its standard library.

**Performance Goals**: std.web within 10% of a hand-written `std.http` API's throughput for 20 routes
(SC-006); `vo check` time on a 5,000-line program unchanged within noise.

**Constraints**: reference ≤ 15,000 tokens (+≤ 700 for this spec, R16); no new effect; existing programs
unchanged (SC-007) except where R10 lets more programs type; deterministic, replayable.

**Scale/Scope**: about 1,500 lines of C (derive ~600, annotations ~250, route checks ~400, inference ~50,
parser ~50), 900 lines of vo (`std/web.vo`), docs, one example.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One form each: routes only as `on_*` in a table (Q1), input only as a record bound by fixed rules, errors only as `kind: detail`, one JSON mapping without options, five field annotations with one meaning each. Names avoid clashes (`on_get`, not `get`). No hidden control flow: steps are functions called in a visible list. | Pass |
| II. Machine-readable feedback | Four new check codes (E0328–E0331), each with span, message, related locations and fixes where unambiguous (E0329's nearest-name fix); answers to bad input are structured problem+json. | Pass |
| III. Determinism | Routing and binding are pure; request ids come from `random_int` (seeded, recorded per request as in spec 028); settings reads are recorded effects. | Pass |
| IV. Test-first | Each part lands as failing golden/step cases first; the catalog gains a case per new code. | Pass |
| V. Single source of truth, minimal surface | Reference, catalog, contracts updated together; budget +700 (R16); YAGNI list in the spec's assumptions (no sessions, views, OpenAPI, ...). The toolchain additions are those a library cannot do (R1). | Pass |

Re-checked after Phase 1: unchanged. Risk noted: the reference budget (≈14,100 of 15,000 after this spec).

## Project Structure

### Documentation (this feature)

```text
specs/029-web-package/
├── plan.md
├── research.md           # R1–R18
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── json.md           # to_json / from_json, the json constraint, field annotations, new Name {}
│   ├── web.md            # std.web
│   └── diagnostics.md    # E0328–E0331
└── tasks.md
```

### Source Code (repository root)

```text
src/json_derive.c, src/check_web.c       # new
src/parse.c, src/check.c, src/check_expr.c, src/check_generic.c, src/rt_call.c, src/vo.h
std/json.vo, std/web.vo                  # web.vo new
docs/reference.md, docs/errors.md, site/docs/guide/api.md (new), site/docs/guide/standard-library.md
examples/notes-api/
tests/cases/json_derive/, tests/cases/annotations/, tests/cases/web/, tests/cases/web_steps/
tests/web_fuzz.py, tests/web_speed.py, bench/web/
```

**Structure Decision**: the library in vo beside `std/http.vo`; the toolchain parts in their own files
beside the existing checks (`check_regex.c`, `check_sqlite.c` are the pattern for `check_web.c`), the
runtime conversion beside `task.c`'s typed transfer form.

## Order of work

1. `new Name {}` and R10 inference (small, unblock the rest).
2. Field annotations (parse, E0329).
3. Derived JSON (`json` constraint, E0328, encode/decode with limits) — usable on its own.
4. `std.web` core: routes, matching, binding, answers, errors, `handle_request` (US1–US4).
5. Route checks (E0330, E0331).
6. Steps, settings, health/info, `serve_app` (US5–US7).
7. notes-api, docs, fuzz, speed, agent benchmark (US8, SC-001–SC-006).

## Complexity Tracking

No constitution violations.

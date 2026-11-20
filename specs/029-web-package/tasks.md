---

description: "Task list for vo-lang spec 029: Web Package for Building APIs (std.web)"
---

# Tasks: Web Package for Building APIs

**Input**: Design documents from `specs/029-web-package/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R18), data-model.md, contracts/json.md,
contracts/web.md, contracts/diagnostics.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's golden or step cases land first and fail before its
code.

**Organization**:
- Setup: test wiring and the results file.
- Foundational: the toolchain parts every story needs: `new Name {}`, effect arguments from the expected
  type (R10), field annotations (R3), derived JSON (R2), and the `std.web` skeleton.
- US1 (P1) routing; US2 (P1) typed input; US3 (P1) answers and errors; US4 (P1) testing without a network;
  US5 (P2) steps; US6 (P2) context, settings and serving; US7 (P3) health and info; US8 (P3) the example
  and the guide.
- Polish: fuzzing, speed, the agent benchmark, verification on three platforms.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: `tests/cases/<area>/<name>.vo` with `.out`/`.err`/`.code` (and `.cmd` for `check`),
  accepted with `tests/run.sh --accept PATH`; step cases in `tests/cases/web_steps/<name>/steps`.
- **std.web code** lives in `std/web.vo` (vo); keep it in the structure rules of spec 027 (`@allow` with a
  reason where one module must exceed W0702, as `std/json.vo` does).
- **Reference budget**: run `python3 tests/ref_examples.py` after every reference change; this spec may
  add at most 700 tokens (R16).
- **Catalog**: every new code gets its section in `docs/errors.md` and a golden case
  (`python3 tests/catalog.py`).
- **GC safety**: values held across allocations in new C code are rooted; run the stress build over the new
  cases before marking a C task done.
- **Between edits**: `make quick AREA=web`; `make test` once per change; never two suites at once.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: area `web` (cases `json_derive`, `annotations`, `web`, `web_steps`, `generic`,
  `json`; scripts `doc_conformance.py`, `"web_fuzz.py --quick"`) and the mapping `src/json_derive.c`,
  `src/check_web.c`, `std/web.vo`, `std/json.vo`, `tests/web_*.py` → `web`.
- [X] T002 [P] `specs/029-web-package/results.md`: SC-001–SC-007 (pending), Verification, Deviations.
- [X] T003 [P] Create `tests/cases/json_derive/`, `tests/cases/annotations/`, `tests/cases/web/`,
  `tests/cases/web_steps/` with a README line each in `tests/cases/README.md` (if that file lists areas).

---

## Phase 2: Foundational (blocking)

**Purpose**: the language and library pieces every story builds on.

- [X] T004 Golden cases first: `tests/cases/parse/new_empty_record.vo` (`record NoInput {}` built with
  `new NoInput {}`, compared, printed: runs), and `tests/cases/generic/effects_from_expected.vo` (a
  `list[Route[uses file.read]]`-like generic record table whose elements come from a generic constructor
  given a pure function and a `file.read` function: checks and runs; a function needing `file.write`
  there: E0502).
- [X] T005 `src/parse.c`: `new Name {}` parses for a record without fields (research R7); a record with
  fields still needs them all (E0305 unchanged). Make the first T004 case pass. `docs/reference.md` Records: one sentence: a record without
  fields is built with `new Name {}`.
- [X] T006 `src/check_generic.c` (`check_generic_call`): when the call has an expected type, bind the
  callee's effect parameters that the expected type determines (`ty_unify` of the declared return type
  against `expected`, effect parameters only) before checking the arguments; function-value arguments are
  then checked against them (coverage, E0502), as research R10. Make the second T004 case pass; every
  `tests/cases/generic`, `effects`, `sqlite` and `func` case unchanged. `docs/reference.md` Generics: the inference sentence becomes: effect arguments
  come from the expected type first when there is one, then from the arguments; type arguments from the
  arguments, then the expected type.
- [X] T007 [P] Golden cases first, `tests/cases/annotations/`: each annotation of contracts/json.md on its
  field types accepted (`vo check` clean); E0329 for: an unknown annotation on a field (`@sise`: fix
  `@size`), a repeated one, `@size` on an `int`, `@range` on a `str`, `@range(5, 1)`, `@default("x")` on an
  `int`, `@default(0)` on a field with `@range(1, 5)`, `@default` with a non-empty list, `@default` on a
  record-typed field, `@pattern` with a
  bad regex (the spec 025 diagnostic), `@header` on a record never used as std.web input (checked in US2:
  leave that case pending).
- [X] T008 `src/parse.c`, `src/check.c`, `src/vo.h`: annotations on the lines above a record field
  (`@name(literal, ...)`, literals: int, float, str, bool, a variant case, `[]`), stored on the field node;
  checks per contracts/json.md table ("`@size(min, max)` | `str` (characters), `list`, `map` | int literals,
  `0 <= min <= max`"; "`@range(min, max)` | `int`, `float` | literals of the field's type, `min <= max`";
  "`@pattern(\"re\")` | `str` | whole-text match, std.regex syntax"; "`@default(v)` | scalars, enum-like
  variants, lists (`[]` only) | must satisfy the field's other limits"; "`@header(\"Name\")` | `str`, `int`,
  `float`, `bool`"); E0329 with the nearest-name fix; `docs/errors.md` E0329 section. `vo fmt` keeps
  field annotations on their own lines. `docs/reference.md` Records: field annotations `@size`, `@range`,
  `@pattern`, `@default`, `@header` (one sentence each or one table); `python3 tests/ref_examples.py`.
  Make T007 pass.
- [X] T009 `tests/fuzz.py`: the generator emits field annotations (valid and malformed: unknown names,
  wrong literal kinds, missing parentheses) and `new Name {}`; `make test`'s `fuzz.py --iterations 200
  --seed 1` passes (no crash, every diagnostic well-formed).
- [X] T010 [P] Golden cases first, `tests/cases/json_derive/`: `to_json` of every row of contracts/json.md's
  mapping table (records nested, lists, `map[str, V]`, bytes as base64, enum-like and valued variant cases,
  generic records), round trips `from_json(to_json(v)) == ok(v)`; `from_json` errors for each listed reason
  with paths (`text: missing; tags[2]: expected string, got number`), all collected in document order;
  limits from annotations (`@size`, `@range`, `@pattern`, `@default` filling absent keys); NaN as `null`;
  a body nested 10,000 levels deep: `(root): nested too deeply` (a limit of 512 levels), never a stack
  overflow;
  E0328 for a record holding a function, a `map[int, str]`, an `ordered_map`, a `result`, a `Database`, with
  the path in the message.
- [X] T011 `src/check_generic.c`, `src/parse.c`, `src/vo.h`: the type-parameter constraint `json` (`T:
  json`, beside `ordered` and `effects`), checked where bound (`check_targ`), E0328 naming the path to the
  first unconvertible part; `docs/errors.md` E0328.
- [X] T012 `src/json_derive.c` (new), `src/rt_call.c`, `src/check_expr.c`, `src/vo.h`, `std/json.vo`:
  primitives `__to_json(v)` and `__from_json(j, mode)` walking the concrete type (`rt_concrete` of the
  call's type, as `src/task.c` `enc_value` does), building `Json` values of std.json's variant; decoding
  strict per research R2 with every problem collected as `path: reason`, limits and defaults from the
  field annotations (T008); `mode` 1 converts text scalars for path/query/header/settings values (R7, R12);
  `std.json.to_json[T: json]` and `from_json[T: json]` with doc comments and examples per
  contracts/json.md. Make T010 pass; `tests/doc_conformance.py` passes. `docs/reference.md` Generics: the
  `json` constraint beside `ordered`; the std table row for `std.json` gains `to_json`, `from_json` with
  the mapping in one sentence.
- [X] T013 `std/web.vo` (new), `src/doc.c` (module list): the skeleton of contracts/web.md: `App[C, E]`,
  `Route[C, E]`, `Step[C, E]`, `ErrorStatus`, `NoInput`, `handle_request` (no routes: 404 problem), the
  problem renderer of research R15 (`application/problem+json`, `type`, `title` from the status's reason
  phrase, `status`, optional `detail` and `errors`); `vo doc std.web` lists it.

**Checkpoint**: `new X {}` works; route-table effects type; annotations check; `to_json`/`from_json`
round-trip and report; `handle_request` answers 404.

---

## Phase 3: User Story 1 — Routes are declared, not parsed by hand (Priority: P1) 🎯 MVP

**Goal**: a route table answers each request with the right operation and path variables; 404, 405,
HEAD, OPTIONS come with it; mistakes in patterns and conflicts are check errors.

**Independent Test**: five operations on two resources, driven through `handle_request`: each reached with
its variables; unknown path 404; wrong method 405 with `Allow`; no routing code in the program.

- [X] T014 [P] [US1] Golden cases first, `tests/cases/web/`: `routing.vo` (US1 acceptance scenarios 1–3,
  5, 6 through `handle_request`, printing status, `Allow` and bodies), `head_options.vo`, `paths.vo`
  (trailing `/`, `//`, `%2F` in a variable per research R5), `route_errors.vo` (`.cmd` check: E0330 for a
  pattern that is not a literal, `/notes/{id`, a repeated `{id}`, a variable the input record lacks;
  E0331 for `/notes/{id}` and `/notes/{name}` on GET in one list, and for a route hidden by an earlier one,
  with related locations).
- [X] T015 [US1] `std/web.vo`: `on_get`, `on_post`, `on_put`, `on_patch`, `on_delete` (contracts/web.md),
  pattern parsing (literal segments "letters, digits, `-`, `_`, `.`, `~`" and `{name}`, no trailing `/`
  except `/`), matching per research R5 (literal beats variable at the first differing segment, empty
  segments match nothing, variables percent-decoded after splitting), 404 / 405 with `Allow` / HEAD as GET
  without body / OPTIONS 204 with `Allow`; the start-time validation of data-model.md ("patterns parse, no
  two routes of one method match the same paths") for tables `vo check` cannot see.
- [X] T016 [US1] `src/check_web.c` (new), `src/check_expr.c`: `vo check` of `on_*` calls into `std.web`
  (as `check_regex.c` / `check_sqlite.c` do): E0330 (not a literal, malformed with position, repeated
  variable, a variable without a path-bindable field in the operation's input record) and, for the
  elements of one list literal, E0331 (same method, same paths; or hidden) with related locations;
  `docs/errors.md` E0330, E0331. Make T014 pass.

**Checkpoint**: US1 independently testable.

---

## Phase 4: User Story 2 — Request input arrives typed and checked (Priority: P1)

**Goal**: an operation's input record is bound from path, query, headers and JSON body; anything that does
not fit answers a problem naming every bad field, and the operation does not run.

**Independent Test**: `POST /notes` with `{text: @size(1, 200) str, tags: @size(0, 5) @default([])
list[str]}`: a good body reaches the operation; missing `text`, empty `text`, six tags, a number for
`text`, and non-JSON each answer the documented problem.

- [X] T017 [P] [US2] Golden cases first, `tests/cases/web/`: `binding.vo` (US2 scenarios 1–5: path,
  query with defaults and lists, `@header`, body; every problem listed; 400 vs 422 per research R7),
  `content.vo` (body on an operation without `body`: 400; `text/plain` body: 415), `no_input.vo`
  (`NoInput` operations), and the pending `@header`-outside-input E0329 case of T007.
- [X] T018 [US2] `std/web.vo`: binding per contracts/web.md's input table (fields named like pattern
  variables, `body`, `@header("Name")`, the rest query parameters; text conversion through `__from_json`
  mode 1; a repeated query parameter for a non-list field is a problem), building one `Json` object and
  decoding it once so every problem is reported together; 400 / 415 / 422 problems with `errors`
  entries `{field, reason}`.
- [X] T019 [US2] `src/check.c` (or `src/check_web.c`): E0329 for `@header` on a field of a record that is
  not an operation's input. Make T017 pass.

**Checkpoint**: US1 + US2.

---

## Phase 5: User Story 3 — Answers and errors in one consistent form (Priority: P1)

**Goal**: records answer as JSON; helpers for 201/204; `err("kind: detail")` mapped in one table; runtime
errors answer 500 problem without internals.

**Independent Test**: an operation returning a record (200 JSON), `created` (201 + `Location`),
`err("not_found: no note 7")` (404 problem with detail), an unmapped `err` (500, no detail, logged), a
division by zero (500, report on stderr).

- [X] T020 [P] [US3] Golden cases first, `tests/cases/web/`: `answers.vo` (`json_reply`, `json_status`,
  `created`, `no_content`, a hand-built `Response` passed through), `errors.vo` (each default kind of
  research R4: "bad_request 400, unauthorized 401, forbidden 403, not_found 404, conflict 409, gone 410,
  unprocessable 422"; an added `error_status("quota", 429)`; an override; an unmapped error: 500 with no
  detail and a stderr line; a runtime error in an operation: 500 problem, report on stderr).
- [X] T021 [US3] `std/web.vo`: `json_reply`, `json_status`, `created`, `no_content` (`Content-Type:
  application/json`), the error table with defaults and the program's entries, the mapping of `kind:
  detail` texts, 500 for the rest with the text written to stderr, and the runtime-error path (the
  operation call under `__serve_call`'s recovery). Make T020 pass.

**Checkpoint**: US1–US3: an API answers correctly for every input.

---

## Phase 6: User Story 4 — The whole API is testable without a network (Priority: P1)

**Goal**: tests send requests to the application value and read JSON answers in one line each.

**Independent Test**: a notes API's `test` blocks drive every operation through `handle_request` with an
`open_memory()` database, with no `serve` effect.

- [X] T022 [P] [US4] Golden case first, `tests/cases/web/testing.vo`: `vo test` of a file whose tests use
  `request`, `with_header`, `handle_request`, `parse_json(r.body)` and std.json readers; the same request
  twice gives identical responses; `vo effects` shows no `serve` for the tests.
- [X] T023 [US4] `std/web.vo`: `request(method, path, body)` (`Content-Type: application/json` when the
  body is not empty) and `with_header`. Make T022 pass.

**Checkpoint**: the MVP (US1–US4).

---

## Phase 7: User Story 5 — Cross-cutting behavior in one place (Priority: P2)

**Goal**: steps run in order around operations, may answer early, and come ready for request ids,
logging, CORS and bearer auth.

**Independent Test**: request ids, `bearer_auth("/admin", check)` and `log_requests()`: `/admin/stats`
without a token is 401 and its operation does not run; every response has `X-Request-Id`; one log line
per request.

- [X] T024 [P] [US5] Golden cases first, `tests/cases/web/`: `steps.vo` (order in and out, early answer,
  prefix scoping, a step changing the request and one changing the response), `ready_steps.vo`
  (`request_ids` with `--seed`, an incoming `X-Request-Id` kept; `log_requests` line format; `cors`
  preflight for a listed and an unlisted origin; `api_key_auth` with a valid, missing and rejected key;
  `bearer_auth` ok → `X-Identity` bound by an operation's
  `@header("X-Identity")`, missing or rejected token → 401 problem), a step with a runtime error → 500.
- [X] T025 [US5] `std/web.vo`: the step chain of data-model.md ("the steps whose prefix matches, in list
  order, ending at routing"), `request_ids`, `log_requests` (method, path, status, milliseconds on
  stderr), `cors(origins)`, `bearer_auth(prefix, check)`, `api_key_auth(prefix, header, check)` per
  research R9. Make T024 pass.

---

## Phase 8: User Story 6 — The application's context and configuration (Priority: P2)

**Goal**: `serve_app` serves an application with a per-worker context; settings load typed from a file and
the environment with every problem reported at start.

**Independent Test**: settings `port` (`@default(8080)`) and `max_notes` (required): without
`MAX_NOTES`, exit 1 naming it; with it, operations read both; a step case serving real requests.

- [X] T026 [P] [US6] Golden cases first: `tests/cases/web/settings.vo` (file only, environment wins,
  defaults, a missing required and a malformed setting reported together, exit 1), and step cases
  `tests/cases/web_steps/serve/` (`serve_app` with 2 workers and a SQLite context: requests, a 404, a 422,
  `signal TERM`, `no-children`) and `tests/cases/web_steps/replay/` (record, delete the database, `vo
  replay`: identical output; settings read from the recording).
- [X] T027 [US6] `std/web.vo`: `serve_app` over `serve_workers` (validating the app first, R11) and
  `load_settings[T: json](file, prefix)` per research R12 (file object, then `PREFIX_FIELD` environment
  variables, decoded with `__from_json` mode 1). Make T026 pass.

---

## Phase 9: User Story 7 — Health and information endpoints (Priority: P3)

**Goal**: `GET /health` and `GET /info` by listing them.

**Independent Test**: a database check passing (200 `{"status":"up"}`) and failing (503 naming it).

- [X] T028 [P] [US7] Golden case first, `tests/cases/web/health.vo`: health up, down naming the failing
  check (503 `{"status":"down","checks":{"database":"..."}}`), info with name and version.
- [X] T029 [US7] `std/web.vo`: `HealthCheck[C, E]`, `health(checks)`, `info(name, version)` per research
  R13. Make T028 pass.

---

## Phase 10: User Story 8 — The example and the guide use it (Priority: P3)

**Goal**: notes-api is rewritten with std.web; agents find one way to build an API.

**Independent Test**: the rewritten notes-api passes its tests, answers its recorded session the same
(error bodies now problem+json), in at least 40% fewer API lines.

- [X] T030 [US8] `examples/notes-api/`: rewrite with `std.web` (route table, input records with limits,
  `err("not_found: ...")`, `serve_app` with 4 workers and `open_notes`, `health` with a database check);
  tests through `handle_request` with `open_memory()` (spec 027: tests next to the code); README, `vo.effects`,
  `recordings/demo.jsonl` re-recorded; count the API lines before and after for SC-001 (record both
  numbers in results.md).
- [X] T031 [P] [US8] `docs/reference.md`: the std table row and the `std.web` bullet group (routes, input
  record rules, answers, `kind: detail` errors, steps, `serve_app`, testing); the total for spec 029 stays
  within +700 tokens (`tests/ref_examples.py`).
- [X] T032 [P] [US8] `site/docs/guide/api.md` (new, linked from the guide's index and
  `standard-library.md`): building an API with std.web, with runnable examples checked by
  `tests/site_examples.py`; `docs/roadmap.md` (spec 029 row and section, with "Not done": sessions, views,
  OpenAPI, rate limiting, HTTPS serving).

---

## Phase 11: Polish and verification

- [X] T033 [P] `tests/web_fuzz.py` (SC-003): a 10-operation app; 1,000 generated malformed requests (bad
  JSON, wrong types, missing fields, broken limits, wrong methods, unknown paths, bad percent-encoding,
  huge numbers, deep nesting): every answer the documented 4xx problem naming the cause, no operation called with bad
  input (the app counts calls in its answers), no 500; `--quick` runs 200 (`make test`).
- [X] T034 [P] `tests/web_speed.py` (SC-006, `make gate`): a 20-route std.web app and the same API
  hand-written on `std.http`, both with 4 workers, measured with `ab` (skipped with a message when `ab` is
  missing): std.web at least 90% of the hand-written throughput.
- [ ] T035 (kit built, run deferred by the user: results.md SC-004) `bench/web/` (SC-004): the CRUD task (a small inventory API with validation and 404/409 rules)
  and its acceptance suite; 20 isolated agent runs with the reference as shipped and 20 told to use plain
  `std.http` (`claude -p`, no tools, empty directory, no user settings); pass rates and times in results.md.
- [X] T036 `make test` gains `"web_fuzz.py --quick"`; SC-007: every existing case unchanged (list any
  golden that changed and why in results.md).
- [X] T037 `make lint`; `make instrumented` (golden suite with `VO_BIN=build-instrumented/vo`); the stress
  build over `tests/cases/json_derive`, `web`, `web_steps`.
- [X] T038 `make test` on x86-64, then `make ARCH=aarch64 test` and `make ARCH=riscv64 test`, one at a time.
- [X] T039 Fill in `results.md`; spec status Implemented; every task [X].

---

## Dependencies & Execution Order

- Phase 1 → Phase 2. In Phase 2: T004 → T005, T006; T007 → T008 → T009 (the fuzzer needs the syntax); T010 → T011 → T012 (T012 needs T008's
  limits); T013 after T012.
- US1 (Phase 3) after Phase 2; US2 needs US1's routing (path variables); US3 needs US1; US4 needs US1–US3
  to be meaningful but its code is independent.
- US5, US6, US7 after US3 (they produce answers and problems); US6's step cases need US1–US3.
- US8 after US1–US7 (the example uses everything); T031/T032 can start after US4.
- Polish last; T033 needs US1–US3, T034 needs US1–US3, T035 needs US8's docs.

## Parallel Opportunities

- T002, T003 together; T007 and T010 (different areas) together, then T008 and T011.
- Each story's case-writing task ([P]) while the previous story's code lands.
- T031, T032 together; T033, T034 together.

## Implementation Strategy

- **MVP**: Phase 1, Phase 2, US1–US4: routing, typed input, answers and errors, tests without a network:
  an API with no hand-written routing or binding.
- Then US5 (steps), US6 (serving and settings), US7 (health), US8 (the example, docs), Polish with the
  measurements (SC-001, SC-003, SC-004, SC-006).
- Every step keeps existing programs unchanged; R10 only lets more programs type.

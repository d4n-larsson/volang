---

description: "Task list for vo-lang spec 011: HTTP server support in std.http"
---

# Tasks: HTTP Server Support in `std.http`

**Input**: Design documents from `specs/011-http-server/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R10), data-model.md, contracts/library.md,
contracts/recording.md, quickstart.md

**Tests**: REQUIRED (constitution, Principle IV): every story starts with golden or step cases that fail
on the current binary.

**Organization**: Setup and the shared foundation (the effect, the harness), then one phase per user story
in spec order (US1-US3 P1, US4-US5 P2), then polish.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- C with the strict warning set on all three architectures (`make lint`); declarations in `src/vo.h`;
  syscall numbers in `src/platform/*.h`.
- Server cases are step cases (`tests/cases/serve/`) using the harness commands of T003; responses are
  compared byte for byte (`Date`-free: the server sends no `Date` header).
- **Regression rule:** existing programs, recordings and the client part of `std.http` behave as before.

---

## Phase 1: Setup

- [X] T001 Create `specs/011-http-server/results.md` (SC table SC-001…SC-006 "open"; sections per story,
  Deviations, Follow-ups).

---

## Phase 2: Foundational (blocks every story)

- [X] T002 The `serve` effect (R2): `src/effects.c` name and canonical order (bit 256, after `env`);
  address-scope validation (`"PORT"` or `"0.0.0.0:PORT"`, 1-65535, else E0506 with the forms in the
  message); the reference's effect table and scope sentence. Golden cases `tests/cases/effects/serve_scope_bad.vo`
  (E0506) and `serve_canonical.vo` (`vo fmt` order). Every existing effect case unchanged.
- [X] T003 `tests/steps.py`: `start vo ARGS...` (background, `{port}` a free port, waits until it
  accepts), `request METHOD PATH [BODY]`, `request-raw TEXT` (`\r\n` escapes), `signal TERM|INT` (waits,
  then appends the started process's stdout, stderr, exit status to `out`).

---

## Phase 3: User Story 1 - A program answers HTTP requests (Priority: P1)

**Goal:** `serve`, `Request`, `request_header`, the limits, 500 on a failing handler (R1, R3, R4, R9).

**Independent test:** `python3 tests/steps.py tests/cases/serve` passes.

- [X] T004 [P] [US1] Step cases in `tests/cases/serve/`: `routes` (GET 200, unknown path 404, POST with a
  body and `Content-Type`, query string, headers echoed in order, `HEAD`), `bad_requests` (garbage line
  400, oversized head 431, body over the limit 413, chunked body decoded), `handler_fails` (division by
  zero and a failure inside a `map` callback: 500, the report on stderr, the next request answered),
  `listen_fails` (address in use: `serve` returns `err`). Confirm they fail now.
- [X] T005 [US1] `src/platform/*.h` syscall numbers (bind, listen, accept4, setsockopt, rt_sigprocmask,
  signalfd4); `src/serve.c`: `__serve_listen`, `__serve_next` (accept, read head and body within the
  limits, answer 400/408/413/431 itself), `__serve_reply`, `__serve_call` (guarded call: save the
  interpreter state, recovery point, report to stderr, restore, `err(code)`); builtin ids, typing and
  effects in `src/check_expr.c`, `src/check_effects.c`, `src/rt_call.c`; `src/interp.c` report rendering
  split from exiting.
- [X] T006 [US1] `std/http.vo`: `Request`, `request_header`, `serve` (the loop, request parsing with the
  client's helpers, response writing with `Content-Length`/`Connection: close`, `HEAD`), doc comments and
  examples.

---

## Phase 4: User Story 2 - Serving is a declared, scoped effect (Priority: P1)

**Goal:** static and run-time checks of `serve` (FR-004-FR-006).

**Independent test:** `tests/run.sh tests/cases/effects` and the serve step cases pass.

- [X] T007 [P] [US2] Cases: `tests/cases/effects/serve_missing.vo` (E0501 with the fix),
  `serve_handler_effects.vo` (a handler reading files: the caller must declare `file.read`),
  `tests/cases/serve/scope_denied` (declared `"8001"`, serving `"8000"`: R0010, nothing opened),
  `tests/cases/serve/all_interfaces` (`"0.0.0.0:{port}"` declared and served), `vo.effects` listing
  `serve("8000")`.
- [X] T008 [US2] Run-time address check in `__serve_listen` against the grant (R0010 naming the declaring
  function, spec 009 rule); loopback for `"PORT"`, all interfaces for `"0.0.0.0:PORT"`.

---

## Phase 5: User Story 3 - Handlers are tested without a network (Priority: P1)

- [X] T009 [P] [US3] `tests/cases/serve/handler_test.vo` (`vo test`: tests and a property build `Request`
  values, call the handler, compare `Response`s; no effect declared) and a failing variant with the usual
  report. Confirm they behave as specified (they may pass as soon as T006 lands).

---

## Phase 6: User Story 4 - Record and replay (Priority: P2)

**Goal:** `listen`/`request`/`response`/`stop` events; replay without a port; changed responses reported
(R7).

- [X] T010 [P] [US4] Step cases `tests/cases/serve/record_replay` (record a run with requests and a stop,
  replay: identical output, no port) and `replay_changed` (replay with a changed handler: a note per
  changed response; `--json` variant).
- [X] T011 [US4] `src/replay.c`: record and replay the four builtins' results; response comparison and
  its note/record; `specs/006-query-edit-trace/contracts/recording.md` and the reference's replay
  paragraph.

---

## Phase 7: User Story 5 - Stopping and timeouts (Priority: P2)

- [X] T012 [P] [US5] Step cases `tests/cases/serve/stop_signal` (SIGTERM and SIGINT while waiting: exit 0,
  `main`'s code after `serve` runs), `handler_timeout` (`--timeout 1`, a looping handler: 500 with R0009,
  next request answered), `long_life` (a server idle longer than `--timeout` still answers).
- [X] T013 [US5] Per-request deadline in `__serve_call`, none while waiting, restored after `serve`;
  signalfd stop (R5, R6).

---

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T014 `examples/status-board/service.vo` replaces the Python service (R10, SC-005); README; re-record
  `recordings/demo.jsonl`; `make test` replays it.
- [X] T015 `tests/serve_endurance.py` (SC-003: 10,000 requests, the server's memory stays bounded, clean
  stop) and an agent check (SC-001: 3 isolated runs on a two-route JSON service with tests).
- [X] T016 Reference growth (≤ 500 tokens, SC-005/SC-006); `vo doc std.http`; catalog; full gate
  (`make gate`, ARM64 and RISC-V suites, `make instrumented`, `make diff-asm COUNT=10000`,
  `stdlib-eval`, `prop-eval`, `refactor-eval`); fill `results.md`.

---

## Dependencies & Execution Order

- Setup → Foundational (T002, T003) → US1 → (US2, US3, US4, US5) → Polish.
- Within a story: cases first, then implementation.

### Parallel opportunities

- T004, T007, T009, T010, T012 write different case directories.

## Implementation Strategy

MVP: the effect, the harness and US1 (a working server with 500 on failures); then scoping, tests,
replay, stopping; then the example, endurance and the gate.

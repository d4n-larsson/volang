---

description: "Task list for vo-lang spec 054: Recorded Tests"
---

# Tasks: Recorded Tests

**Input**: Design documents from `specs/054-recorded-tests/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/cli.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's step cases land first and fail before its code.

**Organization**:
- Setup: results file, quick area, the options parsed (no behavior yet).
- Foundational: per-test start/finish/load of a recording in the runner (R1) and the file-name rule (R5).
- US1 (P1) record and replay; US2 (P1) mismatches and notes; US3 (P1) reviewable files and redaction;
  US4 (P2) CI: missing recordings and JSON; US5 (P2) coverage and mutation.
- Polish: the example, `tests/recorded_tests.py`, docs, three architectures, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Behavior**: contracts/cli.md is the source of every option, message, file name and field.
- **Step cases** (tests/steps.py): `tests/cases/recorded_tests_steps/NAME/` with `main.vo`, `steps`,
  `expected/`. A recording is made against `server http` (tests/http_server.py, `{port}`), the server is
  stopped with `stop-server`, then the replay runs; produced recordings are compared with `keep`.
  Write the case, run `python3 tests/steps.py tests/cases/recorded_tests_steps/NAME`, see it fail, then
  write the code.
- **C**: zero warnings; recording writes allocate nothing (as `rcd_event` today); no static buffer over
  64 KiB; `ponytail:` comments on known ceilings.
- **Unchanged**: `vo run --record` and `vo replay` (their cases under `tests/cases/replay` must not
  change).
- **Between edits**: `make quick AREA=recorded`; `make test` once at the end; never two suites at once.

## Phase 1: Setup

- [X] T001 Create `specs/054-recorded-tests/results.md` with headings SC-001 to SC-008 and "Reference cost"; add area `recorded` to `tools/quick.sh` (case dirs `recorded_tests_steps replay test_steps test`, script `recorded_tests.py --quick`, files `src/replay.c src/testrun.c tests/recorded_tests.py`)
- [X] T002 In `src/main.c`: `test` accepts `--record DIR` (stored apart from `run`'s `--record FILE`: `opt_test_record`), `--replay DIR` (`opt_test_replay`) and repeatable `--redact NAME` (a list); usage errors T0001 for both `--record` and `--replay`, for `--redact` without `--record`, and for `--replay` with `run`; `vo run --record FILE` unchanged; `make`; a case `tests/cases/cli/test_record_options` (`.cmd` with the bad combinations) shows the T0001 texts

## Phase 2: Foundational

- [X] T003 In `src/replay.c`: the file-name rule of contracts/cli.md as `Str *rcd_test_path(Str *dir, Str *file, Str *name)` (letters, digits, `-`, `.` kept; space `_`; any other byte `%XX` upper-case) and its inverse for listing; `rcd_test_start(Node *test, ...)` (open the file, header with `"test": "FILE::NAME"` and `"redact"`, no `args`, `rcd_mode = 1`), `rcd_test_finish(status)` (write `end`; when no event was written, remove the file instead), `rp_test_load(path)` (read a test recording into `rp_events` on the current heap; a missing file gives an empty list) and `rp_test_reset()`; reuse the existing writers and readers
- [X] T004 In `src/testrun.c` `run_one_test`: after `gc_start`, with `opt_test_record` call `rcd_test_start` and with `opt_test_replay` call `rp_test_load`; after the body (however it ended), `rcd_test_finish`/`rp_test_reset`; a test that timed out or crashed while recording keeps its older file (write to `NAME.jsonl.tmp` and rename on a normal end); create directories under DIR as needed

**Checkpoint**: `vo test --record DIR` writes one file per test that touched the outside; `--replay` loads them.

## Phase 3: User Story 1 - Record once, replay offline (P1) 🎯 MVP

**Goal**: record with the network, replay without it, same results.

**Independent Test**: `python3 tests/steps.py tests/cases/recorded_tests_steps` passes `record_replay`,
`pure_test`, `postgres_tcp`.

- [X] T005 [P] [US1] Step case `record_replay`: `main.vo` with three tests (an HTTP GET of `http://127.0.0.1:{port}/rate?from=EUR&to=SEK` parsed into a number, an HTTP POST with a JSON body, and a test that reads a file `data/in.txt` and an environment variable); steps: `server http`, `vo test --record rec main.vo`, `keep rec/main.vo/...` for the three files (the port shows as `{port}`), `stop-server`, `vo test --replay rec main.vo` (all pass, `(replayed)`), run the replay 3 times (identical)
- [X] T006 [P] [US1] Step case `pure_test`: a test with no outside input and one with: `--record` writes only the second's file; `--replay` passes both; removing the outside call and recording again removes its file
- [X] T007 [P] [US1] (done as `tests/recorded_tests.py --group tcp`: step cases have no TCP server) Step case `postgres_tcp` (or `tcp_echo` if no PostgreSQL is reachable in cases): a test that talks to `tests/tcp_server.py` through `std.tcp` (connect, send, read) is recorded and replayed with the server gone
- [X] T008 [US1] Make T005–T007 pass: results print `(recorded)`/`(replayed)` after the test name in text (contracts/cli.md), the recorded seed and clock are those of `vo test` (1 and 0), and replay restores them; `make quick AREA=recorded`

## Phase 4: User Story 2 - A changed request fails the test, and says why (P1)

**Goal**: mismatches are R0014 in that test only; unused inputs and changed code are notes.

**Independent Test**: `mismatch_*` and `notes` step cases pass.

- [X] T009 [P] [US2] (done as one case, `mismatch`, with a test per kind) Step cases `mismatch_url`, `mismatch_method`, `mismatch_body`, `mismatch_file`, `mismatch_env`, `mismatch_more_requests`: record, change the test's code (a `cp` of a second `main.vo`), replay: the test is `error` with R0014, its message names the request made and the one recorded next and ends with "re-record it with `vo test --record DIR --test ID`"; another test in the same file still passes
- [X] T010 [P] [US2] Step case `notes`: fewer requests than recorded → the test passes with the note "1 recorded input was not used, the first: ..."; a changed comment in the program → the note "the program changed since this test was recorded"; both in `--json` under `"notes"`
- [X] T011 [P] [US2] Step case `select`: record two tests, change one test's server answer, `--record --test ID` re-records only that one (the other file is byte-identical)
- [X] T012 [US2] In `src/replay.c`: for test recordings the R0014 messages of contracts/cli.md; HTTP events of test recordings carry `"sent": {"headers": [...], "body": "..."}` and replay compares the body (a different body is R0014 "... with another body"); at the end of a replayed test, unused events give the note; a fingerprint different from the header's gives the changed-code note (not under `--mutate`); `src/testrun.c` prints notes under the test's line and adds them to its JSON record; until T009–T011 pass

## Phase 5: User Story 3 - Recordings are reviewable and safe to commit (P1)

**Goal**: identical re-records, readable exchanges, secrets redacted.

**Independent Test**: `redact` and `stable` step cases pass.

- [X] T013 [P] [US3] Step case `redact`: a test sends `Authorization: Bearer secret-token`, `X-Api-Key: k-123`, the query `?api_key=q-456`, reads `API_SECRET=v-789` and gets `Set-Cookie: s=c-000` back; `vo test --record rec --redact X-Api-Key --redact api_key --redact API_SECRET main.vo`; `keep` the file: every value is `[redacted]`, the header lists the names; replay (no `--redact`) passes; a step greps the file for each secret and finds none
- [X] T014 [P] [US3] Step case `stable`: record the same tests twice against the unchanged server; the second `keep` equals the first (byte-identical)
- [X] T015 [US3] In `src/replay.c`: redaction of contracts/cli.md when writing test recordings (request headers `Authorization`, `Proxy-Authorization`, `Cookie`; response `Set-Cookie`; named headers case-insensitive, named query parameters in the request line, named `env` values) and the same redaction applied to the request in replay before comparing; a redacted `env` value is returned to the test as `[redacted]`; until T013–T014 pass

## Phase 6: User Story 4 - CI runs recorded tests without network (P2)

**Goal**: a missing recording fails clearly; plain `vo test` is unchanged; JSON says how each test ran.

**Independent Test**: `missing` and `json` step cases pass.

- [X] T016 [P] [US4] Step case `missing`: `--replay rec` with no file for a test that makes an HTTP request → `error` R0014 "this test has no recording in rec: record it with `vo test --record rec --test ID`", and no connection is made (the server is not started: a live attempt would be a different error); plain `vo test` with `rec/` present runs live (server started)
- [X] T017 [P] [US4] Step case `json`: `--json` under `--record`, `--replay` and plain shows `"recording": "recorded"|"replayed"|"live"`; an extra file under `rec/` for a test that no longer exists is listed in `"unused_recordings"` and in the text summary "recordings with no test: ..."
- [X] T018 [US4] In `src/testrun.c` and `src/replay.c`: the missing-recording R0014 text, the `recording` field, the unused-recordings listing after the run (files under DIR for the program's files that name no test of it; with a selection, only the selected files); until T016–T017 pass

## Phase 7: User Story 5 - Coverage and mutation testing offline (P2)

- [X] T019 [P] [US5] Step case `offline_quality`: recorded tests of a small HTTP-calling function; `vo test --replay rec --coverage` lists what no test ran; `vo test --replay rec --mutate` runs every mutant offline (server stopped), a mutant that changes the request URL is caught (R0014 counts as a failing test), and no changed-code note is printed
- [X] T020 [US5] Make T019 pass (expected to need only the note suppression of T012 and the per-mutant reload of recordings; fix whatever else it shows)

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T021 Example `examples/rates/` (research R8): `main.vo` serving `GET /convert?amount=&from=&to=` with `std.web`; `rates.vo` with `rate(from, to)` calling a public exchange-rate API over HTTPS (no key) and the conversion; tests through `handle_request` with a fake rate function passed in (pure), and one test of `rate` itself recorded under `recordings/tests/` (if the API is unreachable, record against a local stand-in and say so in the README); `vo.effects`, README (record, replay, redaction, re-recording one test); `examples/README.md` row; `Makefile`'s example loop runs `vo test --replay recordings/tests main.vo` when that directory exists
- [X] T022 [P] `tests/recorded_tests.py` (research R9): SC-002 (replay with networking unavailable: `unshare -rn` when permitted, else no server and an address that refuses), SC-003 (a 100 ms server: replay under 1% of the live time, median of 5), SC-005 (20 tests recorded twice, directories identical), SC-006 (redaction search over a generated set of secrets), SC-007 (`--mutate` on the example offline); `--quick` runs a smaller set; add it to `make test`'s list
- [X] T023 [P] Docs: `docs/reference.md` (the synopsis line of `vo test` gains `[--record DIR [--redact NAME] | --replay DIR]`; one bullet under "Traces and replay": per-test recordings, R0014 in the test, redaction defaults) within 15,000 tokens, cost in results.md; `docs/errors.md` R0014 mentions tests; `site/docs/guide/tests.md` section "Recorded tests"; `docs/security.md` (test recordings redact credentials; bodies are not inspected); `docs/roadmap.md` (054 row and section; the gap removed from the table); `CHANGELOG.md`
- [X] T024 Three architectures and the instrumented build: `tests/cases/recorded_tests_steps`, `tests/cases/replay`, `tests/cases/test_steps` and the example on `build-aarch64/vo`, `build-riscv64/vo`, `build-instrumented/vo`; replayed results byte-identical across them (SC-001)
- [X] T025 `make test` once; fill results.md (SC-001 to SC-008, reference cost); spec.md Status Implemented

## Dependencies & Execution Order

- T001, T002 → T003, T004 → US1 (T005–T008) → US2 (T009–T012) → US3 (T013–T015) → US4 (T016–T018)
  → US5 (T019–T020) → Polish (T021–T025).
- US2–US5 all build on US1's per-test recording; US3's redaction changes the HTTP event written in US2,
  so they run in that order. T021 needs US1–US4; T022 needs US1–US3.

## Parallel Opportunities

- Within each story, the step cases ([P]) are separate folders and can be written together.
- T022 and T023 together.

## Implementation Strategy

1. MVP: Setup, Foundational, US1 — tests record and replay.
2. US2 and US3 before the feature is used in earnest: without them a stale recording passes silently or
   a key is committed.
3. US4, US5, the example, measurements and docs.

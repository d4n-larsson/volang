# Research: Recorded Tests

From reading `src/replay.c` (recording and replay, 1,347 lines), `src/testrun.c` (the test runner),
`src/main.c` (options) and spec 018's decision.

## What exists

- One recording per process: globals `rcd_mode` (0 off, 1 recording, 2 replaying), `rcd_fb` (the file),
  `rp_events`/`rp_next` (replay). Every outside input goes through `io_*` wrappers that record after the
  real call or take the next event instead (`rp_take`, strictly in order; another kind or name is
  R0014, raised as a runtime error at the call).
- Header: program, fingerprint (FNV-1a of every file's path and text), seed, clock, args. Footer: `end`
  with the status. No timestamps.
- An HTTP event holds host, port, scheme and the **request line** only (method, path and query); not the
  request headers or body. The response is recorded whole.
- `vo test` runs tests one after another in one process, each with a fresh heap, its own recovery point
  (`run_one_test`) and `rt_effects_start(1)` (seed 1, clock 0). A runtime error in a test makes that test
  `error` and the run goes on.
- `--record FILE` is accepted only by `run` (`only_with("--record", "run")`).

## R1. One recording per test, opened and closed around the test

**Decision**: with `--record DIR`, `run_one_test` starts a recording before the body (header with the
test identifier, no `args`) and finishes it after; with `--replay DIR`, it loads the test's file into
`rp_events` after `gc_start` (so the events live on the test's heap) and checks for unused events at
the end. The process-wide globals are reset between tests. A test that received nothing writes no
file and removes a stale one of its own.

**Rationale**: the runner already isolates tests (heap, recovery, deadline); the recording globals are
the same kind of per-run state. Reusing `io_*` and `rp_take` keeps one implementation of what is
recorded and how it is matched (spec 018: "in the existing recording schema").

**Alternatives considered**: one recording for the whole run with test markers (one file to review,
but re-recording one test rewrites everything, and a missing test shifts every later event);
running each test in a child process (needless: the runner isolates already).

## R2. A mismatch fails only its test

`rp_mismatch` raises R0014 as a runtime error at the call; under `vo test` that makes the test
`error` with R0014 and the next test runs. Its message already names what the program asked for and
what the recording has next. For tests, the message ends "re-record it with `vo test --record DIR
--test ID`" instead of "record this one". No new code.

**Missing recording** under `--replay`: the test runs with no events; the first outside input raises
R0014 with "this test has no recording in DIR: record it with ...". Same code, its own message
(a test that touches nothing outside passes with no file).

**Unused events** (FR-007): at the end of a passing or failing test, `rp_next < n` adds a note
"N recorded inputs were not used, the first: ...". **Changed code** (FR-008): the fingerprint differs
from the header's: note "the program changed since this test was recorded". Notes go in the test's
text line and JSON record; they never change its status.

## R3. Request bodies and headers in test recordings

**Decision**: test recordings add to the `http` event the request's headers and body as sent
(`"sent"`, after redaction R4). Replay compares the request line as today and also the body; a
different body is R0014 ("... makes an HTTP request POST /convert with another body"). Headers are
written for review and not compared (a `User-Agent` with vo's version would make every upgrade a
mismatch). Program recordings (`vo run --record`) are unchanged (FR-018).

**Rationale**: spec SC-004 lists a different body as a mismatch, and US3 wants the request readable in
review. Comparing headers would be brittle; the body is what the code chose to send.

## R4. Redaction (clarification Q2)

**Decision**: `vo test --record DIR --redact NAME` (repeatable). When writing a test recording:
- in the request headers written under `"sent"`: the values of `Authorization`, `Proxy-Authorization`,
  `Cookie` and of every header named by `--redact` (case-insensitive) become `[redacted]`;
- in the request line (path and query): the value of every query parameter named by `--redact`;
- in the response: the values of `Set-Cookie` and of every header named by `--redact`;
- `env` events of variables named by `--redact`: the value becomes `[redacted]`.

Replay compares requests with the same redaction applied to what the test sends now, and gives the
test `[redacted]` where a redacted variable's or header's value was. The recording's header lists the
redacted names (`"redact": [...]`), so replay redacts the same ones without the option.

**Rationale**: the defaults cover the headers that carry credentials in nearly every API; the option
covers API-key headers (`X-Api-Key`), key query parameters and secret variables, which have no fixed
names. A test whose code checks the secret's value itself (rare) sees the marker.

**Alternatives considered**: refusing to record (rejected in Q2); hashing values (a hash of a short key
can be brute-forced; a marker leaks nothing).

## R5. File names (FR-011)

**Decision**: `DIR/<file path relative to the entry's directory, without .vo>/<test name>.jsonl`, where the test name
keeps letters, digits, `-` and `.`, writes a space as `_`, and writes every other byte (including `_`)
as `%XX` (upper-case hex). Example: `notes.vo::a token signs in` →
`DIR/notes/a_token_signs_in.jsonl`. The rule is reversible, so a file names its test.

**Unused files** (FR-015): after the run, files under `DIR` whose test is not in the program are listed
("recordings with no test: ..."); with a selection, only files of the selected files are considered.

## R6. Identical files on re-record (FR-012)

The header has no time; seed and clock are fixed in `vo test`; events are written in the order the test
made them. Re-recording an unchanged exchange is identical unless the service's answer changed (a
`Date` header): that is a real difference, shown in the diff. The fingerprint changes only when the
program's text changes.

## R7. Options and JSON

- `vo test` gains `--record DIR`, `--replay DIR` (exclusive; with `--replay`, `--record` is a usage
  error T0001) and `--redact NAME` (only with `--record`).
- Each test's JSON record gains `"recording": "live" | "recorded" | "replayed"` and `"notes": [...]`;
  the summary gains `"unused_recordings": [...]`. Fields are added, none renamed (schema 1).
- `--coverage`, `--mutate` and selection work unchanged (they run tests through `run_one_test`).
  Under `--mutate`, the changed-code note is not printed (every mutant changes the code).

## R8. The example

A new example `examples/rates/`: a small service (`GET /convert?amount=&from=&to=`) that calls a public
exchange-rate API over HTTPS, with tests of the conversion through `handle_request` and a recorded test
of the outgoing call, its recording committed under `recordings/tests/` and replayed by `make test`
(the Makefile's example loop gains `vo test --replay recordings/tests` when that directory exists).
If the public API cannot be reached while implementing, the recording is made against a local
stand-in server and the README says so.

## R8b. Reference cost

About 45 words (~60 tokens): the options in the CLI synopsis and one bullet under "Traces and replay".
167 tokens are left.

## R9. Tests

- Golden step cases (as `tests/cases/*_steps`) that record against `tests/http_helpers` or a Python
  server, then replay with the server gone: pass, mismatches (URL, method, body, file, variable,
  program, more and fewer requests), missing recording, changed code note, unused files, selection.
- `tests/recorded_tests.py`: records 20 tests twice and compares directories (SC-005); runs replay with
  networking unavailable (`unshare -n` when present, else no server and an unroutable address)
  (SC-002); timing against a 100 ms server (SC-003); redaction search (SC-006); `--mutate` on the
  example (SC-007).

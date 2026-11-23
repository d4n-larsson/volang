# Feature Specification: Recorded Tests

**Feature Branch**: `054-recorded-tests`

**Created**: 2026-11-23

**Status**: Implemented ([results](results.md))

**Input**: User description: "Specify the recorded tests feature." First in the roadmap's open gaps under
constitution Principle VI (backend network services: easier to write, review or operate). The
mechanism was decided in spec 018's clarifications (2026-11-17): `vo test --record DIR` saves one
recording per test, named by its identifier, and `vo test --replay DIR` runs on them, in the existing
recording schema, with no language change. It replaces mock objects, which vo does not have.

Background: `vo test` runs every test with a fixed seed and clock, so a test is deterministic except for
what it receives from outside: HTTP and HTTPS responses, TCP exchanges (a PostgreSQL server through the
postgres package), files, environment variables, other programs. `vo run --record FILE` already saves
everything a program receives and `vo replay FILE` runs it again with no outside access, but only for a
whole program run. An agent writing a service that calls another API today either tests against the
live service (slow, flaky, needs credentials and network on every run) or leaves that code untested.

From an agent's point of view:
- **The real answer, once.** A test records what the real service said; afterwards it runs offline,
  fast and the same every time, with no hand-written fake that shares the agent's assumptions.
- **A change shows.** When the code asks the service something new, the replayed test fails and says
  what was asked, instead of passing on a stale answer.
- **A reviewer can read it.** A recording is a text file in the repository: the requests the code
  sends and the answers it relies on are visible in a diff.

What this spec must keep:
- **Principle III**: replayed tests give byte-identical results on the three architectures and over runs.
- **Principle II**: per-test results in JSON; new failures have stable codes.
- **Principle VI**: answered above.
- **Effects stay the boundary**: a replayed test is checked against its declared effects and scopes as
  a live one is.
- `vo run --record` and `vo replay` behave as before; no new syntax; the reference stays within 15,000
  tokens (about 167 left), with the cost stated.

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents writing and testing vo services, the people who review their changes,
and CI that runs the tests without network access.

### User Story 1 - Record once, replay offline (Priority: P1)

An agent writes a test of a function that calls an exchange-rate API. It runs `vo test --record
tests/recordings main.vo` once with the network; the test passes against the real API and its
recording is saved. From then on, `vo test --replay tests/recordings main.vo` runs that test with no
network, in milliseconds, with the same result every time.

**Why this priority**: it is the feature.

**Independent Test**: a test that makes an HTTPS request to a local test server is recorded, the server
is stopped, and the replayed test passes with identical output on every run and architecture.

**Acceptance Scenarios**:

1. **Given** a test that makes HTTP requests, **When** run with `--record DIR`, **Then** it runs against
   the real world as `vo test` does today and a recording of everything it received is written for it
   in DIR.
2. **Given** that recording and no network, **When** the test runs with `--replay DIR`, **Then** it
   receives exactly what it received when recorded and gives the same result.
3. **Given** a test that touches nothing outside the program, **When** run with `--record` or
   `--replay`, **Then** it runs as today and no recording is written or needed for it.
4. **Given** the same recordings, **When** replayed 20 times and on the three architectures, **Then**
   the results are byte-identical.
5. **Given** a recorded test of a function using the postgres package against a real server, **When**
   replayed with the server gone, **Then** it passes (TCP exchanges are replayed like HTTP).

---

### User Story 2 - A changed request fails the test, and says why (Priority: P1)

The agent changes the function to ask the API for another currency. Replaying the old recording, the
test fails: the failure names the request the code made and the closest request in the recording.
The agent re-records that one test.

**Why this priority**: a replay that silently answered a changed request would hide exactly the bugs
recorded tests are for.

**Independent Test**: golden cases where the replayed code asks for another URL, another file, another
variable, a different body, more requests and fewer requests than recorded.

**Acceptance Scenarios**:

1. **Given** a recording and code that now sends a request the recording does not have, **When**
   replayed, **Then** that test fails with a stable code, naming the request made and the closest
   recorded one; the other tests still run.
2. **Given** code that sends fewer requests than recorded, **When** replayed, **Then** the test's
   result says that recorded answers were not used, without failing it (a removed call is not a
   wrong answer).
3. **Given** code changed since recording that still sends the same requests, **When** replayed,
   **Then** the test runs and its result notes that the code changed since recording.
4. **Given** `--record` with a selection (`--test ID`, `--spec ID`, `--filter TEXT`), **When** run,
   **Then** only the selected tests are re-recorded; the other recordings in DIR are untouched.

---

### User Story 3 - Recordings are reviewable and safe to commit (Priority: P1)

A reviewer opens the pull request and sees the test's recording next to the code: the request the code
sends and the answer it depends on. Re-recording an exchange that did not change gives the same file,
so the diff shows only real changes. API keys and passwords the test used are not in the file.

**Why this priority**: recordings are meant to be committed; one with a leaked key or a noisy diff
would not be.

**Independent Test**: record twice against an unchanged server and compare the files byte for byte;
record a test that sends an `Authorization` header, an API key header, a password in a form body and
reads a secret environment variable, and search the recording for the secrets.

**Acceptance Scenarios**:

1. **Given** a test recorded twice against unchanged services, **When** the two recordings are
   compared, **Then** they are identical (no timestamps, durations or ordering that varies).
2. **Given** the recording directory, **When** listed, **Then** there is one file per recorded test,
   named from the test's identifier so that it is found from the test and the test from it.
3. **Given** a test that sends credentials or reads secrets, **When** recorded, **Then** those values
   are replaced by a marker in the recording: the values of `Authorization`, `Proxy-Authorization`,
   `Cookie` and `Set-Cookie`, and of every header, query parameter and environment variable the run
   names to redact; replay matches everything else and gives the test the marker where the value was
   (Q2).
4. **Given** a recording, **When** read by a person, **Then** each exchange is readable text: the
   request (method, address, the headers and body the test sent) and the answer.

---

### User Story 4 - CI runs recorded tests without network (Priority: P2)

CI runs the test suite with no network access. Tests with recordings replay; a test that needs the
outside world and has no recording is reported as such, not run live and not silently passed.

**Why this priority**: offline CI is where recorded tests pay off every day.

**Acceptance Scenarios**:

1. **Given** `--replay DIR` and a test that reaches outside with no recording in DIR, **When** run,
   **Then** that test fails with a stable code saying it has no recording and how to make one; it makes
   no outside access.
2. **Given** plain `vo test` and a recording for a test, **When** run, **Then** the test runs live, as
   today: recordings are used only with `--replay` (Q1).
3. **Given** `--json`, **When** tests run, **Then** each test's record says whether it ran live, was
   recorded or was replayed.

---

### User Story 5 - Coverage and mutation testing offline (Priority: P2)

An agent asks which code its tests miss and which changes they would not catch, for code that calls
services. With recordings, `--coverage` and `--mutate` run offline on the replayed tests.

**Why this priority**: mutation testing runs the tests many times; against live services that is slow
and hits rate limits; offline it is practical.

**Acceptance Scenarios**:

1. **Given** recordings, **When** `vo test --replay DIR --coverage` runs, **Then** coverage is reported as
   for any test run, with no outside access.
2. **Given** recordings, **When** `vo test --replay DIR --mutate` runs, **Then** each mutant runs on the
   recordings; a mutant that changes a request fails its test (US2), which counts as caught.

### Edge Cases

- **A test that both reads files and calls a service**: everything it receives is in its recording;
  files it writes are not written during replay, as in `vo replay`.
- **Tests that depend on each other's files** (one writes, the next reads): each recording holds what
  its test read, so replay needs no order; recording runs in source order as today.
- **Properties (`prop`)**: run 100 generated inputs; a recorded property holds the exchanges of all
  of them in one recording (same seed, same inputs).
- **A test that times out or crashes while recording**: no recording is written for it, and the run
  says so; an older recording of it is left as it was.
- **The same test identifier in two files**: identifiers include the file, so names do not collide;
  a renamed test needs re-recording (its old file is reported as unused).
- **Recordings no test uses**: reported (so stale files can be removed), not an error.
- **Clock and randomness**: fixed in `vo test` already; `random_bytes` is recorded as for `vo run`.
- **HTTPS**: the recording holds the exchange, not keys; replay needs no certificates.
- **Servers in tests**: `handle_request` involves no network and needs no recording; a test that
  starts a real server (`serve`) is out of scope for recording (as in `vo run`, a server's requests
  are its inputs; a test does not serve).
- **Large answers**: recorded as received, with no size limit beyond the existing ones.

## Requirements *(mandatory)*

### Functional Requirements

**Recording and replaying**

- **FR-001**: `vo test --record DIR` MUST run the selected tests against the real world and write, for
  each test that received anything from outside, one recording in DIR of everything it received, in
  the schema `vo run --record` uses.
- **FR-002**: `vo test --replay DIR` MUST run each selected test on its recording with no outside
  access, giving the result the test gave when recorded unless the code changed.
- **FR-003**: Tests that receive nothing from outside MUST run as today under both options, with no
  recording.
- **FR-004**: Selection (`--test`, `--spec`, `--filter`, `--diff`) MUST work with both options;
  `--record` MUST leave the recordings of unselected tests untouched.
- **FR-005**: `--coverage` and `--mutate` MUST work with `--replay`.

**Mismatches and missing recordings**

- **FR-006**: A replayed test that asks for an input its recording does not have next MUST fail with
  a stable code, naming the input asked for and the closest recorded one; other tests continue.
- **FR-007**: A replayed test that leaves recorded inputs unused MUST pass or fail on its own result,
  with a note naming the unused inputs.
- **FR-008**: A replayed test whose code changed since recording MUST run, with a note.
- **FR-009**: Under `--replay`, a test that reaches outside and has no recording MUST fail with a
  stable code that says how to record it, without reaching outside.
- **FR-010**: Plain `vo test` MUST run live as today and read no recording; recordings are used only
  with `--replay DIR` and written only with `--record DIR` (Q1).

**Recordings as files**

- **FR-011**: DIR MUST hold one file per recorded test, named from its identifier by a stated,
  reversible rule (file path and test name, with characters a file name cannot hold escaped).
- **FR-012**: Recording an unchanged exchange twice MUST give byte-identical files.
- **FR-013**: Recordings MUST be readable text, one exchange after another.
- **FR-014**: When recording, the values of the headers `Authorization`, `Proxy-Authorization`,
  `Cookie` and `Set-Cookie`, and of every header, query parameter and environment variable named with
  the redaction option of the run, MUST be written as a fixed marker, never as the value; replay MUST
  match requests on everything but those values and give the test the marker in place of a redacted
  variable or header (Q2). Secrets elsewhere (a request body, a URL path) are not detected; the guide
  says to use test credentials.
- **FR-015**: Files in DIR that no selected test uses MUST be listed in the run's summary.

**Fit**

- **FR-016**: A replayed test MUST be checked against its declared effects and scopes as a live one is;
  a recording cannot grant more.
- **FR-017**: `--json` test records MUST say `live`, `recorded` or `replayed`, with the notes of FR-007
  and FR-008.
- **FR-018**: `vo run --record` and `vo replay` MUST behave as before.
- **FR-019**: The reference MUST describe the two options in the fewest words and stay within 15,000
  tokens; the cost is stated in the change.
- **FR-020**: An example MUST include a test of code that calls an outside HTTP service, with its
  recording committed and replayed by `make test`.

### Key Entities

- **Test recording**: everything one test received from outside, in order, for one test identifier;
  a file in the recording directory.
- **Recording directory**: the directory given to `--record`/`--replay`; one file per test.
- **Exchange**: one request the test made and the answer it got (HTTP, TCP, a file read, a variable, a
  program run).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every acceptance scenario is a golden or scripted case; replayed results are
  byte-identical over 20 runs and on x86-64, ARM64 and RISC-V.
- **SC-002**: A test suite whose tests call outside services runs with `--replay` with no network
  access at all (checked with networking unavailable) and passes.
- **SC-003**: A replayed test of an HTTP call takes under 1% of the time of the same test against a
  local server with a 100 ms response delay.
- **SC-004**: For each kind of mismatch (another address, method, body, file, variable, program, more
  or fewer requests), the replayed test fails or notes as specified, naming both the request made and
  the closest recorded one where there is one.
- **SC-005**: Recording 20 tests twice against unchanged local services gives identical directories.
- **SC-006**: No value of a redacted header, query parameter or variable given to a test appears in its
  recording (searched for each value); replaying with the marker gives the same result as recording.
- **SC-007**: `--mutate` on the example's recorded tests runs offline and reports its result.
- **SC-008**: The reference stays at or under 15,000 tokens.

## Assumptions

- **The recording schema of `vo run --record` is reused** (spec 018's decision), with whatever a test
  needs added (its identifier); `vo replay` reads only program recordings.
- **Recording runs tests in source order and one at a time**, as `vo test` does.
- **No automatic expiry**: a recording is valid until re-recorded; staleness shows as a mismatch
  (FR-006) or a note (FR-007, FR-008).
- **Hand-written fake answers are not a feature**; editing a recording by hand works because it is
  text, but nothing helps or checks it beyond replay.
- **The example** is a small new one (a service that calls another API) or an addition to an existing
  one; the plan decides, keeping examples within their structure limits.
- Out of scope: hand-written fakes, parallel test runs, isolating tests that write files, network
  proxies, recording a test that serves.

## Clarifications

### Session 2026-11-23

- Q: When a recording exists, does plain `vo test` replay it? → A: No: plain `vo test` runs live as
  today; recordings are used only with `--replay DIR` (spec 018's form). CI passes the option.
- Q: How are secrets kept out of committed recordings? → A: Redacted at record time: the values of
  `Authorization`, `Proxy-Authorization`, `Cookie`, `Set-Cookie`, and of headers, query parameters and
  environment variables the run names, are written as a marker; replay matches the rest.

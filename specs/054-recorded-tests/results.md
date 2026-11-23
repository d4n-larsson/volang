# Results: Recorded Tests

`make test` passes (exit 0). The instrumented build passes all 1,171 golden cases, the 10 recorded-test
step cases and `recorded_tests.py --quick`.

## SC-001 Cases, byte-identical over runs and architectures

Met. 10 step cases in `tests/cases/recorded_tests_steps/` (record_replay, pure_test, mismatch, notes,
select, redact, stable, missing, json, offline_quality) plus `tests/cases/cli/test_record_and_replay`
and `recorded_tests.py` (tcp, nonet, speed, stable, redact, mutate). The step cases and
`recorded_tests.py --quick` pass on `build-aarch64/vo`, `build-riscv64/vo` and `build-instrumented/vo`;
record_replay replays twice in a row with identical output.

## SC-002 Replay with no network

Met: `recorded_tests.py --group nonet` replays two recorded HTTP tests under `unshare -rn` (no network
at all): both pass.

## SC-003 Replay time against a slow live service

Met: a test of three requests to a server answering after 100 ms: live 305 ms, replayed 1 ms (the
tests' own times, median of 5), 0.3%.

## SC-004 Every kind of mismatch fails or notes as specified

Met. `mismatch`: another URL, method, body, file and variable, and more requests than recorded, each
fail their test with R0014 naming the input asked for and the one recorded next (or "no more inputs");
an unchanged test in the same file passes. `notes`: fewer inputs than recorded is a note. TCP: other
bytes sent are R0014 naming the connection (`--group tcp`).

## SC-005 Identical re-records

Met: 20 tests recorded twice against the unchanged server (`--group stable`) give 20 byte-identical
files; the step case `stable` checks the same for one file in `make test`. A real service's answer
may hold a `Date` header, which changes on every recording: a real difference, shown in the diff.

## SC-006 No redacted secret in a recording

Met: generated secrets in `Authorization`, `Proxy-Authorization`, `Cookie`, a header named with
`--redact`, a query parameter and a variable appear nowhere in the recording (`--group redact`); the
replay passes. The step case `redact` keeps such a recording as expected output.

## SC-007 Mutation testing offline

Met: `vo test --replay recordings/tests --mutate main.vo` on examples/rates under `unshare -rn`:
"mutation: 9 changes, 8 caught, 1 survived, 0 not tried". The step case `offline_quality` runs
`--coverage` and `--mutate` on a replayed test with the server stopped.

## SC-008 Reference within 15,000 tokens

Met: ~14,925.

## Reference cost

~14,833 → ~14,925 tokens (92): the `vo test` synopsis gains `[--record DIR [--redact NAME] | --replay DIR]`
and one bullet under "Traces and replay". 75 tokens are left.

## Deviations and findings

- **TCP sends**: program recordings keep a TCP send only when it failed, so a test of the postgres
  package whose SQL changed would have replayed the old rows. Test recordings now keep every send's
  bytes and replay compares them (R0014 on a difference); program recordings are unchanged.
- **`sent` is one string** (the request's headers and body after the request line), not the structured
  object of the first contract: readable in review, and the body comparison cuts it at the blank line.
- **JSON `recording` field** is written only for recorded and replayed tests (absent means live), so the
  JSON of every existing test run is unchanged; the contract and data model say so.
- **Folders without `.vo`**: `DIR/rates/NAME.jsonl`, not `DIR/rates.vo/...`: a folder named like a source
  file was picked up by tools that look for `*.vo` (the grammar corpus check in `make test`). Folders are
  made only for tests that recorded something.
- **Text results** read `PASS name (3ms, replayed)`; `tests/steps.py` masks the time as it does `(3ms)`.
- **`usage_record_test`**: `vo test --record` is no longer a usage error; the case now checks `vo check
  --record` (renamed `usage_record_check`).
- **The test HTTP server** gained `GET /cookie` (a `Set-Cookie` and an `X-Api-Key` header) for the
  redaction cases.

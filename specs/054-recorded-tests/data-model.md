# Data Model: Recorded Tests

## Test recording (a file)

- **Header**: `v`, `kind: "recording"`, `program`, `fingerprint`, `seed` (1), `clock_ms` (0), `test`
  (`FILE::NAME`), `redact` (names, sorted).
- **Events**: as program recordings, in the order the test received them; `http` events add `sent`.
- **End**: `end` with the test's exit status as the runner saw it (0 pass, 1 otherwise).
- **Identity**: one file per test identifier; path from the identifier by the rule in contracts/cli.md.
- **Lifecycle**: absent → written by `--record` (test received something) → rewritten by a later
  `--record` of that test → removed by a `--record` of that test that received nothing. Never changed by
  `--replay` or plain `vo test`.

## Recording directory

- A tree of test recordings mirroring the program's files. Files whose test is not in the program are
  "unused" and listed, never deleted automatically.

## Redaction set

- Fixed: request headers `Authorization`, `Proxy-Authorization`, `Cookie`; response header `Set-Cookie`.
- Named: `--redact NAME` values at record time, stored in the header and applied again in replay.
- Marker: `[redacted]`.

## Test result (JSON record, added fields)

- `recording`: `recorded` | `replayed`; absent for a live test (so existing records are unchanged).
- `notes`: list of strings (unused recorded inputs; program changed since recording).
- Summary: `unused_recordings`: list of file paths relative to DIR.

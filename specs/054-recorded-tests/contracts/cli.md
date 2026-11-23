# Contract: `vo test` options, results and the recording directory

## Options

```text
vo test [--record DIR [--redact NAME]... | --replay DIR] [existing options] PATH...
```

- `--record DIR`: run the selected tests live; for each that received anything from outside, write
  `DIR/<FILE>/<NAME>.jsonl` (below), its folder made only then; for a selected test that received nothing, remove its file if any.
  Recordings of unselected tests are untouched. DIR is created when missing.
- `--redact NAME` (repeatable, with `--record` only): also redact the header, query parameter or
  environment variable called NAME (headers case-insensitive).
- `--replay DIR`: run each selected test on its file, with no outside access; a test without a file
  runs with no recorded inputs.
- Both at once, or `--redact` without `--record`: T0001.
- Plain `vo test`: live, as before; reads and writes no recording.

## File names

`<FILE>`: the test's file path relative to the entry's directory, without `.vo` (a folder named like a source file would be found by tools that look for `*.vo`). `<NAME>`: the test's name with letters,
digits, `-` and `.` kept, space written `_`, every other byte written `%XX`. Example:
`notes.vo::a token signs in` → `DIR/notes/a_token_signs_in.jsonl`.

## Results

| Situation | Test status | Text | JSON |
|---|---|---|---|
| recorded | the test's own | `PASS name (3ms, recorded)` | `"recording": "recorded"` |
| replayed | the test's own | `PASS name (0ms, replayed)` | `"recording": "replayed"` |
| live (no option) | the test's own | unchanged | no `recording` field (absent means live) |
| replay asks for an input not next in the recording | `error`, R0014 | the request made and the one recorded next; "re-record it with `vo test --record DIR --test ID`" | as any runtime error |
| replay with no file and an outside input | `error`, R0014 | "this test has no recording in DIR: record it with ..." | same |
| recorded inputs left unused | the test's own | note `N recorded inputs were not used, the first: ...` | `"notes": [...]` |
| program changed since recording | the test's own | note `the program changed since this test was recorded` (not under `--mutate`) | `"notes": [...]` |
| files under DIR for tests not in the program | — | `recordings with no test: ...` after the summary | summary `"unused_recordings": [...]` |
| recording could not be written | `error` | T0009 at the end, as `vo run --record` | — |
| a test times out or crashes while recording | its status | no file written; an older file is kept | — |

## Recording file

The schema of `vo run --record` (specs/006 contracts/recording.md and later additions), with:

- header: `"kind": "recording"`, `"test": "FILE::NAME"` (instead of `args`), `"redact": [names]`;
- `http` events: `"sent": {"headers": [[name, value], ...], "body": "..."}` (after redaction);
- redacted values written as `[redacted]`: request headers `Authorization`, `Proxy-Authorization`,
  `Cookie`, response header `Set-Cookie`, and every header, query parameter and `env` value named with
  `--redact`.

Replay compares, as for programs, each event's kind and name, the HTTP request line (after redaction)
and, for test recordings, the request body. Headers are not compared.

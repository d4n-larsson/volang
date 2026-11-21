# Results: Streaming

Measured on the development machine (x86-64), 2026-11-21. The gate was not run.

## Success criteria

| Criterion | Result |
|---|---|
| SC-001: 300 MiB summed line by line under a 16 MiB limit, in under 10 s | passes: 45,000,000 lines, the right total, 6.2 s |
| SC-002: a 100 MiB download with the right hash under a 32 MiB limit | passes with a length, chunked, to the close of the connection, and chunked over TLS; SHA-256 of the written file equals the server's (peak live 8.1 MiB) |
| SC-003: each line of a child within 200 ms; a stopped child is gone | passes: worst delay 1 ms; after `ok(false)` or an error the child's process no longer exists when the call returns |
| SC-004: every program replays with its source gone | passes: lines, chunks, downloads (length and chunked), a followed child and a stopped child, with the file removed, the server stopped and the child's script deleted |
| SC-005: peak memory does not grow with the source | passes: 11.6 MiB for a 10 MiB file, 11.7 MiB for 300 MiB (both under `--max-memory 16`) |
| SC-006: three architectures, reference budget | `tests/stream.py` (sizes divided by 20) and the three step cases pass on ARM64 and RISC-V under qemu; reference about 14,938 of 15,000 tokens |

`make test` passed on the final code (`stream: 5 groups, 42 cases, 0 failures`; `tcp`, `lsp`, `mcp`,
`grammars` and the golden and step cases with 0 failures; the log ends at the last step). Its first run
failed on one check, the structure rules over the library (`each_line` and `run_lines` each had 15
statements without an effect): both were split into smaller functions and the run repeated.
`make site-check` passes. Stress build: `run/oom_small`, `map_keys/many` and the three stream step cases
pass. New static data: the table of followed children, 6 KiB.

## Deviations from the plan

- **File blocks are their own recording event**, `file_block` with `path` and `offset`, not a
  `read_file` event with an added field.
- **A followed child has three operations, not four**: next line, finish and stop; finish and stop
  return the exit code and standard error together.
- **`send_stream` calls the connection's send primitive directly**: `std.http` has a `send` of its
  own, so `std.tcp.send` cannot be imported there.
- **The small behaviours are step cases** (`tests/cases/fs_steps/stream`, `process_steps/stream`,
  `http/stream`), not single-file golden cases: they need files, a FIFO, helper programs and a server.
  `tests/steps.py` gained a `write-bytes` step for files that are not UTF-8.
- **23 existing cases changed**, in two ways only: line numbers of `std/http.vo` in traces (it gained
  import lines), and the fingerprint in kept recordings (it covers the library's text). No output of a
  program changed. FR-012 holds; the convention "no existing golden case may change" did not.
- **`timeout_ms` of 0 means 600,000** for `send_stream` (the longest `std.tcp` allows); `HEAD` is not
  supported, as for `send`.
- **A stopped child usually has code 137**, but one that ended by itself just before the stop keeps its
  own code. The documentation says so.
- **Recording a large stream** was tested with 100 MiB, not 300.
- **Not checked from the server's side** that a stopped download closes the connection; the test checks
  that the call returns at once.
- **Tests and code were written together**, story by story; the step case for `send_stream` (analysis
  finding D1) was written after it.

## Found on the way

- vo has no incremental hash, so a program cannot hash a file larger than memory.
- `bytes` and `test` are keywords: a record field or variable cannot have those names.

## Not verified

- No real web server or long build: peers are local scripts.
- The latency and time bounds are checked on x86-64 only (under qemu they are skipped).
- A malformed response is covered for four cases; the chunked reader was not fuzzed.
- 16 children followed at once (`too many programs are being followed`) has no test.
- The gate (`make gate`), including the collector stress suite over every case.

## Also in this working tree

`tests/web_speed.py` (spec 029's speed check): paired rounds and a bound of 80% instead of 90%, with a
note in `specs/029-web-package/results.md`.

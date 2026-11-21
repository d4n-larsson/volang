---

description: "Task list for vo-lang spec 040: Streaming"
---

# Tasks: Streaming

**Input**: Design documents from `specs/040-streaming/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/streaming.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's cases land in `tests/stream.py` or `tests/cases/`
first and fail before its code.

**Organization**:
- Setup: builtin ids, the test driver, the results file.
- Foundational: none beyond Setup; the three sources are independent.
- US1 (P1) files; US2 (P2) HTTP; US3 (P2) child programs; US4 (P2) record and replay; US5 (P3) docs.
- Polish: memory and timing, stress cases, three architectures, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Signatures, answers, errors, guarantees**: contracts/streaming.md. Pieces, limits, events:
  data-model.md.
- **The test driver**: `tests/stream.py` makes its inputs in a temporary directory, runs vo programs
  with `$VO_BIN` (default `bin/vo`) and compares stdout and exit status. Output: `FAIL <case>: ...`
  per failure and a one-line summary; exit 1 on a failure; `--group NAME`. `VO_STREAM=small` divides
  the large sizes by 20 (for qemu).
- **Every streaming function is vo code** that loops over a primitive (research R1); primitives never
  call back into vo.
- **No static array over 64 KiB** in new code.
- **Unchanged programs**: no existing golden case may change.
- **Reference budget**: `python3 tests/ref_examples.py` after the reference change; at most +40 tokens
  (from about 14,898).
- **Between edits**: `make quick AREA=stream` (T001); `make test` once, through `tools/progress.py run
  test`, reading `build/progress/test.log`; the gate only when asked. Never two suites at once.

## Phase 1: Setup

- [X] T001 Add area `stream` to `tools/quick.sh` (golden areas `fs process http replay tcp doc`; scripts `catalog.py ref_examples.py doc_conformance.py stream.py`; files `src/fs.c`, `src/process.c`, `std/fs.vo`, `std/process.vo`, `std/http.vo`, `tests/stream.py`) and `stream.py` to the test list in `Makefile`
- [X] T002 Add builtin ids `B_FS_BLOCK` (`__fs_block(path, offset, size) -> result[bytes]`, signature `sii>Y`), `B_FS_LINES` (`__fs_lines(path, offset)`, typed loosely as `__sql_query`: the result takes the expected type), `B_PROC_START` (`__proc_start(argv, input, env) -> result[int]`, `SsS>n`) and `B_PROC` (`__proc(op, id) -> result[str]`, `ii>r`) in `src/vo.h`, `src/check_expr.c`, `src/rt_call.c` and their effects (`file.read`, `file.read`, `process`, none) in `src/check_effects.c`, with stubs returning `err("not supported")`
- [X] T003 [P] Create `tests/stream.py` with the driver of Conventions and an empty group list, and `specs/040-streaming/results.md` with headings for SC-001 to SC-006

## Phase 2: User Story 1 - Read a file larger than memory (P1) 🎯 MVP

**Goal**: `each_chunk` and `each_line` for files of any size in bounded memory.

**Independent Test**: `tests/run.sh fs`; `python3 tests/stream.py --group files`.

- [X] T004 [P] [US1] Golden cases in `tests/cases/fs/`: `each_line` over `\n` and `\r\n` lines, a last line without a line end, an empty file (never called), a lone `\r` and a NUL inside a line; early stop with `ok(false)` (returns `ok()`, later lines not delivered); an error from the function returned as it is; a line that is not UTF-8 gives `line N: not UTF-8` after the lines before it; `each_chunk` with a size that divides the file and one that does not (last piece shorter, never empty); chunk size 0 and 16777217 give `the chunk size must be 1 to 16777216` before the file is opened; a missing file and a directory give `read_file`'s errors; a path outside `file.read("data/")` is R0010; a caller without the function's effect is E0501; a nested `each_line` inside the function; a FIFO gives `not a regular file: read it whole with read_file` and delivers nothing; a file the function appends to while it is read is delivered as it is at each read, with no error
- [X] T005 [P] [US1] In `tests/stream.py`, group `files`: a 300 MiB file of numbers summed line by line under `--max-memory 16` gives the right total in under 10 s (SC-001); the same file read in 1 MiB chunks gives Python's byte count and Python's sum of every chunk's first and last byte (vo has no incremental hash); a 17 MiB line gives `line 1: longer than 16 MiB`; `--mem-stats` peak for a 10 MiB and a 300 MiB file within 1 MiB of each other (SC-005)
- [X] T006 [US1] In `src/fs.c`, `fs_block(path, offset, size, &out)`: the grant check and link rules of `read_bytes`, open, `pread` up to `size` at `offset`, close; errors as `read_file`'s, and `not a regular file: read it whole with read_file` when the opened file is not a regular file; wire `__fs_block` through `io_fs_block`, which T018 makes record and replay
- [X] T007 [US1] In `src/fs.c`, `io_fs_lines(path, offset)` per research R3: a 256 KiB block (doubling to 16 MiB + 1 while it holds no line end and the file has more), the whole lines without `\n` or `\r\n` as a `list[str]` whose first element is the offset after the last whole line as decimal text; the rest of the file as a last line at its end; an empty batch at the end; `not UTF-8` and `line too long` as errors only for the first line of a block; wire `__fs_lines`
- [X] T008 [US1] In `std/fs.vo`, `each_chunk` and `each_line` with the signatures of contracts/streaming.md, as loops over the primitives (size check first; line numbers counted in vo for the two line errors), with doc comments and runnable examples

## Phase 3: User Story 2 - Download something large (P2)

**Goal**: `send_stream` delivering a response body in pieces, on `std.tcp`.

**Independent Test**: `python3 tests/stream.py --group http`.

- [X] T009 [P] [US2] In `tests/tcp_server.py`, HTTP modes: `http-length`, `http-chunked` and `http-close` send N MiB (from the request path `/N`) of a repeating pattern with that framing; `http-stall` sends half and goes silent; `http-short` declares more than it sends; `http-status` answers 404 with a small body and 204 with none; each also over TLS with `--cert`
- [X] T010 [US2] In `tests/stream.py`, group `http`: 100 MiB appended to a file under `--max-memory 32` with length, chunked, to-the-end and over TLS, the file's SHA-256 equal to the pattern's (SC-002); pieces are 1 to 65536 bytes; the function sees the status and headers; 404 body delivered, 204 never calls the function; `ok(false)` after the first piece returns `ok` with the head and the server sees the connection closed; an error from the function is returned; the stalled body gives an error after `timeout_ms` with the first half delivered; the short body gives `the response ended early`; a URL outside the `net` scope is R0010; method, headers and a request body arrive as with `send`; a chunk size line that is not hexadecimal, one of 17 digits, a chunk without its line end and a `Content-Length` that is not a number each give `malformed response: ...` and exit status 0 (new modes `http-bad-*` in `tests/tcp_server.py`)
- [X] T011 [US2] A step case in `tests/cases/` next to the existing HTTP step cases (the area that starts `tests/http_server.py`): a program streams one response with `send_stream` and prints the status, the number of pieces and the total length; it must fail before T012 (constitution IV)
- [X] T012 [US2] In `std/http.vo`, `send_stream` per research R4: URL, method and header checks and request text shared with `send`; `connect` or `connect_tls`; the head through the blank line (over 64 KiB is an error) parsed by the existing head code; the body by chunked framing, by `Content-Length`, or to the close; none for `HEAD`, 1xx, 204, 304; pieces of at most 64 KiB; `timeout_ms` on every `std.tcp` call; `std.tcp` errors through `describe`; the connection closed on every path; a doc comment with a runnable example

## Phase 4: User Story 3 - Follow a child program while it runs (P2)

**Goal**: `run_lines` delivering standard output line by line while the child runs.

**Independent Test**: `tests/run.sh process`; `python3 tests/stream.py --group children` and `--group sweep`.

- [X] T013 [P] [US3] Golden cases in `tests/cases/process/`: `run_lines` on `printf` output (lines in order, the exit status and standard error returned, `stdout` empty); input and environment passed as `run_with`; a program outside the `process` scope is R0010; a missing program gives `run`'s error; a last line without a line end is delivered; standard error over the output limit gives `run`'s error
- [X] T014 [US3] In `tests/stream.py`, groups `children` (a child printing a line every 100 ms: each reaches the function within 200 ms of its printing, measured with `now_ms` in the function against the timestamps the child prints (SC-003); `ok(false)` after the third line returns within 1 s and the child's pid is gone; an error from the function likewise; 200 MiB of output counted under `--max-memory 16`; a child that closes its standard output and exits half a second later with status 3: the call returns after it ended, with code 3, not killed; a function that calls `concurrent_map` gets its results and the stream goes on) and `sweep` (a runtime error inside the function, and `exit` inside it, leave no child running; the same in a failing `vo test`)
- [X] T015 [US3] In `src/process.c`, the table of data-model.md (16 entries: pid, owner pid, three pipe ends, input not yet written, a mapped line buffer growing to 16 MiB, a standard error buffer with the existing output limit, the exit status) and `__proc_start` (the checks and spawn of `run_with`, pipes non-blocking), `__proc` op 1 (next line: `poll` on the three pipes under the run's deadline, feeding input and collecting standard error; `err("eof")` when standard output ended; the line rules of R3), op 2 (finish, after the end of output: wait for the child to end as `run` does, still feeding input and collecting standard error under the run's deadline, reap, return standard error), op 3 (stop, after `ok(false)` or an error from the function: close pipes, `SIGKILL`, reap, return standard error), op 4 (the exit status as text); `proc_sweep()` ending every entry this process owns, called from `vo_exit` in `src/sys.c` and after each test in `src/testrun.c`
- [X] T016 [US3] In `std/process.vo`, `run_lines` with the signature of contracts/streaming.md as a loop over `__proc` that finishes (op 2) after the end of output and stops (op 3) when the function answers `ok(false)` or an error, so the child is reaped on every path, with a doc comment and a runnable example

## Phase 5: User Story 4 - Recorded and replayed (P2)

**Goal**: every stream replays with its source gone.

**Independent Test**: `python3 tests/stream.py --group replay`.

- [X] T017 [US4] In `tests/stream.py`, group `replay`: record a run of the line sum (10 MiB), the chunk count, a 5 MiB download (length and chunked), the slow child and the early stops; delete the file, stop the server, point `PATH` away from the child program; `vo replay` gives the same output and exit status (SC-004); a program changed to read another path, and one changed to run another program, stop with R0014; recording the 300 MiB sum stays under `--max-memory 16`
- [X] T018 [US4] In `src/replay.c` and `src/vo.h`: an `offset` field on `read_file` events (written for block reads only, absent for `read_file` and `read_bytes`; read back, and compared with the path in replay by `io_fs_block`); event kinds `proc_line` and `proc_end` in the writer, reader and `rp_check_event`; in `src/process.c`, record the start as the existing `run` event and each line and the end as they happen, and in replay serve them with no child

## Phase 6: User Story 5 - Documented (P3)

**Independent Test**: `vo doc std.fs.each_line`; `python3 tests/ref_examples.py`; `make site-check`.

- [X] T019 [P] [US5] In `docs/reference.md`: the four names in the `std.fs`, `std.http` and `std.process` rows and one sentence on the shape (a function per piece; `ok(false)` stops); keep it under 15,000 tokens
- [X] T020 [P] [US5] In `site/docs/guide/files-and-processes.md`, a section "Large inputs" with a runnable example each for `each_line`, `each_chunk` and `run_lines` (with output blocks) and a checked-only example for `send_stream`; a pointer from `site/docs/guide/standard-library.md` (std.http)
- [X] T021 [P] [US5] Update `docs/roadmap.md` (status row and a "040" section), `CHANGELOG.md` and the "Fixed since the review" table in `docs/review-2026-11/README.md`

## Phase 7: Polish

- [X] T022 Run `tests/run.sh --stress` on `run/oom_small`, `map_keys/many` and the new `fs` and `process` cases; check with `nm` that no new static object exceeds 64 KiB; record in `specs/040-streaming/results.md`
- [X] T023 Run `VO_STREAM=small python3 tests/stream.py` with `VO_BIN` set to `build-aarch64/vo` and `build-riscv64/vo`; record in `specs/040-streaming/results.md`
- [X] T024 Run `make test` once through `tools/progress.py run test`, read `build/progress/test.log`, fix what it finds, and complete `specs/040-streaming/results.md` (SC-001 to SC-006 with figures, deviations, what was not verified), confirming for FR-012 that no existing golden case changed; set the spec's status

## Dependencies

- Setup first. US1, US2 and US3 are independent of each other (US2 needs spec 039's `std.tcp`, which
  exists). US4 needs all three. US5 needs the functions to exist. Polish last.

## Parallel opportunities

- T003 with T002; T004 with T005; T009 with T013; T019, T020 and T021.

## Implementation strategy

MVP is Setup and US1: large files, the case with no workaround. Then HTTP (no C), then child programs
(the only kept state), then replay across all three, then documentation.

# Research: Streaming

## R1. The loop is vo code

**Decision**: each streaming function is an exported vo function that loops: ask a primitive for the
next piece, call the passed function, act on its answer. Primitives never call back into vo.

**Rationale**: a call from vo is an ordinary call (traces, effect parameters, contracts and runtime
errors behave as everywhere), and the function's `match` on the answer is the only control flow, so
"released on every path" is visible in a dozen lines. `std.http.serve` is built the same way.

**Alternatives considered**: a C loop calling the function value (`rt_serve_call` exists, but error
paths and recovery in C are where leaks hide); handles returned to the program (the review argues
against them: nothing in vo closes a value).

**A runtime error inside the passed function** ends the program, or the test. Files hold nothing open
between pieces (R2), a connection is closed by the end of the process or reused slot (spec 039), and a
child is ended by the sweep (R5).

## R2. Files without an open file

**Decision**: one primitive, `__fs_block(path, offset, size) -> result[bytes]`: check the grant as
`read_bytes` does, open the file, read up to `size` bytes at `offset` (`pread`), close it. A result
shorter than `size` is the end. `each_chunk` is a loop over offsets.

**Rationale**: no state, no table, nothing to release, and a task or a nested stream cannot disturb
it. Three system calls per block instead of one is not measurable at 64 KiB and up. The grant is
checked on every block, which is the same check giving the same answer.

**Only regular files.** Reading at an offset needs a file that has offsets. For anything else (a pipe,
a FIFO, a terminal) the primitive returns the error `not a regular file: read it whole with
read_file`, decided from the file's type after opening it. A sequential fallback would bring back the
open file this design avoids.

**A file that changes between blocks** is read as it is at each read (spec edge case). A file replaced
by another between blocks would be read from both; the same is true of any program that reads a path
twice.

## R3. Lines

**Decision**: a second operation of the primitive, `__fs_lines(path, offset) -> result[list[str]]`
(typed loosely, as `__sql_query`): reads a block of 256 KiB at `offset`, and returns the whole lines
in it, without their line ends, preceded by one element holding the offset after the last whole line
(as decimal text). At the end of the file the rest is a line even without a line end; an empty result
with the same offset is the end. A line that does not end within the block makes the block grow
(doubling, up to 16 MiB + 1); beyond that it is the error `line too long`. A line that is not valid
UTF-8 is returned as the error `not UTF-8` when it is the first of the block; lines before it are
returned first, so the vo loop, which counts lines, names the line number.

**Rationale**: splitting in vo would be a byte loop in the VM (about a second per 10 MiB); in C it is
`memchr` speed. Batches keep the primitive stateless.

## R4. `send_stream` on `std.tcp`

**Decision**: in `std/http.vo`, in vo: parse the URL and build the request as `send` does (with
`Connection: close`), `connect` or `connect_tls` to the host and port, `send`, `read_until` the blank
line (a head over 64 KiB is an error), parse status and headers with the existing code, then deliver
the body in pieces of at most 64 KiB:

- `Transfer-Encoding: chunked`: a size line (hexadecimal, extensions ignored), that many bytes, a line
  end; a zero size, then trailers to the blank line;
- else `Content-Length: N`: exactly N bytes; fewer is the error "ended early";
- else: until the peer closes;
- none for `HEAD`, 1xx, 204 and 304.

A size line that is not hexadecimal, is longer than 16 digits or would exceed the integer range, a
chunk not followed by a line end, and a `Content-Length` that is not a number are `malformed
response: ...`, never a runtime error: the body comes from the server.

`timeout_ms` is passed to each `std.tcp` call, so it bounds each wait (FR-007). Errors of `std.tcp`
become the module's text errors through `describe`. The connection is closed on every path.

**Rationale**: no second HTTP reader in C, and the scope check, TLS verification, record and replay are
`std.tcp`'s, already tested. `send` and `get` keep their C path and their 16 MiB cap.

**Consequences**: a streamed request is recorded as connection events, so a replay checks the host and
port, not the path (spec 039's rule: what is written is not compared). `std.tcp` offers no ALPN, which
HTTP/1.1 servers do not require.

## R5. A followed child

**Decision**: in `src/process.c`, a table of 16 entries (pid, owner pid, the three pipe ends, a line
buffer for standard output, a capped buffer for standard error, the input still to write). Primitives:

- `__proc_start(argv, input, env) -> result[int]`: the checks and the spawn of `run_with`, pipes
  non-blocking, returns the entry's id;
- `__proc(op, id) -> result[str]`: `1` the next line of standard output (waiting with `poll` on all
  three pipes under the run's deadline, feeding input and collecting standard error meanwhile);
  `err("eof")` when standard output has ended; `2` finish: after the end of output, wait for the child
  to end as `run` does (still feeding input and collecting standard error, under the run's deadline),
  reap it, return the collected standard error; `3` stop: after `ok(false)` or an error from the
  passed function, close the pipes, `SIGKILL` the child, reap it, return the collected standard error;
  `4` the exit status as text, valid after finish or stop (kept until the id is reused).

Line rules are R3's (16 MiB, UTF-8). There is no time limit per line: a silent child is bounded by
`--timeout` only, as for `run`. Standard error beyond the existing output limit is an error at
end, as `run`.

**The sweep**: `proc_sweep()` kills and reaps every entry whose owner is this process. It is called
from `vo_exit` and from the test runner after each test, so a runtime error or `exit` inside the
passed function leaves no child running. A task (a fork) inherits the table but owns none of the
parent's entries.

**Alternatives considered**: reusing `rt_run` with a callback (its loop drains to the end by design).

## R6. Record and replay

**Decision**:

| Source | Recorded | On replay |
|---|---|---|
| file block (both operations) | a `read_file` event with `offset` and the bytes read (or the error) | the next event must be a `read_file` of the same path and offset; lines are split again from the bytes |
| HTTP | `std.tcp`'s events | `std.tcp`'s replay |
| child | `run` start (program, arguments: the existing `run` event with no output), a `proc_line` event per line or error, a `proc_end` event with status and standard error | no child; the lines in order |

Each event is written when its piece is delivered, so nothing accumulates in memory. Events written
through the existing writer are already streamed to the file.

## R7. Answers and results

**Decision**: the passed function returns `result[bool]`: `ok(true)` go on, `ok(false)` stop (the
stream returns its success value), `err(m)` stop and return `err(m)`. Signatures:

```text
std.fs.each_line[E: effects](path: str, f: fn(str) -> result[bool] uses E) -> result[void] uses file.read, E
std.fs.each_chunk[E: effects](path: str, size: int, f: fn(bytes) -> result[bool] uses E) -> result[void] uses file.read, E
std.http.send_stream[E: effects](method: str, url: str, headers: list[Header], body: str, timeout_ms: int,
    f: fn(Response, bytes) -> result[bool] uses E) -> result[Response] uses net, E
std.process.run_lines[E: effects](program: str, args: list[str], input: str, env: list[str],
    f: fn(str) -> result[bool] uses E) -> result[Output] uses process, E
```

`send_stream` passes and returns the existing `Response` with an empty `body`; `run_lines` returns the
existing `Output` with an empty `stdout`. No new record. `each_line` errors for a bad line are
`"line N: not UTF-8"` and `"line N: longer than 16 MiB"`.

## R8. Documentation budget

**Decision**: the reference gains the four names in the existing rows of `std.fs`, `std.http` and
`std.process` and one sentence on the shape ("streams take a function called per piece; `ok(false)`
stops"), about 40 tokens. The site gets a section "Large inputs" in Files and processes with one
example per function.

## R9. Tests

**Decision**: `tests/stream.py` (in `make test`) with groups files (300 MiB sum under 16 MiB, hash by
chunks, same peak for 10 MiB and 300 MiB with `--mem-stats`), http (100 MiB with length, chunked,
to-the-end, TLS, early stop, a body that stalls, a short body), children (a slow printer: latency per
line; stop after three lines and the child is gone; 200 MiB of output; input and environment), replay
(each program recorded and replayed with its source gone), sweep (a runtime error in the function
leaves no child). `VO_STREAM=small` divides the sizes by 20 for qemu. Golden cases: line ends, last
line without one, empty file, early stop, error from the function, bad UTF-8 with its line number,
chunk size limits, scopes (R0010), missing file, a path that is not a regular file; a step case in
`tests/cases/https_steps/` or `tests/cases/http/` (whichever starts `tests/http_server.py`) for one
streamed response, written before `send_stream` (constitution IV). `tests/tcp_server.py` gains HTTP
body modes.
Regression: every existing `fs`, `process`, `http` and `replay` case unchanged.

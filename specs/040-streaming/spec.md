# Feature Specification: Streaming (files, downloads and child programs piece by piece)

**Feature Branch**: `040-streaming`

**Created**: 2026-11-21

**Status**: Implemented (gate not run)

**Input**: User description: "Streaming, the review's language item 9 (docs/review-2026-11/language.md,
'Streaming'; ecosystem.md order step 3): everything in vo is whole. `read_file` returns the file, `get`
returns the body (capped at 16 MiB), `run` returns the program's output after it ends; a log file of a
gigabyte, a large download or a long-running child process cannot be handled. The review recommends
callbacks over handles (vo has no destructors or `defer`): a function such as `each_line(path, f)`
opens and closes the file inside the call, passes each line to `f`, fits effect parameters, is checked
once against the grant, and is recorded as a sequence of pieces. The same shape for chunks of `bytes`,
for HTTP bodies and for a child program's output. Must keep: determinism under replay, no libc, three
architectures, scopes enforced, no large static buffers, the reference under 15,000 tokens (about
14,898 now: almost nothing can be added), one way to write each thing. Out of scope: iterator or
handle types, `defer`, a streaming JSON parser, streaming rows from SQLite, server-sent events and
WebSockets."

Background: a vo program holds whatever it reads in memory, all of it, before it looks at the first
byte. That is simple and it is why every read can be recorded and replayed, but it puts a ceiling on
the programs vo can write: the memory limit (512 MiB by default) bounds the largest file, 16 MiB the
largest download, and a child program's output is seen only after it has ended, so a build that prints
progress for ten minutes looks hung. `std.tcp` (spec 039) already hands out bytes as they arrive. This
feature does the same for the three whole-value reads, without adding a new kind of value to the
language: the program passes a function, and the library calls it once per piece.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Read a file larger than memory (Priority: P1)

A developer processes a log or a data file of any size: count, filter, sum or find something, line by
line or in chunks of bytes, in memory that does not grow with the file.

**Why this priority**: it is the commonest case and the one with no workaround today.

**Independent Test**: a 300 MiB file of lines is summed line by line under `--max-memory 16`, giving
the right total; the same file read in chunks gives the right byte count and hash.

**Acceptance Scenarios**:

1. **Given** a text file, **When** the program reads it line by line, **Then** its function is called
   once per line, in order, with the line without its line end (`\n` or `\r\n`); a last line without a
   line end is delivered; an empty file calls it never.
2. **Given** a file of any bytes, **When** the program reads it in chunks of a given size, **Then**
   its function gets every byte once, in order, in pieces of exactly that size except the last.
3. **Given** a function that answers "stop", **When** it does, **Then** no further piece is read and
   the call returns success; **Given** one that returns an error, **Then** reading stops and the call
   returns that error.
4. **Given** a file that is missing, a directory, or unreadable, **When** read, **Then** the call
   returns the same error `read_file` gives, and the function is never called.
5. **Given** a line that is not valid UTF-8, or one longer than the line limit, **When** reached,
   **Then** the call returns an error naming the line number; the lines before it were delivered.
6. **Given** a file of 300 MiB and a memory limit of 16 MiB, **When** processed line by line with a
   function that keeps only a running total, **Then** the program finishes within the limit.
7. **Given** a path outside the function's `file.read` scope, **When** read, **Then** the program
   stops with R0010 before anything is read, as `read_file` does; the passed function's own effects
   are the caller's to declare (E0501 otherwise).

---

### User Story 2 - Download something large (Priority: P2)

A developer downloads a file bigger than 16 MiB, or reads a response that arrives slowly, receiving
the body in pieces while it arrives and typically appending each to a file.

**Why this priority**: the 16 MiB cap makes vo unable to fetch a release archive or a dataset.

**Independent Test**: a local server sends 100 MiB (with a length, and again chunked); the program
appends the pieces to a file under `--max-memory 32`; the file's hash equals the server's.

**Acceptance Scenarios**:

1. **Given** a URL, **When** the program makes a streaming request, **Then** it first gets the status
   and headers, and the body is then delivered to its function in pieces, in order, with the transfer
   framing (length or chunked) removed; the call returns the status and headers when the body ends.
2. **Given** a body of 100 MiB, **When** streamed to a file, **Then** it arrives complete and the
   program's memory stays bounded.
3. **Given** a function that answers "stop" or returns an error, **When** it does, **Then** the
   connection is closed and the call returns (success with the head, or that error).
4. **Given** a connection that fails or goes silent mid-body, **When** the request's time limit for
   one wait passes, **Then** the call returns an error; the pieces already delivered stay delivered.
5. **Given** an `https://` URL, **When** streamed, **Then** verification is exactly that of
   `std.http.get`; a URL outside the `net` scope is R0010.
6. **Given** request options, **When** given, **Then** method, headers, a body and the time limit are
   those of `std.http.send`.

---

### User Story 3 - Follow a child program while it runs (Priority: P2)

A developer runs a long program (a build, a test suite, a converter) and handles its output line by
line as it is printed: showing progress, stopping it at the first error, or filtering a large output.

**Why this priority**: today a long child looks hung and a large output must fit in memory.

**Independent Test**: a child prints a line every 100 ms for a second; the parent's function is
called for each line within 200 ms of its printing; a function that answers "stop" after the third
line ends the child, and the call returns within a second.

**Acceptance Scenarios**:

1. **Given** a program under `process`, **When** run with a line function, **Then** each line of its
   standard output is delivered as it is produced, and the call returns the exit status and the
   standard error text when the program ends.
2. **Given** a function that answers "stop" or returns an error, **When** it does, **Then** the child
   is ended and reaped before the call returns.
3. **Given** a child that prints 200 MiB, **When** followed with a function that counts lines,
   **Then** the parent's memory stays bounded.
4. **Given** input text and environment entries, **When** given, **Then** they are passed as
   `run_with` passes them.
5. **Given** a program outside the `process` scope, **Then** R0010, as `run`.

---

### User Story 4 - Recorded and replayed (Priority: P2)

Every streaming read replays from a recording with no file, network or child, piece for piece.

**Independent Test**: record a run of each of the three stories' programs; remove the file, stop the
server, make the child program unavailable; `vo replay` gives the same output and exit status.

**Acceptance Scenarios**:

1. **Given** a recording, **When** replayed, **Then** each function call gets the piece it got when
   recorded; an early stop replays as an early stop.
2. **Given** a changed program that reads another file, connects to another host or port, or runs
   another program, **Then** R0014. (A streamed request is recorded as its connection; the path and
   the rest of what is sent are not compared, as for `std.tcp`.)
3. **Given** a large read, **When** recorded, **Then** the recording holds the pieces that were
   delivered (so it is as large as what was read, as for `read_file` today), and writing it does not
   hold them in memory.

---

### User Story 5 - Documented (Priority: P3)

`vo doc` documents each new function with a runnable example; the site's guide has a section on
reading large inputs; the reference names the functions within its budget.

**Independent Test**: `vo doc std.fs.each_line`; the site examples run; the reference stays under
15,000 tokens.

**Acceptance Scenarios**:

1. **Given** `vo doc std.fs.each_line` (and each of the other three), **When** run, **Then** it shows
   the signature, the three answers of the passed function and a runnable example.
2. **Given** the reference, **When** read, **Then** the four functions are named in their modules'
   rows with one sentence on the shape, and it is under 15,000 tokens.

### Edge Cases

- A file that grows or shrinks while it is read: what is there when each piece is read is delivered;
  no error.
- A file with a `\r` alone, or NUL bytes in a line: delivered as they are (a NUL is valid UTF-8).
- A chunk size of zero or below, or above 16 MiB: an error before anything is opened.
- A path that is not a regular file (a pipe, a FIFO, a terminal, `/dev/stdin`): an error saying that
  only regular files can be read in pieces; nothing is delivered. `read_file` still reads them.
- The passed function reads the same file, starts another stream, or calls `concurrent_map`: allowed;
  streams nest.
- The passed function stops the program (a runtime error, `exit`): the file, connection or child is
  released by the end of the process; a child program is ended, not left running.
- A stream inside a task: works; a task's copy of an open stream does not exist, because a stream is
  never a value.
- `--timeout` passing during a stream: R0009 as for any wait.
- An HTTP response with no body (204, a `HEAD` request): the function is never called.
- A redirect: returned as a status, not followed, as `send`.
- A child that closes its output but keeps running: the call waits for it to end, as `run` does.
- A child that prints nothing and does not end: the call waits, bounded only by `--timeout`, as `run`
  does; there is no time limit per line.
- A child's standard error larger than the existing output limit: an error, as `run`.
- Standard input: `read_line` already delivers one line at a time; nothing is added.
- Writing large outputs: `append_file` and `append_bytes` already write piece by piece.

## Requirements *(mandatory)*

### Functional Requirements

**Shape**

- **FR-001**: Each streaming read MUST be one library function that takes a function, calls it once
  per piece in order, and releases what it opened before it returns, on every path. No value
  representing an open file, response or child exists in the program.
- **FR-002**: The passed function MUST return `ok(true)` to go on, `ok(false)` to stop (the read then
  returns success), or an error, which stops the read and becomes its result. Its effects are an
  effect parameter of the read, so the caller declares them.
- **FR-003**: A streaming read MUST need the same effect and scope as its whole-value counterpart,
  checked the same way (E0501, E0503, R0010) before anything is read.

**Files**

- **FR-004**: `std.fs` MUST read a file line by line (text) and in chunks of a caller-given size
  (`bytes`), for files of any size, in memory bounded by the largest piece.
- **FR-005**: A line is delivered without `\n` or `\r\n`; one over 16 MiB or not valid UTF-8 ends
  the read with an error naming its number; a chunk size is 1 to 16 MiB.

**HTTP**

- **FR-006**: `std.http` MUST make a request (method, URL, headers, body, time limit as `send`) whose
  response body is delivered in pieces of at most 64 KiB as it arrives, with length and chunked
  framing removed, with no limit on the body's total size, returning the status and headers.
- **FR-007**: The time limit of a streaming request MUST bound each wait for the peer (the head, and
  each piece), not the whole transfer.
- **FR-008**: The response head MUST be available to the passed function (so it can ignore the body
  of an error status).

**Child programs**

- **FR-009**: `std.process` MUST run a program and deliver its standard output line by line while it
  runs, with input and environment as `run_with`, returning the exit status and standard error. Lines
  follow FR-005's rules.
- **FR-010**: When the passed function stops the read, the child MUST be ended and reaped before the
  call returns.

**Record and replay**

- **FR-011**: `--record` MUST save each delivered piece in order and how the read ended; `vo replay`
  MUST deliver the same pieces with no file, network or child, and stop with R0014 when the program
  asks for another source: another path or offset, another host or port, another program or
  arguments. Recording MUST NOT hold a whole stream in memory.

**Fit**

- **FR-012**: The whole-value functions (`read_file`, `read_bytes`, `get`, `send`, `run`, ...) MUST
  keep their behaviour and limits.
- **FR-013**: Three architectures, no libc, no static buffer over 64 KiB, in `vo run`, `vo test` and
  built programs.
- **FR-014**: `vo doc` documents every new function with a runnable example; the reference stays
  under 15,000 tokens; site, roadmap, changelog and review are updated.

### Key Entities

- **Piece**: one line (text) or one chunk (`bytes`) handed to the program's function.
- **Step answer**: go on, stop, or an error.
- **Response head**: status and headers of a streamed response.
- **Stream recording**: the source, the pieces in order, and the ending.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A 300 MiB file is summed line by line with a 16 MiB memory limit, in under 10 seconds
  on the development machine, with the right total.
- **SC-002**: A 100 MiB download (once with a length, once chunked, once over TLS) lands in a file
  with the right hash under a 32 MiB memory limit.
- **SC-003**: Each line a child prints reaches the parent's function within 200 ms of being printed,
  and a stopped child is gone when the call returns.
- **SC-004**: Every test program of the three stories replays with the same output with its file
  removed, its server stopped and its child program unavailable.
- **SC-005**: Peak live memory while streaming does not grow with the size of the source: the same
  within 1 MiB for a 10 MiB and a 300 MiB file.
- **SC-006**: The suite passes on the three architectures and the reference stays under 15,000 tokens.

## Assumptions

- **Callbacks, not handles**, as the review recommends: no new type, nothing to close, no `defer`.
- **The step answer is `result[bool]`**: `ok(true)` goes on, `ok(false)` stops. The review's sketch
  had `result[void]`, which cannot stop early without inventing an error; searching a large file for
  the first match needs to.
- **Four functions**: `std.fs.each_line`, `std.fs.each_chunk`, `std.http.send_stream`,
  `std.process.run_lines`, and nothing for standard input or for writing, which already work piece by
  piece. No streaming upload (a request body is still whole), no chunks of a child's output as bytes,
  no streaming of standard error: each is added when a program needs it.
- A streaming request's time limit is per wait; the whole transfer is bounded only by `--timeout`.
- Results the program wants to keep across calls of its function live in a record or list it
  captures, since those are shared references.
- Recordings of large streams are large; that is already true of `read_file`.
- The reference has about 100 tokens left: the new functions are named in the existing library rows
  in a few words, and something else is shortened if needed.
- Out of scope: iterator or handle types, `defer`, a streaming JSON parser, streaming SQLite rows,
  server-sent events and WebSockets, serving a streamed response, following redirects.

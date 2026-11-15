# Research: HTTP Server Support in `std.http`

## R1: The library surface

- **Decision:**
  ```vo
  export record Request { method: str; path: str; query: str; headers: list[Header]; body: str; }
  export fn serve[E: effects](address: str, handle: fn(Request) -> Response uses E) -> result[void] uses serve, E
  export fn request_header(r: Request, name: str) -> result[str]
  ```
  `Response` and `Header` are the client's existing records (status, headers, body). `serve` returns
  `ok()` when the server is stopped (R6) and `err` when it cannot start (address in use, permission, bad
  address). `path` is the target before `?`, `query` the part after it (no decoding); header names keep
  their case, `request_header` compares without case (like `header` for responses).
- **Rationale:** one function and one handler shape; reusing `Response` means a test compares responses
  with the same record a client receives. No routing framework (spec Assumptions).
- **Alternatives:** a handler returning `result[Response]` (an `err` would need its own status rule;
  failures are already covered by R4); a `Server` value with `next_request`/`reply` (exposes the loop
  and its timing rules to every program; rejected).

## R2: The `serve` effect and its address scope

- **Decision:** a ninth effect `serve` (bit 256, last in the canonical order). Its scope is the address
  text: `"8000"` (loopback, `127.0.0.1`) or `"0.0.0.0:8000"` (all interfaces), port 1–65535. Any other
  form is E0506 (bad scope) with the expected forms in the message. At run time `serve(address, ...)`
  needs the caller's grant to contain that exact address (or an unscoped `serve`), else R0010 before any
  socket is opened.
- **Rationale:** the address is written the same way in the call and in the `uses` clause, so review shows
  exactly what is exposed; loopback is the short form (clarified default).
- **Alternatives:** extending `net` with ports (mixes "may connect to" with "may accept from"; rejected);
  a port number scope plus a separate "public" flag (two places to read; rejected).

## R3: Where the protocol lives

- **Decision:** the loop, request parsing (request line, headers, `Content-Length` and chunked bodies) and
  response writing are vo in `std/http.vo`. Four internal builtins in C (`src/serve.c`):
  - `__serve_listen(address) -> result[int]`: socket, `SO_REUSEADDR`, bind, listen; blocks SIGINT/SIGTERM
    and opens a `signalfd` (R6).
  - `__serve_next(listener) -> result[str]`: waits (ppoll on the socket and the signalfd) for the next
    connection, reads the request head and body within the limits; `ok("")` means "stopped"; a request
    over a limit or malformed at the byte level is answered by C (`400`/`413`/`431`) and skipped.
  - `__serve_call(handle, request) -> result[Response]`: the guarded call (R4).
  - `__serve_reply(text)`: writes the response to the current connection and closes it.
- **Rationale:** the client already parses HTTP in vo (`parse_response`, `dechunk`); the server reuses
  those helpers, and C keeps only what needs system calls, limits and recovery.

## R4: A failing handler answers 500

- **Decision:** `__serve_call` saves the interpreter state (value stack pointer, current frame, GC root
  count, the recovery point of an enclosing test) and sets a recovery point (`jmp_save`, as the test
  runner does). A runtime error inside the handler renders its usual report to stderr (human or `--json`,
  through the same code as `rt_raise`, split into "render" and "exit"), restores the state and returns
  `err(code)`; `serve` then replies `500` with an empty body and goes on.
- **Rationale:** the clarified rule; the recovery machinery already exists for tests, so this adds a
  restore of the saved state rather than a new mechanism.
- **Risk:** state that a runtime error leaves half-updated (GC roots, trace output, open builtin loops).
  Covered by the ubtrap and gc-check builds running the server cases, and by a case that fails in a nested
  call and in a callback (`map` inside the handler).

## R5: Timeouts per request

- **Decision:** while `serve` waits for a request, no deadline applies; `__serve_call` sets
  `deadline = now + --timeout` for the handler and clears it afterwards; R0009 in a handler is a failing
  handler (R4: `500`). After `serve` returns, the deadline is `now + --timeout` again for the rest of
  `main`. Reading a request has its own fixed limit (10 s per connection) so a slow client cannot block
  the server.
- **Rationale:** the spec (FR-009): a server lives indefinitely, a request does not.

## R6: Stopping without signal handlers

- **Decision:** `__serve_listen` blocks SIGINT and SIGTERM (`rt_sigprocmask`) and opens a `signalfd`;
  `__serve_next` polls it together with the listening socket. A signal while waiting returns `ok("")`;
  a signal during a request is seen at the next wait, so the request in progress finishes. `serve` closes
  the socket and returns `ok()`; the signal mask is restored.
- **Rationale:** the toolchain has no signal handlers, and a handler would need an assembly restorer on
  x86-64 (platform-layer budget); `signalfd` needs neither.

## R7: Recording and replay

- **Decision:** with `--record`, events `listen` (address, result), `request` (the raw request bytes as
  read, or the C-answered status), `response` (the raw response bytes sent) and `stop`. In replay,
  `__serve_listen` returns the recorded result without a socket, `__serve_next` returns the next recorded
  request (or `ok("")` at `stop` or at the end of the recording), and `__serve_reply` compares the new
  response with the recorded one: each difference is a note on stderr (`--json`: a `response_changed`
  record with the request line, recorded and new status and body) and never changes the exit status.
  Effects inside the handler (file reads, HTTP requests) are recorded and replayed as in any run.
- **Rationale:** spec 006's guarantee (same inputs, same run); differences are what a developer replays
  for, so they are reported, not fatal.

## R8: Testing a server

- **Decision:** `tests/steps.py` gains `start vo ARGS...` (runs vo in the background with `{port}` a free
  port, waits until it accepts), `request METHOD PATH [BODY]` and `request-raw TEXT` (a raw HTTP client:
  the response bytes go to `out`), and `signal TERM|INT` (signals the started vo, waits, appends its
  stdout, stderr and exit status). An endurance script sends 10,000 requests (SC-003).
- **Rationale:** byte-exact responses through the existing step format; no new test runner.

## R9: Limits

- **Decision:** request head (line and headers) ≤ 64 KiB (`431`), body ≤ 16 MiB like client responses
  (`413`), at most 100 headers (`431`), 10 s to read a request (`408`). Responses get `Content-Length`
  and `Connection: close` added unless the handler set them; `HEAD` answers with the handler's response
  for `GET` without the body.

## R10: The status-board example

- **Decision:** `examples/status-board/service.vo` replaces the Python service (SC-005): it serves the two
  JSON files with `read_file` under `file.read("site/")`; the README and the demo recording follow.

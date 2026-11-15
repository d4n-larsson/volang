# Results: HTTP Server Support in `std.http`

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | 3 isolated agent runs (`claude -p`, no tools, empty directory; the reference plus `vo doc std.http` and `std.json`) on a JSON service with `/health`, `/greet?name=` and a 404, with tests: 3/3 check clean and pass their own 6 tests on the first try; run live, 4/4 routes answer correctly in each, and SIGTERM stops each with exit 0. |
| SC-002 | met | `tests/cases/serve/routes`, `bad_requests`, `handler_fails`, `listen_fails`, `scope_denied`, `all_interfaces`, `handler_timeout`, `long_life`, `record_replay`: byte-exact responses for routes, methods, query, headers, bodies (plain and chunked), `HEAD`, 400/413/431, a failing handler (500). |
| SC-003 | met | `tests/serve_endurance.py`: 10,000 requests in 1 s (memory ~8 MiB after warm-up, ~10 MiB at the end) and 30,000 in 2 s; `--minutes 10 --pause 20`: 28,821 requests over 600 s under the default `--timeout`, memory 7,976 → 10,080 KiB, every 100th request a failing handler (500, reported), clean SIGTERM stop with exit 0. |
| SC-004 | met | `serve/record_replay`: a recorded server run replays with identical output and no port; a changed handler gives one note per changed response (2 of 3 changed, 2 notes), `--json` gives `response_changed` records. |
| SC-005 | met | `examples/status-board/service/main.vo` replaces Python's `http.server`; the board and the service were recorded together, and both recordings replay in `make test`. |
| SC-006 | met | Reference ~10,576 → ~10,710 tokens (+134, limit 500); the full gate on all three platforms (below); existing programs give the same results (changed golden outputs differ only by `std/http.vo` line numbers, recording fingerprints and the module's one-line description). |

## US1: answering requests

- `std/http.vo`: `Request`, `request_header`, `serve[E: effects](address, handle)`; request parsing reuses
  the client's helpers (`crlf`, `dechunk`); responses always get the computed `Content-Length` and
  `Connection: close` (a handler's own values for those, or `Transfer-Encoding`, are dropped).
- `src/serve.c`: listen (IPv4, `SO_REUSEADDR`), next request (accept, read within 10 s, head ≤ 64 KiB and
  ≤ 100 headers, body ≤ 16 MiB, `Content-Length` or chunked framing), reply, and the guarded handler call.
  Requests over a limit or with broken framing are answered (400/408/413/431) without the handler; a
  connection closed before sending a byte is ignored (a harness's readiness probe, a port scanner).
- A failing handler: `rt_raise` renders the usual report to stderr, then jumps back to `__serve_call`,
  which puts the value stack, the current frame and the trace depth back (`jmp_save` restores the GC
  roots); `serve` answers 500. Covered for a failure in the handler, in a `map` callback, an index error,
  deep recursion and a timeout.

## US2: the serve effect

- A ninth effect `serve` (bit 256, last in the canonical order); scopes `"PORT"` (loopback) or
  `"0.0.0.0:PORT"`, anything else E0506 with the two forms. The address is checked against the grant at
  run time (R0010, naming the declaring function) before any socket opens.
- The builtin effect table went from bytes to 16-bit entries (nine effects).
- **A checker fix found here**: a generic standard-library function's own effects were required with the
  callee's exact (unscoped) scopes, so `uses serve("8000")` did not cover `std.http.serve`'s `uses serve`
  (E0503). Generic library calls now work like non-generic ones (spec 007): the library's own effects in
  any scope, its effect parameters as bound at the call.

## US3: handlers tested without a network

- `serve/handler_test.vo`: tests and a property build `Request` values and compare `Response`s, with no
  effect declared.

## US4: record and replay

- Events `listen` (address, result, the listener), `request` (raw bytes; `""` is the stop) and
  `response` (raw bytes). Replay opens no socket, feeds the recorded requests, stops at the empty request
  or the end of the recording, and compares each response: a note per difference (never an error). The
  handler's own effects (file reads, HTTP requests) are recorded between its request and response.

## US5: stopping and timeouts

- SIGINT/SIGTERM through a `signalfd` (signals blocked while serving, the mask restored after): no signal
  handlers, no assembly. The request in progress finishes; `serve` returns `ok()`; `main` goes on.
- No deadline while waiting for a request; `--timeout` per handler call (R0009 there answers 500); the
  program's deadline starts again when `serve` returns (`serve/handler_timeout`, `serve/long_life`).

## Test tooling

- `tests/steps.py`: `start vo ...`, `request`, `request-raw` (`{bytes:N}`), `wait MS`, `signal TERM|INT`;
  `{vport}` is a free port, also replaced in the case's files and masked in outputs.
- `tests/serve_endurance.py` (SC-003); `tests/doc_conformance.py` handles function types with `uses E`
  and leaves effect parameters out of generated `uses` clauses.
- `make test` checks, tests and replays nested example programs (`examples/*/*/main.vo`) too.

## Final gate

| Check | Result |
|---|---|
| `make gate` (x86-64) | passed: golden, step cases (with the server cases), ubtrap, diff-asm 2,000 (0 disagreements), gc-check, speed geometric mean 0.895 (limit 1.10) |
| `make ARCH=aarch64 test`, `make ARCH=riscv64 test` | passed |
| `make instrumented` | first run: 2 failures (`run/reentry_map`, `reentry_filter`: R0009 instead of R0005, see Deviations); after the fix: 483 of 483; the server and HTTP step cases under the instrumented build: 29 of 29 |
| `make diff-asm COUNT=10000` | 0 disagreements |
| `make stdlib-eval` | 11 of 11 |
| `make prop-eval` | 62 of 62 mutants caught |
| `make refactor-eval` | passed |

## Deviations

- **Requests answered without the handler are not recorded** (the plan said they would be, with their
  status): replaying them would not reach the program.
- **The stop is an empty `request` event**, not a separate `stop` kind.
- **`408` is not covered by a step case** (it needs a client that waits 10 s); the code path is the same
  deadline check as the others.
- **The listener number** is recorded (it shows in traces of a failing handler, which must replay
  identically).

- **The `run/reentry_*` cases timed out under the instrumented build**: they recurse through `map`,
  `filter`, ... until the stack is exhausted (R0005), which took 11.8 s under the sanitizer today, past
  the default 10 s. The commit before this spec takes the same 11.8 s (built and timed in a temporary
  worktree), so it is the machine, not this change. The six cases test depth, not time: they now run with
  `--timeout 60` (`.cmd` files); their outputs are unchanged.

## Follow-ups

- Concurrency (several requests at once) and keep-alive, when a use needs them.
- IPv6 listening.

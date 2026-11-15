# Implementation Plan: HTTP Server Support in `std.http`

**Branch**: `011-http-server` | **Date**: 2026-11-15 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/011-http-server/spec.md`

## Summary

`std.http` gains `serve(address, handle)`: a generic library function that answers HTTP/1.x requests one
at a time with a handler `fn(Request) -> Response uses E` (research R1). Serving is a new effect `serve`,
scoped by address: `"8000"` listens on loopback, `"0.0.0.0:8000"` on all interfaces (R2). The loop is vo
code in `std/http.vo` over four internal builtins in C: listen, wait for the next request (with the stop
signal), call the handler under a recovery point, reply (R3). A failing or timed-out handler answers `500`
and the error report goes to stderr; `--timeout` bounds each request (R4, R5). Ctrl-C/SIGTERM stop the
server cleanly through a `signalfd`, without signal handlers or assembly (R6). Recording saves each request
and response; replay feeds the requests to the handler without a port and reports changed responses (R7).
The step harness learns to start a vo server, send requests and signal it (R8).

## Technical Context

**Language/Version**: C (gnu11, freestanding; spec 008) for the toolchain; vo for `std/http.vo`.

**Primary Dependencies**: Linux system calls only (socket, bind, listen, accept4, setsockopt, ppoll,
rt_sigprocmask, signalfd4); GCC for x86-64, ARM64, RISC-V; qemu-user; Python 3 for tests.

**Storage**: recordings gain three event kinds (`listen`, `request`, `response`) and a `stop`.

**Testing**: golden cases (effects, typing, request parsing through handlers called directly), step cases
with a vo server started by the harness (`start`, `request`, `signal`), replay step cases, catalog and
reference checks, an endurance check (10,000 requests), the full gate on three platforms.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V (qemu).

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: none beyond SC-003 (10,000 sequential requests without growth); one request at a
time.

**Constraints**: no new assembly (signals through `signalfd`); Principle I (one function, one effect);
Principle III (every request recorded and replayable); the reference grows by at most 500 tokens.

**Scale/Scope**: about 500 lines of C, 150 of vo, 30 golden and step cases, the harness extension.

## Constitution Check

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** One function `serve(address, handle)`; the handler is an ordinary function value; routing is plain `match`/`if`; the address and its scope have one spelling each. |
| II. Machine-readable feedback | **PASS.** Missing or wrong effects use E0501/E0503/E0506/R0010 with their fixes; handler failures are the usual runtime reports (`--json` too); replay differences have a JSON record. Recording schema 1 gains event kinds only. |
| III. Determinism | **PASS.** Network input enters only through the internal builtins, every request is recorded, and replay reproduces the run; the handler itself is as deterministic as its effects. |
| IV. Test-first through the CLI | **PASS.** Cases first for every story; the harness extension is test tooling, not product. |
| V. Single source of truth, minimal surface | **PASS.** One effect, one function, two records (one reused); reference, `vo doc` and catalog in the same change. |
| Implementation constraints | **PASS.** C, zero warnings on three architectures, no assembly (no signal handlers: `signalfd`), syscall numbers in the per-platform headers. |

## Project Structure

### Documentation (this feature)

```text
specs/011-http-server/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/library.md     # serve, Request, the effect, limits, status rules
├── contracts/recording.md   # listen / request / response / stop events, replay comparison
├── checklists/requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
std/http.vo                  # Request, serve, request parsing, response writing, request_header
src/serve.c                  # new: listen, next request (accept, limits, signalfd), reply, guarded call
src/platform/{x86_64,aarch64,riscv64}.h   # syscall numbers
src/effects.c, src/check_effects.c        # the `serve` effect and its address scopes; builtin effects
src/check_expr.c, src/rt_call.c           # the four internal builtins
src/interp.c                 # report rendering without exit (shared with rt_raise); per-request deadline
src/replay.c                 # listen/request/response/stop events; response comparison
docs/reference.md, docs/errors.md (E0506 message), tests/steps.py (start/request/signal)
tests/cases/{http,effects,serve}/...
examples/status-board/        # the service as a vo program (SC-005)
```

**Structure Decision**: one new C file for the server primitives (`net.c` is the client); the protocol
logic stays in vo next to the client's, as in spec 007.

## Complexity Tracking

No violations to justify.

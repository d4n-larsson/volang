# Results: Web Package for Building APIs

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | notes-api rewritten with std.web: API code (no blank or comment lines, no tests, no helpers and imports only tests use) 247 → 139 lines, 43.7% fewer (raw count with test helpers: 254 → 153, 39.8%); all six requirements and their tests kept, plus a test per request through the app; the recorded session's answers are the same except error bodies (now problem+json) and JSON bodies without a final newline. |
| SC-002 | met | `examples/notes-api/api.vo` has no path splitting, method matching, 404/405 answers, query parsing or JSON field reading (std.json's readers appear only in tests). |
| SC-003 | met | `tests/web_fuzz.py`: 1,000 requests wrong by construction (unknown paths, other methods, bad path variables, bodies not JSON or of the wrong shape, missing and unknown fields, broken limits, bad query values, huge numbers, deep nesting, another content type) against a 10-operation app served with 4 workers: every answer the expected 4xx problem with a matching `status`, no operation ran, no 500; a positive control shows valid requests reach the operations. |
| SC-004 | pending | Not run (the user chose to run it later: about 15 minutes per run, 40 runs). The kit is ready: `bench/web/task.md` (an inventory API), `bench/web/accept.py` (29 checks; the std.web reference solution `bench/web/reference/` passes 29/29), `bench/web/run.py --arm web|http --runs N` (isolated `claude -p`, no tools, one attempt, 30-minute limit) and `--summary`. Two sanity runs of the `web` arm: one failed to check (`json_reply(200, x)`, `steps: []`, two ignored `Changes`), which led to `json_reply(status, v)` and `web_app`; one did not answer within 15 minutes. |
| SC-005 | met | `vo test examples/notes-api/main.vo`: 6 tests and a property through `handle_request` with an in-memory database, no `serve` effect: 0.01 s. |
| SC-006 | met | `tests/web_speed.py` (`make gate`): a 20-route app, std.web against the same API hand-written on std.http, one worker, ab, median of 5 interleaved runs: 94–99% of the hand-written throughput (single runs vary ±20% on this machine; JSON answers are rendered in C, `__json_text`). **November 2026:** the bound is now 80%, measured as the median of 9 paired rounds. The cost is a fixed 5 to 8 µs per request, which is 8% to 13% of a request on the development machine (87% to 92%) and 16% on a GitHub runner that serves four times as many requests a second (84%). |
| SC-007 | met | Every existing golden and step case passes unchanged on three platforms, except `doc/all` and `doc/all_json` (`vo doc` lists the new module std.web). The inference changes let more programs check and change no existing result. |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 901/901, steps 252/252, scripts (`web_fuzz.py --quick` included), examples |
| `make ARCH=aarch64 test`, `make ARCH=riscv64 test` | both passed under qemu, as x86-64 |
| `make lint` | clean |
| Instrumented build (ASan, UBSan) | golden suite 901/901; `web_steps` 3/3; `web_fuzz.py --quick` passes |
| Stress build | `json_derive`, `web`, `annotations`, `generic` 49/49; `web_steps` 3/3 |
| Reference | ~13,757 tokens of 15,000 (+344 for spec 029; budget 700) |

## Deviations

- **Steps take their effects as functions** (`request_ids(random_id)`, `log_requests(clock_ms,
  to_stderr)`) instead of using them themselves: a type's effect argument cannot combine a concrete
  effect with an effect parameter (`Step[C, uses random, E]`), and a step must have the application's
  `Step[C, E]` type. The effects then show where the app is assembled.
- **`health_route` and `info_route`** instead of `health` and `info`: `info` clashed with `std.fs.info`
  (one name per program).
- **`json_reply(status, v)`** instead of `json_reply(v)` and `json_status(status, v)`: the agent
  benchmark's sanity run wrote `json_reply(200, item)`; one function with the status written is the
  guessable form. **`web_app(routes)`** added for an application without steps or extra error kinds: the
  same run wrote `steps: [], errors: []`, which vo refuses (E0306).
- **Unmapped errors** are not logged by std.web itself (that would give every application the `console`
  effect): their text travels in a `Vo-Error` header of the 500 answer, which `log_requests` writes and
  `serve_app` removes before sending.
- **Route tables** are validated at start by `serve_app` only; `handle_request` does not check them on
  every request (tests rely on `vo check`'s E0330/E0331 for literal tables).
- **A client's `X-Identity` header** is dropped at `handle_request`'s entry, so only an auth step can set
  it.
- **Toolchain changes beyond the plan**, each found while building std.web and each with golden cases:
  a generic call's type parameters (not only its effects) come from the expected type first; a generic
  call or function value checked as a later argument of another generic call takes its type from that
  parameter (`handle_request(notes_app(), ...)`, `serve_app(..., notes_app())`); `==` gives a generic
  call the other side's type (`from_json(j) == ok(3)`); `try` passes its expected type to the call
  (`let s: Settings = try load_settings(...)`); a type's effect argument (`Route[uses file.read]`) allows
  any scope of the effect; `serve_call` nests (std.web inside a worker's handler call).
- **std.json's `Json`** converts as itself in `to_json`/`from_json` (a field of type `Json` holds any
  JSON), and recursive types are checked once each for E0328.
- **A bare `T`** bound to `json` gets the fix `: json` (E0328).

## Follow-ups

- The agent benchmark's sanity run also wrote `exec(...)` without using its `Changes` (E0405), a vo rule
  agents do not expect; `let _ = ...` is the fix the checker offers, which an agent without tools never
  sees.
- `vo check` cannot see route conflicts across tables built in code (checked when served).

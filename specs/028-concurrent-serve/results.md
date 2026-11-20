# Results: Concurrent Request Handling

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/serve_workers.py slow_vs_fast`: 4 workers, 3 two-second requests in progress, 50 fast requests one after another: the slowest answered in 1.4 ms (want ≤ 50); the same program with `serve`: the first fast request waits for the slow ones. Step case `serve_workers_steps/queue` shows the order: the waiting slow request gets the first free worker, the fast one the next. |
| SC-002 | met | `tests/serve_workers_speed.py` (`make gate`, 12 cores): a computing handler, 2,000 requests from 8 clients: 1 worker 272 req/s, 4 workers 1,066 req/s, 3.92×. |
| SC-003 | met | `tests/serve_workers.py isolation` (`--timeout 1 --max-memory 64`): among 20 requests in flight, the crashing (R0002), timing-out (R0009) and memory-exhausting (R0006) ones answer 500 with their reports on stderr; the other 17 and the next 100 are answered. A worker killed with SIGKILL while handling a request: that request gets 500, the note `worker N ended unexpectedly; it was replaced` appears, 4 workers again, 100 more requests answered. |
| SC-004 | met | `tests/serve_workers.py shutdown` and step case `shutdown`: 5 slow requests in progress (3 workers, 2 waiting), SIGTERM: all 5 answered 200, a new connection is refused, exit 0, no process left. A second SIGINT stops at once (exit 0); a request running past `--timeout` during shutdown gets 500 and shutdown completes in about 1 s. |
| SC-005 | met | `tests/serve_workers.py counter`: 1,000 concurrent read-modify-write transactions through 4 workers: counter 1,000, the database opened 4 times (once per worker). `notes_load`: notes-api under 1,000 concurrent POSTs, then `GET /notes`: exactly 1,000 different notes. |
| SC-006 | met | `tests/serve_workers.py replay`: a recorded 200-request session (150 inserts, 50 counts) against 4 workers, the database deleted, replayed 20 times: identical, exit 0, no output, `data/` not recreated. Step case `serve_workers_steps/replay`: two replays give the same output; a changed setup is R0014 naming the database call; `vo replay --workers 2` is T0001. |
| SC-007 | met | Every `tests/cases/serve*`, `https*` and `replay` case passes unchanged; `serve`'s code in `std/http.vo` is untouched (its lines and error traces stay as they were). Goldens that changed, none in behavior: the unknown-option message lists `--workers N` (`cli/bad_flag`, `https/no_insecure_option`); the recording fingerprint of programs importing `std.http` (it hashes every loaded file, `std/http.vo` included: `http/record_replay`, `https_steps/record_replay`, `tasks_steps/record_replay`). |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 876/876, steps 249/249 (8 `serve_workers_steps`), scripts (`serve_workers.py --quick` included), examples, diff_engines |
| `make ARCH=aarch64 test`, `make ARCH=riscv64 test` | both passed under qemu: golden 876/876, steps 249/249, scripts, examples, diff_engines |
| `make lint` | clean (C for three architectures, SESI, vendored code) |
| Instrumented build (ASan, UBSan) | golden suite 876/876 (after accepting the two option-list goldens); server step cases 23/23; `serve_workers.py --quick` passes |
| Stress build (collector every allocation) | `serve_workers_steps` 8/8; `serve_workers.py --quick` passes |
| `tests/serve_workers.py` (every check) | 8/8 checks (smoke, slow_vs_fast, isolation, shutdown, counter, replacement_setup, replay, notes_load) |
| Reference | ~13,375 tokens of 15,000 (+119 for spec 028; budget 150) |

## Deviations

- T005: the reusable loop is `sv_take_ready`, `sv_round` (one poll round over the signalfd, listener,
  connections and extra file descriptors), `sv_reply_to`, `sv_pending`, `sv_drop_partial`, `sv_end`,
  `sv_child_close` rather than the names in the task.
- T007: `__serve_workers(workers, setup, answer)` takes three arguments (builtins take at most three): the
  listener is the one the wrapper opened just before with `__serve_listen` (kept in `sv_listener`).
- T008: `serve_workers`'s request parsing (`answer_text`, `answer_bytes`) repeats `serve`'s closure
  instead of `serve` calling it, and sits at the end of `std/http.vo`: sharing it would add a frame to
  every handler error trace of `serve` and moving lines would shift their locations (SC-007).
- std.sqlite (found by the `counter` check): `transaction` on a writable database now begins with
  `BEGIN IMMEDIATE`. With a deferred `BEGIN`, a transaction that reads and then writes fails at once with
  `busy` when another process holds the write lock (SQLite does not call the busy handler for that lock
  upgrade): 240 of 1,000 counter requests failed through 4 workers. A read-only database keeps `BEGIN`.
- R7: a worker server writes each request's output as the request ends (flushed then); `serve` keeps
  buffering its output as before.
- R9: a block more than 4,096 requests ahead of the oldest unfinished one is written at once, out of
  arrival order (ponytail: a bounded ring).

## Follow-ups (done after the first commit)

- **Generic functions as values.** A generic function named where a function type is expected takes its
  type and effect arguments from that type: notes-api passes `handle[D: effects](db: Database[D], ...)`
  straight to `serve_workers` (the voledger agent's `Database[D]` problem). Its parameters bind first,
  the expected result fills in the rest; as an argument of a generic call it is checked after the other
  arguments; at run time it is a closure carrying its type arguments, its effect parameters standing for
  the caller's grant as in a generic call. With nothing to fix them it is E0310 ("give the variable a
  type") instead of a confusing E0301 at the call. Cases `generic/fn_value`, `generic/fn_value_unbound`.
- **Throughput ceiling.** The "few thousand requests per second" first noted here was the Python test
  client. Measured with `ab` (20,000 requests, 16 at once, a trivial handler): `serve` 28,718 req/s,
  `serve_workers` with 1 worker 21,979, with 4 workers 39,026. Nothing to change.
- **A worker killed mid-request.** Its block now ends with `{"kind":"worker_ended"}` before the 500
  response, and replay passes that request over instead of running its handler on inputs that were never
  recorded (`serve_workers.py killed_replay`).
- **Tasks in a handler (R10).** `concurrent_map` inside a worker's handler now runs its tasks at once, as in
  a program; tasks inside tasks still run one at a time. `serve_workers.py tasks_in_handler`: 4 tasks of
  300 ms answer in about 0.3 s, output in task order, the recording replays the same.

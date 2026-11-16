---

description: "Task list for vo-lang spec 016: concurrency"
---

# Tasks: Concurrency

**Input**: Design documents from `specs/016-concurrency/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R16), data-model.md, contracts/language.md,
contracts/codes.md, contracts/output.md, quickstart.md

**Tests**: REQUIRED (constitution IV): golden and step cases land first and fail before each piece.

**Organization**: setup, the task runtime every task story needs, then one phase per user story (US1-US4
are P1, US5 is P2), then the gate. US2 (the server) touches only `src/serve.c` and can run in parallel
with the task stories after Setup.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- Golden cases: `tests/cases/tasks/` (single-command cases: `.vo` + `.out`/`.code`, as `tests/run.sh`
  expects), step cases `tests/cases/tasks_steps/<name>/steps` (server, record/replay, timing-sensitive
  runs; format in `tests/steps.py`) and `tests/cases/serve_concurrent/<name>/steps`.
- **Regression rule:** programs that call neither `concurrent_map` nor `serve` keep byte-identical
  results; `make test` after every task.
- Every new C line: zero warnings under the strict set, no UB (explicit length checks on every pipe
  read), syscalls only through `vo_syscall*` (constitution, Implementation Constraints).
- Shortcuts are marked in code with `ponytail:` comments naming the ceiling (plan.md, Complexity
  Tracking).

---

## Phase 1: Setup

- [X] T001 Create `specs/016-concurrency/results.md` (SC-001 to SC-007 table, verification table,
  deviations); add `016-concurrency` to the spec list in `tests/catalog.py` (line ~87) so
  `contracts/codes.md` is checked; create `tests/cases/tasks/`, `tests/cases/tasks_steps/`,
  `tests/cases/serve_concurrent/`.
- [X] T002 [P] Add the route `GET /delay/MS` to `tests/http_server.py` (answers `200`, body `"delay MS\n"`,
  after MS milliseconds; MS 0..60000, else 400) and document it in the module docstring's route list; an
  optional environment variable `VO_TEST_JITTER_MS` adds a random 0..N ms to every delay (used by
  `tests/tasks_determinism.py`, T015).
- [X] T003 [P] System calls (research R16): define `SYS_clone`, `SYS_wait4`, `SYS_pipe2`, `SYS_dup3`,
  `SYS_fcntl`, `SYS_prctl`, `SYS_kill`, `SYS_getppid` in `src/platform/x86_64.h`, `src/platform/aarch64.h`,
  `src/platform/riscv64.h` (and `src/platform/hosted.c` if it maps them); add thin wrappers in `src/sys.c`
  with prototypes in `src/vo.h`: `long sys_fork(void)` (`clone(SIGCHLD, 0, 0, 0, 0)`: the zero pointer
  arguments make the x86-64 vs ARM64/RISC-V argument order irrelevant), `long sys_pipe(int fds[2])`
  (`pipe2` with `O_CLOEXEC`), `sys_dup_to(old, new)`, `sys_pipe_size(fd, bytes)` (`F_SETPIPE_SZ` = 1031),
  `sys_wait(pid, &status)`, `sys_kill(pid, sig)`, `sys_die_with_parent(void)` (`PR_SET_PDEATHSIG` = 1 with
  SIGKILL, then `getppid()` check: exit at once if the parent is already gone).

---

## Phase 2: Foundational — the task runtime (blocks US1, US3, US4, US5)

**Purpose**: `concurrent_map` exists, runs each task in a child process, returns results in order and
re-raises a single task's runtime error (research R1, R2, R4, R5, R6 basic path).

- [X] T004 [P] Foundation cases in `tests/cases/tasks/`, confirmed failing:
  `basic.vo` (`concurrent_map([1, 2, 3], 2, double)` prints `[2, 4, 6]`); `empty.vo` (empty list → empty
  result, no task); `one.vo` (one element); `limit_zero.vo` and `limit_65.vo` (R0007, exit 70, span on the
  call); `wrong_args.vo` (wrong argument count E0302; a non-function third argument and a non-int limit:
  the same type errors `map` gives); `generic.vo` (called inside a generic function with `T` and `U` type
  parameters); `types.vo` (results of every transferable type: int, float incl. -0.0 and inf, bool, str
  with non-ASCII bytes, nested lists, maps in insertion order, records, generic records, variants,
  `result`, `void`-returning `f`, an `ordered_map`, `ordered_set`, `priority_queue` with ties, `deque`);
  `shared.vo` (a result list holding the same record twice: `set` on one changes the other, as in the
  child); `cycle.vo` (a record whose field points back to itself survives the transfer); `div_zero.vo`
  (one task divides by zero: R0002 report identical to `map`'s for the same program, with the trace from
  `main` through the `concurrent_map` call; exit 70).
- [X] T005 Checker and dispatch (research R2, R15): add `concurrent_map` to `builtin_names` and
  `builtin_sigs` in `src/check_expr.c` (`B_COUNT` 61 → 62), its id to `B_LAST` in `src/vo.h`, `BU_CONCURRENT_MAP`
  to the enum and `_Static_assert` in `src/rt_call.c`, `builtin_effects` 0 in `src/check_effects.c`; type
  it as `(xs: list[T], limit: int, f: fn(T) -> U) -> list[U]` following `map`'s path in `check_builtin`
  (`src/check_expr.c`), and make `f`'s effects the caller's exactly as `map` does (the value-call path in
  `src/check_effects.c`, `fx_value_call`). Dispatch `BU_CONCURRENT_MAP` to
  `task_map((List *)a0, a1, (void *)a2, call)` in `src/rt_call.c`. Confirm `tests/cases/tasks/wrong_args.vo`
  passes and `vo doc concurrent_map` (if builtins are documented by `src/doc.c`) shows the signature.
- [X] T006 Task scheduler in new `src/task.c` (data-model.md "Task group"; research R5): `List
  *task_map(List *xs, long limit, void *fn, Node *call)`. R0007 when `limit < 1 || limit > 64`, before any
  task starts. Root `xs`, `fn` and the result list. Before each fork flush the parent's buffered stdout and
  stderr (`src/rt_io.c` FBufs), trace writer and recording writer. Per task: three pipes (stdout, stderr,
  control) with `sys_pipe`, `sys_pipe_size(.., 1 MiB)`, `sys_fork`; in the child: `sys_die_with_parent`,
  `dup_to` the stdout/stderr pipes onto fds 1 and 2, keep the control fd in a global `task_ctl`,
  `task_depth++`, close every other task's pipe ends, call `call_value(fn, call, &xs->data[i])`, send the
  `result` message (T007), flush and `exit_group(0)`. In the parent: keep `slots[64]`; start tasks while
  fewer than `limit` are running; one `ppoll` over the head's three pipes (POLLIN) and every running
  child's control pipe (events 0: POLLHUP means the child exited, research R5 "Slots"); copy the head's
  stdout/stderr through to fds 1/2 as they arrive; read the head's control messages; when all three of the
  head's pipes reach end of file, `sys_wait` it, store its result at its position, advance `head`; reap
  non-head children on POLLHUP (slot freed, status kept). Bound every wait by `deadline_ms` (R0009 via
  `rt_timeout`, after SIGKILLing and reaping all children). Return when every task is done. Add `src/task.c`
  to the build (`Makefile` source list). The invariants of data-model.md ("At most `limit` tasks are
  `running`", "Output is copied only from the head", "The call returns only when every started task is
  `done` or `killed`") as comments at the code that keeps them.
- [X] T007 Result transfer in `src/task.c` (data-model.md "Transfer form"; research R4): `task_send(W
  value, Type *t)` in the child and `W task_recv(Type *t)` in the parent, driven by the concrete static
  result type (`rt_concrete` with the calling frame's type arguments, as `src/prop.c` does). Encoding per
  the data-model table; reference types get a back-reference index on second visit (a small open-addressing
  table keyed by address in the child, an index → object vector in the parent; the parent allocates each
  object before decoding its contents so cycles resolve); collections are sent as their items in
  iteration order (priority queue: pop order on a copy, so ties keep their order) and rebuilt with the
  built-in order; a collection whose `cmp` is set raises R0015 in the child (catalog entry in T024).
  Messages on the control pipe are `kind byte, length word, payload`; every parent read checks the length
  against what remains (a truncated or oversized message is T0003, never a read past the buffer).
- [X] T008 Child exit path and single-failure re-raise (research R6, data-model "Control messages"): in
  the child, set a recovery point at task start (`jmp_save`); `rt_raise` there, and `rt_exit`/`vo_exit`
  when `task_depth > 0`, send an `error` message (code string, message bytes, node pointer, `rt_err_extra`,
  `rt_err_trace_txt`, `rt_err_trace_json`) or an `exit` message (code), then `exit_group(1)` without
  writing the trace end, the recording end or `--mem-stats` (`src/interp.c`, `src/main.c`). In the parent
  add `rt_reraise(...)` (`src/vm.c` or `src/interp.c`, next to `rt_raise`) that sets the same `rt_err_*`
  globals from the message and takes the normal report path, so the report is byte-identical to the one
  `map` gives (`tests/cases/tasks/div_zero.vo`). An `exit` message makes the parent call `rt_exit` with the
  code. `clone`/`pipe2` failure in the parent: T0003 report, exit 125.

**Checkpoint**: `tests/cases/tasks/` foundation cases pass; `make test` otherwise unchanged.

---

## Phase 3: User Story 1 - Fetch many things at once (Priority: P1) 🎯 MVP

**Goal**: concurrent fetches finish in about the time of the slowest one, with results and output in
input order, byte-identical across runs, recorded and replayed exactly.

**Independent Test**: the step cases below and `tests/tasks_determinism.py`: ten 1 s requests under 3 s,
output identical across 100 runs with random server delays, replay identical without a server.

### Tests for User Story 1

- [X] T009 [P] [US1] Step cases in `tests/cases/tasks_steps/`, confirmed failing: `fetch_order/` (`server
  http`; ten `get`s of `/delay/{100*(10-i)}` so later inputs answer first; output lines in input order);
  `print_order/` (each task prints three lines and `eprint`s one: stdout is task 0's lines, then task 1's,
  ...; stderr likewise); `grant/` (`uses net("127.0.0.1")`, one task asks for `localhost.invalid`: R0010
  with the same extra fields as the sequential program); `limit/` (limit 3 over ten `/delay/300` requests:
  same output; the server log or timing shows at most 3 outstanding — use a `/delay` counter route or
  check the run takes ≥ 1.2 s); `record_replay/` (`vo run --record`, `stop-server`, `vo replay`: same
  stdout and exit status; the recording's events are in input order); `json/` (`--json` run with a task
  error: the `runtime_error` record identical to `map`'s).
- [X] T010 [P] [US1] Cases in `tests/cases/tasks/`, confirmed failing: `trace.cmd` (`--trace` over three
  tasks: a `{"v":1,"kind":"task","index":I}` record before each task's `call`/`return` records, depth
  continuing from the call; `end` counts all); `trace_limit.cmd` (`--trace-limit 5`: the first five
  records in merged order, `omitted` counts the rest — byte-identical across runs); `seed.cmd` (each task
  calls `random_int(1, 1000000)` three times: tasks get different numbers; the same output with the same
  `--seed`, different with another seed; the caller's own `random_int` after the call gives the same
  value as without the call); `clock.cmd` (`--clock-ms` in tasks behaves as inherited).

### Implementation for User Story 1

- [X] T011 [US1] Recording (research R10) in `src/replay.c`: when `task_depth > 0` and recording, the
  event writer sends `record` messages (already formatted lines) on `task_ctl` instead of writing
  `rcd_fb`; the parent, when draining the head, writes them to its recording in order. The child never
  writes the recording header or end.
- [X] T012 [US1] Replay (research R10) in `src/task.c` and `src/replay.c`: with `rcd_mode == 2`, run tasks
  with limit 1; each child starts at the parent's `rp_next` and sends a `replay_pos` message with its final
  index before its result; the parent sets `rp_next` from it. R0014 and change notes unchanged. Add a
  recording made with tasks on x86-64 to `tests/cross_replay.py` so it replays on ARM64 and RISC-V.
- [X] T013 [US1] Traces (research R11) in `src/vm.c`: in a task the trace writer sends `trace` messages
  on `task_ctl` (at most `--trace-limit` records, then a final count of the records it omitted); the
  parent writes `{"v":1,"kind":"task","index":I}` before each task's records and applies the limit in
  merged order, so `tr_written`/`tr_omitted` and the `end` record are deterministic. Document the record
  in `contracts/output.md` (already drafted) and the trace section of `docs/reference.md` (T028).
- [X] T014 [US1] Per-task random sequence (research R12) in `src/rt_effects.c`: in the child, reseed the
  generator from the run's seed mixed with the task's position path (parent path × 65 + position + 1, for
  nesting) through the same mixing function the generator already uses; the parent's generator state is
  untouched by the call.
- [X] T015 [US1] `tests/tasks_determinism.py`: runs every case under `tests/cases/tasks/` and
  `tests/cases/tasks_steps/` N times (`--runs`, default 100) with `VO_TEST_JITTER_MS=300` on the server,
  and reports per case `k/N identical` (SC-003); `--timing` runs quickstart.md scenario 1 (ten `/delay/1000`
  requests, limit 10 vs `map`) and prints both wall times (SC-001: concurrent under 3 s, sequential at
  least 10 s). Hook it into `make gate` with `--runs 20`.

**Checkpoint**: US1 is the MVP: fetches run concurrently, deterministically, with replay.

---

## Phase 4: User Story 2 - A slow client does not stall the server (Priority: P1)

**Goal**: `std.http.serve` reads many connections at once; handler calls stay one at a time in the order
requests become complete (research R14, data-model "Server connection").

**Independent Test**: step cases in `tests/cases/serve_concurrent/`; no task runtime needed.

### Tests for User Story 2

- [X] T016 [P] [US2] Step cases in `tests/cases/serve_concurrent/`, confirmed failing (format of the spec
  011 serve cases: `start vo`, `request`, `request-raw`, `wait`, `signal`): `slow_client/` (one
  `request-raw` with half a request head, then five complete `request`s, each answered — add a
  `request-open TEXT` step to `tests/steps.py` that sends TEXT and keeps the connection open without
  waiting for a reply, closed at the end of the case); `stalled_408/` (the half request gets 408 after
  10 s, and requests sent meanwhile were answered); `counter/` (a handler incrementing a field of a
  captured record answers 1, 2, 3 ... for sequential requests, and for 20 requests sent at once answers
  each number 1..20 exactly once); `refusals/` (400, 413, 431 still answered while another connection is
  slow); `replay/` (a recording of a concurrent session replays with the responses in handling order,
  opening no port); `slow_reader/` (a client that does not read its response does not delay the next
  handler call).
- [X] T017 [P] [US2] `tests/serve_endurance.py`: add a mode `--stalled` that holds one half-sent request
  open and sends 100 requests, asserting each is answered within 1 s (SC-002); record the figures in
  `results.md`.

### Implementation for User Story 2

- [X] T018 [US2] Connection loop in `src/serve.c` (research R14): replace the accept-then-`sv_read`
  sequence of `rt_serve_next` with a loop over the listener, `sv_sig` and up to 64 connections (a static
  array of the data-model's connection records: `fd`, read buffer `SB`, `HttpFrame`, `deadline` = accept
  time + `READ_MS`, `out`/`sent`, state `reading`/`ready`/`handling`/`writing`); one `ppoll` with the
  nearest deadline as timeout; accept while fewer than 64 are open; on readable, `recvfrom` and run
  `http_frame` exactly as `sv_read` does today (same limits, same 400/408/413/431 decisions, same
  "closed without sending anything" rule); a refusal queues its response as `writing`; return the
  `ready` connection that became ready first (keep a ready queue in completion order) and mark it
  `handling` in `sv_conn`. Keep every buffer reachable for the collector (root the connection array's
  `SB`s/`Str`s, or allocate them outside the collected heap). The stop signal still stops the server
  after closing all connections.
- [X] T019 [US2] Queued replies in `src/serve.c`: `rt_serve_reply` stores the text in the handling
  connection's `out` and sets it `writing` instead of calling `sv_send`; the loop of T018 writes pending
  `out` on POLLOUT with `MSG_NOSIGNAL`, closes a connection when done, when the client goes away, or 10 s
  after its reply was queued. Update the file's top comment (the `ponytail:` line: concurrency now in
  connections, handlers one at a time; 64 connections) and `std/http.vo` `serve`'s doc comment. Confirm
  `src/replay.c` `io_serve_next`/`io_serve_reply` record and replay unchanged (T016 `replay/`).

**Checkpoint**: US2 passes on its own; the spec 011 and 013 serve cases are unchanged.

---

## Phase 5: User Story 3 - Failure and timeout have one place to go (Priority: P1)

**Goal**: the lowest failing position decides; other tasks are stopped; timeouts, `exit` and test
failures go through the call; no task outlives it (research R6, R7, R9).

**Independent Test**: the cases below; `ps` shows no leftover `vo` process after each.

### Tests for User Story 3

- [X] T020 [P] [US3] Cases in `tests/cases/tasks/` and `tests/cases/tasks_steps/`, confirmed failing:
  `two_failures.vo` (tasks 1 and 3 fail with different codes, task 3 fails first in time — it sleeps less
  via `/delay` in a step case — the report is task 1's every run); `fail_output.vo` (output of tasks 0..k,
  then the report; nothing from tasks above k); `err_result.vo` (a task returning `err` is not a failure:
  the caller gets every `result` in order); `exit_code.vo` (a task calling `exit(3)` → exit 3, output
  before it kept); `timeout/` step case (`--timeout 2`, one task fetching `/slow`: R0009, exit 124, in
  about 2 s); `in_test.vo` via `vo test` (a test whose task divides by zero is `error` with the same
  failure record as `map`'s; the next test still runs); `in_handler/` serve step case (a handler whose
  task fails answers 500 and the server goes on); `contracts.vo` (a task function's `requires` fails:
  R0012 as with `map`); `hole.vo` (R0011 in a task); `nested.vo` (a task calls `concurrent_map` with limit
  8: results correct, tasks inside run one at a time — check with a `/delay` timing step case
  `nested_seq/`); `no_leftovers/` step case (`signal INT` while ten tasks wait on `/slow`: exit as today,
  then a step that checks no child `vo` process remains — add a `no-children` step to `tests/steps.py`
  that fails if any process whose parent was the started `vo` is still alive).

### Implementation for User Story 3

- [X] T021 [US3] Lowest-position rule in `src/task.c` (research R6, data-model invariants): track
  `failed` (lowest failing position: a non-zero wait status of a reaped child, or an `error`/`exit`
  message from the head); when it is set, SIGKILL and reap every running task above it, start no more
  tasks, keep draining tasks below it (a lower failure replaces it); finally drain the failing task's
  output and re-raise (T008). Discard the pipes of killed tasks unread.
- [X] T022 [US3] Timeouts, tests and handlers (research R9): the child inherits `deadline_ms`; R0009 in a
  child is sent as an `error` and re-raised as R0009 (exit 124); the parent's `ppoll` timeout is the time
  left to `deadline_ms`, and on expiry it kills and reaps all children, then `rt_timeout`. In `vo test` and
  inside `rt_serve_call`, the re-raise goes through `recover_point` like any runtime error, so
  `src/testrun.c` and `src/serve.c` need no change beyond what T008 did; verify with `in_test.vo` and
  `in_handler/`.
- [X] T023 [US3] Nesting (research R7) in `src/task.c`: when `task_depth > 0`, use limit 1 (each nested
  task still forks); comment `ponytail: nested calls run one task at a time; a jobserver pipe inherited by
  children if nested concurrency is needed`.

**Checkpoint**: every failure kind ends at the call with the sequential report; no leftover processes.

---

## Phase 6: User Story 4 - No shared mutable state by construction (Priority: P1)

**Goal**: tasks work on copies; only results come back; what cannot cross is a clear error (research R3,
R4, R8).

**Independent Test**: the cases below and `tests/tasks_props.py` (`concurrent_map == map`).

### Tests for User Story 4

- [X] T024 [P] [US4] Cases in `tests/cases/tasks/`, confirmed failing: `copies.vo` (a task appends to its
  list argument, changes a field of a captured record, inserts into a captured map and an `ordered_map`;
  the caller prints all four afterwards: unchanged); `result_function.vo` (`f` returns a record with a
  function field: E0314 naming `field on_done of Job`; also a `list[fn(int) -> int]` result: E0314 naming
  `list element`); `result_order.vo` (a task returns an `ordered_map_by(cmp)` collection: R0015 with the
  catalog message); `reads_input.vo` (`f` calls `read_line`: E0511 with the catalog message);
  `explain` cases for E0314, E0511, R0015 (`vo explain CODE`). Add the three entries to `docs/errors.md`
  (text, `Fixable: no`, example) as in `contracts/codes.md`.
- [X] T025 [P] [US4] `tests/tasks_props.py` and `tests/cases/tasks/props_equal_map.vo`: vo `prop` tests
  that `concurrent_map(xs, k, f) == map(xs, f)` for generated `xs: list[int]`, `list[str]`, a list of
  records and `k` in 1..64 (`assume`), with `f` pure and with `f` changing its argument (the caller's `xs`
  equal before and after); run under 10 seeds, at least 1,000 generated cases in total (SC-004).

### Implementation for User Story 4

- [X] T026 [US4] E0314 in `src/check_expr.c` (research R4): after typing `concurrent_map`, walk `U`
  (record fields, variant cases, list/map/result elements, collection elements; generic records through
  their instantiated field types; type parameters are allowed and checked at their instantiation, so
  also check `U` at generic resolution if `U` is a parameter) and report E0314 at the third argument with
  the path of the first function type found, e.g. "a task's result cannot hold a function (field
  `on_done` of `Job`)". Reuse the recursive function-type search used for E0602 (`ty_has_func` in
  `src/check_generic.c`) extended to return the path.
- [X] T027 [US4] E0511 in `src/check_effects.c` (research R8): when the effect set of `concurrent_map`'s
  function argument contains `input`, report E0511 at that argument with the catalog message. R0015 is
  raised by T007's encoder; check the message matches `contracts/codes.md`.

**Checkpoint**: isolation and the transfer limits are tested and documented.

---

## Phase 7: User Story 5 - Review sees what concurrent code may do (Priority: P2)

**Goal**: making code concurrent changes no line of `vo.effects` (SC-005).

**Independent Test**: the example and cases below.

- [X] T028 [P] [US5] `examples/url_check/` (README, `main.vo` sequential with `map`, `main_concurrent.vo`
  differing in one line, `vo.effects`); step case `tests/cases/tasks_steps/effects_same/`: `vo effects
  --update main.vo`, then `vo check main_concurrent.vo` against the same record (no E0504, no W0502), and
  the two effects listings compared equal; case `tests/cases/tasks/missing_effect.vo` (task function
  prints, caller lacks `console`: E0501 whose fix adds exactly `console`, as for `map`);
  `tests/cases/tasks/scope.vo` (a scope too narrow for the task function: E0503 as for `map`).

---

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T029 Docs: `docs/reference.md` — the `concurrent_map` row in the "Functions as values" table, the
  **Tasks** paragraph, the `serve` bullet, the `task` trace record in "Traces and replay", `--mem-stats`
  reports the program without its tasks — from `contracts/language.md` and `contracts/output.md`; keep the
  growth at most 400 tokens (`tests/ref_examples.py` prints the total; state the growth in `results.md`);
  the example in `contracts/language.md` becomes a checked reference example. `docs/roadmap.md` (016
  status, the "Next: concurrency" section rewritten as done with what is not done: parallel computation
  on purpose, nested concurrency, closures in results); `README.md` if it lists features; `contracts/`
  final (any deviation found during implementation recorded in `results.md`).
- [X] T030 [P] SC-006 and fork cost: `tests/tasks_scale.py` runs 10,000 trivial tasks at limit 10 and
  1,000 at limit 64 under the default `--max-memory`, checks they complete and no file descriptors leak
  (`/proc/self/fd` count of the `vo` process before and after, via a long-running program), and prints
  the time per task; record natively on x86-64 and under qemu in `results.md`. Run `tests/speed.py` to
  confirm the speed gate is under 1.10 for programs without tasks.
- [X] T031 Gate: `make test`, `make gate`, `make ARCH=aarch64 test`, `make ARCH=riscv64 test`, `make
  instrumented` (the suite under ASan/UBSan including every task case), the fuzzer and determinism check,
  `tests/catalog.py` (E0314, E0511, R0015 each have a golden case and entry), `tests/cross_replay.py`,
  `tests/tasks_determinism.py --runs 100`, `tests/tasks_props.py`; fill `results.md` (SC-001 to SC-007,
  behavior changes, deviations).

---

## Dependencies & Execution Order

- Setup (T001-T003) → Foundational (T004-T008) → US1 (T009-T015) → US3 (T020-T023) → US4 (T024-T027) →
  US5 (T028) → Polish (T029-T031).
- **US2 (T016-T019) depends only on Setup**: `src/serve.c` and `tests/steps.py`; run it in parallel with
  the task stories. T020's `in_handler/` case needs both US2-independent serve behavior and T008.
- US3 and US4 both build on the foundation, not on US1; they can follow T008 directly, but US1's
  recording/trace hooks (T011, T013) touch the same child exit path as T008 and T021, so do them in the
  listed order when one person implements.
- Within each story: cases first (failing), then the implementation.

## Parallel Examples

- After Setup: T004 (foundation cases) and T016/T017 (server cases) together; T018-T019 alongside
  T005-T008.
- After the foundation: the case tasks T009, T010, T020, T024, T025, T028 can be written together.
- Polish: T030 alongside T029.

## Implementation Strategy

1. **MVP**: Setup + Foundation + US1 — `concurrent_map` with ordered output, determinism, record/replay
   and traces. Demo: quickstart.md scenarios 1-3.
2. **Server**: US2 (independent; can land first if the stall matters more).
3. **Safety**: US3 (failure rule, timeouts, nesting, no leftovers) and US4 (copies, E0314, E0511,
   R0015).
4. **Review and docs**: US5, the reference text within budget, the gate on three platforms.

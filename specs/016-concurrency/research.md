# Research: Concurrency (spec 016)

Each decision lists what was chosen, why, and what else was considered. Facts about the current
toolchain are from the sources named; line counts are as of commit ff3cc4f.

## R1: How tasks run — one child process per task

**Decision**: each task is a child process created with `clone(SIGCHLD)` (a plain fork, no shared
memory) at the moment the task starts. The child calls the function on its element and sends the result
back over a pipe; the parent waits for all children of the call. Every process stays single-threaded.

**Rationale**:
- The runtime places everything at fixed addresses (`src/vo.h`: heap `ADDR_HEAP`, value stack
  `ADDR_VSTACK`, C root stack `ADDR_ROOTS`, native stack `ADDR_STACK`, permanent arena, code). A forked
  child has the same layout, so the VM, the collector, the root stack and every global work unchanged.
- Isolation (FR-006) comes for free: the child has a copy-on-write copy of every value the task was given
  or captures. Nothing it changes can reach the parent or a sibling; only the result comes back (R4).
- Each task is an ordinary sequential vo run, so its behavior, its collection points, its memory
  figures and its runtime errors are deterministic on their own. Determinism of the whole call reduces to
  combining per-task outputs in a fixed order (R5, R6).
- No assembly: fork needs no stack switching.

**Alternatives considered**:
- *Stackful coroutines in one process* (a context switch in the platform layer, yielding inside
  `net_wait`): needs per-task value stacks, root stacks and native stacks at new addresses, the collector
  scanning all of them, swapping about 15 globals (`cur_frame`, `vsp`, `vbase`, `gc_roots_n`,
  `recover_point`, trace depth, ...) on each switch, a deep copy of every input and closure to get
  isolation, and context-switch assembly on three platforms. Collection points and R0006 would depend on
  the interleaving, i.e. on network timing, breaking Principle III for `--mem-stats` and out-of-memory.
  Rejected: more code in riskier places, weaker determinism.
- *An event-loop VM* (frames that can suspend without a native stack): a rewrite of the call path and
  every builtin that calls back into vo (`map`, `sort_by`, contracts). Rejected.
- *Threads*: the collector and VM are single-threaded (spec clarification Q1). Rejected.

**Consequence**: children can run on several cores at once. FR-004 is amended to say this is incidental
and never observable; the guarantees are about output, not scheduling.

**Cost**: a fork per task. The parent's touched pages are shared copy-on-write; expected cost is well
under 1 ms per task on x86-64 for agent-sized programs (to be measured, SC-006).

## R2: The form in the language — `concurrent_map(xs, limit, f)`

**Decision**: one builtin, in the "Functions as values" table next to `map`:

```
concurrent_map(xs: list[T], limit: int, f: fn(T) -> U) -> list[U]
```

`limit` is required, 1 to 64 (R0007 otherwise). Effects work exactly as for `map`: `f`'s effects become
the caller's. The builtin itself is pure (no new effect, FR-007).

**Rationale**: reads like `map` (FR-001: making a loop concurrent is one call), structured by construction
(nothing escapes the call, FR-003), results in input order (FR-010). Function-last order matches `fold`.
A required limit keeps one form (Principle I: no optional arguments) and makes the agent think about
politeness to servers; 64 bounds processes and file descriptors (3 pipes each, R5).

**Alternatives considered**: `spawn`/`join` handles (handles can escape or be dropped; FR-003); a block
statement (new syntax, more reference tokens); a `std.tasks` module (a library function cannot fork the
calling process any more cheaply than a builtin, and would need a builtin underneath anyway);
`parallel_map` (promises cores, R1); an optional limit with a default (two forms).

## R3: Isolation — copies, not check-time rejection

**Decision**: tasks get copies (R1). Nothing is rejected for being shared. The reference states: "each
task works on its own copy of everything it is given or captures: changes stay in the task, and only its
result comes back."

**Rationale**: the copy is free with fork and needs no new analysis. A check-time rule ("`f` must not
change a parameter or captured value") would need an effect-like mutation analysis through calls and
function values, which vo does not have. Anonymous functions already cannot capture a `var` (E0505), so
captured values are references to lists, maps, records and collections; their contents are the only
shared state, and the copy covers them.

**Alternatives considered**: rejecting reference-typed inputs (too restrictive: most inputs are records
or lists); a new "mutates" analysis (large, and the copy makes it unnecessary).

## R4: Results back to the parent — type-directed transfer

**Decision**: the child writes its result to its control pipe in a binary form driven by the call's
static result type `U` (made concrete with the caller frame's type arguments, as `prop.c` does for
generated values): ints, floats and bools as words; strings as bytes; lists, maps, results, records and
variants by their element, field and case types; collections by their items in order, rebuilt in the
parent with the built-in order. Each heap object gets an index on first visit, so shared objects and
cycles come back shared and cyclic. The parent allocates normally (it is running vo code's call).

Two things cannot cross:
- **Functions**: a result type that contains a function type anywhere (fields, cases, elements) is
  check error **E0314** (`task_result_function`), with the field or element named. Function values point
  at closures and type arguments allocated in the child.
- **Collections with their own order** (`ordered_map_by(cmp)` etc.): their comparator is a function the
  type does not show, so this is found at run time: **R0015** (`task_result_order`) in the task, at the
  `concurrent_map` call, with the message "return its items instead".

**Alternatives considered**: serializing closures (needs type arguments and code allocated after the fork
to exist in the parent); rendering results as vo text and parsing them back (no value parser exists, and
floats, cycles and sharing would need special forms); shared memory for results (the parent's heap at
the same addresses holds other objects).

## R5: Output in a fixed order — head-first draining

**Decision**: each child gets three pipes, installed with `dup3` on fds 1 and 2 plus one control fd:
stdout, stderr, control (result, error, exit, trace and recording messages; R6, R10, R11). The parent
reads only the pipes of the *head* task (the lowest position not yet finished) and copies stdout and
stderr through to its own. Later tasks run until their pipes fill (pipe size raised to 1 MiB with
`F_SETPIPE_SZ`), then block until they become the head. When the head's pipes all reach end of file,
the parent reaps it (`wait4`) and the next task becomes the head.

So the output of a call is: all of task 0's stdout, then all of task 1's, and so on (stderr likewise),
the same on every run whatever order the tasks finish in. JSON records from tasks (`--json` runtime
records, test records) follow the same rule.

Before forking, the parent flushes its own buffered stdout, stderr, trace and recording, so the child
does not inherit and repeat pending output.

**Rationale**: no buffering in the parent at all (memory stays bounded by the pipes), and the order is
fixed by construction. A task that prints little (the common case: fetches) is never slowed.

**Alternatives considered**: draining every child into parent buffers (unbounded memory, more code);
interleaving by line (order would depend on timing). ponytail: head-first; a task that prints more than
1 MiB waits until it is the head. Upgrade: buffer non-head output in the parent if that shows up.

**Slots**: a new task starts when a running task exits. The parent polls every child's control pipe with
no requested events: the read end reports `POLLHUP` once the child has exited (its write ends are
closed), even while unread data remains. It then reaps the child (`wait4`) and frees the slot; a
non-zero status means the task failed, which lets R6 stop the tasks above it early. No `pidfd` is
needed, which keeps qemu-user working. A non-head child that exited holds no slot; its output waits in
its pipes until it is the head.

## R6: Failures — the lowest position wins

**Decision**: a task that stops with a runtime error, a timeout, a failed contract, a hole, or `exit(n)`
sends one message on its control pipe with what the parent needs to raise the same thing: the code,
message, node (AST pointers are valid in the parent: the program is checked and compiled before any
fork), extra JSON fields, and the text and JSON traces already built in the child; or the exit code.
The child sets its own recovery point at task start, so in `vo test` the child never continues the
test runner.

The parent: when task *k* fails, kills (SIGKILL) every running task above *k*, starts no more, keeps
draining tasks below *k* (one of them may fail too and then wins), copies *k*'s output, and finally
re-raises *k*'s error in its own process (`rt_reraise`) or exits with *k*'s code. The report is the one a
sequential `map` would give, with the trace from `main` through the `concurrent_map` call into the task.
A `result` returned as `err` is not a failure (US3 scenario 2).

Children call `prctl(PR_SET_PDEATHSIG, SIGKILL)` and check the parent is still there, so no task
survives the program (Ctrl-C, SIGTERM, a parent timeout).

**Rationale**: deterministic choice of the reported failure (FR-008); one report path for `run`, `test`
and `serve` handlers (the parent's existing `rt_raise` does the rest, including R0010's extra fields).

## R7: Nesting

**Decision**: `concurrent_map` called inside a task runs its tasks one at a time (still each in its own
process, so copies keep their meaning). Processes alive at once are at most the outermost limit plus the
nesting depth.

**Rationale**: makes nesting safe without a shared counter between processes. ponytail: nested calls are
sequential; upgrade path is a jobserver pipe (as in `make`) inherited by children.

## R8: Standard input

**Decision**: a function passed to `concurrent_map` whose effects include `input` is check error
**E0511** (`task_reads_input`): tasks would race each other and the parent for stdin, and the input
buffer (`src/rt_io.c` `in_pos`) would be copied into every child. The fix-free message says to read
the input before the call and pass it in.

## R9: Timeouts and signals

**Decision**: children inherit `deadline_ms`, so `--timeout` covers the whole program across tasks
(FR-009); a task past the deadline stops with R0009, which is re-raised as R0009 (exit 124). The parent's
own waits are bounded by the same deadline. Inside a `serve` handler the deadline is the handler's, as
today. Ctrl-C reaches the whole process group; children die (default action, or PDEATHSIG when the
server has blocked the signals for its signalfd).

## R10: Record and replay — recording format unchanged

**Decision**:
- *Recording*: a child's recording writer targets its control pipe instead of the file; the parent copies
  each task's events into the recording in task order, after the events the parent wrote before the call.
  The file therefore looks like the recording of a sequential `map` and needs no new event kind.
- *Replay*: tasks run one at a time (no real waiting happens in replay). Each child starts at the
  parent's next-event index, consumes its events, and reports its final index on the control pipe; the
  parent continues from there. Mismatches (R0014) and notes work unchanged.

**Rationale**: FR-011 with no change to `contracts/recording.md` of spec 006/011 and cross-platform
replay (`tests/cross_replay.py`) for free.

## R11: Traces

**Decision**: a child's trace records go through its control pipe; before each task's records the parent
writes `{"v":1,"kind":"task","index":I}`. `--trace-limit` is applied by the parent in merged order (a
child sends at most the limit, and its count of omitted records), so `end`'s counts are deterministic.
Call depth continues from the `concurrent_map` call.

## R12: Random numbers and the clock in tasks

**Decision**: each task's random generator starts from the run's seed mixed with its position (and the
parent's position, when nested), so tasks do not all draw the same numbers and the run stays fixed by
`--seed`. The parent's generator is unchanged by the call. The clock (`--clock-ms` or real) is inherited.

## R13: Memory

**Decision**: each task process has its own `--max-memory` limit (R0006 in the task, re-raised).
`--mem-stats` reports the parent process only. The spec's memory edge case is amended. ponytail: no
program-wide memory limit across tasks; total is bounded by limit × 64.

## R14: The server — concurrent connections, sequential handlers

**Decision**: `rt_serve_next` (`src/serve.c`) becomes a small event loop over the listener, the signalfd
and up to 64 open connections. Each connection has its own read buffer, framing state (`HttpFrame`) and
10 s deadline (408 as today). The first connection whose request is complete is handed to the handler;
handler calls stay one at a time (clarification Q2). `rt_serve_reply` queues the response on its
connection instead of sending synchronously; the loop writes queued responses as their sockets accept
data, with the existing 10 s send limit, and closes them. Above 64 open connections, new ones wait in the
accept backlog.

The recording still has `request` and `response` events in handling order; replay feeds them in that
order (unchanged).

**Rationale**: fixes the slow-client stall (US2) entirely in C behind the existing three builtins;
`std/http.vo`'s loop does not change. ponytail: a handler that itself waits still delays the next
handler call (documented in the spec, FR-013).

## R15: Effects and review

`concurrent_map` is checked like `map` (`check_expr.c`, `check_effects.c` value-call path): the caller
declares `f`'s effects with scopes (E0501, E0503); at run time the child has the caller's grant
(inherited: `rt_root_frame` and the frame chain are copied). `vo.effects` lists functions, not calls, so it
is unchanged when a loop becomes concurrent (SC-005).

## R16: Platform and tooling notes

- New system calls (all through the existing `vo_syscall*` interface, numbers per platform header):
  `clone`, `wait4`, `pipe2`, `dup3`, `fcntl` (`F_SETPIPE_SZ`), `prctl`, `kill`, `getppid`. No assembly.
- `clone` with `SIGCHLD` and a zero stack behaves as `fork` on all three architectures; the argument
  order differs between x86-64 and ARM64/RISC-V only in the unused pointer arguments.
- qemu-user supports fork-style `clone`; `pidfd` is avoided (R5) because qemu-user support is uneven.
- The instrumented (ASan/UBSan) build: a raw `clone` in a single-threaded process is safe for ASan; the
  child runs the same code under the same shadow memory.
- Tests need a delaying HTTP route: `tests/http_server.py` gains `GET /delay/MS` (answers after MS
  milliseconds) and the server is already threaded.

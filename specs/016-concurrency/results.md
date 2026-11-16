# Results: Concurrency

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/tasks_determinism.py --timing`: ten `/delay/1000` requests take 1.01 s with `concurrent_map` (limit 10) and 10.03 s with `map` (limits: under 3 s, at least 10 s). `examples/url-check`: URLs of 500 and 400 ms checked in 0.52 s. |
| SC-002 | met | `tests/serve_endurance.py --stalled`: with one client holding half a request open, 100 requests answered, the slowest in 1 ms (limit 1 s). Step cases `tests/cases/serve_concurrent/slow_client` and `slow_reader` (a client that never reads an 8 MiB response): 5 requests at once, all within 1 s; both failed before the change ("within 1 s: no"). The half request still gets 408 after 10 s. |
| SC-003 | met | `tests/tasks_determinism.py --runs 100`: every task golden case (33) and task step case (11, against a server adding 0-300 ms at random to each delayed response) ran 100 times with byte-identical stdout, stderr, kept files and exit status, equal to the expectation; the gate runs 20. `tests/cross_replay.py`: the task recording made on each platform replays identically on the other two. |
| SC-004 | met | `tests/tasks_props.py`: 10 seeds, 4,000 generated cases, 0 failures: `concurrent_map(xs, k, f) == map(xs, f)` for ints (with k from 1 to 64), strings, results, variants, records, and with functions that change their arguments (the caller's values equal before and after). 1,200 more under the instrumented build. |
| SC-005 | met | Step case `tasks_steps/effects_same`: `vo effects --update` on the `map` version, then the `concurrent_map` version (one line changed) checks with 0 errors and 0 warnings against the same record. `examples/url-check` has `main.vo` and `sequential.vo`, differing in one call. |
| SC-006 | met | `tests/tasks_scale.py`: 10,000 tasks at limit 10 in 5.08 s (0.51 ms per task), 1,000 at limit 64 in 0.37 s (0.38 ms per task), default `--max-memory`; 20 calls of 500 tasks leave the open file descriptors at 3 before and after. |
| SC-007 | met | Reference: about 11,393 tokens, +286 (limit +400, budget 15,000). Programs without tasks: every existing golden and step case unchanged (one recorded fingerprint changed, see Behavior changes). The full gate passes on x86-64, and `make test` on ARM64 and RISC-V. |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 563, step 205 + 155 + 35 repeated, catalog 104 sections, reference 35 examples, site 42 pages / 184 examples, doc conformance, fuzzers, examples, diff-engines 1,000 (0 disagreements) |
| `make gate` (x86-64) | passed in 16 min: lint; test (golden 563, step 205 + 156 + 35); ubtrap 563; diff-asm 2,000 (0 disagreements); tasks determinism (20 jittered runs of 33 golden and 11 step cases, all identical); gc-check (stress golden 563, stress diff-engines 1,000: 0 disagreements, fault injection: every kind caught); speed geometric mean 0.898 (limit 1.10) |
| `make instrumented` | 563 of 563 under ASan and UBSan; the task, server and spec 011 serve step cases (26) and `tasks_props.py --seeds 3` also pass under it |
| `make lint` | clean: strict C for x86-64, ARM64 and RISC-V; SESI; no new assembly |
| ARM64, RISC-V | `make ARCH=aarch64 test` and `make ARCH=riscv64 test` (qemu): golden 563, step 205 + 156 + 35, catalog, reference, site, fuzzers, diff-engines 1,000 (0 disagreements) on both |
| `tests/tasks_determinism.py` | 100 runs with 300 ms jitter: 33 golden and 11 step cases identical in every run |
| `tests/cross_replay.py` | 6 platform pairs, 18 cases each (the spec 006 replay cases and the task recording): all identical |

## Behavior changes

- **New builtin** `concurrent_map(xs: list[T], limit: int, f: fn(T) -> U) -> list[U]` (the name is no longer
  free for a user function). Three new codes: E0314 `task_result_function`, E0511 `task_reads_input`,
  R0015 `task_result_order`.
- **`std.http.serve`** reads up to 64 connections at once and writes responses without holding up the
  next handler call; handler calls stay one at a time, in the order requests become complete (before:
  in accept order, after each request was read). The spec 011 and 013 serve cases are unchanged.
- **Trace schema v1** gains the record kind `task` (`index`), written only by programs that use tasks.
- **`--mem-stats`** reports the program without its tasks; each task has its own `--max-memory`.
- **`std/http.vo`** documents the new `serve` behavior in its doc comment. Recordings fingerprint the
  program's files, the standard library included, so the fingerprint of recordings of programs that use
  `std.http` changed: `tests/cases/http/record_replay/expected/r.jsonl` was updated (fingerprint only). The
  example recordings already replayed with a "program differs" note before this spec.
- **`tests/steps.py`** gains `request-open`, `read-open`, `request-many` and `no-children`;
  `tests/http_server.py` gains `/delay/MS` and `/peak`.

## Deviations

- **Tasks are processes** (research R1), not turns on one thread; FR-004 was amended in planning. Tasks may
  compute on several cores at once; nothing observable depends on it.
- **The limit is required** (FR-002 amended): one form, no default.
- **Nesting** runs inner tasks one at a time (research R7); **memory** is per task (R13).
- **Tasks that have ended keep their slot until drained** (found by `tests/tasks_scale.py`: the first
  version could run out of its 64 slots with many quick tasks and a small limit, T0003); a new task starts
  only when a slot is free. Golden case `tasks/many`.
- **R0015 also covers** a function reaching a result through a type parameter (E0314 cannot see it at
  check time).
- **Timeouts**: the task that reaches the deadline reports R0009 at its own location, as a sequential
  program would; the parent stops waiting one second after the deadline (a backstop that has not been
  needed in the cases).

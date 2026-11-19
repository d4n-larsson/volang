# Results: Sleep

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/sleep_check.py`: `sleep(200)` adds 201 ms of real time (limit 200–300) and 0.8 ms of CPU (limit 10 ms), median of 5 runs; step case `real_wait`. |
| SC-002 | met | `time/sleep_test_hour`: tests that sleep an hour of fixed-clock time pass; `sleep_check.py`: 2 ms (limit 1 s). |
| SC-003 | met | `sleep_steps/replay`: the replay's output equals the recorded run's; `sleep_check.py`: the replay of a 2 s sleep takes 1 ms (limit 500 ms). |
| SC-004 | met | `sleep_steps/timeout`: R0009 at the call, exit 124; `sleep_check.py`: after 1.00 s (limit 1.5 s). |
| SC-005 | met | `sleep_steps/tasks`: ten tasks sleeping 300 ms give their results; `sleep_check.py`: 0.31 s in total (limit 1.5 s). |
| SC-006 | met | A case per behavior: fixed clock (`sleep_fixed`, `sleep_fixed_timeout`), tests (`sleep_test_hour`), replay (`replay`, `replay_changed`), negative (`sleep_negative`), overflow (`sleep_overflow`), time limit (`timeout`), signal (`sleep_check.py`, FR-007: SIGTERM ends a sleep in 2 ms), tasks (`tasks`), serve (`serve_handler`), docs (`sleep_doc`). Reference: +1 line in two places, about 12,473 tokens (+14). |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 747, step 211 + 235 + 35, the self-tests, catalog, reference, site (194 examples), doc conformance (129 examples), `math_vectors`, `bytes_fuzz`, examples, diff-engines 1,000 (0 disagreements) |
| ARM64, RISC-V | `make ARCH=aarch64 test` and `make ARCH=riscv64 test` (qemu): the same counts, all passed |
| `make gate` | not run (only when asked); its new step's parts were run: `tests/cases/sleep_steps` 3 times (all passed), `sleep_check.py` (0 failures) |

## Size

- `src/rt_effects.c`: `rt_sleep`, about 25 lines; one builtin id (`__sleep`, 81) in the tables.
- `std/time.vo`: `sleep`, 4 lines plus its `vo doc` comment.
- Tests: 6 golden cases in `tests/cases/time/`, 7 step cases in `tests/cases/sleep_steps/`,
  `tests/sleep_check.py` (in `make gate`).

## Deviations

- **Doc examples run with a fixed clock** (`--clock-ms 0` in `tests/doc_conformance.py`), as `vo test`
  does, so the `sleep` example is exact and does not wait.
- **The signal check is in `tests/sleep_check.py`**, not a step case: the step runner's `start` waits for
  a server to accept connections, which a sleeping program never does.
- **The site example** uses a `console` block with `--clock-ms 0`: a test-only example is only checked,
  and every program that imports the standard library gets W0504 (no `vo.effects` record) from
  `vo check`, which is existing behavior (std imports count as files of the program).

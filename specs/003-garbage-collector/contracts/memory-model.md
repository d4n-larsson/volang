# Contract: Memory Model (v3)

## What programs can rely on

1. **Reachable values stay valid.** A value the program can still reach is never reclaimed. Its contents and identity stay the same: shared lists, maps and records stay shared.
2. **Unreachable values are reclaimed automatically.** There is no language-level way to free, finalize, weakly reference or inspect memory.
3. **Output does not depend on memory.** Reclamation never changes what a program prints, its exit status, or where a runtime error happens, except R0006 itself.
4. **Out of memory means live data.** R0006 happens only when, right after a reclamation pass, the live data exceeds the limit. It happens at the same step on every run of the same program, input, arguments and options.
5. **The limit:** 512 MiB of live data by default, set with `--max-memory MiB` on `run` and `test`. The limit applies per invocation: every test in a `vo test` run has the same limit. The process also uses a fixed overhead (stacks, the permanent arena), which is not counted.
6. **Tests are independent.** Each test starts with an empty collected heap. A test's memory report does not depend on the tests before it.
7. **Time limit.** `--timeout` applies everywhere: vo loops, calls, builtins and reclamation. A run past its deadline stops with R0009 within 1 second.

## What is collected

| Created... | Lives in | Collected |
|---|---|---|
| before the program starts (parsing, checking, docs) | the permanent arena | never |
| while the program runs: strings, lists, maps, records, variants, results, closures, and their buffers | the collected heap | when unreachable |
| while the program runs: interned types, effect sets, generic memo entries, named-function values | the permanent arena (perm brackets) | never; bounded by the program's text, not its input |

## Determinism mechanics (normative for the implementation)

- All regions are mapped at fixed addresses (data-model.md).
- A collection is triggered only by bytes allocated since the last one: when they reach `min(max(8 MiB, live after the last), limit − live after the last)`. The second bound means a collection always happens before live data can pass the limit, so out-of-memory is detected at most one allocation past it. The stress build adds its fixed interval.
- Sweeping rebuilds free lists and page runs in ascending address order.
- The heap is reset before each test.

## Stress mode (a maintainers' build; not part of the shipped toolchain)

`make STRESS=1` builds `build-stress/vo`. There, `VO_GC_STRESS=N` (default 64) collects before every Nth allocation, poisons reclaimed memory, and checks object kinds at runtime entry points. The shipped binary has none of this.
- **Output** must be byte-identical to normal mode.
- **A use of reclaimed memory** that is detected stops with T0003 (`internal_error`), with the message "a reclaimed value was used".

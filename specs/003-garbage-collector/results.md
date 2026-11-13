# Results: vo-lang v3 Memory Reclamation and Runtime Robustness

Measured on 2026-11-13 under qemu-riscv64 on an x86-64 host, with the `003-garbage-collector`
build. Raw commands are in quickstart.md. The machine was loaded, so single timings vary by up to
±40%; every timing below is a median.

## Success criteria

| Criterion | Target | Result | Met |
|---|---|---|---|
| SC-001 | 5 workloads, each ≥ 1 GiB allocated, peak live < 64 MiB | 1.02–2.40 GiB allocated, 8.0–12.2 MiB peak (table below) | yes |
| SC-002 | Existing corpus and benchmark solutions unchanged | 220 golden cases pass, with only intended changes (below). All 703 bench2 reference runs are byte-identical to the pre-collector baseline | yes |
| SC-003 | Stress mode: whole corpus and benchmark solutions identical | Every golden, edge and fault-target case is byte-identical with a collection every 64 allocations (at least; large heaps use their own interval). 12 cases whose output depends on collections or time are left out, with reasons. 703/703 bench2 reference runs are identical under stress. The stress fuzzer passed 3,000 iterations with 0 failures | yes |
| SC-004 | Fault matrix: each of ≥ 8 kinds is caught | 8/8 kinds caught by their targeted program at interval 1. The extra PERM kind is caught by none, as expected (below) | yes |
| SC-005 | 20 out-of-memory runs identical, `limit ≤ in_use ≤ 1.1 × limit` | 20/20 identical R0006 records at 16 MiB; `in_use` 16.0005 MiB; at 32 MiB it fails later | yes |
| SC-006 | ≤ 20% slower; 1,000-line programs under 1 s | perf.vo 51–62 ms vs 59 ms before; perf2.vo 68–78 ms vs 65 ms before. A head-to-head run of the old and new binaries gave medians of 53 → 38 ms for perf2 (noise dominates). Both are far under 1 s | yes |
| SC-007 | ≥ 5 single-builtin programs stop ≤ 1 s after a 2 s limit | 5/5 stop within 2.01–2.39 s (they ran 8–14 s before) | yes |
| SC-008 | Generic hot loops ≤ 1.5× the type-specific version | max 1.21×, equality 1.17×, stack 1.16× (before the memo: 1.65×, 1.50×, 1.26×) | yes |
| SC-009 | Reference within 15,000 tokens, no memory workaround, examples pass | ~8,190 tokens, 28/28 examples pass, workaround sentence removed | yes |

## SC-001 workloads (`tests/gc_bounds.py`)

| Workload | Allocated | Peak live | Collections | Time |
|---|---|---|---|---|
| string_build (200,000 characters by `+`) | 2.40 GiB | 8.1 MiB | 311 | 3.3 s |
| list_churn | 1.12 GiB | 8.0 MiB | 144 | 1.7 s |
| map_churn | 1.08 GiB | 12.2 MiB | 138 | 1.8 s |
| closures | 1.02 GiB | 8.0 MiB | 130 | 1.0 s |
| generic_container | 1.06 GiB | 8.0 MiB | 135 | 1.8 s |

"Peak live" is the reported peak: live data after the last collection plus everything allocated
since. It is an upper bound, so it sits near the 8 MiB collection trigger for these workloads.
Expected outputs come from Python transcriptions (`tests/gc/workloads/transcribe.py`).

## Intended changes to existing behavior

- **`run/oom_small`** used to stop with R0006 after 512 MiB of total allocation. It now finishes and
  prints its result (`tests/migrate.py` records this as an intended v3 change).
- **`run/oom`** (a string that doubles forever) still stops with R0006, with the new message: "The
  program holds 768.2 MiB of live data, over the 512 MiB limit."
- **`cli/bad_flag`:** the option list in the message gained `--max-memory MiB, --mem-stats`.

## Fault matrix (`tests/gc_faults.py`, SC-004)

| Kind (SC-004) | Fault build | Caught by |
|---|---|---|
| locals and parameters | `VSTACK` | 6 targeted cases, the first `fault_targets/closure` |
| builtin temporaries | `NATSTACK` | 9 targeted cases (all of them) |
| error or failure data | `GLOBALS` | `fault_targets/globals` (a runtime error's message while the runner reports) |
| list elements | `LISTBUF` | 6 targeted cases |
| map entries | `MAPARR` | `fault_targets/maparr` |
| record fields | `RECORD` | `fault_targets/record` |
| variant payloads | `VARIANT` | `fault_targets/variant` |
| captured values | `CLOSURE` | `fault_targets/closure` |
| (extra) permanent arena | `PERM` | none: nothing on the heap is reachable only from the permanent arena. So resetting the heap between tests is safe, and the arena scan is a safety net only |

A case counts only if it fails with the fault build and passes with the plain stress build. A case
too slow for a collection at every allocation fails both ways and proves nothing. That rule stopped
the first matrix run from counting `oom_small`'s timeout as a catch.

The fault builds stop with T0003 "a reclaimed value was used" (the stress build's kind checks at
list, map, string, record-field and closure-call entry points), crash on poisoned memory, or print
corrupted values.

## Bugs found on the way

- **The stress comparison in `tests/run.sh` compared the stress run with itself.** `run_once` set
  the caller's `pre` variable, so after the stress run both sides of the `cmp` were the stress
  output. Found when the fault matrix caught nothing. Until then, the "passes under stress" results
  for the golden corpus meant nothing (the bench2 stress pass and the stress fuzzer do not use
  `run.sh`, and were valid). Every stress result in this file comes from after the fix.
- **Maps kept removed values, and every grow doubled the capacity.** A small map with heavy churn
  (1,000 live keys, 250,000 inserted) held 512 MiB and took 27 s. `map_grow` now sizes from the
  live count, and `map_remove` clears the removed key and value: 12 MiB and 1.8 s.
- **Test-order dependence of memory reports.** Found by the test-order check in
  `tests/gc_bounds.py`. The allocator had saved `a1`–`a7`, `t5` and `t6` on its stack "for safety".
  Their stale contents became conservative roots, and those contents depended on earlier tests.
  The allocator now follows the plain calling convention, and the dead stack is scrubbed at each
  test start.
- **Nested timeouts.** Building the R0009 message polls the budget too. Without clearing the
  deadline first, a second timeout with no location crashed the run.
- **`rt_concrete`'s 16-byte frame** was overwritten by a scratch store once perm brackets were
  added (caught by the generic golden cases).

## Deviations from the plan

- **Addresses** are below 2^38, so the map also fits Sv39 RV64 hardware. The planned `0x7c00_…`
  needs 47-bit user space.
- **Free and marked state** are kept in the object header (`OBJ_FREE`, bit 31 of the kind word),
  not in per-page bitmaps. Pages have no header, and the page table is its own region.
- **The out-of-memory report** switches the collector off and allocates from the permanent arena,
  instead of using a 1 MiB heap reserve.
- **The value stack** has its own fixed region, so the permanent arena scan never covers it.
- **T028** (content-interned type-argument vecs) was not needed. The `rt_targs` memo already
  returns one vec per (bindings, caller type arguments), which makes the memo keys stable.
- **SC-007's cases:** the five are sort, `==`, rendering a failed `expect_eq`, `find`, and
  `split` with the collections it triggers. `join` and map growth are polled too, but cannot be
  made to run 5 s inside one call within the memory limit.
- **The stress pass** uses a per-case interval (`<case>.stress-every`) for cases with large live
  heaps or time limits. `baseline.py --stress` retries a benchmark run that times out at interval 64
  with interval 4096 (3 of 703: `h09-window-max` hidden 12–14, about 1.9 MB inputs). Every case still
  runs under stress, at an interval that finishes.

## Follow-ups

- The permanent arena is scanned at every collection, although nothing on the heap is reachable
  only from it today (the PERM fault row shows this). A remembered set, or dropping the scan, would
  save time per collection.
- Conservative roots retain garbage that a stale word points at. Precise frame layouts for the
  interpreter's value stack would be the first step.
- `tests/perf.sh`'s 20% check uses medians of 5 runs; on a loaded machine it can still flake.
- SESI linter issues 15–17 (macros, suppression notes, a missed saved-slot overwrite) are in
  `sesi-linter-issues.md`.

# Research: vo-lang Memory Reclamation and Runtime Robustness

Phase 0 decisions for `specs/003-garbage-collector/spec.md`. There are no NEEDS CLARIFICATION items.

## Starting point (measured in the v2 code)

- **One allocator.** `alloc` in `src/sys.s` bumps through 16 MiB chunks of an arena that is never freed. The chunks are `mmap`ed at kernel-chosen addresses, and the whole arena is capped at 512 MiB (`arena_cap`; `rt_oom` reports R0006). The checker (AST, symbols, interned types) and the running program share it.
- **Values are untagged 64-bit words.** Whether a slot holds a pointer is known only from static types. Heap values (`str`, list, map, record, variant/result, closure) carry an 8-byte header (`size:u32 | kind:u32`), but their backing buffers do not: list data, the map index and entry arrays, string-builder bytes and `vec` arrays are plain `alloc` calls.
- **Where pointers live while a program runs:**
  - the value stack (frames and the argument areas of calls through values: `[vbase, vsp)`);
  - the native stack (the 64 MiB stack mapped at startup), including callee-saved registers spilled by the runtime's own routines;
  - live registers;
  - globals in `.data`/`.bss` (current frame, return value, the error and test-failure records, caches);
  - permanent structures such as `FX_CLOSURE` caches and the interned type tables, which the runtime extends while running.
- **The runtime routines** (about 12,000 lines of assembly) keep raw object pointers in `s` registers and stack slots across calls that allocate, including interior pointers. For example, `hof_map` passes `&data[i]` to `call_value`.

## 1. Collector kind: non-moving mark-sweep, conservative roots and conservative object scanning

- **Decision:**
  - **Non-moving mark-sweep.** Objects never move.
  - **Conservative roots.** Every word in the native stack, the live part of the value stack, the program's `.data`/`.bss`, and the permanent arena is treated as a possible pointer.
  - **Precise object boundaries.** Every collected allocation records its start and size.
  - **Conservative object contents.** Every word of a scanned object is a possible pointer, except objects allocated as byte data (string contents, builder buffers), which are not scanned.
  - **Interior pointers** keep their whole object alive.
- **Rationale:**
  - A moving collector would have to rewrite every pointer, and the runtime holds untyped, sometimes interior pointers in registers and stack slots across almost every allocating call. Making those precise would mean annotating every call site in 12,000 lines of hand-written assembly, which is exactly where a reclamation bug would hide.
  - Conservative scanning needs no cooperation from the runtime code beyond spilling registers at collection time.
  - The known cost is false retention: an integer that happens to equal a heap address keeps that object alive. That costs memory, never correctness, and the addresses are deterministic (§3).
- **Alternatives considered:**
  - **Precise copying collector** (best compaction): rejected because of the precise-root burden above.
  - **Mostly-copying collector** (Bartlett-style: conservative roots pin their pages, everything else is copied): its compaction is not needed for vo's workloads, and it adds forwarding logic to every object kind.
  - **Reference counting:** every store in the runtime would need increment and decrement calls, and cycles occur (closures stored in records they capture).

## 2. Heap layout: size-classed pages, a large-object space, and a permanent arena

- **Decision:** Two allocators.
  - **The permanent arena.** The existing bump allocator for everything created before the program starts (lexing, parsing, checking), and for runtime metadata that must outlive any single test: interned types, effect sets and the generic caches (§6), plus named-function closures. It is never collected, and while a program runs it is a root region.
  - **The collected heap.** One reserved virtual region (`limit` plus headroom, `MAP_NORESERVE`), carved into 64 KiB pages:
    - **Small objects** (up to 16 KiB) come from size-classed pages: 16, 32, 48, 64, 96, 128, 192, 256, 384, 512 bytes, and so on, up to 16 KiB, with a free list per class. A page header records the size class, a mark bitmap, an allocated bitmap and a no-scan flag.
    - **Large objects** take whole runs of pages, tracked in a page table. Freed runs coalesce, and placement is first-fit in address order.
  - **Finding the object for an address** is arithmetic: page index, then page header, then `(addr - page_base) / size_class` for small objects, or the run start for large ones.
  - **Routing.** While a program runs, every allocation goes to the collected heap through `gc_alloc(size, flags)`. `alloc` becomes a thin router: permanent before the program starts and within explicit `perm` brackets, collected otherwise. Byte buffers call `gc_alloc_bytes`, which is never scanned.
- **Rationale:**
  - Size classes give constant-time allocation and freeing with no fragmentation bookkeeping, and bounded waste (at most 33%) for the small objects that dominate allocation.
  - Page arithmetic makes interior-pointer lookup cheap, which matters because conservative marking does it for every word.
- **Alternatives considered:**
  - **A single first-fit free list:** slower interior lookups and fragmentation.
  - **A separate mapping per large object:** non-deterministic addresses (§3).

## 3. Determinism: fixed addresses and allocation-driven triggers

- **Decision:**
  - The native stack, the permanent arena chunks, the value stack and the collected heap are mapped at fixed virtual addresses (`MAP_FIXED_NOREPLACE`), in an otherwise unused part of the address space.
  - Collection is triggered only by allocation volume: when bytes allocated since the last collection reach `min(max(8 MiB, live after the last collection), limit − live after the last collection)`. The first bound lets live data double between collections, which keeps collection cost proportional to live data. The second bound guarantees a collection before live data can pass the limit, so out-of-memory is detected within one allocation of the limit (SC-005), including tiny limits below 8 MiB.
  - Free lists and page runs are rebuilt in address order on every sweep.
  - In tests, the collected heap is reset to its initial state before every test.
- **Rationale:**
  - With fixed addresses, the same program, input and options produce the same allocation addresses. So the same words look like pointers, the same objects are retained, the same collections happen, and out-of-memory strikes at the same step on every run (FR-004, SC-005).
  - Random placement (ASLR) would make conservative retention, and with it the out-of-memory point, vary from run to run.
  - Resetting the heap per test makes each test's memory behavior independent of the order and contents of earlier tests (spec edge case). This is safe because no test value outlives its test: anything that must persist lives in the permanent arena (§2, §6).
- **Verified:** under qemu-user on the reference host, a 1 GiB `MAP_FIXED_NOREPLACE | MAP_NORESERVE` reservation at `0x7c00_0000_0000` maps at exactly that address, and pages at both ends can be written (a 20-line RV64 test program).
- **Alternatives considered:**
  - **Trigger by a timer or wall-clock:** non-deterministic.
  - **Collect only when the limit is reached:** peak memory would always approach the limit, failing SC-001's 64 MiB bound.

## 4. Roots and register spills

- **Decision:** `gc_collect`:
  1. Saves `s0`–`s11` (and `ra`, `sp`) into its own frame.
  2. Scans the native stack from its current `sp` to the stack top.
  3. Scans the value stack from `vbase` to `vsp`.
  4. Scans the program's `.data`/`.bss` between linker symbols placed by the Makefile's link order (the first and last object files define `gc_data_start` and `gc_data_end`).
  5. Scans every chunk of the permanent arena that was used.
- **Rationale:**
  - The ABI guarantees a caller's live values are in `s` registers or its own stack slots at any call, and `gc_alloc` is only ever reached through a call. Spilling the `s` registers therefore makes every live value visible on the stack.
  - Floating-point registers never hold pointers.
- **Cost and ceiling:** a collection scans the whole permanent arena, a few MiB for large programs. This is acceptable because collections happen only after at least 8 MiB of allocation. Marked `ponytail:`, with tracking writes into the permanent arena (a remembered set) as the upgrade.

## 5. Marking, sweeping and safety checks

- **Decision:**
  - **Marking** uses an explicit mark stack. It lives in its own region and doubles when full; if it cannot grow, it falls back to rescanning marked pages until nothing new is marked. This bounds native-stack use, so deep or long structures cannot overflow (spec edge case).
  - **Sweeping** is eager, right after marking. It rebuilds the free lists, coalesces large runs, and counts live bytes.
  - **After a collection, if live bytes exceed the limit**, the runtime raises R0006 with `in_use` and `limit`. A reserve region of 1 MiB is kept for producing the report.
  - **Stress mode** exists only in the stress build (`make STRESS=1`, assembled with `--defsym GC_STRESS_BUILD=1` into `build-stress/`). There, `VO_GC_STRESS=N` (default 64) collects before every Nth allocation (N=1 means every allocation) and fills freed memory with a poison pattern. Freed blocks get the header kind `OBJ_FREE`.
  - **Use-after-free checks.** The runtime's object accessors (list, map, string and record entry points) check the header kind in the stress build. Finding `OBJ_FREE` or poison stops the program with T0003 (`internal_error`, "a reclaimed value was used"). This covers FR-011 wherever a check is cheap. The shipped build contains none of this code.
- **Rationale:**
  - Poison and kind checks turn a missed root into a loud failure in the stress corpus instead of silently wrong output (SC-003, SC-004).

## 6. Generic code: interned type-argument vectors and memoized resolution

- **Decision:**
  - Type-argument vectors are interned like types, so equal argument lists share one pointer.
  - `rt_targs(bindings, caller targs)`, `rt_concrete(type)` and `es_subst(set, owner, args)` are memoized in a permanent-arena hash table keyed by (input pointer, argument-vector pointer).
  - A generic call in a hot loop then costs one hash lookup instead of a substitution walk plus allocations.
  - The memo is created in a `perm` bracket, so it survives per-test heap resets.
- **Rationale:**
  - This removes both the per-call substitution cost (US5) and the unbounded allocation of effect sets per generic call. Without the fix that allocation would be garbage the collector has to chase, and before this spec it was a memory leak.
- **Alternatives considered:**
  - **Monomorphization** (one checked copy of the body per instantiation): it duplicates AST and checking work, which is out of proportion for an interpreter.

## 7. Time limit inside the runtime

- **Decision:**
  - A cheap work counter, `rt_budget`, is decremented in every loop of a long runtime operation, reading the clock only every 2^16 units:
    - sort and merge, join, split, slice and copy;
    - `values_equal` and `render_value` over large values;
    - map insert and grow;
    - the HOF loops (their callbacks already tick);
    - string building (`sb_bytes` for large appends);
    - the marking and sweeping loops.
  - When the counter runs out, it checks the deadline the same way `tick` does and raises R0009.
- **Rationale:** the deadline is wall-clock and was already nondeterministic by nature (R0009). This only closes the places it was not checked. The counter keeps the cost negligible.

## 8. Options and reports

- **Decision:**
  - **`--max-memory MiB`** on `run` and `test`: the live-data limit, default 512, per invocation (for `test`, every test gets it).
  - **`--mem-stats`** on `run` and `test`:
    - Human mode: a line on stderr, `memory: peak live 3.2 MiB, allocated 1.4 GiB, 212 collections`.
    - With `--json`: a `memory` record (for tests, one per test).
  - **Stress mode is a build, not an option.** `make STRESS=1` produces `build-stress/vo`, where `VO_GC_STRESS=N` picks the interval. The shipped binary has no stress code and reads no setting, so the agent-facing surface and the reference are unchanged (constitution V: every CLI surface must be in the reference).
- **Rationale:**
  - The limit and the report are user-facing and follow the existing flag style (`--timeout`, `--seed`).
  - Stress mode is a maintainer tool. An environment variable keeps it out of the agent-facing surface and the reference budget.

## 9. Verification

- **Decision:**
  - **`tests/gc/`:** the five SC-001 workloads, plus a growing-live-data program for SC-005. Each has expected output and a bound check: `tests/gc_bounds.py` reads the `--mem-stats --json` record.
  - **`tests/run.sh --stress [N]`:** runs the whole golden corpus with the stress build at interval N (default 64) and compares it byte-for-byte with normal mode (SC-003). A smaller "every allocation" pass covers the `run` and `func` areas.
  - **`tests/gc/baseline.py --stress`:** runs every benchmark reference solution with the stress build and compares it with the recorded baseline (SC-003).
  - **`tests/fuzz.py --stress`.**
  - **`tests/gc_faults.py`** (SC-004):
    - builds fault-injection binaries (`make FAULT=<kind>`, using an assembler `--defsym`; a fault build is always also a stress build) that skip one root source or one object kind's scan, for each of: value stack, native stack, globals, permanent arena, list buffers, map arrays, record fields, variant payloads and closure captures;
    - checks that the stress corpus fails for each one.
  - **Timeout cases** (SC-007): golden cases that spend their time in one builtin, run with `--timeout 2`.
  - **Generic speed** (SC-008): `tests/perf.sh` gains timed pairs, generic versus type-specific.
  - **Regression** (SC-002): the whole existing gate, plus `bench2/validate.py`.

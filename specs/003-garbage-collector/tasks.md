---

description: "Task list for vo-lang v3: memory reclamation and runtime robustness"
---

# Tasks: vo-lang Memory Reclamation and Runtime Robustness

**Input**: Design documents from `specs/003-garbage-collector/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/ (memory-model.md, cli-v3.md, output-schema-v3.md, error-codes-v3.md), quickstart.md

**Tests**: REQUIRED. Constitution v1.1.0, Principle IV: every behavior change lands first as a golden case or test script that fails before the implementation exists. Within each phase, test tasks come before the implementation tasks.

**Organization**: by user story, in this order:
- US1, bounded memory (P1, MVP). It includes the collector and a basic stress switch, since building the collector without one is unsafe.
- US2, nothing else changes (P1).
- US3, a deterministic out-of-memory error (P1).
- US4, the time limit everywhere (P2).
- US5, generic speed (P2).
- US6, safety net: poison, kind checks, fault matrix, stress fuzzing (P2).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: The user story this task belongs to (US1–US6)

## Conventions

**Carried over** from `specs/001-vo-lang-core/tasks.md` and `specs/002-effects-functions-generics/tasks.md`:
- RV64IMAFD, lp64d, GNU as. The SESI hook checks every edit; every report gets fixed or explained.
- C-word prototype comments and `.globl` on every function.
- Shared constants live in `src/defs.inc`. No libc.
- Golden-case conventions and the behavior of `tests/run.sh --accept`.
- Append every SESI false positive or negative to `sesi-linter-issues.md`.

**Docs land with the change** (Principle V): any task that changes the CLI, the output or a code's meaning updates `docs/reference.md`, `docs/errors.md` and the spec 003 contract in the same task.

**Where the definitions live:**

| What | Where |
|---|---|
| Layouts and addresses | data-model.md |
| Guarantees | contracts/memory-model.md |
| Options | contracts/cli-v3.md |
| Records | contracts/output-schema-v3.md |
| Codes | contracts/error-codes-v3.md |

**The determinism rule.** Nothing in the collector may depend on wall-clock time, or on any address not in the fixed map (data-model.md).

**Shortcuts** get `ponytail:` markers: conservative false retention, the full scan of the permanent arena, the mark-stack overflow fallback.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Freeze the "before" state that SC-002 and SC-006 compare against, and wire the new gates.

- [X] T001 Record the baseline in `tests/gc/baseline/`:
  - `perf.txt`: the output of `sh tests/perf.sh`, three runs, keeping the median per line.
  - `bench2.txt`: the stdout and exit status of every `bench2/tasks/*/ref/vo` and `bench2/features/*/ref/vo` solution on every visible and hidden case, as `<task> <case> <exit> <sha256 of stdout>` lines.
  - Write the script that produces them as `tests/gc/baseline.py --write`. Its default mode compares the current toolchain against the baseline. With `--stress` it compares `build-stress/vo` instead (SC-003).
  - The golden corpus needs no copy; its expectations are the baseline.
- [X] T002 [P] Create `tests/gc/` (workloads go here) and update the build and test entry points:
  - **`Makefile`:**
    - `test` additionally runs `python3 tests/gc_bounds.py` and `python3 tests/gc/baseline.py`, skipping either while it does not exist yet.
    - A new `gc-check` target runs `tests/run.sh --stress`, `python3 tests/gc/baseline.py --stress`, `python3 tests/fuzz.py --stress --iterations 3000 --seed 9` and `python3 tests/gc_faults.py`.
    - A `STRESS=1` variable adds `--defsym GC_STRESS_BUILD=1` to `ASFLAGS`, with output in `build-stress/`. A `FAULT=<kind>` variable adds `--defsym GC_FAULT_<KIND>=1 --defsym GC_STRESS_BUILD=1`, with output in `build-fault-<kind>/`. The default build contains no stress or fault code.
    - `gc-check` first runs `make STRESS=1`.
  - **`tests/run.sh --stress [N]`** (default 64): runs every case twice, with the normal build and with `build-stress/vo` (`VO_BIN`) and `VO_GC_STRESS=N`. It requires byte-identical stdout, stderr and exit status from both, in addition to the usual expectations.
  - **`tests/fuzz.py --stress`:** runs the vo processes it starts with `build-stress/vo` and `VO_GC_STRESS=16`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The constants, the fixed-address map and the allocation router, with no behavior change yet.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete. Checkpoint: `make test` passes 210/210 and `tests/gc/baseline.py` is clean.

- [X] T003 Add to `src/defs.inc` (data-model.md):
  - **Addresses:** `ADDR_STACK`, `ADDR_PERM`, `ADDR_VSTACK`, `ADDR_HEAP`, `ADDR_MARKSTACK`.
  - **Pages:** `PAGE_SIZE 65536`, `PAGE_HDR 256`, `NCLASSES 20`, `MAX_SMALL 16384`.
  - **Page kinds:** `PG_FREE 0`, `PG_SMALL 1`, `PG_LARGE 2`, `PG_TAIL 3`.
  - **Object kinds:** `OBJ_BYTES 8`, `OBJ_FREE 15`.
  - **Allocation flags:** `GCF_NOSCAN 1`.
  - **Defaults:** `GC_MIN_TRIGGER` (8 MiB), `GC_DEFAULT_LIMIT` (512 MiB), `GC_RESERVE` (1 MiB), `BUDGET_UNITS 65536`.
- [X] T004 Map the native stack and the permanent arena at fixed addresses in `src/sys.s`.
  - **Mappings:** `_start` maps the 64 MiB stack at `ADDR_STACK`. `alloc`'s chunk refill maps each 16 MiB chunk contiguously from `ADDR_PERM` upward. The value stack (32 MiB) moves out of the permanent arena into its own mapping at `ADDR_VSTACK` (in `interp_init`, `src/interp.s`), so a scan of the permanent arena never covers it. All three use `MAP_FIXED_NOREPLACE` (0x100000).
  - **On failure:** print `error[T0003]: cannot map memory at <address>` and exit 125.
  - **Arena limit:** the permanent arena keeps its 512 MiB cap.
  - **Check:** run `make test`; the result must be 210/210 with no output change.
- [X] T005 Make `alloc` a router in `src/sys.s`, with the collector off:
  - **`alloc(size)`:** when `gc_on == 0` or `gc_perm > 0`, it does the existing bump; otherwise it calls `gc_alloc(size, 0)`.
  - **`alloc_bytes(size)`:** the same, but routes to `gc_alloc(size, GCF_NOSCAN)`.
  - **Brackets:** `perm_begin()` and `perm_end()` increment and decrement `gc_perm`.
  - **A stub `src/gc.s`** declares `gc_on` (0), `gc_perm` (0) and a `gc_alloc` that is never called.
  - **Byte buffers:** switch the allocation calls for byte data to `alloc_bytes`: string bytes in `str_new`/`sb_finish`, string-builder storage in `sb_new`/`sb_grow` (`src/text.s`), and `read_whole_file` buffers (`src/sys.s`).
  - **Check:** run `make test`; the result must be 210/210 and `make lint` clean.
- [X] T006 [P] Find the globals' bounds for root scanning:
  - Check with `riscv64-linux-gnu-nm build/vo` that the default linker script defines `__DATA_BEGIN__` (or `__data_start`) and `_end`. Use them as `gc_data_start` and `gc_data_end` in `src/gc.s`.
  - If the default script has neither, add `src/gc_first.s` and `src/gc_last.s` marker files at the ends of the link order, in the `Makefile`.
  - Record which approach is used in a comment in `gc.s`.

**Checkpoint**: Foundation ready. Behavior is unchanged, and every region is at a fixed address.

---

## Phase 3: User Story 1 — Allocation-heavy programs run in bounded memory (Priority: P1) 🎯 MVP

**Goal**: A working collected heap. Every value created while a program runs is allocated there and reclaimed when unreachable. The five workloads run with a peak live size under 64 MiB.

**Independent Test**: quickstart.md US1 (`tests/gc_bounds.py`).

### Tests for User Story 1 (write first, must fail)

- [X] T007 [P] [US1] Write the SC-001 workloads in `tests/gc/`. Each is a `.vo` file with a `.out`, and each allocates at least 1 GiB in total while keeping under 16 MiB live:
  - `string_build.vo`: builds a 200,000-character string by repeated `+` of 10-character pieces, about 2 GB copied in total, printing the final length and a checksum of a few slices.
  - `list_churn.vo`: 1,000,000 times, builds a 50-element list, folds it and discards it.
  - `map_churn.vo`: inserts and removes 1,000,000 keys in a map, keeping at most 1,000.
  - `closures.vo`: creates and calls 1,000,000 closures that capture a record.
  - `generic_container.vo`: pushes and pops 1,000,000 items through a generic `Stack[T]`, with both `int` and `str`.
  - Expected outputs come from a Python transcription of each program, written in the same task.
- [X] T008 [P] [US1] Write `tests/gc_bounds.py`. For each workload it runs `bin/vo run --mem-stats --json`, then checks:
  - stdout equals `.out`;
  - the `memory` record exists, with `peak_live < 64 MiB` and `allocated >= 1 GiB`;
  - the exit status is 0.

  It prints one line per workload with the numbers. (The out-of-memory checks are added in US3.)
- [X] T009 [P] [US1] Write the golden edge-structure cases in `tests/gc/` (each with `.out`):
  - `deep_list.vo`: a 2,000,000-element list of records.
  - `deep_variant_chain.vo`: a 200,000-deep linked list of variants, built and then summed. This proves marking does not use the native stack recursively.
  - `shared_values.vo`: one list stored in a record and in a map, changed through one and read through the other across many collections.
  - `builtin_temporaries.vo`: `map`/`filter`/`sort_by`/`join`/`split` on 100,000-element lists, with callbacks that allocate heavily.
  - `captured_after_return.vo`: closures returned from functions, called after much allocation.

  Run each case in `tests/run.sh` normally and with `--stress 4`.

### Implementation for User Story 1

- [X] T010 [US1] Heap reservation and the small-object allocator in `src/gc.s` (data-model.md "Heap pages"):
  - **`gc_init(limit)`:** reserves `limit + 64 MiB` at `ADDR_HEAP` (`MAP_NORESERVE | MAP_FIXED_NOREPLACE`) and sets up the page table at its start. The class table holds the 20 sizes.
  - **`gc_alloc(size, flags)`:**
    1. Adds 8 for the header and picks the size class.
    2. Pops the class free list for its no-scan or scanned pages. When the list is empty, it takes a fresh page (the lowest free page index) and builds its free list in ascending address order.
    3. Sets the alloc bit, zeroes the block, and adds the bytes to `gc_since` and `gc_total`. It returns the block start; the caller owns the header (data-model.md "One header per block").
    4. Above 16384 bytes it uses the large-object path (next task).
  - **Free-slot links** go through the slot's first payload word.
  - **Callers:**
    - `alloc_obj(kind, payload)` calls `gc_alloc(payload + 8, flags)` directly and writes `size` and `kind` into the block's own header, so typed objects keep exactly one header. `OBJ_STR` uses `GCF_NOSCAN`.
    - The router's plain `alloc(n)` calls `gc_alloc(n + 8, flags)`, writes an `OBJ_RAW` header (`OBJ_BYTES` for `alloc_bytes`) and returns the block start + 8.
    - Every existing caller works unchanged.
- [X] T011 [US1] Large objects in `src/gc.s`:
  - `gc_alloc_large(size, flags)` does first-fit over free page runs in ascending address order, marking the head (with its run length) and the tails in the page table.
  - **Coalescing:** freed runs merge with free neighbors during sweeping.
- [X] T012 [US1] `gc_find(addr)` in `src/gc.s`: the object start for any address inside an allocated object (interior pointers included), or 0. It works by page-table lookup: a small slot by division with its alloc bit checked, a large tail walked back to its head. Add a self-check routine, `gc_selfcheck`, assembled only in the stress build (`.ifdef GC_STRESS_BUILD`): for each live object, `gc_find(start)`, `gc_find(start + size - 1)` and `gc_find(middle)` must return the start.
- [X] T013 [US1] Marking in `src/gc.s` (research §4–§5):
  - **`gc_collect()`:**
    1. Saves `s0`–`s11` and `ra` into its frame.
    2. Pushes conservative candidates from each root range in data-model.md "Roots" onto the mark stack: the native stack from its own `sp` up to the stack top, the value stack `vbase..vsp`, the globals `gc_data_start..gc_data_end`, and each used permanent chunk up to its bump pointer.
    3. Drains the mark stack: `gc_find`, test and set the mark bit, and push the object's payload words, unless the object is on a no-scan page or of kind `OBJ_STR`/`OBJ_BYTES`.
  - **The mark stack** lives at `ADDR_MARKSTACK` and doubles inside its 64 MiB reservation. On overflow it sets `gc_overflow` and rescans all marked scanned objects until nothing new is marked (`ponytail:` marker).
  - **Fault hooks:** each root source and each object-kind scan is wrapped in `.ifndef GC_FAULT_<KIND>` (`VSTACK`, `NATSTACK`, `GLOBALS`, `PERM`, `LISTBUF`, `MAPARR`, `RECORD`, `VARIANT`, `CLOSURE`), for US6.
- [X] T014 [US1] Sweeping and triggering in `src/gc.s`:
  - **Sweep:** walks the pages in ascending order. On small pages, unmarked allocated slots go back to the free list (rebuilt in address order). A page with no live slots returns to the free page pool. On large heads, an unmarked run is freed and coalesced. The sweep clears the mark bits and computes `gc_live`, then sets `gc_trigger = max(GC_MIN_TRIGGER, gc_live)`, resets `gc_since` and increments `gc_count`.
  - **Trigger:** `gc_alloc` collects when `gc_since + size >= gc_trigger`, where `gc_trigger = min(max(GC_MIN_TRIGGER, gc_live), gc_limit − gc_live)`. In the stress build it also collects before every Nth allocation.
  - **Stress mode** (inside `.ifdef GC_STRESS_BUILD` only) reads `VO_GC_STRESS` from `envp` in `gc_init`, defaulting to 64.
- [X] T015 [US1] Turn the collector on and off in `src/interp.s`:
  - **Turning it on:** `run_main` and the test runner's start call `gc_init(gc_limit)` once, then set `gc_on = 1` before evaluating `main` or the first test.
  - **Types and caches:** type machinery that runs while the program runs (`rt_targs`, `rt_concrete`, `es_subst`, `ty_func`, `ty_instance`, `ty_subst`) is wrapped in `perm_begin`/`perm_end` in `src/types.s` and `src/effects.s`, because interned types must never be collected. So is `fn_closure`'s cache fill in `src/interp.s`.
  - **The value stack** stays in the permanent arena, allocated before the collector turns on.
- [X] T016 [US1] `--max-memory MiB` and `--mem-stats` in `src/main.s` (contracts/cli-v3.md):
  - **`--max-memory`** parses 1–65536, otherwise T0001 with the message "`--max-memory` needs a whole number of MiB from 1 to 65536.", and sets `gc_limit`.
  - **`--mem-stats`** prints, after the run or each test, the human line on stderr or the `memory` JSON record (contracts/output-schema-v3.md).
  - **Peak:** `gc_peak` tracks `max(gc_peak, gc_live + gc_since)`, sampled at each collection and at the end.
  - Update `vo help`, contracts/cli-v3.md and the reference's command-line section.
- [X] T017 [US1] Reference and catalog in `docs/reference.md` and `docs/errors.md`:
  - **Reference:**
    - Remove the "memory is never reclaimed … use `push` + `join`" caveat from the Strings rule; string building with `+` is now fine.
    - Add one paragraph to "Command line": unreachable values are reclaimed automatically; the limit is on live data, set with `--max-memory`; `--mem-stats` reports usage.
    - Keep `tests/ref_examples.py` passing within budget.
  - **Catalog:** update R0006 (contracts/error-codes-v3.md): text, an Example that grows live data without bound, and a Fixed version that keeps a bounded window. Keep `tests/catalog.py` passing.

**Checkpoint**: `tests/gc_bounds.py` passes, the US1 edge cases pass normally and with `--stress 4`, and `make test` is green.

---

## Phase 4: User Story 2 — Nothing else changes (Priority: P1)

**Goal**: Byte-identical behavior for everything that worked before, with deterministic collection and independent tests.

**Independent Test**: quickstart.md US2.

- [X] T018 [P] [US2] Write `tests/gc/test_order.vo`, a file with 6 tests that allocate different amounts. Add a script check in `tests/gc_bounds.py`: run `vo test --mem-stats --json` on the file, and on a copy with the tests in reverse order, and require each test's `memory` record to be identical across the two orders.
- [X] T019 [US2] Reset the heap per test in `src/testrun.s` and `src/gc.s`:
  - **`gc_reset()`:** marks every page free in the page table, clears the free lists and counters (`gc_since`, `gc_live`, `gc_peak`, `gc_count`, `gc_total`), and zeroes the first page header. Pages are re-zeroed when allocated.
  - **The test runner** calls it before each test, after the previous test's report has been written.
  - **No dangling pointers:** confirm that nothing in the permanent arena or globals can point into the heap across tests. Clear `tf_*`, `rt_err_*`, `ret_val` and `cur_node` after reporting each test.
- [X] T020 [US2] Run the full regression and fix every difference at its root: `make test`, `tests/gc/baseline.py` (the bench2 reference outputs) and `python3 tests/migrate.py`. Expected changes are limited to R0006 cases (US3) and new golden cases. Any other difference is a collector or root bug.
- [X] T021 [US2] Determinism: run `tests/run.sh --repeat 5` over `tests/gc` and over `tests/cases/run`, with `--mem-stats --json` added through a temporary `.cmd` override in the script. Confirm identical memory records across repeats. Make the check permanent as `tests/gc_bounds.py --repeat`.
- [X] T022 [US2] Performance: run `sh tests/perf.sh` and compare with `tests/gc/baseline/perf.txt`. Each program must be at most 20% slower, and the 1,000-line checks stay under 1 s (SC-006).
  - **If slower:** profile `gc_alloc`'s fast path first. It should be a handful of instructions: class lookup through a 64-entry table for sizes up to 1 KiB, a list pop and a bit set.
  - Add the comparison to `tests/perf.sh`, failing above 20%.

**Checkpoint**: The full gate and the baseline comparisons are clean; repeats and test order make no difference.

---

## Phase 5: User Story 3 — Running out of memory is a clear, deterministic error (Priority: P1)

**Goal**: R0006 means live data over the limit, measured after a collection, at the same step every run, and reports `in_use` and `limit`.

**Independent Test**: quickstart.md US3.

### Tests for User Story 3 (write first, must fail)

- [X] T023 [P] [US3] Write the out-of-memory cases in `tests/gc/`:
  - **`grow_forever.vo`:** pushes 1 KiB strings onto a list forever.
  - **Golden cases:** `grow_forever_16.cmd` (`run --max-memory 16 --json {dir}/grow_forever.vo`), and `tiny_limit.cmd` (`run --max-memory 1 …` on a program that needs more than 1 MiB). Each expects R0006 with exit 70 (`.code`), and its `.err` holds the JSON. The `in_use` value is normalized by the test script, not by `run.sh`.
  - **`tests/gc_bounds.py`:** runs `grow_forever.vo` 20 times at `--max-memory 16` and requires identical R0006 records with `limit <= in_use <= 1.1 × limit` (SC-005). It also requires that `--max-memory 32` fails later in the program than 16.

### Implementation for User Story 3

- [X] T024 [US3] Out-of-memory decision and report in `src/gc.s` and `src/interp.s`:
  - **When to fail:** after a collection, if `gc_live + size` of the pending request exceeds `gc_limit`, collect once more (reclaiming anything the last pass's conservative roots left behind on the stack is deterministic). If it still exceeds the limit, call `rt_oom_live(in_use)`.
  - **The report:** `rt_oom_live` switches the allocator to the 1 MiB reserve (`GC_RESERVE`) for the report's own allocations. It raises R0006 with the message "The program holds X of live data, over the Y limit." and with `in_use` and `limit` in `rt_err_extra` (contracts/output-schema-v3.md).
  - **The permanent arena** keeps its own cap and old handler (`rt_oom`) for check-time exhaustion.
  - Update `docs/errors.md` R0006 and the reference sentence. Keep `catalog.py` passing.

**Checkpoint**: The out-of-memory cases pass, 20/20 runs are identical, and the gate is green.

---

## Phase 6: User Story 4 — The time limit stops everything (Priority: P2)

**Goal**: `--timeout` is enforced inside builtins and the collector (research §7).

**Independent Test**: quickstart.md US4.

### Tests for User Story 4 (write first, must fail)

- [X] T025 [P] [US4] Write golden cases in `tests/cases/run/`. Each has `.cmd` `run --timeout 2 {}`, expects R0009 (exit 124), and spends its time inside a single builtin call:
  - `timeout_in_sort.vo`: `sort` of a 3,000,000-element list.
  - `timeout_in_join.vo`: `join` of a 2,000,000-element list.
  - `timeout_in_equality.vo`: `==` on two equal 3,000,000-element lists of strings.
  - `timeout_in_render.vo`: `expect_eq` failing on two huge records, run with `test --timeout 2`.
  - `timeout_in_map_grow.vo`: one `fold` that inserts 3,000,000 keys.

  Each program must take more than 5 s without a timeout. Write `tests/gc/timeout_check.py`, which runs each case and requires the stop within 3 s of wall-clock time (SC-007).

### Implementation for User Story 4

- [X] T026 [US4] `rt_poll` and the work budget in `src/interp.s`:
  - `rt_budget` starts at `BUDGET_UNITS`. `rt_poll(units)` subtracts from it; when it reaches zero or below, it refills it and checks `deadline_ms` like `tick`, calling `rt_timeout` with `cur_node` when the deadline has passed.
  - **Call sites:**
    - every loop iteration in `list_sort`/`hof_merge`/`elem` merges (`src/rt_list.s`, `src/rt_func.s`);
    - `join`, `split`, `slice` and copy loops (`src/rt_str.s`, `src/rt_list.s`);
    - `values_equal`'s list, map and record loops (`src/interp.s`);
    - `render_value`'s loops (`src/rt_render.s`);
    - `map_grow` and the insert probing (`src/rt_map.s`);
    - `sb_bytes` per 4 KiB (`src/text.s`);
    - every 1,024 objects in the marking drain and every page in the sweep (`src/gc.s`).
  - **Timeout during a collection:** raising R0009 must leave the heap consistent. Do the poll only at points where the mark or sweep can be abandoned, then mark the heap "collection interrupted", and have `gc_reset`/the next `gc_collect` restart cleanly. The program ends anyway.

**Checkpoint**: `timeout_check.py` passes. `make test` is green, with no output changes, because polling is invisible below the deadline.

---

## Phase 7: User Story 5 — Generic code is not slower in hot loops (Priority: P2)

**Goal**: Memoized type resolution and interned type-argument vectors (research §6), with generic hot loops at most 1.5× slower than the type-specific version.

**Independent Test**: quickstart.md US5.

### Tests for User Story 5 (write first, must fail)

- [X] T027 [P] [US5] Add three timed pairs to `tests/perf.sh`, each running 300,000 iterations: `max_of[T: ordered]` against `max_int`, `same[T]` (equality) against `same_int`, and a generic `Stack[T]` push/pop against an `IntStack` record. The script prints the ratio per pair and fails above 1.5 (SC-008). Record the current ratios in the task's results for comparison.

### Implementation for User Story 5

- [X] T028 [US5] Intern type-argument vectors in `src/types.s` (**done differently**: the `TARGS` memo of T029 returns one vec per (bindings, caller type arguments), which already makes type-argument pointers stable for the memo keys; content interning was not needed):
  - `ty_args_intern(vec)` returns the canonical vec for equal contents.
  - **Who uses it:** `bindings_vec` in `src/check_generic.s` (bindings stored on call nodes), `rt_targs`, and `ty_subst_vec`'s results when they are used as type arguments.
  - The canonical vecs are permanent.
- [X] T029 [US5] The run-time memo in `src/types.s` (data-model.md "Run-time memo"):
  - **The table:** `memo_get(kind, a, b)` and `memo_put(kind, a, b, value)` over an open-addressing table in the permanent arena.
  - **Memoized calls:**
    - `rt_targs(bindings, caller FR_TARGS)` (kind `TARGS`);
    - `rt_concrete(type)` with the frame's `FR_TARGS` (kind `CONCRETE`);
    - `es_subst` (kind `ESUBST`), so the per-generic-call effect-set allocation disappears.
  - **Fast paths:** `rt_concrete` on a type without parameters returns at once, with the `ty_has_params` result cached per type in `TY_*` spare bits or a second memo kind.
  - **Check:** run `tests/perf.sh`; the ratios must be ≤ 1.5. Run `make test`.

**Checkpoint**: SC-008 is met and the gate is green.

---

## Phase 8: User Story 6 — Confidence that nothing in use is ever reclaimed (Priority: P2)

**Goal**: Stress mode poisons freed memory and checks kinds; the stress corpus, stress fuzzing and the 9-kind fault matrix all pass (SC-003, SC-004).

**Independent Test**: quickstart.md US6 (`make gc-check`).

### Tests for User Story 6 (write first, must fail)

- [X] T030 [P] [US6] Write `tests/gc_faults.py`. For each kind in `VSTACK NATSTACK GLOBALS PERM LISTBUF MAPARR RECORD VARIANT CLOSURE`:
  - build `make FAULT=<kind>`;
  - run `tests/run.sh --stress 1` (with `VO_BIN` set to the fault build) over `tests/gc` and over `tests/cases/run`, `func` and `generic`;
  - require at least one failing case.

  It prints one line per kind, with the first failing case. A kind that is not caught fails the script.

  How SC-004's kinds map to fault kinds:

  | SC-004 kind | Fault kind |
  |---|---|
  | locals and parameters | `VSTACK` |
  | captured values | `CLOSURE` |
  | list elements | `LISTBUF` |
  | map entries | `MAPARR` |
  | record fields | `RECORD` |
  | variant payloads | `VARIANT` |
  | builtin temporaries | `NATSTACK` |
  | error or failure data | `GLOBALS` |
  | (extra) runtime caches and named-function values | `PERM` |
- [X] T031 [P] [US6] Add a targeted case for each kind to `tests/gc/fault_targets/`. Each program keeps a value alive only through that kind and then allocates heavily: a value only in a local slot; only in a callee-saved register across an allocating builtin; only in a global (the test-failure value); only in a permanent cache; only in a list element; only in a map value; only in a record field; only in a variant payload; only in a closure capture. These guarantee the fault matrix has a case that depends on each kind.

### Implementation for User Story 6

- [X] T032 [US6] Poison and kind checks in `src/gc.s` and the runtime entry points:
  - **Poison:** in stress mode, the sweep writes `OBJ_FREE` into a freed slot's header and the poison word into its payload.
  - **Kind checks:** add `gc_check(obj, kind)`, assembled only in the stress build (`.ifdef GC_STRESS_BUILD`; in the default build the call sites are an empty macro), at the entry of `list_push`, `list_get`/index, `map_find`/`map_set`, `str_*` helpers, record field access and `call_value`. When the header says `OBJ_FREE`, or the kind does not match, it raises T0003 (`internal_error`) with "a reclaimed value was used" (contracts/error-codes-v3.md).
  - Run `gc_selfcheck` at every stress collection.
- [X] T033 [US6] Run `make gc-check`: `make STRESS=1`, `tests/run.sh --stress`, `python3 tests/gc/baseline.py --stress` (SC-003: benchmark reference solutions), `tests/fuzz.py --stress --iterations 3000 --seed 9` and `tests/gc_faults.py`. Fix every root or scan omission at its source until everything passes, and record the numbers.

**Checkpoint**: SC-003 and SC-004 are met.

---

## Phase 9: Polish & Cross-Cutting Concerns

- [X] T034 [P] Run `make lint` (all `src/*.s`, including `gc.s`, clean) and `make test`. Append any linter findings to `sesi-linter-issues.md`.
- [X] T035 [P] Reference budget: `tests/ref_examples.py` must pass at ≤ 15,000 tokens with no memory workaround text (SC-009). `grep -n "never reclaimed\|push.*join.*memory" docs/reference.md` must be empty.
- [X] T036 Update `docs/roadmap.md` (003 marked done, the memory limits revised) and `README.md` (remove "Not yet: a garbage collector").
- [X] T037 Write `specs/003-garbage-collector/results.md`: SC-001 to SC-009 with measured numbers (a workload table, perf before and after, the fault matrix, stress corpus counts, generic ratios, timeout stop times), plus deviations and follow-ups.
- [X] T038 Validate quickstart.md end to end; fix any command or expectation that changed.

---

## Dependencies & Execution Order

- **Setup (Phase 1)**: no dependencies. T001 (the baseline) MUST run before any `src/` change.
- **Foundational (Phase 2)**: needs Setup. Blocks every story.
- **US1 (Phase 3)**: needs Phase 2. It is the MVP and contains the collector; every later story needs it.
- **US2 (Phase 4)**: needs US1. The per-test reset and the regression sweep.
- **US3 (Phase 5)**: needs US1. It can run alongside US2, but touches `interp.s`, so coordinate.
- **US4 (Phase 6)**: needs Phase 2 only for the polling in runtime loops; the polling in `gc.s` needs US1. It can start right after Phase 2 for the non-collector call sites.
- **US5 (Phase 7)**: independent of the collector except through the perm brackets (US1). It can start after US1.
- **US6 (Phase 8)**: needs US1 (the fault hooks are in the marking code), and the US2 reset for the stress corpus.
- **Polish**: after everything.

## Parallel Examples

```text
# After Phase 2:
US1 tests (workloads, gc_bounds.py, edge cases)  ||  US4 tests and polling outside gc.s  ||  US5 perf pairs
# After US1:
US2 (testrun reset, regression)  ||  US3 (OOM report)  ||  US5 (memo, interning)  ||  US6 fault targets
```

## Implementation Strategy

- **MVP**: Phases 1–3. A working collector with bounded memory on the workloads; validate with quickstart US1 and the stress edge cases.
- **Then trust**: US2 (identical behavior, independent tests) and US3 (an honest out-of-memory error). Together with US1, that is the release criterion.
- **Then robustness**: US4 (timeout everywhere) and US5 (generic speed), which are independent of each other.
- **Finally the safety net** (US6). Running the fault matrix last proves the earlier phases have no hidden omissions.

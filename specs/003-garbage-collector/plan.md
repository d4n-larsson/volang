# Implementation Plan: vo-lang Memory Reclamation and Runtime Robustness

**Branch**: `003-garbage-collector` | **Date**: 2026-11-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/003-garbage-collector/spec.md`

## Summary

vo's runtime gets a collected heap, with no change to the language or its output. It has three parts.

**1. A non-moving mark-sweep collector** (research §1–§5):
- **Roots:** conservative. The native stack (after spilling the `s` registers), the live value stack, the program's globals and the permanent arena.
- **Objects:** precise boundaries, conservative contents, with interior pointers supported.
- **Heap:** size-classed 64 KiB pages for small objects, and page runs for large ones.
- **Determinism:**
  - every region is mapped at a fixed address;
  - collections are triggered only by allocation volume;
  - free lists are rebuilt in address order;
  - the heap is reset before every test.
- **Out-of-memory** (R0006) now means live data over a configurable limit (`--max-memory`), and `--mem-stats` reports usage.

**2. Run-time robustness:**
- A work counter in every long runtime loop enforces `--timeout` inside builtins and the collector.
- Interned type-argument vectors and memoized resolution remove the per-call substitution walk and allocation in generic code.

**3. Verification:**
- The five SC-001 workloads with memory bounds.
- A stress build (`make STRESS=1`) that collects constantly, with poison and kind checks. It is not part of the shipped binary.
- A fault-injection matrix that proves the stress suite catches a missing root in each of nine kinds.

## Technical Context

**Language/Version**: RISC-V assembly (RV64IMAFD, lp64d), GNU as. This is the existing v2 toolchain, extended in place.

**Primary Dependencies**: None at run time. To build: `binutils-riscv64-linux-gnu`. To run on x86: `qemu-user`. To lint: the SESI `riscv64` pack.

**Storage**: N/A. The collected heap is in-process memory.

**Testing**:
- The existing gate: golden cases, determinism repeats, the fuzzer, `apply_fixes`, `catalog`, `ref_examples`, `fmt_corpus`, `migrate`, `record_changes`, `scope_fuzz`.
- New:
  - `tests/gc/` workloads and `tests/gc_bounds.py`;
  - `tests/run.sh --stress` and `tests/fuzz.py --stress`;
  - `tests/gc_faults.py`;
  - timeout cases in builtins;
  - generic-speed pairs in `tests/perf.sh`;
  - `bench2/validate.py` as a regression check.

**Target Platform**: Linux user space, RV64 (qemu-riscv64 on x86 hosts).

**Project Type**: Compiler front end and interpreter, as a CLI.

**Performance Goals**:
- Checking and running a 1,000-line program stays under 1 s (currently 87–95 ms).
- Existing performance programs run at most 20% slower.
- The SC-001 workloads allocate at least 1 GiB in total with peak live memory under 64 MiB.
- Generic hot loops run at most 1.5× slower than the type-specific version.

**Constraints**:
- Byte-identical output for every existing program.
- Deterministic collection points and out-of-memory step.
- No new language surface: `--max-memory` and `--mem-stats` are CLI options. Stress mode exists only in a separate maintainers' build, so the shipped binary has no undocumented surface.
- The reference stays within 15,000 tokens (currently about 8,160).

**Scale/Scope**:
- About 2.5–3.5k new lines of assembly: `gc.s`, plus changes to `sys.s`, the `rt_*` files, `types.s` and `interp.s`.
- About 15 new golden cases, 6 new workloads, and 9 fault-injection builds.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluated against constitution **v1.1.0**.

| Gate | Status | Evidence / obligations |
|---|---|---|
| I. Agent-first design | PASS | No language change. Two CLI options in the existing style. The reference loses a workaround rather than gaining rules. |
| II. Machine-readable feedback | PASS | R0006 gains structured `in_use` and `limit` fields. `--mem-stats --json` emits a `memory` record. Schema version 1 is only extended (contracts/output-schema-v3.md). |
| III. Determinism and defined behavior | PASS | Fixed-address regions, allocation-triggered collection, ordered sweeps and per-test heap reset make collection, reports and out-of-memory identical across runs (research §3). Every failure is a structured error (R0006, R0009, T0003). |
| IV. Test-first through the CLI | PASS | **Obligation**: workloads, out-of-memory determinism cases and timeout cases are golden cases or scripts written before the collector. The stress corpus and fault matrix are gate scripts. |
| V. Single source of truth, minimal surface | PASS | **Obligations**: the reference and catalog are updated in the same tasks as the behavior. Shortcuts carry `ponytail:` markers: conservative false retention, the full scan of the permanent arena, the mark-stack overflow fallback. |
| Implementation constraints | PASS | Assembly only. `gc.s` is lint-clean; fault-injection variants are built with `--defsym`, not separate sources. Python is used only in `tests/`. |
| Quality gates | PASS | `make test` gains `gc_bounds.py` and `tests/gc/baseline.py`. The stress golden pass, the stress benchmark-solution pass, the stress fuzz run and `gc_faults.py` are the separate `make gc-check` target, since they are slow. |

**Post-design re-check**: PASS. The design adds no requirements beyond the spec. The one design-level risk, conservative retention breaking determinism, is closed by fixed addresses (research §3).

## Project Structure

### Documentation (this feature)

```text
specs/003-garbage-collector/
├── plan.md
├── research.md                # decisions (Phase 0)
├── data-model.md              # heap regions, pages, object headers, roots, caches, reports
├── quickstart.md              # validation scenarios
├── contracts/
│   ├── memory-model.md        # what is collected, roots, determinism, limits, stress mode
│   ├── cli-v3.md              # --max-memory, --mem-stats
│   ├── output-schema-v3.md    # memory record, R0006 fields
│   └── error-codes-v3.md      # R0006 meaning, T0003 for use after reclaim
└── tasks.md                   # /speckit-tasks
```

### Source Code (repository root)

```text
src/
├── defs.inc        # + heap constants: page size, size classes, OBJ_FREE, flags
├── gc.s            # NEW: heap reservation, gc_alloc / gc_alloc_bytes, mark, sweep, roots, stress, reports
├── sys.s           # alloc becomes a router (permanent / collected, perm brackets); fixed-address mappings
├── interp.s        # program start: switch to the collected heap; R0006 report; per-test reset hook
├── testrun.s       # heap reset before every test; per-test memory records
├── text.s          # string-builder and string bytes from gc_alloc_bytes; budget polling in large copies
├── rt_list.s       # list buffers; budget polling in sort, copy, slice
├── rt_map.s        # map arrays; budget polling in grow and insert
├── rt_func.s       # budget polling in the HOF loops (callbacks already tick)
├── rt_render.s     # budget polling in render_value
├── types.s         # interned type-argument vectors; memo of rt_targs / rt_concrete; perm brackets
├── effects.s       # memoized es_subst
└── main.s          # --max-memory, --mem-stats
docs/reference.md   # remove the memory caveat; document the limit and the report
docs/errors.md      # R0006 text and fields
tests/gc/           # SC-001 workloads, out-of-memory determinism, deep structures
tests/gc_bounds.py  # SC-001 / SC-005 checks from --mem-stats --json
tests/gc_faults.py  # SC-004 fault matrix
tests/cases/run/    # timeout-in-builtin cases (SC-007)
```

**Structure Decision**:
- The collector is one new file, `gc.s`. Every other change is local: allocation call sites switch to `gc_alloc_bytes` where the memory is byte data, and the runtime's long loops add polling.
- Fault injection uses assembler symbols (`.ifdef GC_FAULT_*`) inside `gc.s`, so the production binary is unchanged.

## Complexity Tracking

No constitution violations. Deliberate shortcuts, each with its ceiling:

| Shortcut | Ceiling | Upgrade path |
|---|---|---|
| Conservative roots and object contents | An integer that equals a heap address keeps that object alive. This is deterministic, but costs memory | Precise stack maps for frames, and typed object layouts from the checker's types |
| Full scan of the permanent arena at every collection | Collections in programs with very large ASTs cost proportionally more | A remembered set for writes into the permanent arena |
| Non-moving heap (no compaction) | Fragmentation in long-running programs with mixed sizes; size classes bound it to about 33% per class | Mostly-copying evacuation of sparse pages |
| Wall-clock check every 2^16 work units | The timeout can overshoot by one unit's worth of work | None needed (the units are small) |

# Data Model: The Toolchain in C

The port changes no data a user sees. These are the internal structures the two languages share
during the mixed phase, and the records the port itself keeps.

## Shared layouts (research R14)

Every layout in `src/defs.inc` keeps its offsets: tokens (16 bytes), AST nodes (`N_KIND`,
`N_START`/`N_END`, `N_FILE`, `N_TYPE`, `N_A`–`N_E`), types (`TY_*`), effect sets (`ES_*`), FX records,
file records (`FILE_*`), vectors (`VEC_N`, `VEC_CAP`, `VEC_DATA`), strings (`STR_LEN`, `STR_DATA`),
string builders (`SB_*`), heap objects (header `size:u32 | kind:u32`), lists, maps, closures,
compiled functions (`CF_*`), 32-byte instructions (`I_*`), frames (`FR_*`), recording events
(`EV_*`).

- C declares one struct per layout; every field is checked against the `.equ` value with
  `_Static_assert(offsetof(struct node, a) == N_A, ...)`.
- Object sizes stay the same, so the heap and permanent-arena allocation sequence is identical to
  the assembly release's, which is what memory figures and the fixed-address determinism depend on.
- All targets are little-endian with 8-byte pointers and `long`, so the layouts are valid on all three.

## Address map (research R5)

Unchanged: heap `0x10_0000_0000`, page table `0x21_…`, mark stack `0x22_…`, value stack `0x23_…`
(32 MiB), permanent arena `0x24_…`, code `0x28_…`, native stack `0x30_…` (64 MiB). The instrumented
x86-64 build adds a constant offset (R9).

## Root stack (research R4)

`gc_roots`: an array of `void *` at the fixed address `ADDR_ROOTS` (a new `defs.inc` constant, 1 Mi
entries = 8 MiB reserved, mapped on first use like the value stack), with a top index
`gc_roots_n`. `GC_ROOT(&p)` pushes the address of a C local; `GC_UNROOT(n)` pops n. The collector
marks from `*entry` for every entry.

- Sizing: every recursion that roots values per level is bounded by a research R7 limit, so the
  maximum use is (limit × roots per level); the stress build records the high-water mark and
  fails if it exceeds 1/8 of the reservation anywhere in the suite. Reaching the reservation means
  a missing R7 limit: T0003.
- Invariant: balanced on every return path, checked by the stress build (an unbalanced root stack
  when `vm_run` returns to `run_main` or the test runner is T0003).

## Platform layer interface (contracts/platform-layer.md)

`_start`, `vo_syscall0..6`, `jmp_save`/`jmp_restore` with a jump buffer of `JB_SIZE` bytes
(callee-saved registers, `sp`, `ra`/return address; per-architecture size, opaque to C),
`gc_spill_registers` while conservative scanning remains.

## Migration step (results.md log)

| Field | Meaning |
|---|---|
| step | sequence number, stage (R2) |
| files | assembly files (or parts) replaced |
| asm lines removed / C lines added | sizes |
| gate | pass, with the date; a failing step is not recorded until it passes |
| speed | geometric-mean ratio against `build-asm/vo` under qemu |
| notes | audit findings (R13), deliberate deviations (R4 memory figures) |

States of a file: assembly → ported (C, gate passed) → (stage 8) built natively on all platforms.

## Build variants (contracts/build.md)

| Variant | Target | Purpose |
|---|---|---|
| `build/vo` | RV64 mixed (stages 1–7), then native host | the toolchain |
| `build-stress/vo` | same, `STRESS=1` | collector stress (spec 003) |
| `build-ubtrap/vo` | same, UB and bounds traps | per-step UB detection (R9) |
| `build-asm/vo` | RV64 assembly at `ASM_REV` | behavioral oracle and speed baseline |
| `build-rv64/vo`, `build-arm64/vo`, `build-x86_64/vo` | each architecture (stage 8) | cross-platform checks |
| `build-instrumented/vo` | hosted x86-64, ASan + UBSan (stage 8) | memory-error detection (SC-005) |

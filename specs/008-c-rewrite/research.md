# Research: The Toolchain in C (Native Builds)

Facts from the assembly toolchain (after spec 007) that shape the port:

- 45 files, about 53,000 lines. The runtime files link first, at fixed page offsets, for qemu's sake.
- Fixed address map (`defs.inc`):
  - heap `0x10_0000_0000`
  - page table `0x21_0000_0000`
  - mark stack `0x22_0000_0000`
  - value stack `0x23_0000_0000`
  - permanent arena `0x24_0000_0000`
  - code `0x28_0000_0000`
  - native stack `0x30_0000_0000` (64 MiB, switched to in `_start`)
- The collector (`gc.s`) is non-moving mark-sweep. Its roots are scanned conservatively: the `s` registers, the native stack, the value stack, the globals and the permanent arena.
- `vm.s` keeps pc, frame and slots in `s1`–`s3` across direct-threaded handlers. `trace.s` handlers share that convention.
- `jmp.s` provides `jmp_save`/`jmp_restore` (setjmp/longjmp) for `try` propagation and test recovery.
- Every assembly function has a C prototype comment, enforced by SESI.
- Recursion limits:
  - The parser: an explicit 5,000-level limit, plus a native-stack guard.
  - Callbacks into vo (`call_fn`): R0005 when the native stack runs low.
  - vo-to-vo calls: run on the value stack.
- Installed tools: gcc 13.3 (x86-64), `qemu-riscv64`, `qemu-aarch64`. Not yet installed: `gcc-riscv64-linux-gnu`, `gcc-aarch64-linux-gnu`.

## R1: Port strategy: a mixed RISC-V toolchain, one file at a time

- **Decision:**
  - C compiled for RV64 (lp64d) links with the remaining assembly into one `build/vo`, run under `qemu-riscv64` as today.
  - A migration step ports one assembly file (or one coherent part of a large one), keeping every global symbol's name and signature. The prototype comments already are C prototypes.
  - The step deletes the assembly and must pass the gate (R10) before the next step starts.
  - Native x86-64 and ARM64 builds come once only the platform layer is assembly.
- **Rationale:**
  - The calling convention and data layouts are shared, so either side can call the other.
  - Every bug stays local to one step, and the whole existing suite is the oracle at every step (spec US3, FR-018).
- **Alternatives considered:**
  - A separate C toolchain switched over at the end: rejected, because nothing is verified until late.
  - A big-bang rewrite: rejected.

## R2: Order of the steps

- **Decision:** dependencies first, the engine last. Within a stage, files go in the order listed.
  1. **Leaf code:** `text`, `num`, `sha256`, `stdprim` (big integers and float text), `ast` (vectors), `data` (embedded documents, R8).
  2. **Front end:** `lex`, `parse`, `diag`, `fmt`.
  3. **Checker:** `types`, `effects`, `check`, `check_expr`, `check_func`, `check_generic`, `check_effects`, `loader`, `packages`, `effects_record`.
  4. **Commands:** `query`, `edit`, `doc`, `replay`, `testrun`, `main`.
  5. **Compiler:** `compile`.
  6. **Runtime:** `rt_str`, `rt_list`, `rt_map`, `rt_render`, `rt_io`, `rt_func`, `rt_call`, `rt_test`, `rt_effects`, `net`, `prop`, `interp`.
  7. **Engine:** `vm` and `trace` (R6), `gc` (R4), then `sys` and `jmp`, which become the platform layer (R5).
  8. **Native:** the x86-64 and ARM64 platform layers, the native builds, the instrumented build (R9), cross-platform checks, then removal of the qemu page pins (FR-013).
- **Rationale:**
  - Leaf and front-end code is pure and heavily covered by golden cases, so it is the cheapest place to settle the C conventions.
  - The engine depends on everything, and its speed is measured (R11).
  - The collector goes after all heap-touching code is C, so precise native roots (R4) can be switched on at once.
- **Alternatives considered:**
  - Runtime first, for early speed data: rejected. The VM's speed cannot be judged while it calls assembly builtins, and the runtime is what the collector's roots depend on.

## R3: Compiler, language level and flags

- **Decision:**
  - GCC 13 for all three targets: native `gcc` for x86-64, plus `gcc-riscv64-linux-gnu` and `gcc-aarch64-linux-gnu`. One compiler family keeps code generation questions uniform.
  - Language: `-std=gnu11`, which is C11 plus the two GNU extensions the engine needs: labels as values for the VM dispatch (R6), and `returns_twice`/`noreturn` attributes (R5).
  - Common flags: `-O2 -ffreestanding -fno-builtin -nostdlib -static -fno-pie -no-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-strict-aliasing -ffp-contract=off -fno-math-errno -fno-trapping-math`.
  - RISC-V extra flags: `-march=rv64imafd -mabi=lp64d -mcmodel=medany -mno-relax -msmall-data-limit=0`. These avoid gp-relative data, since the assembly never sets `gp` and links with `--no-relax`.
  - Warnings, with zero allowed (FR-012): `-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wvla -Wimplicit-fallthrough -Wundef -Wdouble-promotion -Wformat=2`.
  - The C sources provide `memcpy`, `memmove`, `memset` and `memcmp` themselves: GCC may emit calls to them even with `-fno-builtin`.
- **Rationale:**
  - `-ffp-contract=off` matters: GCC contracts `a*b+c` into `fmadd` on RV64 and ARM64 at `-O2` by default, which changes rounding (constitution: IEEE double per operation).
  - x86-64 uses SSE2 for `double`, so no extended precision arises.
  - `-fno-strict-aliasing` because heap objects are typed by a header word and read through different views.
- **Alternatives considered:**
  - clang: rejected because it is not installed, and two compilers double the questions.
  - Strict `-std=c11`: rejected. It rules out computed goto, and a `switch` dispatch is measurably slower (R6).

## R4: Collector roots: conservative native stack in the mixed toolchain, precise in the C one

- **Decision:**
  - While any assembly remains, `gc_collect` keeps scanning the `s` registers and the native stack conservatively. This covers C frames too, since they live on the same stack.
  - From the first C step, C code that holds a heap reference across an allocation registers it with `GC_ROOT(p)` / `GC_UNROOT(n)`. These push and pop a root stack in a fixed-address region (`ADDR_ROOTS`), which `gc.s` scans; a small change made in the foundational phase.
  - The region reserves 1 Mi entries (8 MiB, mapped like the value stack). Every recursion that roots values per level is bounded by an R7 limit, so its maximum use is (limit × roots per level), well below the reservation. `tests/cases` gains no case, but T008's stress check asserts the high-water mark stays below 1/8 of the reservation across the suite. Overflowing it would mean a missing R7 limit: T0003.
  - When `gc` is ported (stage 7), native-stack scanning is removed. Roots become: the value stack, the root stack, the globals and the permanent arena.
  - The stress build (collect every allocation, poison reclaimed memory, spec 003) then proves that no reference is missing.
  - A build flag (`PRECISE_ROOTS=1`) can try precise roots earlier for the C files already ported.
- **Rationale:**
  - Conservative scanning of a C stack makes retention depend on the compiler and the architecture: which stale words a frame leaves differs. That would make `--mem-stats` figures and out-of-memory points differ between platforms (FR-001, FR-008).
  - The value stack, root stack, globals and permanent arena hold the same words on every platform. The heap is at a fixed address and allocation order is identical.
- **Consequence:**
  - Retention can only shrink compared with the assembly release, where stale words kept extra objects.
  - The cases that print memory figures: `cli/mem_stats_*` (MiB, rounded), `prop/mem_stats` (exact, but with 0 collections) and `cli/max_memory_*` (option errors). None of them is expected to change.
  - Any figure that does change is recorded in `results.md`. This is the spec's one exception to byte identity with the assembly release (FR-001): memory figures must be identical across platforms, and may only be smaller than the assembly release's.
- **Alternatives considered:**
  - Conservative scanning of the C stack (deterministic per platform only): rejected by FR-008.
  - A moving or precise-by-type collector: rejected as out of scope.

## R5: The platform layer

- **Decision:** per architecture (`src/platform/{riscv64,x86_64,aarch64}.S`, at most 300 lines each, SC-008):
  - `_start`: maps the fixed 64 MiB native stack at `ADDR_STACK` as today, switches to it and calls `vo_main(argc, argv, envp)`.
  - `vo_syscall0`–`vo_syscall6`.
  - `jmp_save`/`jmp_restore` (`returns_twice` in the C declaration; C locals live across it are `volatile`).
  - No signal handling: the toolchain installs no handlers (timeouts are deadline checks with `ppoll` and the clock), so no `rt_sigreturn` restorer is needed on x86-64.
  - Everything else is C: wrappers, the arena, buffered output.
- **System calls:**
  - Only calls with the same semantics on all three architectures: the `*at` family, `ppoll`, `clock_gettime`, `mmap`/`munmap`, `getdents64`, `statx`, `socket`/`connect`/`sendto`/`recvfrom`.
  - Numbers come from one table per architecture. RISC-V and ARM64 share the generic table; x86-64 has its own.
  - `statx` replaces `fstat`, so `struct stat`, whose layout differs on x86-64, is never used.
- **Fixed addresses:** the address map stays as it is. Every address is below 2^39, which is valid user space on all three (RV64 Sv39 is the smallest at 256 GiB), and far from where a non-PIE static binary is loaded.
- **Alternatives considered:** inline-assembly syscalls in C headers: rejected, because they spread assembly over C files that SESI and the line budget cannot see.

## R6: The VM in C

- **Decision:**
  - `vm_run` becomes one C function with a direct-threaded dispatch: each instruction's handler is a `&&label` stored in the 32-byte instruction, and each handler ends with `goto *pc->op`. The format and the compiled-function layout stay the same.
  - pc, frame and slots are local variables, so the compiler keeps them in registers.
  - The tracing handlers (`trace.s`) become a second label table, swapped in when `--trace` is on, as now.
  - Calls out to runtime functions pass the instruction's node explicitly; there is no hidden `s1`.
- **Rationale:** this is the same structure as `vm.s`. GCC generates comparable code for labels-as-values dispatch, and SC-004 (at most 10% slower under qemu) checks it.
- **Alternatives considered:** a `switch` dispatch (portable C11): kept as the fallback if SC-004 is met with it, which would drop the GNU extension.

## R7: Recursion limits independent of native frame sizes

- **Decision:**
  - Every native recursion with a documented limit uses an explicit counter:
    - the parser's 5,000 levels (already explicit);
    - the checker's and formatter's matching walks;
    - value rendering;
    - callback re-entry in `call_fn`: R0005 after a fixed number of nested native re-entries.
  - The re-entry limits are measured on the assembly release **per builtin that calls back** (`map`, `filter`, `fold`, `any`, `all`, `sort_by`, and the property runner's calls): for each, the depth at which a program recursing only through that builtin reports R0005. Each builtin gets its own limit (a table indexed by builtin id), recorded in `results.md`. Mixed recursion (through several builtins) counts each frame against the limit of the builtin that entered it, and stops at the first exceeded limit. Where the assembly's depth for mixed recursion cannot be matched this way, the difference is recorded as a deviation.
  - The native-stack guards stay only as a last line of defence, at a lower threshold than any counter can reach.
- **Rationale:** C frames differ in size per architecture and per compiler flag. A limit that depends on stack bytes would trigger at different depths and give different traces (FR-009).

## R8: Embedded documents

- **Decision:** a build script (`tools/embed.py`, a developer script) turns `docs/reference.md`, `docs/errors.md` and `std/*.vo` into `build/embedded.c`: byte arrays plus the `std_files` table. `data.s` (`.incbin`) is deleted in stage 1.
- **Rationale:** portable to every target, and no assembly needed. The constitution allows scripts for building.

## R9: Instrumented builds

- **Decision:**
  - **During the mixed phase:** the RV64 C objects are also built with `-fsanitize=undefined,bounds -fsanitize-trap=all` (no runtime library needed, so it works freestanding and under qemu). The collector stress build runs with them.
  - **Once native (stage 8):** `make instrumented` builds a hosted x86-64 variant with `-fsanitize=address,undefined`, linked with the system libc only for the sanitizer runtimes. `main()` calls `vo_main`, and the platform layer stays the same. Two things differ in this variant:
    - The heap base and other fixed regions move above ASan's shadow memory: `0x1000_0000_0000` + offsets, a build constant. Figures do not depend on addresses.
    - The collector poisons free slots and page tails with `__asan_poison_memory_region`, so accesses to dead objects are reported.
  - The full suite must pass under it with 0 reports (SC-005, constitution).
- **Constitution:** 1.3.0 names exactly this phasing: UB and bounds traps plus the stress build while assembly remains, address sanitizing from the first native build.
- **Rationale:**
  - ASan does not support static or freestanding binaries, and the fixed heap at `0x10_0000_0000` lies in ASan's x86-64 shadow gap.
  - UB traps cover the C-level undefined behavior from the first step.
  - Memory errors inside the collected heap need the poisoning hooks in either case.
- **Alternatives considered:** valgrind on the static binary: rejected, because it cannot see object boundaries inside the heap mapping.

## R10: The per-step gate and the oracle

- **Decision:**
  - `build-asm/vo`: the last assembly release (the spec 007 merge commit, `ASM_REV` in the Makefile), built like `build-base/vo`.
  - `make gate` runs, in order:
    0. `make lint` (SESI on the remaining assembly, the strict-warning C build);
    1. `make test` (golden, step, catalog, reference, conformance, fuzzers, determinism, engine comparison);
    2. `make gc-check` (stress);
    3. the UB-trap build's suite;
    4. `tests/diff_asm.py --count 2000` (the generated-program differential, R12);
    5. `tests/gc/baseline.py` (the 703 bench2 reference runs);
    6. `tests/speed.py --baseline build-asm/vo`, informational until stage 7.
  - Each step's result goes into `results.md` (the migration log: file, lines of C, gate result, speed ratio).
  - SC-002's 10,000 programs run at the end of stages 4, 7 and 8, and on each native platform.
- **Rationale:** the full differential takes too long for every step. 2,000 programs per step plus the golden suite catch regressions within the step that caused them.

## R11: Speed

- **Decision:**
  - Under qemu, the mixed toolchain is compared with `build-asm/vo` by `tests/speed.py --baseline` at every step: informational before the VM is ported, enforced at 10% (geometric mean, SC-004) from stage 7 on.
  - Natively on x86-64 (stage 8), `tests/speed.py` compares the native build with `build-asm/vo` under qemu on the same machine (SC-003: at least 3 times faster, no program slower).
  - The page-offset pins (`.org`/alignment in `vm.s`, `rt_map.s`, `rt_list.s`, and the fixed runtime link order) disappear as those files are ported.
- **Rationale:** the pins compensated for qemu's translation-block boundaries in hand-placed assembly. Compiled C is laid out by the linker, and the native target, the one that matters, has no such effect.

## R12: Differential testing against the assembly release

- **Decision:** `tests/diff_asm.py` reuses `tests/gen_programs.py` and `tests/diff_engines.py`'s comparison:
  - it runs each generated program on `build-asm/vo` (qemu) and on the build under test (native or qemu);
  - it compares stdout, stderr and exit status;
  - disagreements go to `build/diff-failures/<seed>/`.
  
  With `--platform aarch64|riscv64`, it runs the C build of that architecture under its qemu.
- **Rationale:** generated programs exercise combinations the golden cases do not. The tooling exists since spec 004.

## R13: Behavior that the hardware decided in assembly

- **Decision:** before porting each file, its assembly is audited for operations whose RISC-V result the code relies on, and the C version computes them explicitly:
  - **Division:** RISC-V `div`/`rem` by zero gives -1 / the dividend, and the minimum integer divided by -1 gives the minimum, with no trap. x86-64 traps on both, and C makes them undefined.
  - **Shift counts:** RISC-V uses the low 6 bits; in C, a count of 64 or more is undefined.
  - **Float to integer conversion:** RISC-V saturates; in C, an out-of-range conversion is undefined.
  - **Explicit rounding modes:** `fcvt` with `rne` (`src/num.s:380`, round half to even) or `rdn`/`rup` has no C operator: a cast truncates, and `rint`/`nearbyint` need libm or, on x86-64, SSE4.1 (above the SSE2 baseline). Such conversions are written with integer and IEEE arithmetic (for example, adding and subtracting 2^52 for round-half-even on values below 2^52), never as library calls. `rtz` conversions (`prop.s:1241`, `rt_str.s:511`) are a range check plus a cast.
  - **Fused operations:** the assembly uses none (no `fmadd`/`fmsub`), so `-ffp-contract=off` reproduces it exactly.
  - **Unsigned and signed overflow** that relied on wrapping: written with unsigned arithmetic.
  - **NaN:** RISC-V produces canonical positive NaNs, x86-64 negative ones. Formatting and anything that exposes bits must ignore a NaN's sign and payload. `str` prints `nan`; JSON refuses NaN.
- **Rationale:** these are exactly the places where "the same C" would behave differently per platform (FR-008). The UB-trap build (R9) catches what the audit misses.

## R14: Shared layouts during the mixed phase

- **Decision:**
  - `src/defs.inc` stays the single source of numeric constants and offsets while any assembly uses them.
  - `tools/gen_defs.py` generates `build/defs.h`: the same `.equ` values as `#define`s. C code declares structs for nodes, types, effect sets, frames and objects, each field checked with `_Static_assert(offsetof(...) == N_A)` and so on.
  - `tools/gen_protos.py` generates `build/protos.h` from the prototype comments of the remaining assembly, so C calls assembly with checked types.
  - In stage 8, the C headers become the source and `defs.inc` shrinks to what the platform layer needs.
- **Rationale:** one source of offsets means the two languages cannot disagree at a boundary (spec edge case "Mixed states"). The static assertions turn any drift into a build error.

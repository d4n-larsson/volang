# Results: The Toolchain in C (Native Builds)

## Success criteria

| SC | Status |
|---|---|
| SC-001 | x86-64: met (441 golden cases, 91 step cases, the reference examples and the catalog; see "Native x86-64"); ARM64 and RISC-V: T057 |
| SC-002 | open |
| SC-003 | met on the development machine: native 7.4 times faster than `build-asm/vo` under qemu (geometric mean ratio 0.136), every program faster |
| SC-004 | open |
| SC-005 | open |
| SC-006 | met: a fresh checkout builds with `make` in 6.4 s (serial) on the development machine |
| SC-007 | open |
| SC-008 | open |
| SC-009 | open |

## The oracle

`ASM_REV` = `bc610cb` (spec 007, merged incomplete). `build-asm/vo` has the same code and data sections as the
tree it was built from (`objcopy` of `.text`, `.rodata`, `.data` compared; only debug paths differ).

Stress check of the oracle (T001, run in its worktree): all 435 golden cases under stress, 703/703 bench2
runs, 3,000 fuzz iterations, every collector fault kind caught, 1,000 generated programs with 0
disagreements against the tree walker, 91 step cases. All passed.

Speed baseline (T003, `tests/speed.py --baseline build-asm/vo`, qemu on the development machine, after
step 1): closures_generics 388 ms, fib 182, int_loop 398, list_work 285, map_count 472, str_build 612.

## Migration log

| Step | Stage | Files | Asm lines removed | C lines added | Gate | Speed (qemu, vs build-asm) | Notes |
|---|---|---|---|---|---|---|---|
| 0 | setup | (mixed build, `vo.h`, root stack in `gc.s`, 15-word jump buffer) | | 41 (`vo_c.c`) | `make ubtrap` 435/435 | | the jump buffer now saves `gc_roots_n`, so `jmp_restore` drops roots of skipped C frames |
| 1 | 1 | `ast.s` → `ast.c` | 109 | 46 | gate-step passed (7m50s) | 0.975–1.013 | no hardware-defined behavior |
| 2 | 1 | `text.s` → `text.c` | 597 | 236 | stage-1 gate (below) | | `mem_copy` stays a forward byte loop (overlap semantics); `GC_ROOT` where a heap pointer is held across an allocation |
| 3 | 1 | `sha256.s` → `sha256.c` | 342 | 113 | stage-1 gate | | package hashes unchanged (19 step cases) |
| 4 | 1 | `num.s` → `num.c` | 523 | 277 | stage-1 gate | | R13: `fcvt.l.d … rne` as the 2^52 add-and-subtract; `mulhu` overflow test as a comparison; negation through unsigned. `str` of 60,000 floats identical to `build-asm/vo` |
| 5 | 1 | `stdprim.s` → `stdprim.c` | 794 | 328 | stage-1 gate | | `unsigned __int128` for `mulhu`; floor division instead of an arithmetic right shift of a negative product (R13). 20,000 random doubles through the JSON driver: 5.6 MB identical |
| 6 | 1 | `data.s` → `data.c` + `tools/embed.py` | 186 | 49 + 39 | stage-1 gate | | `ref_md` gets its own NUL (its only readers stop inside it); `vo reference` byte-identical |
| 7 | 2 | `lex.s` → `lex.c` | 1015 | 335 | stage-1 gate (with steps 2-6) | | allocation order of diagnostics and fixes kept |
| 8 | 2 | `parse.s` → `parse.c` | 3802 | 1459 | gate below | | `p_last_diag` compares the code pointer: one static `e0110` for all parser E0110s; `p_deep` reads `__builtin_frame_address(0)`; the 5,000-level counter decides first (R7). 5,000 mutated sources: `vo check --json` and `vo run` byte-identical to `build-asm/vo` |
| 9 | 2 | `diag.s` → `diag.c` | 1974 | 598 | stage-2 gate | | `levenshtein` keeps its early exit and arena row; a muted `diag_add` builds but neither records nor counts |
| 10 | 2 | `fmt.s` → `fmt.c` | 1711 | 642 | stage-2 gate | | `vo fmt` on 700 files (cases, benchmarks, std) byte-identical to `build-asm/vo`; 600 `fmt --check` runs identical |
| 11 | 3 | `types.s` → `types.c` | 1387 | 448 | stage-3 gate | | memo hash with 64-bit wrapping multiplication (`unsigned long`); `GC_ROOT` in the interning functions, which can run during programs |
| 12 | 3 | `effects.s` → `effects.c` | 1375 | 461 | stage-3 gate | | scope normalization and pair order unchanged; record_changes (24 sets) and scope_fuzz (300 paths) clean |
| 13 | 3 | `check.s` → `check.c` | 3124 | 986 | stage-3 gate | | expression arguments that allocate are sequenced into locals (C leaves argument order open); `GC_ROOT` in `ty_make`; `vo check --json` on all 1,104 tracked `.vo` files identical to `build-asm/vo` |
| 14 | 3 | `check_expr.s` → `check_expr.c` | 2932 | 880 | stage-3 gate | | the depth guard compares `__builtin_frame_address(0)` with `stack_floor` + 128 KiB as before; builtin tables as designated initializers; `vo check --json` on 1,104 files identical |
| 15 | 3 | `check_func.s` → `check_func.c` | 858 | 271 | stage-3 gate | | capture slots and the E0505 fix text unchanged; the context record is a struct (72 bytes, as `CX_SIZE`) |
| 16 | 3 | `check_generic.s` → `check_generic.c` | 957 | 248 | stage-3 gate | | bindings arrays as `Type **`; E0310 annotation fix (`infer_fix`) unchanged |
| 17 | 3 | `check_effects.s` → `check_effects.c` | 913 | 313 | stage-3 gate | | `builtin_effects` stays a 64-byte table (read by `rt_call.s`, `rt_effects.s`); call edges as a 24-byte struct |
| 18 | 3 | `loader.s` → `loader.c` | 1128 | 353 | stage-3 gate | | import DFS, cycle chain and the missing-import directory scan (`getdents64`) unchanged |
| 19 | 3 | `packages.s` → `packages.c` | 1418 | 478 | stage-3 gate | | manifest parse, content hash and `vo package` unchanged; entry record as an 80-byte struct |
| 20 | 3 | `effects_record.s` → `effects_record.c` | 1711 | 549 | stage-3 gate | | record parse, compare (rename pairing) and `vo effects` output unchanged |
| 21 | 4 | `query.s` → `query.c` | 3087 | 1050 | stage-4 gate | | impact roles stay compared by address; a removed name (a str) still tells itself from an item by the kind field, as before. 19,874 `vo query` runs (refs, callers, type-at, human and JSON, every tracked file) and 2,206 `vo impact` runs on generated diffs identical to `build-asm/vo` |
| 22 | 4 | `edit.s` → `edit.c` | 2178 | 686 | stage-4 gate | | the `unlinkat` call moved into a `sys_unlink` wrapper in `sys.s` (platform layer); the helpers that read their caller's registers take parameters. 7,344 dry-run edits (rename, add-field, human and JSON) and 144 written renames on copies identical to `build-asm/vo` |
| 23 | 4 | `doc.s` → `doc.c` | 1739 | 563 | stage-4 gate | | `vo doc` for every builtin and library item and their misspellings (692 runs, human and JSON) identical to `build-asm/vo`; doc_conformance, ref_examples and catalog clean |
| 24 | 4 | `replay.s` → `replay.c` | 1913 | 701 | stage-4 gate | | the recording writer still allocates nothing (fixed buffers on the stack); the `io_*` wrappers root what they hold across allocations. 400 programs recorded (byte-identical recordings except wall-clock `now_ms` values) and 2,110 replays of them and of mutated recordings identical to `build-asm/vo` |
| 25 | 4 | `testrun.s` → `testrun.c` | 600 | 191 | stage-4 gate | | `jmp_save` is redeclared `RETURNS_TWICE` in `vo.h` (the generated prototypes carry no attributes); the locals used after it are `volatile`; R0009 is still recognized by the address of `r0009`. `vo test` (human, JSON, `--filter`, `--seed`) on all 204 files with tests identical apart from timings, except `tests/gc/edge/oom_in_test.vo` (see Deviations) |
| 26 | 4 | `main.s` → `main.c` | 1898 | 543 | stage-4 gate | | 1,081 command lines (every command and option error, `explain` for all 91 catalog codes, `check`/`fmt --check`/`test` on 400 case directories, human and JSON) identical to `build-asm/vo`, after keeping one assembly defect (see Deviations) |
| 27 | 5 | `compile.s` → `compile.c` | 2053 | 733 | stage-6 gate (with steps 28-31) | | instructions still take their handler addresses from `vm_ops`/`vm_ops_trace` (`c_ops`); the code arena stays at `ADDR_CODE` |
| 28 | 6 | `rt_str.s`, `rt_list.s`, `rt_map.s` → C | 1556 | 540 | stage-6 gate | | FNV-1a and growth policies unchanged; the page pins are gone (speed below); `GC_ROOT` where values live across allocations |
| 29 | 6 | `rt_render.s`, `rt_io.s`, `rt_test.s` → C | 849 | 283 | stage-6 gate | | `render_value` has no depth limit, as before (values are acyclic); `rt_expect*` return through `jmp_restore` |
| 30 | 6 | `rt_func.s`, `rt_call.s` → C | 865 | 281 | stage-6 gate | | `btab` is a `switch` bounded by `B_LAST`; `builtin_call` takes no argument addresses (see Deviations: a stale stack word) |
| 31 | 6 | `rt_effects.s`, `net.s` → C | 816 | 300 | stage-6 gate | | `clock_gettime`, `readlinkat` and `getcwd` moved into `sys.s` wrappers (`sys_clock_ns`, `sys_readlink`, `sys_getcwd`), as `unlinkat` did in step 22 |
| 32 | 6 | `prop.s` → `prop.c` | 2184 | 735 | stage-6 gate (with step 33) | | generated values held across allocations are rooted (`GC_ROOT`); `vec_new`/`vec_push` root their vector and item when they grow (they run during properties); `fcvt.l.d rtz` semantics for shrinking floats (R13); `vo test` on 204 files identical apart from timings |
| 33 | 6 | `interp.s` → `interp.c` | 1243 | 421 | stage-6 gate | | `r0009` stays one global whose address the test runner compares; re-entry depths pinned first (Recursion limits) |
| 34 | 7 | `vm.s`, `trace.s` → `vm.c` | 3059 | 1139 | stage-7 gate | | one `vm_run` with `&&label` handlers (the tracing ones too), published by `vm_init` from `compile_program`; `MIN / -1` overflows and `MIN % -1` is 0 explicitly (R13); the page pin is gone; speed 0.80-0.99 of `build-asm/vo` (geometric mean 0.90), now enforced at 1.10; `--trace` on 1,206 runs (`run` and `test` of every case) writes identical traces |
| 35 | 7 | `gc.s` → `gc.c` | 1672 | 665 | stage-7 gate (with step 34) | | precise roots: the value stack, `GC_ROOT`, the globals and the permanent arena; no native-stack or register scanning (`stack_scrub` gone); fault kind NATSTACK replaced by ROOTS (`fault_targets/roots.vo`); a root audit of the run-time C files (below) |
| 36 | 7 | `sys.s`, `jmp.s` → `sys.c`, `platform/riscv64.S` (+ `.h`) | 810 | 312 + 166 + 20 | full gate (below) | geometric mean 0.880 | `_start`, `vo_syscall0`-`6` and `jmp_save`/`jmp_restore` are the only assembly; `sys_start` (C) maps the native and root stacks; `fstat` became `statx` (R5); the jump buffer now also saves `fs0`-`fs11` (216 bytes): a `jmp_restore` past a C frame that kept a double in an `fs` register would otherwise have corrupted its caller; the `RUNTIME` link order is gone; `run/timeout_in_sort` doubles its input (the C engine sorted the old one inside the limit) |
| 37 | 7 | finalize: `defs.inc`, `tools/gen_defs.py`, `tools/gen_protos.py` removed | 634 (`defs.inc`) | constants and declarations in `vo.h` | full gate (below), `make diff-asm COUNT=10000` | geometric mean 0.885 | no `src/*.s` remains; `vo.h` holds the constants (with their comments) and every global's declaration by file, so a definition without one fails `-Wmissing-prototypes`; the normal, stress and UB-trap binaries are byte-identical (stripped) to step 36's; `make lint` checks `src/platform/riscv64.S` |

## Audit findings (R13)

## Memory figures (R4)

Step 35 (precise roots) changed no figure: `cli/mem_stats_*` and `prop/mem_stats` (exact bytes) and
`cli/max_memory_*` pass unchanged, and bench2's 703 reference runs are unchanged in the stress build.

## Recursion limits (R7)

Recursion that re-enters the engine through a builtin's callback, measured on `build-asm/vo` (T046) and
pinned by golden cases `tests/cases/run/reentry_<builtin>.vo` (the R0005 trace shows the depth):

| builtin | depth of R0005 (first frame's `n`) | limit reached |
|---|---|---|
| `map` | 190,650 | value stack (`rt_stack_full`) |
| `any` | 190,650 | value stack |
| `all` | 190,650 | value stack |
| `filter` | 182,361 | value stack |
| `fold` | 182,361 | value stack |
| `sort_by` | 199,728 | value stack |
| property runner | not re-entrant | a program cannot call a property |

The value stack fills before the 64 MiB native stack in every case, in `build-asm/vo` and in the mixed
build with the builtins and their dispatch in C (steps 30-31), so the depths are the same. They stay
pinned while the engine moves to C (T047). Evidence that the value stack binds: 32 MiB / 190,650 = 176
bytes per level, and eight more locals in `f` lower the depth to 139,810 (240 bytes per level) in both
builds. For T047: the C engine must keep the native stack per level below 64 MiB / (32 MiB / the
smallest value frame per level), about 288 bytes, or the native stack must grow, so that the value
stack keeps binding. After T047 (measured on the compiled objects): one `map`/`any`/`all` level is `vm_run`
64 + `builtin_call` 64 + `hof_*` 80 + `call_value` 64 = 272 bytes (`sort_by`: 304 plus its sort), under
64 MiB / 199,728 = 336 bytes, and the six cases keep their depths. The UB-trap build's larger frames made
the native stack bind first (R0005 at 187,672 for `all`/`any`), so the native stack is now 128 MiB
(`STACK_SIZE`): about 690 bytes per level, which leaves room for other compilers and the x86-64 and
ARM64 builds. The parser keeps its 5,000-level counter, so E0115 is unchanged.

## Deviations

- **Memory figures depend on the absolute path of the checkout** (found, not introduced, by the port):
  `cli/mem_stats_test` and `prop/mem_stats` allocate path-sized strings at run time, so their exact
  byte counts change when the tree is copied to a longer path; `build-asm/vo` fails them the same way
  there. Gates on snapshot copies re-record these two cases with `build-asm/vo` at the snapshot's path
  first, so they are still compared with the oracle.
- **Stage 1 gate result**: `make test` passed in full (bench2 703/703, fuzzers, 91 + 35 step cases,
  1,000-program engine comparison); the UB-trap suite failed one case, `run/timeout_in_sort`, whose
  4 s limit fired one statement earlier because two gates were running at once; it passes under
  normal load (twice). Gates no longer run concurrently.
- **Stage 1's step gates were combined**: the step 2 gate ran on a snapshot and stopped at the two
  path-sensitive cases above; steps 2-6 had each passed the golden cases and 2,000 generated programs
  when made, and one full gate covered them together with step 7.
- **Full gate through step 8** (snapshot after the parser port): passed (435/435, 2,000 programs and
  2,000 mutants with 0 disagreements, gc-check 435/435, bench2 stress 703/703, every collector fault
  kind caught, 91 step cases).
- **Gate through step 13** failed one timing check, `json_corpus` (100 KB document in 2 s or more),
  because golden runs and 3,000-program comparisons ran beside it; alone it takes 0.55 s (`build-asm/vo`
  0.59 s). Rerun on the step 14 snapshot with nothing beside it.
- **Steps 15-20 were checked together**: they were written while the step 14 gate ran, then tested at once
  (435/435, 3,000 programs and 3,000 mutants with 0 disagreements, `vo check --json` on 1,104 files
  identical, record_changes, the 91 step cases, apply_fixes, catalog, migrate and scope_fuzz clean).
- **Gate through step 14** (rerun alone): passed (435/435, 2,000 programs and 2,000 mutants, ubtrap
  435/435, gc-check 435/435, bench2 stress 703/703, every collector fault kind caught, 91 step cases).
- **A defect kept for identical output** (found by the port, to fix after the migration): when a
  source file cannot be read, `main.s`'s `load_source` called `cmd_name` after loading `opt_json` into
  the first argument register, so the command name's address went to `summary_emit` as the JSON flag:
  the summary line is always JSON, even in human mode (`vo test nosuchfile.vo`). `main.c` passes 1 there,
  with a comment.
- **Stage 4 gate** (snapshot after step 26): passed (435/435, 2,000 programs and 2,000 mutants, ubtrap
  435/435, gc-check 435/435, bench2 stress 703/703, every collector fault kind caught, 91 step cases;
  speed ratios 0.88-1.05 against `build-asm/vo`).
- **Steps 27-31 were written together and tested together** (435/435, 3,000 programs and 3,000 mutants
  with 0 disagreements, the 91 step cases, `vo test` on 204 files), then gated together.
- **A stale stack word under conservative scanning** (`cli/mem_stats_json`, exact bytes): the first
  `builtin_call` in C took the addresses of its arguments to root them across the effect check, which
  put them in stack slots; a stale one kept 64 bytes alive (`peak_live` 8395232 instead of 8395168).
  The roots were unnecessary (`rt_check_bit` allocates only when it raises) and are gone; the figure
  is identical again. Until T048 removes native-stack scanning, such words can change exact figures.
- **Re-entry limits without counters** (R7, T046): R7 assumed the assembly's callback re-entry
  depth was set by native stack bytes. Measured, it is set by the value stack (Recursion limits),
  whose words are the same on every platform, so it is already independent of native frame sizes
  and matches the assembly for mixed recursion too, which per-builtin counters would not. No counter
  table was added. The condition R7 cares about moves to T047: the native stack must never be the
  first to run out on these paths (its guard stays the last line of defence); the six
  `run/reentry_*` cases check it.
- **A speed-dependent timeout location** (found, not introduced, by the port): in
  `tests/gc/edge/oom_in_test.vo` the 10 s limit can fall inside one of the collector's ~1 s pauses on a
  large heap, which poll the deadline late in both builds; the C build is about 5% faster on that
  loop, so its deadline lands in a pause (reported at `str(i)`, ~700 ms late) where `build-asm/vo`'s
  lands in the loop. Timing the same growth loop with both builds shows identical pauses. The file is
  not a golden case (its output depends on machine speed).
- **Stage 3 checkpoint** (`make diff-asm COUNT=10000`, with the query port in): 10,000 programs, 0
  disagreements, 0 timed out.

- **The gate is split** (research R10 asked for the full gate after every step). A full gate takes about 30
  minutes, mostly the collector stress suite; about 45 steps would cost a day of waiting for little: code
  before the runtime stage never allocates on the collected heap. `make gate-step` (lint, `make test`
  with bench2, fuzzers and the engine comparison, the UB-trap suite, 2,000 generated programs against
  `build-asm/vo`) runs after every step; `make gate` (adds `make gc-check` and the speed check) at every
  stage boundary and after every step of stages 6 and 7.
- **Gate through step 31** (snapshot): passed (435/435, 2,000 programs and 2,000 mutants, ubtrap, gc-check,
  bench2 stress 703/703, every fault kind caught, 91 step cases; speed ratios 0.90-1.04).
- **Gate through step 33** (snapshot): gate-step passed (441/441, 2,000 programs and 2,000 mutants with 0
  disagreements, ubtrap 441/441); gc-check failed the six `run/reentry_*` cases, which time out under the
  stress build (a collection every 64th allocation, and even every 8,192nd, scans a value stack 190,000
  frames deep). They are now `skip` in `<case>.stress-every`, like the other timing-bound cases; the
  normal suite still pins their depths. The full gate reran with step 34 (below).
- **Speed is enforced from step 34** (T047, SC-004): `make gate` runs `tests/speed.py --baseline
  build-asm/vo --max-geomean 1.10`; the new option checks the geometric mean of the per-program
  ratios instead of spec 005's 5% per program.
- **Gate through step 34** (snapshot): `make test` passed, then the UB-trap suite failed `run/reentry_all`,
  `run/reentry_any` and `run/reentry_sort_by`: with UB checks the frames are larger and the 64 MiB native
  stack ran out before the value stack. The native stack is now 128 MiB (Recursion limits); the gate
  reran with step 35.
- **A root audit with step 35**: with native-stack scanning gone, the stress suite found two missing
  roots (`report_test`'s builder, `pr_value_text`'s builder). The run-time C files were then audited by
  reading: 24 more locals or parameters held across allocations were rooted (testrun, interp, rt_call,
  rt_effects, rt_io, rt_test, rt_str, prop, diag). `tests/run.sh --stress 1` (a collection before every
  allocation) then passes every case but `run/oom_small`, which only times out at that rate.
- **Gate through step 35** (snapshot, steps 34 and 35 together): passed (441/441, ubtrap 441/441,
  2,000 programs and 2,000 mutants with 0 disagreements, gc-check 441/441, bench2 stress 703/703,
  every collector fault kind caught (ROOTS by 11 fault targets), 91 step cases; speed geometric-mean
  ratio 0.884 against `build-asm/vo`, limit 1.10).
- **Gate through step 36** (snapshot): passed (441/441, ubtrap 441/441, 2,000 programs and 2,000 mutants with
  0 disagreements, gc-check 441/441, bench2 stress 703/703, every collector fault kind caught, 91 step cases;
  speed geometric-mean ratio 0.880, limit 1.10).
- **Gate through step 37** (snapshot): passed (441/441, ubtrap 441/441, 2,000 programs and 2,000 mutants with
  0 disagreements, gc-check 441/441, bench2 stress 703/703, every collector fault kind caught, 91 step cases;
  speed geometric-mean ratio 0.885, limit 1.10). `make diff-asm COUNT=10000`: 10,000 programs, 0
  disagreements, 0 timed out.
- **US3's independent test** (every migration-log line names a gate that passed): every step 1-37 names
  one, and the stage gates and every step of stages 6 and 7 ran the full `make gate`. The steps were not
  committed one by one (the branch is committed when the maintainer asks), so "re-running `make gate` on a
  recorded step's commit" is not possible; each gate instead ran on a snapshot of the tree at that step
  (`gate-pNN`), and the final tree (step 37) passed the full gate and 10,000 generated programs. The
  toolchain is C plus `src/platform/riscv64.S` (166 lines) and `src/platform/riscv64.h`.

## Native x86-64 (US1, T051-T055)

- **Platform layer**: `src/platform/x86_64.S` (83 lines) and `x86_64.h` (the x86-64 call numbers of R5).
  The seven `vo_syscallN` share one body (the kernel ignores unused argument registers). Both platform
  files now define `vo_data_begin`, the start of `.data`, linked first: only RISC-V's linker script has
  `__DATA_BEGIN__`, which the collector used as the start of the globals.
- **Build**: `ARCH` defaults to `uname -m`; `build/vo` is the host build, a static ELF (`readelf -d` is
  checked on every link). `make ARCH=riscv64` builds `build-riscv64/vo`. `make gate`'s speed check now
  times `build-riscv64/vo` against `build-asm/vo`, both under qemu (SC-004 compares like with like).
- **`bin/vo`** reads the ELF machine field and runs the binary directly or through `qemu-riscv64` /
  `qemu-aarch64`.
- **Suite, natively**: `make test` passes (441/441, the repeat runs, every checker and fuzzer, 91 step
  cases, 1,000 programs against the oracle), `make ubtrap` 441/441, `make gc-check` passes (stress
  441/441, bench2 703/703, 3,000 fuzz iterations, every fault kind caught, 1,000 programs under stress).
  `make test` takes 3m43s and `gc-check` 6m27s natively. No x86-64-specific disagreement appeared: the
  division traps and NaN signs of R13 are handled explicitly in C.
- **Timing cases reworked**: the four `run/timeout_in_*` cases whose work ran shorter than their limit
  natively (sort, equality, render, collection) now spend under 0.1 s in setup even under qemu and 3 to 10
  times their limit in the builtin natively: sort orders 524,288 references to strings with a 4 KiB common
  prefix (a 32 KiB prefix first: under qemu the deadline, checked every 65,536 comparisons, then came 1.1 s
  late, past spec 003's 1 s bound); equality
  compares 20,000 list pairs; render prints 26 million floats under `--max-memory 2048`; collection runs
  200 rounds under `--max-memory 2048`. Expected output recorded with `build-asm/vo`; all three builds
  pass. `tests/gc/timeout_check.py` now runs each case with its own `.cmd` options (the time limit forced
  to 2 s).
- **Memory parity check**: the collection workload run without a limit runs out of memory at round 60 on
  both `build/vo` (native) and `build-asm/vo`, with byte-identical reports.
- **US1's independent test** (fresh checkout of `ae1973e`, `make && make test`, native): builds in 6.4 s;
  everything passes except `cli/mem_stats_test`, whose figures depend on the length of the checkout's
  absolute path (the toolchain keeps the working directory on the heap during a test). That is not a
  native difference: `build-asm/vo` gives the same figures at that path (1,167,024 bytes for "maps fail"),
  and at the repository's own path both match the recorded output. Making the figures path-independent
  would change memory accounting against the assembly release; left open for the maintainer.
- **SC-003**: `tests/speed.py --baseline build-asm/vo` with native `build/vo`: closures_generics 375 → 38 ms,
  fib 181 → 38, int_loop 407 → 36, list_work 286 → 67, map_count 469 → 60, str_build 621 → 70.

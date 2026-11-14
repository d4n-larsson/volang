# Results: vo-lang Bytecode Execution

Measured on 2026-11-14 under qemu-riscv64 on an x86-64 host, with the `004-bytecode-compiler` build. "Oracle" is the tree-walking interpreter of commit `1fff558` (`make oracle`). Speed figures are medians of 5 runs; the machine was moderately loaded (load average 2–3).

## Success criteria

| Criterion | Target | Result | Met |
|---|---|---|---|
| SC-001 | Geometric mean ≥ 4×, no program < 2× | 4.59× geometric mean, range 2.33× to 8.61× (table below) | yes |
| SC-002 | 10M-iteration loop ≤ 1.35 s; ratio to Python reported | 687 ms (was 5.4 s); Python 990 ms, so vo/Python 0.69 | yes |
| SC-003 | Corpus, examples, migrations, 703 bench2 runs identical | 243 golden cases (repeats 136/136), 28/28 reference examples, migrations, and 703/703 bench2 runs. Exceptions: memory figures (below) and `vm_recursion_200k` | yes |
| SC-004 | Stress-identical corpus; every fault kind caught | 243/243 under stress, 703/703 bench2 under stress, stress fuzzer 3,000 iterations with 0 failures; 8/8 fault kinds caught by targeted programs (PERM informational) | yes |
| SC-005 | ≥ 10,000 generated programs identical | 10,000 programs, 0 disagreements (654 stopped on a deliberate runtime error); 1,000 more under stress, 0 disagreements | yes |
| SC-006 | 1,000-line programs < 1 s; bench2 CPU total ≤ +10%, no run slower by > max(10%, 5 ms) | 82 ms and 111 ms. bench2 CPU total 0.96× the oracle's; no run over the limit | yes |
| SC-007 | 100,000-deep recursion; spec 003 workloads < 64 MiB | 100,000 and 200,000 complete; the limit is now about 418,800 calls. `gc_bounds.py` passes | yes |
| SC-008 | A tight loop stops within 1 s of a 2 s `--timeout` | `timeout_in_tight_loop`: 2.03 s | yes |
| SC-009 | Reference unchanged | `docs/reference.md` and `docs/errors.md` untouched; ~8,190 tokens, 28/28 examples | yes |

## Speed (SC-001, SC-002)

| Program | Oracle | New | Speedup | Python | vo / Python |
|---|---|---|---|---|---|
| closures_generics | 2,178 ms | 639 ms | 3.41× | 251 ms | 2.55 |
| fib | 2,605 ms | 302 ms | 8.61× | 130 ms | 2.32 |
| int_loop | 5,420 ms | 687 ms | 7.89× | 990 ms | 0.69 |
| list_work | 2,852 ms | 507 ms | 5.62× | 448 ms | 1.13 |
| map_count | 2,446 ms | 791 ms | 3.09× | 396 ms | 2.00 |
| str_build | 2,378 ms | 1,019 ms | 2.33× | 306 ms | 3.33 |
| **Geometric mean** | | | **4.59×** | | |

vo still runs under qemu while Python runs natively, so the vo/Python column understates vo on real RV64 hardware.

### Before tuning (T020)

The first complete engine, with no constant operands, fused branches or fast calls, gave a geometric mean of 2.32×: int_loop 3.20×, fib 3.65×, closures_generics 2.22×, list_work 2.13×, map_count 1.93×, str_build 1.47×.

### What the tuning did (T021–T024)

| Change | Effect |
|---|---|
| Constant operands (`x + 1`, `i % 7`), fused compare-and-branch for `if`/`while`, one back-edge instruction per loop iteration, inline step counting | int_loop 1.7 s → 0.70 s |
| A fast call path for non-generic named functions (the frame set up inline) | fib 0.72 s → 0.33 s |
| Pure builtins skip the effect-check call; `len`, `push` and `has` get their own instructions; builtin arguments are read in place | list_work, map_count, closures_generics +10–20% |
| `int_to_str` allocates once, not three times | str_build 1.67× → 2.17× |
| `sort` on `int` lists: indexes in registers, comparison inline | list_work 3.3× → 5.7× |
| Generic calls cache `rt_targs`/`rt_es_subst` per call site; `==` and `<` on a type parameter bound to `int` resolve inline | spec 003's generic/specific ratios back under 1.5× (now 1.05–1.31×) |

## Intended changes to existing behaviour

- **Memory figures** (FR-011). They are still deterministic from run to run; the numbers moved:

  | Case | Before | After |
  |---|---|---|
  | `cli/mem_stats_json` | allocated 76,813,408, 9 collections, peak 8,395,328 | allocated 25,613,024, 3 collections, peak 8,395,168 |
  | `cli/mem_stats_run` | 73.3 MiB allocated, 9 collections | 24.4 MiB allocated, 3 collections |
  | `cli/mem_stats_test` | "small" 29,920, "strings" 38,400,864, "maps fail" 1,805,488, "runtime error" 9,602,208 allocated | 17,120; 12,800,864; 1,165,488; 3,201,952 |

  Most of the drop comes from `int_to_str` allocating once. The R0006 cases (`run/oom`, `gc/edge/*`) did not change.
- **Recursion depth** (FR-005, FR-010). The new golden case `run/vm_recursion_200k` stops with R0005 on the oracle and prints `200000` now. Simple recursion goes about 418,800 calls deep: 32 MiB of value stack at about 80 bytes per frame.
- **`run/timeout_in_sort`**: made robust, not changed in meaning. On a loaded machine, `split` of 2²⁰ copies took the whole 2 s limit, so R0009 fired there instead of in `sort`, on the oracle too. Now 2¹⁹ copies with `--timeout 4`: `split` takes about 2 s and `sort` about 6 s.

## Bugs and findings

- **Instructions pinned heap objects.** Compiled code was first allocated in the permanent arena, which the collector scans conservatively. An instruction packs its `a` and `b` operands into one 64-bit word. With `b` = slot 2 (byte offset 16), that word reads as 0x10_0000_00xx, an address in the first heap page. It kept the earliest heap objects alive.
  - **How it showed:** the CLOSURE and VARIANT fault targets stopped being caught.
  - **Fix:** compiled code, its scratch buffer and generic call-site records live in their own region (`ADDR_CODE`), which is never scanned. Nothing there points into the heap.
- **Stale words in live native frames.** Frames with words the function never writes (`vm_run`, `vm_push`, `builtin_call`, the new `int_to_str` digit buffer, `str_new` before its allocation returns) are now zeroed on entry. The VARIANT target also builds its value in a helper function, because a stale temporary in `main`'s live frame kept it alive.
- **`b_min`/`b_max` overwrote a saved register.** They used `24(sp)`, `eval_builtin`'s saved-`s3` slot, as scratch. This was harmless in the tree walker, but the VM keeps state in `s3`. `builtin_call` now has its own scratch slot. The SESI linter did not report it (issue 18 in `sesi-linter-issues.md`).
- **`gc_faults.py` crashed** decoding a fault build's poisoned (non-UTF-8) output; it now decodes with replacement.
- **Effects (spec 002 gap, not fixed here).** A lambda passed through an effect parameter runs under its caller's effect set, which binds the effect parameter to the lambda's own unscoped `env`. So `vm_effect_forward`'s lambda reads `env("PATH")` although `main` declares only `env("HOME")`; a named function in the same position gets R0010. Spec 004 reproduces this exactly. It should be fixed in the effects checker, in a later spec.

## Deviations from the plan

- **Code region.** Compiled code is not in the permanent arena (research R7 said it would be), for the reason above.
- **Cheaper builtins.** Pure builtins skip `rt_check_bit`. `len`, `push` and `has` got dedicated instructions, and builtin arguments are read in place. The plan had all three only as possible follow-ups.
- **Runtime tuning outside the engine.** `int_to_str` (one allocation) and `list_sort` (inline `int` comparison) were tuned, because `str_build` and `list_work` were below 2× on builtin time alone.
- **Generic call sites.** They get a cached record: `CALL_GENERIC` with a `GS_*` record in the code region. Without it, spec 003's generic/specific ratio check in `tests/perf.sh` failed (up to 2.5×), because type-specific code got much faster.
- **SC-006 measurement.** bench2 runs are CPU-timed, with oracle and new runs alternated. A run over the limit after a median of 5 gets a second look with 15 pairs, because on this machine a median of 5 still moves by about 5 ms. The real overhead of a small run is 1–3 ms: qemu translating the compiler and VM code, plus compiling every function.
- **Equivalence-case outputs** were recorded from the oracle, except `vm_recursion_200k` (Principle IV: it records the change).
- **Developer build `make VMREPORT=1`** (fallback report and code hash) existed during T011–T019 and was removed with the tree walker in T030. It showed the compiled code's hash identical over 3 runs of 20 programs (FR-003).

## Follow-ups

- **AST aliasing.** Syntax nodes in the permanent arena pack small fields the same way instructions did. The kind/flags/file word of a node in file 16 reads as a heap address, so it can pin early heap objects. This predates spec 004 (spec 003's arena scan). The fix is to stop scanning the AST, or to move it to an unscanned region.
- **Temporaries are never cleared** after a statement, so a dead temporary keeps garbage alive until it is overwritten. This showed in the VARIANT target.
- **Builtin-heavy code gains least** (str_build 2.3×). More dedicated instructions (`str`, `get`, `find`) and cheaper string building would help.
- **Native code or a JIT** remains out of scope (roadmap).

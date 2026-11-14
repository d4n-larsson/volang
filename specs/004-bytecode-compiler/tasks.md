---

description: "Task list for vo-lang spec 004: bytecode execution"
---

# Tasks: vo-lang Bytecode Execution

**Input**: Design documents from `specs/004-bytecode-compiler/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R16), data-model.md, contracts/equivalence.md, quickstart.md

**Tests**: REQUIRED (constitution v1.1.0, Principle IV). This spec changes no behaviour, so the test-first rule takes three forms:
- The equivalence golden cases are written first and must pass on the oracle (`VO_BIN=build-oracle/vo`). That proves they record today's behaviour.
- The speed benchmark is written first and fails on the tree walker.
- The differential tester runs before the VM exists (oracle against oracle: 0 disagreements).

**Organization**: phases follow the plan. US2 (nothing changes, P1) comes before US1 (speed, P1). A faster engine is worthless until it is equivalent, and the speed work tunes an engine that already runs everything.
- **MVP:** Setup, Foundational, US2 and US1.
- **US3 and US4 (P2):** add the limit and collector guarantees.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependencies on incomplete tasks).
- **[Story]**: US1–US4 from spec.md.

## Conventions

**Carried over** from specs 001–003:
- RV64IMAFD, lp64d, GNU as. The SESI hook checks every edit; every report is fixed or explained.
- C-word prototype comments and `.globl` on every function. Shared constants in `src/defs.inc`. No libc.
- Golden cases are `tests/cases/<dir>/<name>.vo` plus `.out`, `.err`, `.code`, and optional `.cmd` and `.stress-every` files, accepted with `tests/run.sh --accept`.
- Append every SESI false positive or negative to `sesi-linter-issues.md`.
- `pkill` patterns are anchored with `^`.

**What the definitions are:**
- The behaviour to reproduce is in contracts/equivalence.md.
- Layouts are in data-model.md.
- When the tree walker (`src/interp.s` at `1fff558`) and the contract disagree, the tree walker wins. Fix the contract in the same task.

**The equivalence rule:** after every task that touches `src/`, `make test` passes. From T011 on, the corpus also passes with the VM running every function it has compiled.

**Determinism rule:**
- The compiled form depends only on the program: no addresses outside the fixed map, and no clock.
- Each instruction that can raise carries the node whose span the tree walker used.

**Shortcuts get `ponytail:` markers:**
- 32-byte instructions;
- the recursive compiler;
- uncleared temporaries;
- builtins behind `builtin_call`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: the comparison engine, the speed benchmark and the differential tester, all before any engine work.

- [X] T001 Add the `oracle` target to `Makefile` (research R12):
  - If `build/oracle-src` does not exist, create it with `git worktree add --detach build/oracle-src 1fff558`.
  - Build it with `$(MAKE) -C build/oracle-src`, then copy `build/oracle-src/build/vo` to `build-oracle/vo`.
  - `clean` does not remove `build-oracle/`. `.gitignore` already covers `build*/`; check that `build-oracle/` is ignored.
  - Run `VO_BIN=build-oracle/vo tests/run.sh`: every golden case passes, since nothing has changed yet.
- [X] T002 [P] Create the speed benchmark in `tests/speed/` (research R14, SC-001, SC-002). Six programs, each with a `.vo`, a `.py` that prints exactly the same output, and a `.out`:
  - `int_loop`: the roadmap loop, `for i in 0..10000000 { set total = total + i % 7; }`;
  - `fib`: naive recursive Fibonacci;
  - `list_work`: build, index, sum and sort;
  - `map_count`: word counts over generated text;
  - `str_build`: split, join, find and `+`;
  - `closures_generics`: `fold`/`map` with lambdas, and a generic `Box[T]`/`largest` container.

  Tune each so that `build-oracle/vo` takes 2–6 s. `int_loop` stays at exactly 10 million iterations, because SC-002 names it.
- [X] T003 [P] Write `tests/speed.py`:
  - **Measurement:** for each program in `tests/speed/`, the median of 5 runs on `build-oracle/vo`, on `bin/vo` and on `python3`. Every output is checked against the `.out` file.
  - **Report:** one table row per program (oracle ms, new ms, speedup, vo/Python ratio), then the geometric-mean speedup.
  - **Exit status:** 1 if the geometric mean is below 4.0, if any program is below 2.0, or if `int_loop` takes more than 1,350 ms.
  - **`--bench2`** measures CPU time (user plus system, from `os.wait4`) for every bench2 reference run on each engine (SC-006):
    - it fails if the new engine's total exceeds the oracle's by more than 10%;
    - it re-times any single run that is slower by more than max(10%, 5 ms) with a median of 5, and fails if that run is still over the limit.
  - Add `make speed` to `Makefile`.
  - Check that it fails now, at a speedup of about 1.0.
- [X] T004 [P] Write `tests/gen_programs.py`, a seeded generator of type-correct vo programs (research R13):
  - **API:** `generate(seed) -> str`, plus `python3 tests/gen_programs.py --seed S` to print one program.
  - **Grammar:** a small typed grammar over `int`, `bool`, `str`, `float`, `list[int]`, `list[str]`, `map[str, int]`, one record, one variant with and without payloads, `result[int]` and one generic function.
  - **Constructs:** `let`/`var`/`set` (locals, fields, list and map elements), `if`/`else if`, bounded `while`, `for` over ranges and lists (including a list that grows inside its own loop), `match`, `return`, `break`, `continue`, `try`, calls, recursion bounded by a depth parameter, lambdas with captures, `sort_by`/`map`/`filter`/`fold`/`any`/`all`, string building, `==` on composite values, and `and`/`or` with side-effecting operands.
  - **Runtime errors on purpose:** about 10% of programs reach a runtime error: division by a value that may be zero, an index that may be out of range, a missing key, or overflow.
  - **Output:** every program prints intermediate values, so a divergence shows up in stdout.
  - **Size:** loop bounds and recursion depth are capped so that every program runs in under 1 s on `build-oracle/vo`.
  - **Requirement:** at least 99% of generated programs pass `vo check`.
- [X] T005 Write `tests/diff_engines.py --count N --seed S [--stress]` (research R13, FR-015):
  - **Run:** each generated program on `build-oracle/vo` and on `bin/vo`. With `--stress`, the second engine is `build-stress/vo` with `VO_GC_STRESS=64`. Both get a `--timeout 10`.
  - **Compare:** stdout, stderr and exit status. Write each disagreement to `build/diff-failures/<seed>-<n>/` (program and both outputs).
  - **Timeouts:** a run that ends in R0009 on either engine is counted as T (timed out) and skipped, not counted as a disagreement. Any T > 0 is a generator bug: the size caps are wrong.
  - **Report:** `N programs, D disagreements, E stopped on a runtime error, C did not check, T timed out`.
  - **Exit status:** 1 if D > 0, if C > 1%, or if T > 0.
  - Run it with `bin/vo` set to the oracle (`VO_BIN`). Expected: 0 disagreements.
- [X] T006 Wire the new tools into `Makefile`:
  - `test` runs `python3 tests/diff_engines.py --count 1000 --seed 4` when `build-oracle/vo` exists, and skips it with a message otherwise.
  - New `speed-check` target: `oracle`, then `speed`, then `diff_engines.py --count 10000 --seed 1`.
  - `gc-check` additionally runs `diff_engines.py --stress --count 1000 --seed 2`.

**Checkpoint:** `make oracle`, `make speed` (fails as expected), and `diff_engines.py --count 200` with 0 disagreements.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: layouts, the builtin entry, golden cases that pin down today's behaviour, and a VM and compiler that run a subset. Functions they cannot compile yet fall back to the tree walker.

**⚠️ CRITICAL**: no user-story work starts before this phase is complete.

- [X] T007 Add to `src/defs.inc` (data-model.md):
  - instruction offsets `I_HANDLER` 0, `I_A` 8, `I_B` 12, `I_C` 16, `I_NODE` 24, `I_SIZE` 32;
  - compiled-function offsets `CF_CODE` 0, `CF_N` 8, `CF_SLOTS` 16, `CF_NPARAMS` 24, `CF_DECL` 32, `CF_HDR` 40;
  - frame fields `FR_CODE` 40, `FR_RPC` 48, `FR_RDST` 56, with `FR_SLOTS` moving from 40 to 64;
  - `FX_CODE` 72 in the effects FX record, with that record's `FX_SIZE` changing from 72 to 80. Edit line 292, not the earlier unrelated `FX_SIZE` at line 204.

  The tree walker uses `FR_SLOTS` symbolically, so it must keep working: run `make test`.
- [X] T008 Split `eval_builtin` in `src/rt_call.s` (research R8):
  - the argument loop stays in `eval_builtin`, for the tree walker;
  - a new `.globl builtin_call(void *node, long a0, long a1, long a2)` does everything after the arguments, exactly as today: sets `cur_node`, calls `rt_check_bit(N_D(node))`, and jumps through `btab` with `s0` = node and the arguments in their stack slots.

  `eval_builtin` ends by calling `builtin_call`. No `btab` body changes. `make test` must stay identical.
- [X] T009 [P] Write the equivalence golden cases in `tests/cases/run/` (contracts/equivalence.md, FR-005–FR-010). Each must pass with `VO_BIN=build-oracle/vo` before it is accepted. The one exception is `vm_recursion_200k`: it records the intended change (FR-005, FR-010), so it must fail on the oracle with R0005 and pass only on the new engine (Principle IV).

  | Case | Covers |
  |---|---|
  | `vm_eval_order` | binary operands, call arguments, value-call callee then arguments, `set xs[i] = e` order, record initialisers, through functions that print |
  | `vm_short_circuit` | `and`/`or` skip the right side |
  | `vm_for_range` | the end is evaluated once, and its side effects happen once; a body that changes the variables the end was computed from does not change the iteration |
  | `vm_for_list_grow` | a list that grows inside its own loop |
  | `vm_nested_loops` | `break`/`continue` inside `match` inside nested loops |
  | `vm_match_order` | arm order, bindings, a variant without a matching arm |
  | `vm_try_chain` | `try` across three functions, `err` from the innermost |
  | `vm_closure_capture` | lambdas made in a loop capture each iteration's value, and outlive their creator |
  | `vm_callback_error` | R0003 inside a `sort_by` comparator reports the lambda's span |
  | `vm_int_edges` | overflow on `+ - *`, `-MIN`, `MIN / -1`, `% 0`; one case per error, via `.cmd` variants or separate files |
  | `vm_index_set_error` | an out-of-range `set xs[i] = f()`: `f`'s output appears before the R0003 |
  | `vm_generic_ordered` | `<` and `==` on type parameters with `int`, `float` and `str` |
  | `vm_effect_forward` | a forwarded callback with an effect parameter: R0010 at the same span |
  | `vm_deep_recursion` | 100,000 calls deep; prints 100000 (FR-010) |
  | `vm_many_locals` | a function with 300 locals and a 2,000-line body: compiles and runs |
  | `vm_deep_nesting` | expressions and blocks nested just under the parser's limit: compiles and runs (FR-004; the compiler is recursive) |
  | `vm_recursion_200k` | 200,000 calls deep; prints 200000. The oracle stops with R0005, which was measured at 200,000 before this spec; the intended change |
- [X] T010 Create `src/vm.s`, the VM core (research R3, R4, R11; data-model "Frame", "VM registers"):
  - **`vm_run(void *frame) -> long`:** loads `pc` = `CF_CODE` of `FR_CODE`, keeps `pc`, frame and slots in `s1`–`s3`, and dispatches with `ld t0, I_HANDLER(pc); jr t0`. It returns the result when a frame with `FR_RPC` = 0 executes `RET`.
  - **`vm_push(cf, decl, call node, closure) -> frame`:** factored out of `call_fn`'s frame setup. It does the R0005 value-stack check at the call node, zeroes the slots, sets `FR_PARENT`, `FR_FN`, `FR_CALL`, `FR_TARGS` and `FR_EFFECTS` (the `rt_targs`/`rt_es_subst` rules) and copies the captured values. It updates `cur_frame` and `vsp`.
  - **Handlers:** `MOV`, `LOADK`, `ADD_INT`/`SUB_INT`/`MUL_INT`/`DIV_INT`/`REM_INT`/`NEG_INT` with the checks from `eval_binary`, the float operations, `NOT`, the comparisons, `JMP`/`JT`/`JF`, `CALL` (arguments from slots `b…`, a step via the `tick` rules for call nodes, then `vm_push`, `FR_RPC` and `FR_RDST`), `CALL_BUILTIN` (`builtin_call(node, slot[b], slot[b+1], slot[b+2])`), and `RET`.
  - **Errors:** raised with `I_NODE` through the existing `rt_overflow`/`rt_raise*` paths.
  - **Fallback:** a `CALL` whose callee has no `FX_CODE` calls `call_fn` natively.
  - **Lint:** the handlers are local labels inside `vm_run`, which has a prototype; there are no `.globl` handler symbols. Run `make lint` on the skeleton as soon as it assembles. If SESI cannot follow the `jr t0` dispatch, either put an explained `sesi: allow` on `vm_run`'s prototype line, or add `src/vm.s` to `LINT_EXEMPT` in `Makefile` with the reason next to `jmp.s`'s. Log it in `sesi-linter-issues.md` either way. `vm_push` and every other function in `vm.s` must lint clean.
- [X] T011 Create `src/compile.s`, the compiler core (research R6, R15; data-model "Compiler context"):
  - **`compile_program`:** walks every file's `N_FN` and `N_TEST` items and compiles each. Lambdas are compiled when their enclosing body reaches them.
  - **`compile_fn(decl)`:**
    - context on the native stack; the growable buffer from `alloc_bytes` before `gc_start`;
    - temporaries start at `N_E(decl)`;
    - the result is copied to the permanent arena inside `perm_begin`/`perm_end` and stored in `FX_CODE` of `item_fx(decl)`.
  - **`cexpr`/`cstmt`:** cover literals, identifiers (their own slot), `let`/`var`, `set` of a local, binary and unary operators with known types, `if`/`else if`, `while` (a step before the condition), `for` over ranges (a hidden counter slot) and lists (re-reading the length), `break`/`continue`, `return`, user calls and builtin calls.
  - **Anything else** (data, `match`, `try`, lambdas, value calls, generics): `compile_fn` gives up on the whole function and leaves `FX_CODE` at 0, so the tree walker runs it.
  - **Fallback report:** add `.ifdef VM_REPORT_FALLBACK` to `call_fn`, printing `fallback: <name>` to stderr, and a `make VMREPORT=1` build in `build-vmreport/`. In the same build, `compile_program` ends by printing `code-hash: <FNV-1a of every compiled function's bytes, in compile order>` to stderr. T019 uses it to check FR-003.
- [X] T012 Wire it in:
  - `src/main.s` (`run`) and `src/testrun.s` (`test`) call `compile_program` after checking succeeds and before the first `gc_start`.
  - `call_fn` in `src/interp.s` runs `vm_push` plus `vm_run` when `FX_CODE` is set, and the tree walker otherwise.
  - `run_main` and `run_test_body` go through `call_fn`.
  - Gate: `make test`, `tests/gc/baseline.py` 703/703, `diff_engines.py --count 1000` with 0 disagreements. `make VMREPORT=1` over `tests/cases/run` shows that the VM runs most simple functions.

**Checkpoint:** a mixed engine that passes every gate. From here, each construct moves to the VM.

---

## Phase 3: User Story 2 - Nothing else changes (Priority: P1) 🎯 MVP part 1

**Goal**: the VM runs every function, lambda and test, with byte-identical behaviour.

**Independent test**:
- `make test`;
- `tests/gc/baseline.py` (703/703);
- `diff_engines.py --count 10000 --seed 1` (0 disagreements);
- `make VMREPORT=1` over the corpus with 0 fallbacks.

- [X] T013 [US2] Data instructions in `src/compile.s` and `src/vm.s` (data-model "Data"):
  - list literals (`LIST_LIT` from consecutive temporaries), `NEW_LIST`, `NEW_MAP`, `NEW_RECORD`, `VCASE` (allocate, then evaluate the payload, as the contract says);
  - `GET_FIELD` (with the stress `gc_check` from `eval_expr`), `SET_FIELD`;
  - `INDEX_LIST` (R0003 through `rt_index_error`), `INDEX_MAP` (`map_find` with the key type from `TY_A` of the object node's type; R0004 through `rt_key_error`);
  - `SET_INDEX_LIST`, and `SET_INDEX_MAP` (`cur_node` = the statement, then `map_set`);
  - the `set` forms for fields and indexes, in contract order.
- [X] T014 [US2] `match` and `try` (research R5):
  - `match` evaluates the scrutinee once into a temporary, then one `JTAG_NE` per `N_ARM` in source order. A matching arm stores `V_PAY` into its binding slot when `N_E` ≥ 0.
  - `TRY_UNWRAP a, b, c`: `ok` → `V_PAY` into `a`; `err` → `ret_val` = the value, then jump to the function's return sequence.
  - Functions run by the VM no longer set `try_target` or call `jmp_save` in `call_fn`. Keep both only on the tree-walker path until T019.
- [X] T015 [US2] Closures and value calls:
  - `CLOSURE a`: calls `make_closure(I_NODE)`, which reads the captured slots through `cur_frame`; the VM keeps `cur_frame` current.
  - `FNVAL a`: calls `fn_closure`.
  - `CALL_VALUE a, b, c`: the callee in slot `c` was evaluated first, then the arguments into `b…`. Same dispatch as `eval_call_value` (the `CV_*` kinds, including fields holding functions).
  - Builtin callbacks: `call_value` → `call_fn` → `vm_push` with `FR_RPC` = 0 → a recursive `vm_run`. `check_stack` stays on that native path.
  - Lambdas are compiled when reached (T011), and their `FX_CODE` is set before the enclosing function finishes.
- [X] T016 [US2] Generics, equality and strings (research R9):
  - `CMP_PARAM` for `OP_*_PARAM`, through `rt_concrete` of the left operand's `N_TYPE`;
  - `EQ_INT`/`NE_INT` when the static type is `int`, `bool` or a variant without payloads; otherwise `EQ_VAL`/`NE_VAL` calling `values_equal(a, b, type)` with the same type argument `eval_binary` passes. The handler sets `cur_node`, as `==` does today;
  - string ordering (`LT_STR`, …) and `CONCAT`;
  - generic calls: `CALL` takes `FR_TARGS` from `rt_targs` when `N_E` of the call node is set, through `vm_push`.
- [X] T017 [US2] Tests through the VM: `compile_program` compiles `N_TEST` bodies (`FX_CODE` via `item_fx`), and `run_test_body` in `src/interp.s` runs the compiled body. The per-test `gc_start`, report and global clearing from spec 003 stay as they are. Gate: `tests/cases/test/*` and `tests/cases/cli/mem_stats_test`.
- [X] T018 [US2] Make the corpus run with zero fallbacks:
  - `make VMREPORT=1`, then run `tests/run.sh`, `tests/gc/baseline.py` and `diff_engines.py --count 2000` with `VO_BIN=build-vmreport/vo`. Collect every `fallback:` line and close each construct still missing.
  - Then remove the fallback branch from `call_fn`: a missing `FX_CODE` becomes T0003 `internal_error`. The tree walker is now unreachable.
- [X] T019 [US2] Full equivalence gate:
  - `make test`;
  - `tests/gc/baseline.py` 703/703;
  - `diff_engines.py --count 10000 --seed 1`, 0 disagreements (SC-005);
  - `tests/run.sh --repeat 3`.
  - Some cases print memory figures: the `tests/cases/cli/mem_stats_*` cases and the R0006 cases `run/oom`, `gc/edge/grow_forever`, `gc/edge/tiny_limit` and `gc/edge/oom_in_test`. For each one whose figures changed: accept the new figures with `tests/run.sh --accept`, check with a diff that only the figures changed, and list it in `specs/004-bytecode-compiler/results.md` as an intended change (FR-011). Also list `vm_recursion_200k` there.
  - FR-003: with `build-vmreport/vo`, run 20 corpus programs 3 times each and check that each program's `code-hash:` line is identical on every run.
  - Confirm `check`, `fmt`, `explain`, `effects` and the migration path are untouched (FR-014): `fmt_corpus`, `migrate` and `record_changes` pass.

**Checkpoint:** the bytecode engine is the only engine, and it is equivalent.

---

## Phase 4: User Story 1 - Compute-heavy programs run several times faster (Priority: P1) 🎯 MVP part 2

**Goal**: SC-001 and SC-002: a geometric mean of at least 4×, no program below 2×, `int_loop` at most 1.35 s. SC-006: no slowdowns.

**Independent test**: `make speed` passes; `python3 tests/speed.py --bench2` passes; `sh tests/perf.sh` is under 1 s.

- [X] T020 [US1] Record the starting point: run `make speed` on the unoptimised VM and save the table in `results.md` under "Before tuning".
- [X] T021 [US1] Add constant operands and fused branches in `src/compile.s` and `src/vm.s` (research R15):
  - `_K` forms of `ADD`, `SUB`, `MUL` and `REM` when the right operand is an `int` literal that fits in 32 bits;
  - fused compare-and-branch (`JLT_INT`, `JGE_INT`, `JEQ_INT`, `JNE_INT`, and their `_K` forms) for the conditions of `if`, `while` and `for`;
  - `MOV` elision: `set x = <expr>` computes straight into `x`'s slot, unless the expression reads `x` after a store could clobber it.

  The output must not change: `diff_engines.py --count 2000` after the change.
- [X] T022 [US1] Add single-instruction loop back-edges (research R11):
  - **`FOR_RANGE a, b, c`:** counter slot `a`, end slot `b`, body address `c`. It increments the counter, compares with the end, stores the counter into the loop variable and jumps.
  - **`FOR_LIST`:** the same, re-reading `L_LEN` and `L_DATA` on every iteration.
  - **Inline step:** `steps += 1`; when `steps & 0xFFFF` = 0, call the clock check through `tick`'s slow half.
  - Verify the `while` step order against `vm_nested_loops` and the timeout cases.
- [X] T023 [US1] Add a fast call path:
  - `CALL` inlines `vm_push` for the common case: no closure, no type arguments, an effect set inherited unchanged or taken from the declaration, and the inline step.
  - `RET` restores `pc`, the frame and `vsp`, and stores the result into `FR_RDST`.
  - No native calls on the fast path. Check `fib` against T020's figure.
- [X] T024 [US1] Tune until SC-001, SC-002 and SC-006 hold. Repeat:
  1. `make speed`;
  2. find the hottest handlers with a dev-only instruction histogram (`.ifdef VM_PROFILE` in `vm.s`, `make VMPROFILE=1` into `build-vmprofile/`, printed to stderr at exit);
  3. fix the top cost.

  Stop when `make speed` passes, `tests/speed.py --bench2` passes and `sh tests/perf.sh` is under 1 s. Record each change and its effect in `results.md`. Remove nothing that FR-006 needs: every raising instruction keeps its node.

**Checkpoint:** MVP complete: equivalent and at least 4× faster.

---

## Phase 5: User Story 3 - Limits stay at least as generous (Priority: P2)

**Goal**: recursion of at least 100,000 calls, deterministic R0005, timeouts in tight loops, spec 003's memory bounds.

**Independent test**: `vm_deep_recursion`, `run/stack`, `tests/gc_bounds.py`, `tests/gc/timeout_check.py`.

- [X] T025 [P] [US3] Add `tests/cases/run/timeout_in_tight_loop`: `while true { set x = x + 1; }` with no calls, `.cmd` `run --timeout 2 {}`, `.stress-every` `skip`. Expect R0009 at the loop's span, as `build-oracle/vo` gives. Add it to `tests/gc/timeout_check.py`'s list (stop within 1 s of the limit, SC-008).
- [X] T026 [US3] Check the stack limit:
  - `vm_deep_recursion` and `vm_recursion_200k` pass;
  - `run/stack` gives the same R0005 span as the oracle;
  - one run at 300,000 calls deep, recorded in `results.md` (the new limit, FR-010).

  If the value stack frame for a simple function is larger than data-model.md says, fix it before moving on.
- [X] T027 [US3] Memory:
  - `python3 tests/gc_bounds.py` passes (five workloads under 64 MiB peak, out-of-memory determinism, test order, repeat);
  - `run/oom` and `gc/edge/grow_forever` R0006 reports are identical over 10 runs.

  If stale temporaries cause a measurable increase in peak memory, clear temporaries at statement ends (data-model note) rather than raising any bound.

---

## Phase 6: User Story 4 - Confidence that the new engine is equivalent (Priority: P2)

**Goal**: SC-004, SC-005: the stress pass, the fault matrix with new targeted programs, and generated programs under stress.

**Independent test**: `make gc-check`.

- [X] T028 [P] [US4] Add fault-target programs in `tests/gc/fault_targets/` (research R7), each with a `.out` from the oracle:
  - `temporaries.vo`: `f(g(), [1, 2])`, where `g` returns a fresh list that sits in a temporary while the list literal allocates;
  - `builtin_args.vo`: `join(split(s, " "), make_sep())`, with a staged argument live while the next one allocates;
  - `callback_frame.vo`: a `sort_by` comparator that allocates, with values held in the callback's frame.

  `tests/gc_faults.py` picks them up automatically. Check that the VSTACK fault build fails each of them, and that the plain stress build passes them.
- [X] T029 [US4] Run `make gc-check`:
  - the stress pass over the corpus;
  - bench2 under stress;
  - the stress fuzzer;
  - the fault matrix (every kind caught, PERM informational);
  - `diff_engines.py --stress --count 1000`.

  Any new `.stress-every` entry needs a one-line reason in the file. Record the results in `results.md`.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T030 Delete the tree walker from `src/interp.s` (research R16):
  - remove `eval_expr`, `eval_binary`, `exec_block`, `exec_stmt`, `eval_list_literal`, `eval_new_record` and `eval_call_value`;
  - remove `try_target` and the `jmp_save` in `call_fn`, and the tree-walker half of `eval_builtin` in `src/rt_call.s`;
  - remove the `VM_REPORT_FALLBACK` code and `make VMREPORT`.

  `tick` keeps only what the VM uses. Run `make test` and `make gc-check`.
- [X] T031 [P] Add `ponytail:` markers, each naming the ceiling and the upgrade path from plan.md's Complexity Tracking:
  - 32-byte instructions (`src/vm.s` header);
  - the recursive compiler and uncleared temporaries (`src/compile.s`);
  - builtins behind `builtin_call` (`src/rt_call.s`).
- [X] T032 [P] Run `make lint` until clean. Append any SESI false positive or negative to `sesi-linter-issues.md`.
- [X] T033 [P] Docs:
  - `README.md`: "interpreter" becomes "bytecode compiler and VM", with the new line count.
  - `docs/roadmap.md`: 004 marked done, with a link to results, and the "Out of scope" line updated.
  - `docs/reference.md` and `docs/errors.md` stay unchanged. Check that `tests/ref_examples.py` passes and that the reference token count is unchanged (SC-009).
- [X] T034 Write `specs/004-bytecode-compiler/results.md`:
  - every SC with its measured number;
  - the speed table: oracle, new, speedup, Python ratio;
  - the intended changes (memory figures, recursion depth);
  - bugs found, deviations from the plan, and follow-ups.
- [X] T035 Final gate: `make test`, `make gc-check` and `make speed-check` all pass. Update `results.md` with the final figures. Mark every task `[X]`.

---

## Dependencies & Execution Order

- **Setup (T001–T006):** T001 first, since the others compare against the oracle. T002, T003 and T004 can run in parallel. T005 needs T004; T006 needs T003 and T005.
- **Foundational (T007–T012):** T007, then T008. T009 can run in parallel with T008. T010 and T011 need T007 and T008. T012 needs T010 and T011.
- **US2 (T013–T019):** T013–T016 extend the same two files, so they run in sequence. T017 needs T012. T018 needs T013–T017, and T019 needs T018.
- **US1 (T020–T024):** after US2. T021–T023 in sequence (same files), then T024.
- **US3 (T025–T027)** and **US4 (T028–T029):** after US1. T025 and T028 can be written earlier, in parallel, because they are test files.
- **Polish (T030–T035):** after everything else. T031–T033 can run in parallel.

### Parallel opportunities

- **Phase 1:** T002, T003 and T004 (different files).
- **Phase 2:** T009 (golden cases) alongside T008, T010 and T011.
- **US3/US4:** T025 and T028 (test files) at any time after T001.
- **Polish:** T031, T032 and T033.

### Implementation strategy

1. **MVP:** Setup, then Foundational (a mixed engine that is always green), then US2 (the VM is the only engine, and equivalent), then US1 (4× or better). At this point it can be merged.
2. **Then** US3 and US4 confirm the limits and the collector guarantees; most of their tests already pass by then.
3. **Polish** removes the dead tree walker and records results.

Every step keeps `make test` green. The mixed engine (T012) exists so that no step is a big-bang switch.

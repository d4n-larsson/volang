# Research: vo-lang Bytecode Execution

Each entry: the decision, why, and what else was considered. "Today" means the tree-walking interpreter in `src/interp.s` as of commit `1fff558`.

## What the tree walker already does well, and where its time goes

Today's interpreter is not naive about values:
- **Values** are raw 64-bit words: `int`, `bool` and `float` bits unboxed, everything else a heap pointer.
- **Operators are typed by the checker:** `OP_ADD_INT`, `OP_LT_FLOAT` and so on. Only `ordered` type parameters resolve at run time (`OP_*_PARAM` through `rt_concrete`).
- **Locals** live in numbered frame slots on the value stack (`local_addr(slot)`).

So the 5.4 s for 10 million iterations of `set total = total + i % 7` (about 540 ns per iteration under qemu) is almost all overhead around the arithmetic:

- **Native recursion per node:** `exec_block` → `exec_stmt` → `eval_expr` → `eval_binary` → `eval_expr` … Each level has a prologue and epilogue saving four to ten registers.
- **Dispatch by compare chains:** `eval_expr` tests the node kind against up to 20 constants in order; `eval_binary` tests the operator against up to 20.
- **Small helpers on every step:** `vec_get` for every statement in a block, `local_addr` for every variable read, `tick` for every loop iteration.
- **Every call runs `jmp_save`** (a setjmp) so that `try` can unwind, and saves ten registers.

The design below removes these costs and keeps values, frames, typed operators, the runtime and the error paths as they are.

## R1. Register (frame-slot) bytecode, not a stack machine

**Decision:** three-address instructions over frame slots. `ADD_INT a, b, c` means `slot[a] = slot[b] + slot[c]`, with an overflow check. Locals keep the slots the checker already assigns (`N_E` of a function is its local count, and `N_C`/`N_E` of identifiers, lets, fors and arms are slot numbers). The compiler allocates temporaries for intermediate results in slots above the locals, in stack order. A function's frame size is its locals plus its maximum temporary depth.

**Rationale:**
- The checker has already numbered every local, so a register VM is the natural target.
- About half as many instructions are dispatched as on a stack machine. The loop body above becomes `REM_INT_K t, i, 7; ADD_INT total, total, t`, plus the loop step.
- Temporaries live in frame slots on the value stack, which the collector already scans conservatively. No new root kind appears (R7).

**Alternatives:**
- **A stack machine:** simpler to compile, but more dispatches and more loads and stores per operation.
- **Caching the tree walk in place** (superinstructions on AST nodes): it keeps the native recursion, which is the biggest cost.

## R2. Fixed 32-byte instructions with direct threading

**Decision:** every instruction is 32 bytes:

| Bytes | Field | Use |
|---|---|---|
| 0–7 | `handler` | address of the instruction's handler code |
| 8–11 | `a` (i32) | usually the destination slot |
| 12–15 | `b` (i32) | a source slot, or the first argument slot |
| 16–23 | `c` (i64) | a source slot, an immediate (`int` constant, `float` bits, `str` pointer), a jump target (instruction address), a compiled-function pointer, or a builtin id |
| 24–31 | `node` | the syntax node the instruction came from (for errors, types and `cur_node`) |

**Dispatch:** each handler ends with `addi pc, pc, 32; ld t0, 0(pc); jr t0`. A jump sets `pc` from `c`.

**Rationale:**
- **One load and an indirect jump per dispatch.** There is no opcode table and no decode step.
- **Every operand fits in place.** No instruction needs extension words: calls pass arguments in consecutive slots (R3), and 64-bit constants fit in `c`.
- **No separate source map.** `node` is in the instruction, so a handler that raises an error passes `node` to the existing `rt_raise` paths unchanged (FR-006). Builtins get their call node the same way.
- **Deterministic:** the binary is static and non-PIE (`ld -static`), so handler addresses are fixed and the compiled form is byte-identical from run to run (FR-003).
- **No size limits in practice:** 32-bit slot fields mean no function the checker accepts can run out of slot numbers (FR-004).

**Cost:** 32 bytes per instruction. A 1,000-line program compiles to roughly 5,000–10,000 instructions, 160–320 KiB, allocated once per run. Marked with a `ponytail:` comment (8-byte encodings with a decode step are the upgrade path if memory ever matters).

**Alternatives:**
- **Opcode byte plus a jump table:** an extra load and add per dispatch.
- **Variable-length encodings:** smaller, but harder to compile and decode in assembly.
- **16-bit slot fields:** a size limit FR-004 would have to explain.

## R3. Calls stay inside the dispatch loop; the loop is re-entrant for callbacks

**Decision:**
- **`CALL a, b, c`** (callee in `c`, arguments in slots `b…`, result to slot `a`) pushes a new frame on the value stack. It copies the arguments, saves the return `pc` and destination slot in the new frame, and continues in the same dispatch loop. **`RET`** pops the frame and resumes the caller.
- **Builtins that call back into vo** (`sort_by`, `map`, `filter`, `fold`, `any`, `all`) go through `call_value` → `call_fn`. `call_fn` becomes a small wrapper that pushes a frame marked "return to native" and calls `vm_run` recursively. That frame's `RET` returns from `vm_run`.

**Rationale:**
- vo-to-vo calls no longer use the native stack. So:
  - recursion depth is bounded by the 32 MiB value stack, not the 64 MiB native stack plus deep native frames;
  - a simple recursive function gets about 96 bytes per call, so roughly 300,000 calls before R0005 (FR-010 needs 100,000; today's limit is between 100,000 and 200,000).
- Re-entry keeps builtins untouched: they still call `call_value(closure, call node, values)`.

**R0005:**
- Raised when a new frame would pass `vstack_end`, from the call instruction's `node`, so it is deterministic (FR-010).
- Native re-entry keeps `check_stack`.

**Alternatives:**
- **A recursive `vm_run` for every call:** simpler, but it keeps the native-stack cost and makes the depth limit depend on handler frame sizes.
- **Converting builtins to continuation style:** a large rewrite with no benefit to the speed criteria.

## R4. Frames: today's layout, extended

**Decision:** keep `FR_PARENT`, `FR_FN`, `FR_CALL`, `FR_TARGS` and `FR_EFFECTS` at their current offsets, so that `rt_raise`, `frame_fn_name`, effect checks (`rt_check_bit`) and generic resolution (`rt_targs`, `rt_concrete`) work unchanged. Add three fields:

| Field | Holds |
|---|---|
| `FR_CODE` | the compiled function |
| `FR_RPC` | the return `pc`, or 0 for "return to native" |
| `FR_RDST` | the caller's destination slot |

`FR_SLOTS` moves from 40 to 64. Every slot of a new frame is zeroed, as today. Arguments, captured values (closures) and type arguments are filled by the same rules `call_fn` uses today.

**Rationale:** frames are where runtime errors, effects and generics find their context. Keeping the fields means none of that code changes (FR-006, FR-007).

## R5. `try`, `return`, `break` and `continue` become jumps

**Decision:**
- **`try e`:** compiles to `TRY_UNWRAP a, b, Lerr`. On `ok` the payload goes to `a`; on `err` it jumps to the function's return sequence with the `err` value as the result.
- **`break` and `continue`:** jumps to the loop's exit and step labels.
- **`return`:** moves the value to the result and executes `RET`.
- **No more `jmp_save`:** the per-call setjmp and the `try_target` chain are removed.

**Rationale:** `try` only ever leaves the current function. Today's longjmp is there only because the tree walker has no other way to unwind several native levels. Removing it is a large part of the call cost.

**Not affected:** runtime errors (`rt_raise`) still exit the run, or, under `vo test`, longjmp to the test runner. That path is separate and unchanged.

## R6. Evaluation order and loop semantics are copied exactly

**Decision:** the compiler emits operations in the tree walker's exact order. The full list is in [contracts/equivalence.md](contracts/equivalence.md). The important cases:
- **Binary operators:** left, then right. `and`/`or` short-circuit with jumps.
- **Call arguments:** left to right.
- **Builtins:** arguments first, then `cur_node`, then the effect check, then the operation.
- **Indexing:** object, then index, then the bounds or key check.
- **`set xs[i] = v`:** object, index, value, then the check.
- **`set r.f = v`:** object, then value.
- **`for x in a..b`:** evaluates `a` and `b` once.
- **`for x in xs`:** re-reads `len(xs)` on every iteration. A list that grows inside its own loop is iterated further, exactly as today.
- **`match`:** scans the arms in source order and binds the payload when the arm has a binding.
- **`while`:** counts a step before each test of the condition.

**Rationale:** FR-005 and FR-006 need not just the same results but the same first error. The tree walker's order is the specification.

## R7. Collector roots: no new kind of place

**Decision:** the VM holds vo values in only two places:
1. **Frame slots on the value stack:** locals, temporaries, and call arguments being staged. The collector scans `vbase..vsp` conservatively, so these are covered by the existing VSTACK root kind.
2. **Machine registers within one handler.** Handlers keep the VM state in callee-saved registers (`pc`, frame base). A handler that calls an allocating runtime function first writes its operands to slots, or keeps them in `s` registers, which `gc_collect` spills to the native stack (NATSTACK kind).

Compiled code is allocated in the permanent arena (`perm_begin`/`perm_end`), because it must survive the heap reset before every test. Its `str` constants are the parser's perm strings, as today.

**Verification (FR-016):**
- The existing VSTACK and NATSTACK fault builds are re-run on the new engine.
- New targeted programs cover:
  - a temporary that is live across an allocating instruction (`f(g(), [1, 2])`: the result of `g()` sits in a temporary while the list literal allocates);
  - builtin arguments staged in slots while another argument allocates;
  - a callback's frame during `sort_by`.
- The stress build is unchanged and runs the whole corpus.

**Rationale:** adding no new root kind is the strongest form of FR-012. Stale temporaries can retain garbage until they are overwritten. This only affects memory figures, which FR-011 allows to move.

## R8. Builtins: values in, same bodies

**Decision:** split `eval_builtin` (`src/rt_call.s`) in two:
- the argument evaluation moves into the compiler, which evaluates the arguments into consecutive temporaries;
- a new entry `builtin_call(node, a0, a1, a2)` does the rest exactly as today: sets `cur_node`, runs `rt_check_bit` for the effect, and jumps through `btab` with `s0` = call node.

The `btab` bodies are not changed. `CALL_BUILTIN a, b, c` loads up to three arguments from slots `b…` and calls `builtin_call(node, …)`.

**Rationale:** the 47 builtin bodies use `s0` (the call node) for types and error locations. Keeping that contract leaves about 2,000 lines of runtime untouched. Builtin time is already native, which is why the spec does not expect builtin-heavy programs to get much faster.

**Later, if measurements show it pays:** specialised instructions for hot builtins such as `len`, `push` and `get`.

## R9. Generics, `==` and effects

**Decision:**
- **Known types:** the compiler reads every expression's checked type (`N_TYPE`) and emits typed instructions.
- **`ordered` type parameters** (`OP_*_PARAM`): one `CMP_PARAM` instruction resolves the concrete kind through `rt_concrete` (memoised since spec 003) and then compares.
- **`==` and `!=`:** `EQ_INT` when the type is `int`, `bool` or a variant without payloads; otherwise `EQ_VAL`, which calls `values_equal(a, b, type)` with the static type, or the concrete type for type parameters, exactly as today.
- **Effects:** checked in the same places as today:
  - at builtin calls (R8);
  - when a frame is set up (`FR_EFFECTS` from the callee's declaration, or the caller's for an anonymous function, with effect parameters substituted through `rt_es_subst`).

## R10. Functions, lambdas, tests and the compiled program

**Decision:**
- **What gets compiled:** every `fn`, every `lambda` and every `test` body, as a compiled function.
- **Where the pointer goes:** a new field `FX_CODE` at offset 72 of the FX record (`FX_SIZE` 72 → 80). Functions, lambdas and tests all have one, and `item_fx(node)` (`src/check_effects.s`) finds it for each:
  - for `N_FN` and `N_LAMBDA`, it is `N_B(N_A(node))`;
  - for `N_TEST`, it is `N_D`, created on demand.

  `defs.inc` defines `FX_SIZE` twice, once for an unrelated earlier record (lines 199–204), so the edit must touch the effects one at line 292.
- **Closures:** they keep pointing to their declaration node (`CL_FN`), so `call_fn` finds the code through `FX_CODE`.
- **When:** the whole program is compiled once, after checking and before `gc_start`, into the permanent arena. This applies to `vo run` and to `vo test`, where it happens before the first test.
- **On demand:** lambdas inside generic functions are compiled once, like everything else. Type arguments are runtime data (`FR_TARGS`, `CL_TARGS`), not part of the code.

**Rationale:** compiling everything up front is simple, deterministic, and cheap next to checking (one linear pass over the tree). Compiling lazily would save nothing measurable.

## R11. Timeouts: the same step points, cheaper

**Decision:** today `tick` counts a step at every call with a call node, every `while` condition test and every `for` iteration, and checks the clock every 65,536 steps. The VM keeps exactly these step points:
- `CALL` does the step;
- loop back-edges do it inline: increment `steps`, mask, branch to the clock check every 65,536.

The `rt_budget` POLL sequences inside builtins and the collector are unchanged.

**Rationale:** FR-008 asks only for "within 1 s of the limit". Keeping the same step points keeps the `steps`-based determinism work from spec 003 valid. A tight loop with no calls still hits a back-edge step.

## R12. The comparison engine: the pre-004 binary

**Decision:**
- **`make oracle`** builds the binary of commit `1fff558` (the last commit before this spec) into `build-oracle/vo`, using `git worktree` in `build/oracle-src`. That binary is the before-measurement for every speed figure and the reference engine for differential testing (FR-015, SC-005).
- **The tree walker is deleted from `src/`** once the VM runs every program (spec assumption).

**Alternatives:**
- **Keeping both engines behind a flag:** two engines to maintain through specs 005–007, for no user benefit.

## R13. A generator of valid programs

**Decision:** `tests/gen_programs.py` writes random, type-correct vo programs from a seed. It works over a small typed grammar:
- **Types:** `int`, `bool`, `str`, `float`, `list[int]`, `list[str]`, `map[str, int]`, one record, one variant, `result[int]`, and a generic function.
- **Statements and expressions:** `let`/`var`/`set`, `if`, `while` with a bounded counter, `for` over ranges and lists, `match`, `return`, `break`, `continue`, `try`, calls, recursion with a depth bound, lambdas with captures, `sort_by`/`map`/`filter`/`fold`, and string building.
- **Runtime errors on purpose:** occasional division by a value that may be zero, an index that may be out of range, a missing map key, and overflow.

**Comparison:** `tests/diff_engines.py --count N --seed S` runs each program on `build-oracle/vo` and on `bin/vo` and compares stdout, stderr and exit status. A program that fails to check is a generator bug and is counted separately. Disagreements are written to `build/diff-failures/`.

**Rationale:** the fuzzer from spec 001 mutates corpus files. Most mutants fail to check, so it barely exercises execution. A typed generator reaches unusual combinations of the cases in R6.

## R14. The speed benchmark

**Decision:** `tests/speed/` holds six vo programs, each with a Python equivalent, the same expected output, and a size tuned so that the pre-004 binary takes 2–6 s:

| Program | Exercises |
|---|---|
| `int_loop` | the roadmap's loop |
| `fib` | naive recursion: calls and returns |
| `list_work` | build, index, sum and sort |
| `map_count` | word counting over generated text |
| `str_build` | split, join, find, `+` |
| `closures_generics` | `fold`/`map` with lambdas, a generic container |

`tests/speed.py` runs each program five times on `build-oracle/vo`, `bin/vo` and `python3`. It reports medians, the speedup, the geometric mean and vo's ratio to Python. It fails if the geometric mean is below 4× or any single program is below 2× (SC-001, SC-002). It is part of a new `make speed` target, not `make test`, because it takes about a minute and is sensitive to machine load.

## R15. Compiler structure

**Decision:** `src/compile.s` makes one recursive pass over each function body:
- `cexpr(node, dst) -> slot`: compiles an expression and returns the slot holding its value. An identifier returns its own slot with no instruction. Constants and computations go into `dst`, or a fresh temporary.
- `cstmt(node)`: compiles a statement.

It keeps a per-function context: the temporary watermark, the maximum depth, and a stack of loop labels for `break`/`continue`. Forward jumps are patched once their target is known. Instructions are appended to a growable buffer and copied to the permanent arena when the function is done.

**Peephole rules, only where the speed criteria need them:**
- `x op constant` with a small constant (`ADD_INT_K`, `REM_INT_K`, `LT_INT_K`, …);
- compare-and-branch fused into one instruction for `if`, `while` and loop conditions;
- `for` over a range as a single `FOR_RANGE` back-edge instruction (step, compare, branch).

Nothing that could move or remove a runtime error is optimised (spec: out of scope).

## R16. What leaves `interp.s`

The tree walker goes: `eval_expr`, `eval_binary`, `exec_block`, `exec_stmt`, `eval_list_literal`, `eval_new_record`, `eval_call_value`, the `try_target` chain, and the argument-evaluation half of `eval_builtin`.

What stays:
- the error paths: `rt_raise*`, `rt_overflow`, `rt_index_error`, `rt_key_error`;
- timeouts: `tick`, `rt_poll`, `rt_timeout`;
- `values_equal`, `make_closure`, `fn_closure`;
- `call_fn` and `call_value`, now wrappers that enter the VM;
- `run_main`, `run_test_body`, `gc_start` and `interp_init`.

`make_closure` keeps building closures from a lambda node and the current frame: it reads the captured slots through `FX_CAPS`, as today.

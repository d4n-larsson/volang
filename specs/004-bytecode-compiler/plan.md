# Implementation Plan: vo-lang Bytecode Execution

**Branch**: `004-bytecode-compiler` | **Date**: 2026-11-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/004-bytecode-compiler/spec.md`

## Summary

vo programs stop being executed by walking the syntax tree. After checking, each function, lambda and test body is compiled once into register bytecode, and a direct-threaded dispatch loop in assembly runs it. Nothing visible changes.

**1. The compiler** (`src/compile.s`, research R1, R2, R6, R15):
- **Instructions:** three-address, over the frame slots the checker already assigns, plus temporaries above the locals.
- **Encoding:** fixed 32 bytes per instruction `{handler, a, b, c, node}`. The source node in every instruction makes runtime errors report the same spans with no separate source map.
- **Order:** operations are emitted in exactly the tree walker's evaluation order ([contracts/equivalence.md](contracts/equivalence.md)).
- **Optimisation:** constant operands, fused compare-and-branch, and a single range-loop instruction. Nothing that could move a runtime error.

**2. The VM** (`src/vm.s`, research R3–R5, R7–R11):
- **One dispatch loop** with the VM state in callee-saved registers. vo-to-vo calls push frames on the value stack without native recursion. Builtin callbacks re-enter the loop.
- **Frames keep today's fields,** so error reporting, effect checks and generic resolution are unchanged.
- **`try` becomes a jump,** so the per-call setjmp disappears.
- **Builtins keep their bodies** behind a new `builtin_call(node, args…)` entry.
- **The collector sees the VM's values** in value-stack slots and spilled `s` registers only, so no new root kind appears.

**3. Verification** (research R12–R14):
- `make oracle` builds the pre-004 binary as the comparison engine.
- A generator of random valid programs, compared on both engines.
- A six-program speed benchmark with Python equivalents.
- The existing corpus, the stress pass and the fault matrix, extended with targeted programs for temporaries, staged builtin arguments and callback frames.

**4. Removal:** the tree walker leaves `src/interp.s` once the VM runs everything (research R16).

## Technical Context

**Language/Version**: RISC-V assembly (RV64IMAFD, lp64d), GNU as. This is the existing v3 toolchain, extended in place.

**Primary Dependencies**:
- None at run time.
- Build: `binutils-riscv64-linux-gnu`.
- Running on x86: `qemu-user`.
- Lint: the SESI `riscv64` pack.
- Comparison engine: `git worktree`.

**Storage**: N/A. Compiled code lives in the permanent arena for one run.

**Testing**:
- **The existing gate, unchanged:** `make test` (golden cases, repeats, fuzz, catalog, `ref_examples`, `fmt_corpus`, `migrate`, `record_changes`, `scope_fuzz`, `gc_bounds`, the bench2 baseline, `timeout_check`) and `make gc-check` (stress pass, bench2 under stress, fault matrix).
- **New:**
  - `tests/speed/` and `tests/speed.py` (`make speed`);
  - `tests/gen_programs.py` and `tests/diff_engines.py`;
  - golden cases for the equivalence edge cases;
  - fault-target programs for temporaries, builtin arguments and callbacks.

**Target Platform**: Linux user space, RV64 (qemu-riscv64 on x86 hosts).

**Project Type**: Compiler front end and interpreter, as a CLI.

**Performance Goals**:
- Geometric-mean speedup of at least 4× on the speed benchmark, with no program below 2×.
- `int_loop` (10 million iterations) at most 1.35 s; it takes 5.4 s today.
- Checking and running a 1,000-line program stays under 1 s (87–95 ms today).
- bench2 reference runs: at most 10% more CPU time in total, and no single run slower by more than 10% or 5 ms, whichever is larger.

**Constraints**:
- Byte-identical output, apart from the memory figures and the recursion depth (FR-005).
- The same error spans.
- The same evaluation order.
- No user-facing surface.
- The language reference unchanged.
- A deterministic compiled form.

**Scale/Scope**:
- About 2,500 lines for `compile.s` and 1,500 for `vm.s`.
- About 1,200 lines removed from `interp.s`.
- About 100 lines changed in `rt_call.s`, `testrun.s`, `main.s`, `rt_func.s` and `defs.inc`.
- 3 Python test tools, 6 speed programs, about 15 golden cases and about 3 fault-target programs.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluated against constitution **v1.1.0**.

| Gate | Status | Evidence / obligations |
|---|---|---|
| I. Agent-first design | PASS | No language, syntax or CLI change. Agents see faster programs and nothing else. |
| II. Machine-readable feedback | PASS | Diagnostics, runtime errors and schemas are unchanged. Every raising instruction carries the node whose span today's error uses (data-model, `I_NODE`). |
| III. Determinism and defined behavior | PASS | The compiled form is deterministic (static non-PIE binary, fixed-address arena; research R2). The evaluation order is a written contract (contracts/equivalence.md). R0005 is raised deterministically from the value stack (research R3). Timeout step points are unchanged (research R11). |
| IV. Test-first through the CLI | PASS | **Obligation**: the speed benchmark (which fails on the tree walker by construction), the differential tester and the equivalence golden cases are written before the compiler. The golden cases pass on the oracle first, which proves they state today's behaviour. |
| V. Single source of truth, minimal surface | PASS | The reference, catalog and contracts are unchanged. **Obligation**: shortcuts carry `ponytail:` markers (32-byte instructions, a recursive compiler, the stale temporaries kept for the collector). |
| Implementation constraints | PASS | Assembly only. `compile.s` passes the SESI lint. In `vm.s`, the handlers are local labels inside one prototyped `vm_run`, and the other functions lint clean. If SESI cannot follow the direct-threaded `jr` dispatch, `vm_run` gets an explained `sesi: allow`, or `vm.s` joins `LINT_EXEMPT` with a written reason, as `jmp.s` did (T010). The comparison binary and test tools are developer-only and not shipped. |
| Quality gates | PASS | `make test` gains the differential tester (1,000 programs; 10,000 in `make speed-check`). `make speed` is separate, because timing depends on machine load. |

**Post-design re-check:** PASS.

The design keeps every runtime contract (frames, `rt_raise`, builtins, closures) and changes only how control reaches them. That is the main way it protects FR-005–FR-009.

The one intended behaviour difference, a deeper recursion limit, is written into the spec (FR-005, FR-010) and the contract.

## Project Structure

### Documentation (this feature)

```text
specs/004-bytecode-compiler/
├── plan.md
├── research.md                # R1–R16 decisions
├── data-model.md              # instruction, instruction set, compiled function, frame, VM registers
├── quickstart.md              # validation scenarios
├── contracts/
│   └── equivalence.md         # observable equality, evaluation order, statements, frames, errors, depth
├── checklists/requirements.md
└── tasks.md                   # /speckit-tasks
```

### Source Code (repository root)

```text
src/
├── defs.inc        # + instruction and compiled-function offsets, FR_CODE/FR_RPC/FR_RDST, FR_SLOTS 64,
│                   #   FX_CODE (the effects FX record; FX_SIZE 72 → 80)
├── compile.s       # NEW: compile_program, compile_fn, cexpr, cstmt, labels and patching, peepholes
├── vm.s            # NEW: vm_run, the handlers, frame push/pop, CALL/RET, re-entry from call_fn
├── interp.s        # tree walker removed; call_fn/call_value enter the VM; run_main/run_test_body
│                   #   run compiled code; error, timeout, equality and closure helpers stay
├── rt_call.s       # eval_builtin → builtin_call(node, a0, a1, a2): arguments arrive as values
├── testrun.s       # compile before the first test; test bodies run through the VM
├── main.s          # compile after checking, before gc_start (run)
└── rt_func.s       # HOF callbacks: unchanged calls to call_value (verify only)
Makefile            # oracle, speed and speed-check targets; vm.o and compile.o in OBJS
tests/speed/        # NEW: six programs, each .vo, .py and .out
tests/speed.py      # NEW: oracle vs new vs python3, medians, ratios, thresholds
tests/gen_programs.py   # NEW: seeded generator of type-correct programs
tests/diff_engines.py   # NEW: run generated programs on both engines and compare
tests/cases/run/    # + equivalence edge cases (evaluation order, loops, try, match, closures, depth)
tests/gc/fault_targets/ # + temporaries, builtin_args, callback_frame
```

**Structure Decision:**
- **Two new files** with one job each: `compile.s` turns trees into bytecode, and `vm.s` runs it.
- **The runtime around them does not move.** Builtins, collections, errors, effects, generics and the collector are reused through their current entry points. The only new entry point is `builtin_call`.
- **The tree walker is deleted** at the end, rather than kept behind a flag. The pre-004 binary is the comparison engine.

## Phases (for /speckit-tasks)

1. **Setup:**
   - `make oracle`;
   - the speed benchmark and `tests/speed.py`, which fails on the current build;
   - the program generator and differential tester (0 disagreements on oracle vs oracle).
2. **Foundation:**
   - `defs.inc` layouts;
   - the `builtin_call` split (the tree walker uses it first, behaviour identical);
   - the frame fields;
   - the VM skeleton and compiler skeleton (constants, locals, arithmetic, `if`/`while`/`for`, calls, `RET`).
   - Each step is checked against the full golden corpus.
3. **US2 (P1), equivalence:**
   - every construct in contracts/equivalence.md: data, `match`, `try`, closures, value calls, generics, effects, tests;
   - the tree walker unreachable;
   - golden corpus, baseline, 10,000 generated programs.
4. **US1 (P1), speed:** `_K` forms, fused branches, `FOR_RANGE`, cheap steps. Iterate on `make speed` until SC-001 and SC-002 hold.
5. **US3 (P2), limits:** the depth golden case, R0005 from the value stack, `gc_bounds`, the timeout cases.
6. **US4 (P2), confidence:** the stress pass and fault matrix with the new targeted programs.
7. **Polish:**
   - delete the tree walker;
   - `ponytail:` markers;
   - `results.md`;
   - roadmap status;
   - a README line ("compiled to bytecode").

## Complexity Tracking

No constitution violations. Deliberate shortcuts, each with its ceiling:

| Shortcut | Ceiling | Upgrade path |
|---|---|---|
| Fixed 32-byte instructions with the node inline | About 4× the size of a packed encoding, around 300 KiB for a 1,000-line program | 8-byte encoding with an opcode table and a side table for nodes |
| A recursive compiler over the tree | Native stack depth limits expression nesting. The parser already bounds it | An explicit work stack, if the parser's bound is ever raised |
| Temporaries are never cleared after use | A dead temporary can keep garbage alive until it is overwritten. This affects memory figures only | Clear temporaries at statement ends, or use precise frame maps |
| Builtins keep their bodies behind `builtin_call` | Builtin-heavy code gains little | Specialised instructions for hot builtins (`len`, `push`, `get`), if measurements show they pay |

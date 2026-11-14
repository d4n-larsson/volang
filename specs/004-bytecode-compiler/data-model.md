# Data Model: vo-lang Bytecode Execution

Everything here is internal to the toolchain. Nothing is visible to vo programs or in any output format. Offsets are in bytes; slots are 8 bytes.

## Instruction (32 bytes)

| Offset | Field | Type | Meaning |
|---|---|---|---|
| 0 | `I_HANDLER` | address | handler entry point; dispatch is `ld t0, I_HANDLER(pc); jr t0` |
| 8 | `I_A` | i32 | usually the destination slot |
| 12 | `I_B` | i32 | a source slot, or the first argument slot |
| 16 | `I_C` | i64 | a source slot, a constant (`int`, `float` bits, `str` pointer), a jump target (absolute instruction address), a compiled function, or a builtin id |
| 24 | `I_NODE` | pointer | the source node: the span for runtime errors, the static type for `==` and map keys, `cur_node` for builtins |

**Rules:**
- `pc` always points at the instruction being executed. Falling through adds 32.
- Jump targets are resolved before the function is copied to the permanent arena. Absolute addresses are deterministic, because the arena and the binary are at fixed addresses (spec 003).
- Every instruction that can raise has a non-zero `I_NODE`.

## Instruction set (initial)

The compiler chooses the instruction from the checked type of the operands. `K` forms take a small constant in `I_C` instead of a slot. The list is a starting point. Instructions are added only when a speed criterion or a correctness case needs them.

| Group | Instructions | Notes |
|---|---|---|
| Moves and constants | `MOV a, b`, `LOADK a, c` | `LOADK` covers `int`, `float` bits, `bool`, and `str` pointers |
| Integer arithmetic | `ADD_INT`, `SUB_INT`, `MUL_INT`, `DIV_INT`, `REM_INT`, `NEG_INT`, plus `_K` forms of `ADD`, `SUB`, `MUL`, `REM` | overflow → R0001; zero divisor → R0002; `MIN / -1` → R0001, as `eval_binary` does today |
| Floats | `ADD_FLOAT`, `SUB_FLOAT`, `MUL_FLOAT`, `DIV_FLOAT`, `NEG_FLOAT` | same results as today |
| Comparisons to a slot | `LT/LE/GT/GE` for `_INT`, `_FLOAT` and `_STR`, `EQ_INT`, `NE_INT`, `EQ_VAL`, `NE_VAL`, `CMP_PARAM` | `EQ_VAL` calls `values_equal` with the type from `I_NODE`; `CMP_PARAM` resolves the concrete type through `rt_concrete` |
| Branches | `JMP c`; `JT`/`JF b, c`; fused compare-and-branch: `JLT_INT`, `JGE_INT`, `JLT_INT_K`, … | used for `if`, `while`, `and`/`or` and loop conditions |
| Loops | `FOR_RANGE a, b, c` (step slot `a`, end slot `b`, back-edge target `c`); `FOR_LIST` | each counts one step (timeouts), as today |
| Strings and logic | `CONCAT a, b, c`, `NOT a, b` | `CONCAT` calls the existing concatenation |
| Data | `NEW_LIST`, `NEW_MAP`, `LIST_LIT a, b, n`, `NEW_RECORD`, `VCASE`, `GET_FIELD`, `SET_FIELD`, `INDEX_LIST`, `INDEX_MAP`, `SET_INDEX_LIST`, `SET_INDEX_MAP`, `TAG a, b`, `PAYLOAD a, b` | the same runtime calls and checks as the tree walker |
| `match` | `JTAG_NE b, k, c` | one per arm, in source order |
| `try` | `TRY_UNWRAP a, b, c` | `ok` → payload into `a`; `err` → the value becomes the result and control jumps to `c`, the return sequence |
| Calls | `CALL a, b, c` (compiled function in `c`, arguments in slots `b…`), `CALL_BUILTIN a, b` (arguments in `b…`, id from the node), `CALL_VALUE a, b, c` (callee slot `c`, arguments `b…`) | the call node is in `I_NODE` |
| Closures | `CLOSURE a` (lambda node in `I_NODE`), `FNVAL a` (named function) | wrap `make_closure` and `fn_closure` |
| Return | `RET b` (value slot, or −1 for none) | pops the frame; returns from `vm_run` when `FR_RPC` = 0 |

## Compiled function

Allocated in the permanent arena. It is reached from `FX_CODE` in the declaration's FX record (`item_fx(node)`), for functions, lambdas and tests alike.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `CF_CODE` | address of the first instruction |
| 8 | `CF_N` | instruction count |
| 16 | `CF_SLOTS` | frame size in slots: locals (`N_E` of the declaration) plus the maximum temporary depth |
| 24 | `CF_NPARAMS` | parameter count |
| 32 | `CF_DECL` | the `N_FN`, `N_LAMBDA` or `N_TEST` node |

The instructions follow the header contiguously. There is no separate source map: `I_NODE` in each instruction is the source map.

## Frame (on the value stack)

The current fields keep their offsets, so `rt_raise`, `frame_fn_name`, `rt_check_bit`, `rt_targs` and `rt_concrete` are unchanged.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `FR_PARENT` | caller frame (0 for the outermost) |
| 8 | `FR_FN` | declaration node (as today) |
| 16 | `FR_CALL` | call node, or 0 |
| 24 | `FR_TARGS` | type arguments of a generic call or closure, or 0 |
| 32 | `FR_EFFECTS` | effect set the running code is held to |
| 40 | `FR_CODE` | compiled function (new) |
| 48 | `FR_RPC` | return `pc`; 0 means return to the native caller of `vm_run` (new) |
| 56 | `FR_RDST` | caller's destination slot for the result, or −1 (new) |
| 64 | `FR_SLOTS` | slot 0 … `CF_SLOTS`−1: parameters, then locals, then temporaries |

**Lifecycle:**
- **Push** (`CALL`, or `call_fn` from native code): check `vsp + 64 + 8 × CF_SLOTS ≤ vstack_end`, otherwise R0005 at the call node. Then zero the slots, copy the arguments, captured values and type arguments, and set the effect set, by today's rules.
- **Pop** (`RET`): `vsp` returns to the frame base, `cur_frame` to `FR_PARENT`, and the result goes to the caller's `FR_RDST`.

## VM registers (inside `vm_run`)

| Register | Holds |
|---|---|
| `s1` | `pc` |
| `s2` | current frame base |
| `s3` | the address of `FR_SLOTS` in the current frame |

All three are callee-saved, so `gc_collect`'s spill of `s0`–`s11` covers them. They are pointers into the permanent arena and the value stack, never the only reference to a heap object. Temporaries that hold heap values are always in slots (research R7).

## Compiler context (during compilation only)

| Field | Meaning |
|---|---|
| buffer | growable instruction buffer |
| temporaries | watermark and maximum depth |
| loop labels | for each enclosing loop: the `continue` target and the list of `break` jumps to patch |
| return sequence | the label `try` and `return` jump to, patched at the end |

The context lives on the native stack and in a scratch buffer; it is gone once the function is copied out.

## The comparison engine and test data

| Item | Location | Contents |
|---|---|---|
| Comparison binary | `build-oracle/vo` | built by `make oracle` from commit `1fff558` |
| Speed programs | `tests/speed/<name>.vo`, `.py`, `.out` | six programs (research R14) |
| Generated-program failures | `build/diff-failures/<seed>-<n>.vo` | with both outputs |

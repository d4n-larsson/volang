# Contract: the bytecode engine behaves like the tree walker

This spec adds no new user-facing interface: no command, option, output field, diagnostic or error code. The CLI, the JSON schemas, the error catalog and the language reference are unchanged (FR-013). The only contract is equivalence with the release before this spec (`build-oracle/vo`, commit `1fff558`). This file states that contract precisely, because the compiler must reproduce it rule by rule. Each rule is how `src/interp.s` behaves at that commit.

## Observable equality

For every program, input, argument list and environment:
- stdout, stderr and the exit status are byte-identical;
- for `vo test`, the human and `--json` reports are byte-identical.

**Exceptions:**
- **Memory figures** in `--mem-stats` and the `in_use` field of R0006 may differ, and stay deterministic (FR-011).
- **Timing:** with `--timeout`, the step at which time runs out depends on the clock, as it does today.
- **Recursion depth:** see the last section.

## Evaluation order

| Construct | Order |
|---|---|
| `a op b` (arithmetic, comparison, `+` on strings) | `a`, then `b`, then the operation and its check (overflow, division by zero) |
| `a and b`, `a or b` | `a`; `b` only if needed |
| `-a`, `not a` | `a`, then the operation (`-MIN` → R0001) |
| `f(x, y)` (user function) | the frame is reserved, then `x` and `y` are evaluated left to right, then the step is counted and the body runs |
| `builtin(x, y, z)` | `x`, `y`, `z`, then `cur_node` = the call, then the effect check (R0010), then the operation |
| `callee(x, y)` (a value call) | `callee`, then the arguments left to right, then the call |
| `xs[i]` | `xs`, `i`, then the bounds check (R0003) or key lookup (R0004) |
| `r.f` | `r`, then the field load |
| `set x = e` | `e`, then the store |
| `set r.f = e` | `r`, `e`, then the store |
| `set xs[i] = e` | `xs`, `i`, `e`, then the bounds check, or the map insert with `cur_node` = the statement |
| `[a, b, c]` | elements left to right, then the list |
| `new R { f: a, g: b }` | initialisers in source order |
| `Case(e)` | the variant object is allocated, then `e` is evaluated into it |
| `try e` | `e`; `ok(v)` gives `v`; `err` makes the `err` value the function's result and returns |
| `let x = e` / `let _ = e` | `e`, then the store (or nothing) |

**Calls with nested calls:** the tree walker reserves the callee's frame before evaluating the arguments. The bytecode engine evaluates the arguments first, then reserves the frame. The difference can only be seen in which call raises R0005 when the value stack is exactly full. That falls under the depth exception below.

## Statements

| Statement | Behaviour |
|---|---|
| `if c { } else if d { } else { }` | `c`; the first true branch runs |
| `while c { }` | a step is counted, then `c` is tested, before every iteration |
| `for i in a..b { }` | `a` and `b` once, in that order. A hidden counter runs from `a` while it is below `b`. Each iteration counts a step and stores the counter into `i`. The loop variable cannot be assigned (E0208) |
| `for x in xs { }` | `xs` once. Each iteration re-reads the list's current length and data, counts a step and stores the element. A list that grows inside its own loop is iterated further |
| `match v { case … }` | `v` once; the arms are tested in source order against the tag. The first match binds its payload (if it has a binding) and runs. No match does nothing |
| `break`, `continue` | the innermost loop |
| `return e` | `e`, then the return. In `main`, the existing `run_main` rules for `err` results apply |

## Frames, effects and generics

- **A frame's effect set:**
  - **Named function:** its own declaration, with effect parameters substituted through the call's type arguments (`rt_es_subst`).
  - **Anonymous function:** the caller's set.
- **Type arguments:**
  - **Generic call:** resolved from the caller's frame (`rt_targs`).
  - **Closure:** from the closure (`CL_TARGS`).
- **Captured values:** copied into the callee's slots by `FX_CAPS`, as today.

## Runtime errors

- **Spans:** every runtime error uses the span of the same node as today. For each instruction that can raise, that node is in `I_NODE`.
- **R0005:**
  - The program stops with R0005 when a new frame does not fit the value stack.
  - The stop happens at the same call node on every run.
  - For a simple recursive function, it happens no earlier than 100,000 calls deep.

## Recursion depth (the one intended difference besides memory figures)

The exact depth at which R0005 fires changes. It can only increase for ordinary recursion: 32 MiB of value stack, at 64 bytes plus 8 per slot for each frame, against today's native-stack limit of between 100,000 and 200,000 calls. A program that recursed successfully before still does. A program that hit R0005 at a finite depth between today's limit and the new one now completes. No existing golden case or benchmark run depends on that depth; the `stack` golden case recurses without bound. The golden case `run/vm_recursion_200k` records the change: it stops with R0005 on the tree walker and prints `200000` on the bytecode engine.

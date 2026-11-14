# Results: Spec 005 (typed holes, property tests, contracts)

## Success criteria

| SC | Result |
|---|---|
| SC-001 | **Met (stand-in).** 62 mutants over h01–h10, each passing every visible case and failing a hidden one; properties catch 61 (98%), at least 4 per task; the visible cases catch none. |
| SC-002 | **Met.** Every property passes on its reference, byte-identical over 3 runs (`make prop-eval`); `make test` repeats every `tests/cases/prop` case 3 times. |
| SC-003 | **Met.** 15 hole positions have `check` golden cases with W0601, expected type and scope; `run_reached`, `test_reached` and `hole_in_prop` stop with R0011 at the `todo` call. |
| SC-004 | **Met.** 11 broken functions, every counterexample checked minimal by hand, 1–3 ms each (table below). |
| SC-005 | **Met.** Golden cases for direct calls, function values, `map`, `try`, void functions (early `return` and closing `}`), generics, recursion, `main` and tests. |
| SC-006 | **Met** after a layout fix (below): every speed program within 0.6% of the spec 004 binary on the final run; the 703 bench2 runs and the corpus are byte-identical; bench2 CPU 0.98× the oracle. |
| SC-007 | **Met.** ~8,190 → ~9,144 tokens (+954); every example passes. |
| SC-008 | **Met.** `make test`, `make gc-check` (340 stress cases, 703 bench2 runs under stress, 3,000 fuzz iterations, every fault kind caught, 1,000 differential programs), `make speed-check` (4.55×, 10,000 differential programs), `make lint`. |

## Shortcuts (`ponytail:` markers)

- `src/prop.s`: fixed 100 cases / 1,000 attempts; a 2,000-run shrink budget; greedy shrinking.
- `src/vm.s`, `src/rt_map.s`: fixed page offsets for qemu (see SC-006 below).

## Follow-ups

- The agent benchmark run with properties (roadmap, Benchmarks item 3).
- A case-count option for `vo test`; a strict mode where holes are errors; `old()` in `ensures`.
- Pre-existing toolchain issues the evaluation agents hit (US4 below).
- SESI: `sesi lint` hangs on an open stdin (`sesi-linter-issues.md` #19).

## Name clashes (T002)

No `.vo` file under `tests/`, `bench/`, `bench2/` or `docs/` uses `prop`, `requires`, `ensures`,
`todo`, `assume` or `ret` as a word, and `docs/reference.md` does not either. No migration case
changes.

## Speed baseline (T003)

`make baseline-004` builds `71375e5` in a worktree at `build-004/src` (not `build/base004-src`
as tasks.md said: `make clean` removes `build/`, which would orphan the worktree; this matches
`build-oracle/src`). `tests/speed.py --baseline PATH` times each `tests/speed` program on PATH and
`build/vo` alternately (median of 5, then 15 for one still over 5%).

Noise check, `build/vo` against itself: ratios 0.986–1.003 (int_loop 0.986, str_build 0.992,
the rest 1.001–1.003). No false failure.

## US1: holes

- **Changed from research R3:** a statement hole `todo("...");` takes the function's return type
  as its expected type (`void` in functions without one and in tests), not always `void`. The
  reports contract's example (`needs a value of type \`int\`` for a statement hole in
  `fn parse(...) -> int`) needs this, and it is what an agent wants to know. E0405 still does not
  apply, and the statement still ends its path.
- **T011 needed no code:** binary operands, conditions, builtin parameters, field initializers,
  list elements and function-typed arguments already pass their expected type.
- **T015 needed no compiler change:** `todo` compiles as an ordinary builtin call
  (`CALL_BUILTIN`), and `b_todo` in `rt_call.s` raises R0011.
- **`vo effects --update`** records a function whose body is a hole with its declared effects
  (checked by hand; the `effects_record` golden case covers `vo effects`).
- **Scope** lists every local visible at the hole, including those of an enclosing function
  inside a lambda (they can be captured).
- **Reference:** the W0601 report appears in the `vo-test` example's output (vo test prints
  warnings), so no separate `text` block. `ref_examples.py` now runs `vo test` with a relative
  path so reports read `main.vo:...`. Growth: ~8,190 → ~8,472 tokens (with the keyword line).
- **`catalog.py`** reads the new codes from `contracts/codes.md`; until US2/US3 land it reports
  their five codes as missing.

## US2: properties

### SC-004: the 11 deliberately broken functions (`tests/cases/prop`)

Times are the `--json` `ms` under qemu-riscv64, for the whole property (generation, the failing
cases, shrinking and the final run). Minimality was checked by hand against research R8: no single
shrink candidate of the reported counterexample still fails the same way.

| Case | Status | Counterexample | Shrink steps | ms | Why no single step still fails |
|---|---|---|---|---|---|
| dedupe_drops | fail | `xs = [1, 0, -1, 2]` | 8 | 2 | 4 distinct values are needed; removing one, halving, or shrinking any element toward 0 makes a duplicate or leaves 3 |
| off_by_one | fail | `n = 0` | 1 | 1 | 0 has no candidates |
| empty_list | error R0003 | `xs = []` | 0 | 2 | `[]` has no candidates |
| overflow | error R0001 | `a = -9223372036854775806, b = -3` | 2 | 1 | `a + 1`, `b / 2`, `b + 1` and 0 all give sums that fit |
| utf8 | error R0007 | `s = "é"` | 1 | 2 | `""` is excluded by `assume`; `"a"` passes |
| mutates_arg | fail | `xs = [0, 0]` | 2 | 2 | removing an element leaves length 1, which passes after the push; the case is unaffected by the body's `push` |
| lost_duplicate | fail | `a = [0], b = [0]` | 0 | 1 | emptying either list passes |
| negative | fail | `n = -1` | 0 | 2 | 0 passes (the `if n != 0` branch) |
| variant_case | fail | `Expr.add(Pair { num(0), num(0) })` | 2 | 2 | the subtree candidates `num(0)` pass; `add` is the forgotten case and needs both children |
| record_fields | fail | `Span { start: 0, stop: 1 }` | 63 | 3 | equal fields pass; `stop: 0` is the only candidate of `stop` |
| map_merge | fail | `a = {"": -1}, b = {"": 0}` | 1 | 3 | the value candidate 0 for `a` makes them equal; emptying either map passes |

All 11 are far under the 2 s limit. Repeated runs (3 times each) are byte-identical; `make test`
repeats every `tests/cases/prop` case 3 times.

### Deviations

- **Generator:** SplitMix64 for both seeding and the stream, instead of PCG-XSH-RR seeded by
  SplitMix64 (research R7). Same determinism, one routine instead of two.
- **Roots:** generated values are kept alive by the collector's conservative roots (the native
  stack and s registers of `prop.s`) and by `pr_args`/`pr_best`/`pr_cand` in `.bss` (the GLOBALS
  root kind). `tests/gc/fault_targets/globals_prop.vo` crashes the GLOBALS fault build and passes
  the plain stress build, so leaving them out is caught.
- **Variants at the depth limit:** only cases without a payload, or whose payload does not contain
  the variant again, are chosen. When a variant has no such case, any case is chosen (a
  `ponytail:`-level gap: such types are rare and E0602 has already shown a finite value exists).
- **Text wording:** `(1 case, Nms)` is singular, and so is `shrunk in 1 step`. A timed-out
  property prints its counterexample without a shrink line (JSON still has `"shrinks":0`).
- **`tests/run.sh` and `tests/ref_examples.py`** now normalise every `Nms)`, so `(100 cases, 14ms)`
  compares as `(100 cases, 0ms)`.
- **The item list in E0110's "expected `fn`, `record`, ..." message** is unchanged: adding `prop`
  would change existing outputs (FR-027).

## US3: contracts

- **Compiled in the callee** (research R12, R13): one false-jump per `requires` at entry, to a
  failure stub placed after the function's code; with `ensures`, every exit (`return`, the `err`
  path of `try`, the closing `}` of a void function) moves into the `ret` slot, stores its node in
  a hidden slot, and jumps to one epilogue that checks each condition and returns. The failure
  stubs are `CALL_BUILTIN` with a compile-time node (id 50 or 51, the condition in `N_A`, the
  function in `N_C`), so no new opcode was needed.
- **FR-024, instruction counts:** a temporary `.ifdef` in `compile_fn` wrote each function's
  `CF_N` to fd 9, built into both this tree and the spec 004 worktree. Over all 73 programs in
  `tests/cases/run` and `tests/speed` the per-function counts are identical. The debug code was
  removed from both trees.
- **Purity (E0603):** each condition is checked with its effects going to a scratch FX record with
  no declaration, the path anonymous functions take, so calls to effectful functions and to
  effect-parameter callbacks show up as needs, and the function's own `FX_REQUIRED` and
  `vo.effects` do not change.
- **Added to the data model:** `FX_CLAUSE_AT` (offset 120, `FX_SIZE` 128): where the first clause
  starts. The E0501/W0501 fixes that rewrite `uses` end there, so they never delete a clause.
- **Token names:** the parser's token-name table had `?` for `uses` and the three new keywords
  (`Expected \`{\`, found \`?\``); they now print their spelling. No existing golden output showed
  the `?`.
- **E0203 for a parameter named `ret`** is reported at the first `ensures` condition, with a note
  at the parameter (the existing shadowing report).

### SC-006: speed against the spec 004 binary

The first measurement was 7–20% slower on every `tests/speed` program, although the bytecode was
identical and `vm.o` was byte-identical. The cause is qemu-user: translation blocks end at 4 KiB
page boundaries, so a hot loop that straddles a page is split. Relinking the unchanged spec 004
objects with the VM moved by the same amount reproduced the slowdown (fib 179 → 209 ms), and
moves by whole pages did not. Fixes:

- the Makefile links the runtime first, in a fixed order (`RUNTIME`), so growth in the parser,
  checker and compiler can no longer move it;
- `src/vm.s` and `src/rt_map.s` start at their spec 004 page offsets (`.balign 4096` plus a
  fixed `.skip`), with `ponytail:` notes.

After the fixes, `python3 tests/speed.py --baseline build-004/vo`:

| Program | 004 ms | new ms | ratio |
|---|---|---|---|
| closures_generics | 370 | 375 | 1.013 |
| fib | 178 | 178 | 1.002 |
| int_loop | 401 | 399 | 0.993 |
| list_work | 283 | 285 | 1.008 |
| map_count | 457 | 463 | 1.012 |
| str_build | 594 | 598 | 1.006 |

## US4: evaluation (SC-001, SC-002)

`make prop-eval` (`tests/prop_eval.py`):

| Task | Mutants | Caught by visible cases | Caught by properties | Missed |
|---|---|---|---|---|
| h01-interval-merge | 6 | 0 | 6 | |
| h02-competition-ranking | 6 | 0 | 6 | |
| h03-rle-decode | 6 | 0 | 6 | |
| h04-bracket-errors | 6 | 0 | 6 | |
| h05-date-weekday | 7 | 0 | 7 | |
| h06-roman-numerals | 7 | 0 | 7 | |
| h07-word-wrap | 6 | 0 | 6 | |
| h08-bignum-add | 7 | 0 | 7 | |
| h09-window-max | 5 | 0 | 4 | 05: a correct O(n·k) rescan that times out on a hidden case |
| h10-matrix-spiral | 6 | 0 | 6 | |
| **Total** | **62** | **0** | **61 (98%)** | |

Every mutant passes all visible cases and fails at least one hidden case (the script checks this).
The properties pass on every reference, three times byte-identically (SC-002).

**How the material was written:** five agents, two tasks each, in this order: the properties from
`task.md` and the reference's signatures only, then frozen, then the mutants from the code. None
read `hidden/`; they saw only the script's pass/fail counts. No property was changed after its
mutants ran.

**Deviation: refactored references.** h01, h02, h07, h09 and h10 keep their logic (or part of it)
in `main`, which a property cannot call, and h07 and h09 have no other function. The task's
fallback ("test the smallest function that exists") would leave h07 and h09 with nothing to test.
Those tasks have `tests/prop_eval/hNN/ref/main.vo`: the same program with the logic in pure
functions and `main` only reading and printing. The script checks that it passes every visible
and hidden case, and the mutants are made from it. `bench2/` is unchanged. This matches how an
agent writing properties would structure its own solution.

**Findings about the references** (not changed; `bench2/` is frozen):
- h03 and h04 report columns in characters where the statements say bytes; h08 and h01 slice
  bytes and crash with R0007 on non-ASCII input; h06 treats a line of 19 or more digits as a
  number where the statement says `invalid`. The hidden cases are evidently ASCII and short, so
  the properties keep to that domain.

**Toolchain notes from the agents** (none in spec 005 code; follow-ups):
- Generated `int`s are mostly within ±100 (research R7's sizes), so `abs(n) % 3999` rarely
  reached 4xx numbers. The reference now says so.
- E0208's text report prints the fix under the `set` line although the fix (correctly, in JSON)
  targets the `let`.
- A failed import (E0205) is followed by cascading E0307s on lines that use the imported function.
- `vo fmt` removes a blank line between the imports and a following comment line.
- `prop_eval.py` first compared raw `vo test` output across runs; the `Nms` timings differ, so it
  now masks them.

## Reference size (SC-007)

`tests/ref_examples.py`: ~8,190 tokens before, ~9,144 after (+954, limit +2,000; budget 15,000).

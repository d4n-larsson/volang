# Data Model: Typed Holes, Property Tests and Contracts

These are additions to the layouts in `src/defs.inc` and specs 001–004's data models. Nothing existing changes size, except the FX record and the diagnostic record.

## Tokens (lex.s)

- Three keywords are appended at the end of the keyword range: `T_PROP` 44, `T_REQUIRES` 45 and `T_ENSURES` 46. `T_KW_LAST` becomes 46.
- Punctuation starts at 50, so there is room.
- The reference's keyword list gains the three.

## AST

**`N_TEST` with `NF_PROP` (flag 64): a property.** It has the same fields as a test:
- `N_A`: the name, a str;
- `N_B`: the body;
- `N_C`: `N_USES` or 0;
- `N_D`: the FX record;
- `N_E`: the local count.

Its parameters are in `FX_PARAMS`. They occupy slots 0..n−1, as a function's do.

**`N_FN` with clauses.** The clauses are vecs in the FX record:
- `FX_REQUIRES` and `FX_ENSURES`, each a vec of expression nodes in source order;
- 0 when the function has none.

**Hole calls** are ordinary `N_CALL` nodes with builtin id `B_TODO`. **`assume` calls** use `B_ASSUME`.

## FX record (`FX_SIZE` 80 → 120)

| Field | Offset | Content |
|---|---|---|
| `FX_PARAMS` | 80 | properties: vec of `N_PARAM` |
| `FX_REQUIRES` | 88 | vec of conditions, or 0 |
| `FX_ENSURES` | 96 | vec of conditions, or 0 |
| `FX_HOLES` | 104 | 1 when the body (or a clause) contains a hole; suppresses W0201/W0501 |
| `FX_RETSLOT` | 112 | slot of `ret` (functions with `ensures` and a return type), else −1 |

## Diagnostic record (`D_SIZE` 88 → 96)

| Field | Offset | Content |
|---|---|---|
| `D_SCOPE` | 88 | W0601 only: vec of `{name str, type}` pairs, innermost first, or 0 |

`D_EXPECTED` holds the hole's expected type, rendered the same way as for E0301.

## Builtins (check_expr.s table)

| Id | Name | Signature | Notes |
|---|---|---|---|
| 48 | `todo` | `(desc: str) -> <expected>` | Special typing (research R1–R3). Pure. |
| 49 | `assume` | `(cond: bool)` | Properties only (E0604). Pure. |

There are also internal ids that the compiler emits and source can never name:
- `rt_requires_fail(clause)`;
- `rt_ensures_fail(clause, exit_node)`.

They are reached through `CALL_BUILTIN` with ids beyond the name table.

## Runtime state (prop.s)

| Name | Content |
|---|---|
| `pg_state`, `pg_inc` | The generator's PCG state, separate from `random_int`'s |
| `pr_cases`, `pr_attempts`, `pr_shrinks` | Counters for the running property |
| `pr_args` | A list object holding the current argument values. It is a heap object, so the collector sees the arguments between runs through a root slot. |
| `pr_best` | A list object holding the smallest failing arguments so far |
| `pr_fail_kind` | The failure signature: the check node for an `expect` failure, or the code pointer for a runtime error |
| `pr_stopped` | 1 when the deadline cut shrinking short |

**Roots.** `pr_args` and `pr_best` live in `.data` words that the collector scans, under the existing GLOBALS root kind. The fault matrix's GLOBALS target gains a property case, so leaving them out is caught (FR-012 of spec 004 still applies).

## Generation and shrinking

These are functions, not data, but their contracts matter for the tasks:

- **`long prop_gen(void *type, long size, long depth)`** returns a new value of `type`. It allocates through the normal heap, so it may collect. Callers keep partial values reachable through `pr_args`.
- **`void *prop_shrink(long value, void *type, long k)`** returns the k-th shrink candidate of `value`, or 0 when there are no more.
  - Candidate order is research R8's table.
  - Candidates are new objects; the original is never mutated.
  - This matters because the body may have mutated the list it received. So `pr_best` holds a value regenerated or copied *before* the run: each run receives a deep copy made by `prop_copy(value, type)`.
- **`int prop_generatable(void *type)`** returns 0 when the type is ok. Otherwise it returns 1 (contains a function type) or 2 (no finite value). It is used by the checker for E0602.

## Report additions

### `test` record

The `test` record gains these members:
- **Properties only:**
  - `"prop": true`;
  - `"cases": N`, the accepted cases run;
  - `"discarded": D` (only when D > 0).
- **Failing properties only:**
  - `"shrinks": K`;
  - `"counterexample": [{"name": "xs", "value": "[0, 0, 0, 0]"}, ...]`;
  - `"shrink_stopped": true` (only when the deadline cut shrinking short).

### `summary` record

- **For `check`:** `"holes": N`, only when N > 0.
- **For `test`:** properties count in `passed`/`failed`/`errored`/`timed_out`, like tests.

### Diagnostic

`"scope": [{"name": "line", "type": "str"}, ...]`, on W0601 only.

The exact text and JSON forms are in [contracts/reports.md](contracts/reports.md).

## State transitions: one property run

```text
start → [attempt: reset, generate, run]
   pass ─→ cases+1 ─→ (cases = 100 → PASS) | (attempts = 1000 → FAIL "too many discarded") | next attempt
   discard ─→ attempts+1 ─→ same checks as above
   fail ─→ shrink loop ─→ (no candidate fails the same way | budget 2000 | deadline) ─→ final re-run ─→ FAIL/ERROR
   timeout / out of memory (not shrinking) ─→ TIMEOUT / ERROR, with the running input
```

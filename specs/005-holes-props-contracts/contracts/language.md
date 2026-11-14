# Contract: Language Additions

This is the user-visible language surface that spec 005 adds. The reference (`docs/reference.md`) is written from this contract. Golden cases assert it.

## Grammar

```text
item       := ... | prop
prop       := "prop" STRING "(" param { "," param } ")" uses? block
fn_decl    := "fn" NAME tparams? "(" params? ")" ( "->" type )? uses? clause* block
clause     := "requires" expr | "ensures" expr
```

- **New keywords:** `prop`, `requires`, `ensures`.
- **New builtins:** `todo` and `assume`. They are ordinary names in the builtin table, so they follow the same rules as `len`.
- **Restrictions on clauses:**
  - They are not allowed on anonymous functions or tests. There, the existing syntax error reports an unexpected token.
  - A condition ends at the next `requires`, `ensures` or the body's `{`.

## Holes: `todo(desc)`

| Rule | Behaviour |
|---|---|
| Argument | One `str` expression (E0301 otherwise). |
| Type | The type its context expects. Contexts that give a type:<br>- a `let`/`var` annotation;<br>- a `set` target;<br>- a function argument;<br>- `return` (the return type);<br>- a record field;<br>- a list element after the first;<br>- the right operand when the left one is known;<br>- a condition (`bool`);<br>- `print` and the other typed builtin parameters. |
| No type known | E0601. |
| As a statement | `todo("...");` ends the path: it satisfies E0403, and the next statements are W0401. |
| Check | W0601 per hole, with `expected` and `scope`. The check exit status is unchanged by holes. |
| Warnings | The enclosing function, lambda, test or property does not get W0201 or W0501. |
| Effects | Pure. The enclosing function keeps its declared effects. |
| Run | Reaching it stops with R0011 (exit 70), message ``Hole reached: <desc>.``, span = the `todo(...)` call, and the usual trace. In `vo test` the status is `error`. |

## Property tests: `prop`

| Rule | Behaviour |
|---|---|
| Parameters | One or more, each `name: type`. The type must be generatable (E0602). |
| Names | Unique among the file's tests and properties (E0202). `--filter` matches them like tests. |
| Body | Like a test body: `expect`, `expect_eq`, `assume`, `return;`. There is no `try` (tests cannot use `try`, E0406). |
| Effects | An optional `uses` clause, which is the grant for every case. It appears in `vo.effects` as `prop "name"`. |
| Cases | 100 accepted cases, with at most 1,000 attempts. Each case starts with seed and clock defaults (the `--seed`/`--clock-ms` values or 1 and 0). |
| Inputs | Deterministic for a given `--seed` (default 1), file and version. Arguments are fresh values: mutating them in the body does not affect later cases or shrinking. |
| Failure | A failing `expect`/`expect_eq`, or any runtime error other than R0009 and R0006. The failing input is then shrunk. |
| Timeout | `--timeout` applies to the whole property. |
| `vo run` | Ignores properties, as it ignores tests. |

## `assume(cond)`

| Rule | Behaviour |
|---|---|
| Where | Only in a property body. Elsewhere it is E0604. |
| False | The current input is discarded and does not count as a case. |
| Too many discards | Fewer than 100 accepted after 1,000 attempts fails the property with ``too many inputs were discarded by `assume` (N of 1000 accepted)``. |

## Contracts: `requires` and `ensures`

| Rule | Behaviour |
|---|---|
| Where | Named functions only, after `uses`, before `{`. Zero or more of each, in any order. |
| Type | Each condition is `bool` (E0301). |
| Scope | `requires`: the parameters. `ensures`: the parameters, plus `ret` (the returned value) when the function has a return type. A parameter named `ret` together with `ensures` is E0203. |
| Purity | No effects, including effect parameters (E0603). Clauses do not change the function's effects or its `vo.effects` line. |
| Generics | Conditions use what the type parameters allow (`ordered` for `<`). |
| `requires` check | On every call, in source order, before the body. False is R0012, spanning the call expression, with the trace starting at the caller. |
| `ensures` check | On every exit, in source order: `return`, `try` propagation, and the end of a function without a return type. False is R0013, spanning the `return` (or `try`, or the closing `}`), with the trace starting at the function. |
| Indirect calls | Calls through function values and builtins (`map`, `sort_by`, ...) are checked the same way. |
| Cost | A function without clauses compiles exactly as before. |

## Example (for the reference)

```vo
fn isqrt(n: int) -> int
    requires n >= 0
    ensures ret * ret <= n and (ret + 1) * (ret + 1) > n {
    var r = 0;
    while (r + 1) * (r + 1) <= n {
        set r = r + 1;
    }
    return r;
}

fn parse_header(line: str) -> list[str] {
    todo("split the header on commas");
}

prop "isqrt is the floor square root" (n: int) {
    assume(n >= 0 and n < 1000000);
    let r = isqrt(n);
    expect(r * r <= n);
}
```

`vo check` passes with one W0601 warning, for `parse_header`. `vo test` prints `PASS isqrt is the floor square root (100 cases, Nms)`.

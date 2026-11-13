# Data Model: vo-lang v2 Core

This extends `specs/001-vo-lang-core/data-model.md`. Layouts are in bytes; nodes stay 64 bytes, with the fields A–E as in v1.

## Effect set

An effect set is a bitmask of 8 effects plus optional scope lists:

| Bit | Effect | Scope kind |
|---|---|---|
| 0 | console (stdout, stderr) | — |
| 1 | input (stdin) | — |
| 2 | file.read | directories |
| 3 | file.write | directories |
| 4 | net | hosts |
| 5 | clock | — |
| 6 | random | — |
| 7 | env | variable names |

Layout: `mask:u64 | scopes:ptr`. `scopes` points to a vector of `{bit, str}` pairs. An effect whose bit is set and that has no pairs is unscoped (covers everything). Directory scopes are normalized when parsed: `./` is stripped, a trailing `/` is ensured, and `..` components, absolute paths and empty scopes are rejected with E0506.

Operations, all in `effects.s`:

| Operation | Result |
|---|---|
| `union(a, b)` | combined set |
| `covers(d, r)` | true when `d` covers `r` |
| `missing(d, r)` | the part of `r` that `d` does not cover |
| `unused(d, r)` | the part of `d` nothing needs |
| `render(set)` | canonical text, e.g. `console, file.write("out/")` |

Effect parameters (`E: effects`) appear in sets as **effect variables**: the set additionally holds a vector of type-parameter references, substituted at instantiation.

## Types (new kinds)

| Kind | Fields |
|---|---|
| `TY_FUNC` | `A` = parameter type vector, `B` = return type, `DECL` = effect set |
| `TY_PARAM` | `A` = index in the declaring item's parameter list, `B` = constraint (0 none, 1 ordered, 2 effects), `DECL` = the declaring item |
| `TY_RECORD`, `TY_VARIANT` | existing kinds, plus a type-argument vector in `B` (0 when not generic). Interning includes the arguments. |

Substituting a vector of type arguments into a type: `types.s:subst(type, params_owner, args)`. Unification for inference: `unify(param_type, arg_type, bindings)`. On a failure the caller reports E0301 or E0310.

## AST additions

| Node | Fields |
|---|---|
| `N_FN` | adds `flags.HAS_TPARAMS`. The node has no free field (`N_TYPE` holds the resolved return type), so the side record `FX` {`FX_TPARAMS` vec, `FX_USES` node, `FX_DECLARED` effect set, `FX_REQUIRED` effect set} hangs off field `B` of its name node (`N_NAME` uses only `A`) |
| `N_TPARAM` | `A` = name node, `B` = constraint kind |
| `N_USES` | `A` = vector of `N_EFFECT` |
| `N_EFFECT` | `A` = effect bit, `B` = vector of scope `N_STR` nodes, `C` = effect-variable name node (for `uses E`) |
| `N_LAMBDA` | `A` = params, `B` = return type node, `D` = body, `E` = local count; captures vector `{symbol, outer slot, inner slot}` in a side record; inferred effect set |
| `N_FN_TYPE` (type node) | `A` = parameter type nodes, `B` = return type node, `C` = `N_USES` |
| `N_CALL_VALUE` | `A` = callee expression, `B` = args. The checker marks it as a closure call or a record-field call, or rewrites it for the "no methods" diagnostic. |
| `N_TEST` | adds `C` = `N_USES` |

## Symbols

- Function symbols get the effect set of their declaration (`SY_EFFECTS`; `SYM_SIZE` grows to 80).
- Local symbols get `SY_DEPTH`, the lambda nesting level at definition. A use from a deeper lambda is a capture. For a `var`, that is E0505.

## Runtime values (new)

| Object | Layout after the header |
|---|---|
| closure | `CL_FN` fn:ptr (`N_FN` or `N_LAMBDA`) \| `CL_N` n:u32 \| `CL_TARGS` type_args:ptr (vector or 0) \| `CL_CAPS` captures: slot[n] |

A named function used as a value becomes a closure with `n = 0`.

Frames add `FR_TARGS` (a type-argument vector or 0) and `FR_EFFECTS` (the declared effect set of the called named function; a lambda's frame copies its caller's). The frame header grows from 24 to 40 bytes.

## Effects record entry

`path | name | kind (fn/test) | effect set`, parsed from and written to `vo.effects` (contracts/effects-record.md).

Comparing it with the program produces a list of capability changes: `{kind: added|removed|renamed, path, name, old_name?, effects, via: [call chain]}`.

## Benchmark v2 task

| Field | Meaning |
|---|---|
| `category` | `multi_file`, `refactor`, `hidden_property` or `capability` |
| `task.md` | the statement shown to the agent |
| `starter/` | optional existing tree |
| `visible/` | example cases shown to the agent |
| `hidden/` | grading cases, never shown |
| `allowed_effects` | the permitted effects (capability tasks only), in the `uses` syntax |
| `ref/` | a reference solution in each language, used to validate the hidden cases |

## Validation rules (from the requirements)

| Rule | Requirement |
|---|---|
| A function's required effects ⊆ its declared effects, including scopes | FR-003, FR-009 |
| A declared effect that nothing needs is a warning | FR-004 |
| Function value compatibility: effects(value) ⊆ effects(expected type); parameter and return types match exactly after substitution | FR-006 |
| No capture of `var` | FR-019 |
| Ordering comparisons on a type parameter need `ordered` | FR-025 |
| Record growth fails `check` | FR-013 |
| Scope widening counts as growth | FR-009a |

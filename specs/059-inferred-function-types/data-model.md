# Data Model: Anonymous Functions Take Their Types From Context

No run-time data changes: bytecode, the VM, recordings, `vo.effects` and `vo.packages` are unchanged. What
changes is where an anonymous function's types come from.

## Anonymous function (`N_LAMBDA`)

| Field | Today | After |
|---|---|---|
| parameters (B, `N_PARAM`) | name and a written type node (required) | name; a type node only in 0.4 code, reported by E0342 |
| parameter type (`p->type`) | resolved from the written type | taken from the expected function type before the parameters are declared |
| return type node (C) | optional `-> T` | only in 0.4 code, reported by E0342 |
| return type (`node->type` while checking) | written, or `void` | expected, or collected from the body when open (research R4) |
| body (D) | block | block, or a one-`return` block flagged `NF_ARROW` for `=> expr` |
| effects | from the body (`fx->direct`) | unchanged |

Rules: as today, parameter names are distinct, `_` is allowed for an unused parameter, and an anonymous
function has no type parameters, `uses` (E0507) or contracts.

## Expected function type

The function type required at the lambda's position: a call parameter (after substituting the bindings so
far), a typed `let`/`var`, `return`, `set` target, `new` field, list element, variant payload, `ok`/`err`
payload. **Open** when it mentions a type variable of the generic callee not yet bound
(`ty_mentions_owner`). An open parameter type is E0343; an open return type is inferred.

## Inference order in a call

1. Bind from the expected result type (as today).
2. Check and unify every non-lambda argument, in source order (function values included).
3. Check each lambda argument, left to right, against the substituted parameter type; unify its type back,
   fixing variables of its return type for the lambdas after it.
4. Variables still open: E0310, as today.

# Data Model: A Generic Function's Own Scope

## The mark on a call (checker → run time)

| Name | Where | Meaning |
|---|---|---|
| `NF_FX_PARAM` | `Node.flags` of `N_CALL` and `N_CALL_VALUE` | A call of a function value (also one handed to a builtin): the effects of its type are effect parameters of the function the call is written in, and nothing else. A generic function called by name: its effect parameters are bound to such parameters and nothing else |

Set in `src/check_effects.c` where a call's effect set is computed. Never set in a function without
effect parameters. Not printed by any command.

## Closure

| Field | Type | Meaning |
|---|---|---|
| `owner` (new, `CL_OWNER`) | integer | Serial of the activation of the named generic function in which the closure was created (through any depth of anonymous functions); 0 when it was not created under one |

`caps` moves by 8 bytes (`CL_CAPS`). The collector does not follow `owner`.

## Activation table (`src/rt_effects.c`)

| Field | Meaning |
|---|---|
| `frame` | address of a live frame |
| `fn` | its function node (checked on lookup) |
| `serial` | for a named function with effect parameters: a new number; for an anonymous function's frame: its closure's `owner` |

- Ordered by frame address. Entering a frame that registers first removes the entries at or above its
  address.
- 256 entries. When full, a frame is not registered and nothing is exempted on its account.
- Not a collector root (addresses and integers).

## The walk (replaces `path_scoped`)

State (`PathIt`): the frame the walk is at, the set last returned (to skip repeats), `carried`, up to 8
pending exempt serials.

`carried`: what is being checked runs as an effect parameter of the frame the walk is at.

For an effect performed in frame `X`, in order:

1. `X`'s own declaration, if it has scopes for the effect. `carried` starts set when `X`'s declaration has
   effect parameters and does not itself name the effect.
2. Then repeat with `Y = X`: let `P = Y.parent` (stop after the root), and judge the call that entered `Y`:
   - **a marked call of a function value** (`NF_FX_PARAM`, and not `P` calling `Y`'s function by name):
     `P` is skipped and `carried` is set. If `P` is an anonymous function's frame, the serial of the
     activation that created it becomes pending.
   - **a marked call by name while `carried`**: `P` is skipped and `carried` stays (`P` handed its own
     effect parameter on).
   - **`P`'s serial is pending**: `P` is skipped, the serial removed, `carried` set (what runs is `P`'s
     effect parameter).
   - **anything else**: `P`'s own declaration is returned if it scopes the effect; `carried` is cleared.
   - the root frame is never skipped.
3. The root grant, if the walk did not judge the root frame (as before).

A frame without a declaration (anonymous functions, standard-library functions) returns nothing at its
step and is otherwise walked the same way.

## What does not change

- `Frame` (64-byte header), `EffSet`, `rt_es_grant`'s result, the recording format, R0010's fields.
- `rt_denied_by(frame, ...)` receives the frame whose declaration refused, as today.

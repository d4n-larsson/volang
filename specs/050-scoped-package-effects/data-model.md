# Data Model: Scoped Effects Through Package Functions

## Effect set (unchanged structure)

| Field | Meaning |
|---|---|
| `mask` | the effects declared; an effect here with no pair in `scopes` is declared without scopes |
| `scopes` | `{effect, text}` pairs, sorted |
| `any` | effects needed "in some scope": never part of a declaration, only of a requirement |
| `evars` | effect parameters |

## Two readings of a set

| Reading | Where | An effect without scopes means |
|---|---|---|
| **Declaration** | a `uses` clause, `vo.effects`, a frame's grant | in the root (`main`, a test, a property): every target; elsewhere: no limit of its own, the callers' limits apply |
| **Requirement** | what a caller must cover (`es_requirement` of a callee's declaration, plus builtin uses) | the caller must declare the effect, with or without scopes |

`es_requirement(d)`: for `file.read`, `file.write`, `file.delete`, `net`, `env`, `serve`: in `mask`
with no pair → removed from `mask`, set in `any`. Everything else is copied.

## Covering at check time (`es_missing`, unchanged)

| Caller declares | Requirement | Result |
|---|---|---|
| nothing | `any` or scoped | E0501 |
| scoped | `any` | covered |
| scoped | scoped | covered when each required scope is under a declared one, else E0503 |
| unscoped | anything | covered |

## Call path at run time

```text
root frame (main / test / property)      effects: the grant
  └─ frame                               effects: its declaration
       └─ frame (anonymous, std)         effects: the caller's pointer        ← skipped: same set
            └─ frame (generic)           effects: declaration ∪ caller's
                 └─ operation(target)
```

A target is allowed when, for every frame on the path whose set scopes the operation's effect, the
target is inside those scopes (hosts and ports, names, or resolved paths). The first set that refuses,
from the operation outwards, is the one R0010 names.

## States

None. No frame field, bytecode, recording format or effects-record format changes.

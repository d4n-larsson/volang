# Data Model: Readable Signatures and Named Effect Sets (spec 030)

## Effect set (item)

| Field | Meaning |
|---|---|
| name | capitalized identifier, unique among the file's items |
| exported | `export effects ...` |
| effects | the written list: effects with optional scopes, and other set names |
| expanded | the plain effects (mask and scopes) after expanding every name, computed once |

Rules: no cycles (E0513); at least one effect (E0110); every written effect valid as in a `uses` clause
(E0506, E0509, E0512); used somewhere in the program, else W0505.

## Effect clause (`uses ...`, unchanged form)

Each entry: an effect (with scopes), an effect parameter, or a set name. Meaning: the union. In a type's
effect argument, scopes from sets are dropped.

## Header layout (formatter)

States of one header: one line (≤ 100) → clause on its own line → parameters one per line (→ clause on its
own line after `) -> Type`).

# Data Model: Module Imports

No run-time data changes: bytecode, the VM, recordings, `vo.effects` and `vo.packages` are unchanged. What
changes is the checker's view of names.

## Import (`N_IMPORT`)

| Field | Meaning |
|---|---|
| path segments | as today (`std`, `json`) |
| target file | as today (`d.i - 1`), resolved by R3 |
| form (new) | `module` (binds the last segment) or `item` (0.3 form, R3: binds the item, E0215) |

Rules: a module name is the last segment, a lowercase identifier; unique in its file among module names
and items (E0202).

## Qualified name (a name node with `NF_QUAL`)

| Field | Meaning |
|---|---|
| text (`a.s`) | `m.n` (or `m.Var` for a case's variant, `m.Set` for an effect set) |
| span | the whole `m.n` |
| module part | text before the first `.`; span `start` to `start + len(m)` |
| item part | text after it; span `start + len(m) + 1` to `end` |

Created only by the parser, only when `m` is a module name of the file (R1).

## Symbols

| Kind | Name | Points to |
|---|---|---|
| `SYM_MODULE` (new) | `m` | the imported file |
| item symbol (existing kinds) | `m.n` | the declaration in that file; private ones marked, for E0209 |
| item symbol, 0.3 form | `n` | as today, plus the import node for E0215 |

The shadowing check treats `SYM_MODULE` like an item: a local of that name is E0203.

## Migration region (for E0215's fix)

| Field | Meaning |
|---|---|
| start | first import of the file |
| end | end of the last use that changes (qualified, renamed by E0214, or a renamed local) |
| replacement | the region with imports regrouped, uses qualified, renames applied |

Built from the reference log (`q_note_sym`), one per file.

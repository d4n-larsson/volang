# Data model: Iteration (spec 046)

## The loop forms

| Form | Over | First variable | Second variable | Follows changes in the body |
|---|---|---|---|---|
| `for x in xs` (unchanged) | list | | element | yes: the list as it is at each step |
| `for i, x in xs` | list | index from 0 | element | no: one round per index the list had at the start, the element as it is at the visit; ends when the index is past the end |
| `for i, x in d` | deque | position from the front | element | no: the elements at the start |
| `for k, v in m` | map | key | value | keys: those at the start, in insertion order, minus those removed before their turn; values: as they are at the visit |
| `for k, v in m` | ordered_map | key | value | as a map, keys ascending |

`_` may stand for either variable. Two variables over a text, a range, an `ordered_set`, a
`priority_queue` or anything else is E0301.

## In the syntax tree

`N_FOR`: A first name (0 with `NF_DISCARD`), B the collection, C the second name when `NF_PAIR` is
set (0 for `_`), D the body, E the first variable's slot; the second variable's slot is E + 1.

## The warning

W0403 `for_reads_entry`: `for K in keys(M) { let V = M[K]; ...` (fix: `for K, V in M { ...`) and
`for I in 0..len(XS) { let X = XS[I]; ...` (fix: `for I, X in XS { ...`), with `M`/`XS` a `let` or a
parameter.

## `std.lists`

See research.md R5. All pure except for the effects of the key function; only `sort_by_desc` changes
its argument.

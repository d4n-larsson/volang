# Research: Iteration (spec 046)

## R1: The syntax tree

**Decision**: `N_FOR` keeps its five fields. A two-variable loop sets the new flag `NF_PAIR`; its
second name is in `C` (the range end of a range loop, which a two-variable loop never has), or 0 for
`_`; its two variables take consecutive slots, `E` and `E + 1`, whether named or not. A macro
`FOR_RANGE(st)` (`st->c.n && !(st->flags & NF_PAIR)`) replaces the eight tests of `st->c.n` in
`check.c`, `check_structure.c`, `fmt.c`, `compile.c` and `vm.c`.

**Rationale**: `Node` has no free field, and its size is asserted. `vo query`, `vo edit`, `vo lsp`
and `--mutate` do not walk statements by kind (no `N_FOR` in them): they work from the symbols the
checker records, so a second `new_local` is all they need.

**Alternatives considered**: a sixth field in every node (memory for every node of every program); a
new node kind `N_FOR2` (every `case N_FOR` duplicated).

## R2: Lists and deques: a count fixed at the start

**Decision**: a two-variable loop over a list is compiled as a range loop from 0 to the list's
length at the start (`OPC_FOR_RANGE`, the index in slot `E`), with the list kept in a temporary; the
body begins with `OPC_FOR_VALUE` (R3), which for a list puts the element at that index in slot
`E + 1` or, when the index is no longer in the list, jumps out of the loop. A deque is snapshotted
to a list first (`OPC_SNAPSHOT`), as its one-variable loop is, and then looped the same way.

**Rationale**: every two-variable loop then visits what was there when it began (maps: R3), and
`for i, x in xs` means what `for i in 0..len(xs) { let x = xs[i];` means, so the warning's fix (R4)
is safe for any body. The one difference: where the old form stops with R0003 because the body
shortened the list, the new one ends the loop. `for x in xs` is unchanged and follows the list.

**Alternatives considered**: following the list as `for x in xs` does (the first plan): the two
spellings then differ when the body pushes, no fix is safe, and both stay in use, against the
constitution's one form per construct.

## R3: Maps: the keys the loop began with, one new instruction

**Decision**: the loop takes the list of keys at its start (`OPC_SNAPSHOT`, extended to a `map`; an
`ordered_map` already gives its keys in order), runs the list loop over it with the key in slot `E`,
and begins the body with one new instruction, `OPC_FOR_VALUE`: slot `E + 1` = the value of that key,
or, when the key is no longer in the map, jump to the loop's back-edge. One hash lookup per entry.
The instruction looks at the object in its operand: a map or ordered map (by key, to the back-edge)
or a list (by index, out of the loop: R2).

That is the meaning of FR-002 exactly: the keys the map had at the start, in order; each visited if
the map has it at its turn (so a key removed and put back before its turn is visited, with the new
value); keys new to the map are not visited.

**Rationale**: the map's entries array is compacted when it grows (`map_grow`), so a position in it
does not survive an insertion in the body. Walking the array in place would need a sequence number
per entry or a way to pin the array for the life of the loop, including when the loop is left by
`return`, `try` or a runtime error. The key list costs 8 bytes per entry for the life of the loop,
which is what `for k in keys(m)` costs today, and the lookup is the one `m[k]` did.

`ponytail:` ceiling: 8 bytes per entry while the loop runs. Upgrade: sequence numbers in `MapEntry`
(the `live` word has room) and a search by sequence after a compaction.

**Spec amended**: FR-005 and SC-002 said "no list of keys"; they now say "no more time or memory than
the `keys` form".

## R4: Errors and the warning

- **A two-variable loop over something else**: the existing E0301 with the message "A `for` with two
  variables goes over a map (key, value), a list or a deque (index, element), found `T`." Over a
  text, a range, an `ordered_set` or a `priority_queue` it carries a fix that removes the first
  variable and the comma. No new error code.
- **Same name twice, shadowing, assignment to a loop variable, unused variable**: `new_local` and the
  existing checks, unchanged.
- **W0403 `for_reads_entry`** (new, in the control-flow group after W0402): on
  `for K in keys(M) {` whose first statement is `let V = M[K];` (no type annotation needed to match;
  an annotation is kept out of the fix by not matching it), where `M` is an identifier bound by `let`
  or a parameter. One fix: the text from `for` to the end of that `let` statement becomes
  `for K, V in M {`.
- **W0403 also covers the index loop**: `for I in 0..len(XS) {` whose first statement is
  `let X = XS[I];`, with `XS` an identifier bound by `let` or a parameter. Fix: `for I, X in XS {`.
  The warning's name is `for_reads_entry`, for both shapes.

## R5: List helpers: `std.lists`, in vo

**Decision**: a module `std/lists.vo`:

| Function | Result |
|---|---|
| `contains[T](xs, x) -> bool` | whether some element equals `x` |
| `index_of[T](xs, x) -> int` | the first index, or -1 (as `find` on text) |
| `reversed[T](xs) -> list[T]` | a new list |
| `sort_by_desc[T, C: ordered, E: effects](xs, key)` | in place, stable, largest key first |
| `min_by`, `max_by[T, C: ordered, E: effects](xs, key) -> result[T]` | the first element with the smallest or largest key; `err("empty list")` |
| `sum(xs: list[int]) -> int` | 0 for an empty list |
| `group_by[T, K: key, E: effects](xs, key) -> map[K, list[T]]` | keys in order of first appearance |

A prototype of all of them runs. `sort_by_desc` is the builtin `sort_by` between two reversals in
place (a stable ascending sort of the reversed list, reversed, is a stable descending sort). For that
the builtin must accept a key whose type is an `ordered` type parameter, which it refuses today
(E0313). The change is one condition in the checker (`check_func.c`, the `B_SORT_BY` case): the
runtime already resolves the key's type through the running frame's type arguments (`hof_sort_by`
calls `rt_concrete`). Tried: 200,000 ints sort descending in 26 ms, against 12 ms for the builtin
ascending and 204 ms for a merge sort written in vo. So `sort_by` in any generic function may now
sort by a `C: ordered` key; a key of an unconstrained type parameter stays E0313.

Names checked against builtins and the library: `contains`, `index_of`, `reversed`, `sum`, `min_by`,
`max_by`, `group_by` and `sort_by_desc` are free (`min`, `max`, `sort_by` are builtins with other
names; `std.decimal.total` sums decimals).

## R6: The reference budget

95 tokens are free. Changes: the loops row gains two forms; the sentence "Loop over a map with
`for k in keys(m)`" becomes the two-variable forms; the word-count example loops with
`for word, n in counts` (shorter than today's); one library row for `std.lists`. If the total passes
15,000, the `std.security` row is shortened (it lists more names than its neighbours).

## R7: Tools

- `vo fmt`: prints `for a, b in x`.
- Grammars: `for_statement` in `editors/tree-sitter-vo/grammar.js` takes an optional second
  variable; the parser is regenerated (`make grammars`); the TextMate and Vim grammars and the site's
  highlighter are token-based and are checked by `tests/grammars.py` and the site checks.
- `vm.c` `fl_stmt` (the locals in scope at a position, for holes): adds the second variable.
- `check_structure.c`: its five `N_FOR` cases use `FOR_RANGE`.

## R8: Tests

- Golden cases: `run/for_pair*` (the stories' scenarios and edge cases), `check/for_pair_*` (E0301
  with and without a fix, the existing errors on the two names), `check/w0403*` and its fix through
  `tests/apply_fixes.py`, `fmt/`, a rename, a coverage and a mutation case on a two-variable loop.
- `tests/iteration.py` (in `make test`): group `loops`, 1,000 generated programs whose expected
  output comes from a model of R2 and R3 in Python (maps, ordered maps, lists and deques; bodies that
  read, update, add, remove, push, pop, `break` and `continue`), SC-001; group `lists`, each helper
  against a Python loop on 10,000 random lists, SC-004; group `speed`, a million-entry map with both
  forms, time and peak memory, SC-002.
- The catalog (`docs/errors.md`, `tests/catalog.py`) gets W0403.
- Area `iteration` in `tools/quick.sh`.

## R9: Moving `std/` and `examples/`

The W0403 fixes are applied with `vo check --fix` (or the tool `tests/apply_fixes.py` uses), for
both patterns. Loops the warning does not match (the collection is a `var` or a field) are read one
by one and rewritten by hand where the body does not reassign the collection. Recordings' fingerprints cover the library source: the `record_replay` step cases
whose programs import a changed module get their fingerprint line updated.

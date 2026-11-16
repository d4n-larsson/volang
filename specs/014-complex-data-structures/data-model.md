# Data Model: Complex Data Structures

All four are mutable reference values on the collected heap (FR-007), like `list` and `map`. Each knows
its element type (for the built-in order, rendering and equality) or holds a comparison function.

## ordered_map[K, V]

| Part | Meaning |
|---|---|
| root | the AVL tree's root node, or none |
| size | number of entries |
| order | the key type (built-in order) or a comparison function `fn(K, K) -> int` |
| node | key, value, left, right, height (1 for a leaf) |

**Invariants** (checked by the property tests after every operation, SC-001):
- In-order traversal gives strictly increasing keys under `order`; no key twice.
- Every node's height is 1 + the larger child height; children's heights differ by at most 1, so the
  height is at most 1.44 log2(size + 2).
- `size` equals the number of nodes.

**Operations** (contracts/api.md): insert or replace, lookup, remove, neighbors (`first_key`,
`last_key`, `key_at_or_above`, `key_at_or_below`), `keys_between`, `keys`, `len`, `copy_map`.

## ordered_set[T]

The same tree with keys only. Union, intersection and difference build a new set from the in-order
sequences of both (linear merge), in the left set's order.

## priority_queue[T]

| Part | Meaning |
|---|---|
| items | array of (item, sequence) in heap order |
| next | the next sequence number (pushes so far) |
| order | the element type or a comparison function |

**Invariants:** each item is not greater than its children under (order, then sequence); ties leave in
push order (FR-003); `len` is the array length.

## deque[T]

| Part | Meaning |
|---|---|
| buffer | ring buffer, capacity a power of two |
| head, length | the front's index and the number of elements |

**Invariants:** elements are `buffer[(head + i) mod capacity]` for `i < length`; capacity doubles when
full.

## Ordering

| Type | Built-in order |
|---|---|
| `int`, `float`, `str` | as `<` today (floats by value; `str` by bytes) |
| record whose fields are all ordered | field by field in declaration order (FR-012) |
| anything else | none: E0313 naming the field where an order is needed; E0602 for generation |

A comparison function returns negative (less), zero (same key) or positive (greater). Its answers decide
the order of that collection; contradictory answers give a wrong order, never a crash (FR-013).

## Iteration and change

Iteration (`for x in c`, `keys(m)`) copies the elements first, so the loop sees the collection as it was
when it began. A change to a collection while one of its own operations runs (from inside a comparison
function) is R0007.

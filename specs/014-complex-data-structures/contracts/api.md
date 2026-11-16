# Contract: The Built-In Collections and `std.collections`

Four built-in generic types, written and created like `list` and `map` (clarified 2026-11-16: `new`,
lowercase): `ordered_map[K, V]`, `ordered_set[T]`, `priority_queue[T]`, `deque[T]`. The names are types
only in type positions and after `new`; elsewhere they stay ordinary identifiers, so existing programs
that use them as names keep working. `std.collections` (imported item by item, `vo doc
std.collections`) has the operations no builtin covers. All operations are pure (no effects).

## Types

| Type | Order | Iteration (`for x in c`) |
|---|---|---|
| `ordered_map[K, V]` | keys ascending | not iterable directly: `for k in keys(m)` (keys ascending), as for `map` |
| `ordered_set[T]` | ascending | ascending |
| `priority_queue[T]` | smallest first, ties in push order | in the order `pop` would give |
| `deque[T]` | insertion | front to back |

## Creating

| Form | Meaning |
|---|---|
| `new ordered_map[K, V]`, `new ordered_set[T]`, `new priority_queue[T]` | empty, ordered by the built-in order of `K`/`T`, which must be ordered (E0313 otherwise, naming the `_by` function) |
| `new deque[T]` | empty |
| `ordered_map_by(cmp)`, `ordered_set_by(cmp)`, `priority_queue_by(cmp)` (std.collections) | empty, ordered by `cmp: fn(T, T) -> int` (negative, zero, positive), any element type |

## Existing builtins on the collections

| Builtin | Map | Set | Queue | Deque |
|---|---|---|---|---|
| `len(c)` | entries | elements | items | elements |
| `has(c, k)`, `remove(c, k)` | key | element | | |
| `get_or(m, k, d)`, `keys(m)`, `m[k]` (R0004), `set m[k] = v;` | yes | | | |
| `push(c, x)` | | add | add | add at the back |
| `pop(c) -> result[T]` (`err("empty")`) | | | remove the smallest | remove the back |
| `c[i]` (R0003), `set c[i] = v;` | | | | by position from the front |
| `==`, `!=` | contents in order (the comparison function is not compared) | | | |

## Library functions (`std.collections`)

| Function | Signature | Result |
|---|---|---|
| `first_key`, `last_key` | `(m: ordered_map[K, V]) -> result[K]` | `err("empty")` |
| `key_at_or_above`, `key_at_or_below` | `(m: ordered_map[K, V], k: K) -> result[K]` | `err("none")` |
| `keys_between` | `(m: ordered_map[K, V], lo: K, hi: K) -> list[K]` | keys with `lo <= key <= hi`, ascending |
| `first`, `last` | `(s: ordered_set[T]) -> result[T]` | `err("empty")` |
| `at_or_above`, `at_or_below` | `(s: ordered_set[T], x: T) -> result[T]` | `err("none")` |
| `between` | `(s: ordered_set[T], lo: T, hi: T) -> list[T]` | ascending |
| `union`, `intersection`, `difference` | `(a: ordered_set[T], b: ordered_set[T]) -> ordered_set[T]` | a new set, ordered like `a` |
| `peek` | `(q: priority_queue[T]) -> result[T]` | `err("empty")` |
| `push_front` | `(d: deque[T], x: T)` | |
| `pop_front` | `(d: deque[T]) -> result[T]` | `err("empty")` |
| `front`, `back` | `(d: deque[T]) -> result[T]` | `err("empty")` |
| `copy_map`, `copy_set`, `copy_queue`, `copy_deque` | `(c) -> same type` | an independent collection with the same order |
| `ordered_map_by`, `ordered_set_by`, `priority_queue_by` | `(cmp: fn(T, T) -> int) -> collection` | see Creating |

Emptiness is `len(c) == 0`.

## Complexity (FR-006)

`O(log n)`: map and set insert, lookup, remove, neighbors; queue push and pop. `O(1)` amortized: deque
ends and position access. `O(n)`: iteration, `keys`, copies; set algebra `O(n log n)`; `keys_between`
and `between` are `O(log n + k)` for `k` results.

# Contract: Language Changes

The reference describes these in at most 300 more tokens (FR-014), plus one line pointing to
`vo doc std.collections`.

## Ordered records (FR-012)

- A record whose fields are all ordered (`int`, `float`, `str`, or an ordered record) is ordered: it
  satisfies `T: ordered` and works with `<`, `<=`, `>`, `>=`, `min`, `max`, `sort` and as a key or
  element of the ordered collections.
- Comparison is field by field in declaration order: the first field that differs decides.
- A generic record is ordered per instance (`Pair[int, str]` is, `Pair[int, list[int]]` is not).
- Where an order is needed and a record is not ordered: E0313, naming the first field without an order:
  "`Slot` is not ordered: its field `tags` (`list[str]`) has no order."
- Unchanged: `==` stays structural for every type; variants, lists, maps and functions have no order.

## Type-argument inference (FR-011)

- `new Node { ... }` of a generic record without written type arguments infers them from the expected
  type (an annotated `let`, a return type, a parameter, a field, a list element) and from the field
  values. Written arguments (`new Node[int] { ... }`) stay valid.
- A generic variant case without a value (`Tree.leaf`), or such a `new`, passed as an argument of a
  generic function takes its type from the parameter, once the other arguments have bound it:
  `insert(Tree.leaf, 5)` checks as `insert[int]`.
- When nothing determines an argument: E0310 as today ("Cannot infer `T` for `Tree.leaf`: give the
  variable a type."). E0312 remains for a wrong number of written arguments.

## Existing builtins (FR-001, contracts/api.md)

`len`, `has`, `remove`, `get_or`, `keys`, `push`, `pop`, indexing and `for` accept the new collection
types as listed in contracts/api.md. Their reference rows gain the new types in their signatures
(for example `len(x: str | list[T] | map[K, T] | collection)`), with one sentence naming the collections.

# Contract: the two-variable `for` and `std.lists`

## Syntax

```text
for_stmt = "for" binding [ "," binding ] "in" expr [ ".." expr ] block
binding  = NAME | "_"
```

Two bindings with `..` is E0301.

## Meaning

```vo
for k, v in m { BODY }
```

is, for a `map` or `ordered_map`, what this does, with `ks` a name the program cannot see:

```vo
let ks = keys(m);
for k in ks {
    if has(m, k) {
        let v = m[k];
        BODY
    }
}
```

(`continue` in BODY goes to the next key.) So a key removed and put back before its turn is visited,
with its new value.

For a list, `for i, x in xs { BODY }` is

```vo
for i in 0..len(xs) {
    if i >= len(xs) {
        break;
    }
    let x = xs[i];
    BODY
}
```

For a deque, the same over a list of the elements it has when the loop begins.

## Diagnostics

| Code | When | Fix |
|---|---|---|
| E0301 | two variables over a type other than map, ordered_map, list, deque | over text, a range, an ordered_set or a priority_queue: remove the first variable |
| W0403 (new) | `for K in keys(M) { let V = M[K];` with `M` a `let` or a parameter | `for K, V in M {` |
| W0403 | `for I in 0..len(XS) { let X = XS[I];` with `XS` a `let` or a parameter | `for I, X in XS {` |

## `std.lists`

```vo
export fn contains[T](xs: list[T], x: T) -> bool
export fn index_of[T](xs: list[T], x: T) -> int                                   # -1 when absent
export fn reversed[T](xs: list[T]) -> list[T]
export fn sort_by_desc[T, C: ordered, E: effects](xs: list[T], key: fn(T) -> C uses E) uses E
export fn min_by[T, C: ordered, E: effects](xs: list[T], key: fn(T) -> C uses E) -> result[T] uses E
export fn max_by[T, C: ordered, E: effects](xs: list[T], key: fn(T) -> C uses E) -> result[T] uses E
export fn sum(xs: list[int]) -> int
export fn group_by[T, K: key, E: effects](xs: list[T], key: fn(T) -> K uses E) -> map[K, list[T]] uses E
```

`min_by` and `max_by` of an empty list are `err("empty list")`. `sum` stops with R0001 on overflow.

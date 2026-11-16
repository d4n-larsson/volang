# Collections

`ordered_map`, `ordered_set`, `priority_queue` and `deque` are built-in collections for data kept in order, created with `new` like a map, with the operations no builtin covers in `std.collections`.

## When a list or a map is not enough

A [list](lists-and-maps.md) keeps elements in the order you add them, and a `map` keeps keys in
insertion order. Many programs need something else:

- keys kept **sorted**, with questions like "the first key at or after 9:00" (a timetable, a price
  ladder, version ranges);
- a **set** of distinct values in order, and the union or difference of two sets;
- the **smallest** item first, again and again, while new items arrive (a scheduler, a triage queue,
  Dijkstra's algorithm);
- cheap work at **both ends** of a sequence (a sliding window, a breadth-first search, an undo
  history).

Sorting a list after every change, or scanning a map for the smallest key, works but costs `O(n)` or
worse per operation. The four built-in collections do these in `O(log n)` or `O(1)`.

| Type | Holds | Order |
|---|---|---|
| `ordered_map[K, V]` | keys with values | keys ascending |
| `ordered_set[T]` | distinct elements | ascending |
| `priority_queue[T]` | items, duplicates allowed | `pop` gives the smallest; ties in push order |
| `deque[T]` | a sequence | insertion, cheap at both ends |

They are types of the language, written in lowercase like `list` and `map`, and need no import. They
are created empty with `new`, like a map: `new ordered_map[str, int]`, `new deque[int]`. Like lists and
maps, they are references: passing one to a function shares it.

The rules are in the [Types](../reference/types.md) and [Builtins](../reference/builtins.md) sections
of the reference. The library functions are listed in [std.collections](../std/collections.md).

## The builtins that work on them

The builtins you know from lists and maps work on the collections where they make sense:

| Builtin | `ordered_map` | `ordered_set` | `priority_queue` | `deque` |
|---|---|---|---|---|
| `len(c)` | entries | elements | items | elements |
| `has(c, k)`, `remove(c, k)` | key | element | | |
| `m[k]`, `set m[k] = v;`, `get_or`, `keys` | yes | | | |
| `push(c, x)` | | add | add | add at the back |
| `pop(c)` (a `result`, `err("empty")`) | | | remove the smallest | remove the back |
| `d[i]`, `set d[i] = v;` | | | | by position from the front |
| `for x in c` | (loop over `keys(m)`) | ascending | in `pop` order | front to back |
| `==`, `!=` | same contents in the same order | same | same | same |

Emptiness is `len(c) == 0`. Everything else is a function in `std.collections`, imported one item at
a time: `import std.collections.key_at_or_below;`. All of them are pure.

## `ordered_map[K, V]`: keys in sorted order

An `ordered_map` is a map whose keys are always sorted. `keys(m)` lists them ascending, and the library
finds a key's neighbors without a scan. The key type must be ordered: `int`, `float`, `str`, or a
record of ordered fields ([E0313](../errors/E0313.md) otherwise).

```vo
import std.collections.first_key;
import std.collections.key_at_or_below;
import std.collections.keys_between;

fn main() -> result[void] uses console {
    let on_call = new ordered_map[int, str];
    set on_call[900] = "ana";
    set on_call[1300] = "bo";
    set on_call[0] = "night desk";
    set on_call[1800] = "cy";
    print(join(map(keys(on_call), fn(k: int) -> str {
        return str(k);
    }), " "));
    let start = try key_at_or_below(on_call, 1415);
    print("14:15 goes to " + on_call[start]);
    print("first shift starts at " + str(try first_key(on_call)));
    for k in keys_between(on_call, 800, 1400) {
        print(str(k) + " " + on_call[k]);
    }
    return ok();
}
```

```output
0 900 1300 1800
14:15 goes to bo
first shift starts at 0
900 ana
1300 bo
```

| Function | Result |
|---|---|
| [`first_key(m)`](../std/collections.md#first_key), [`last_key(m)`](../std/collections.md#last_key) | the smallest or largest key; `err("empty")` |
| [`key_at_or_above(m, k)`](../std/collections.md#key_at_or_above), [`key_at_or_below(m, k)`](../std/collections.md#key_at_or_below) | the nearest key on that side, `k` itself included; `err("none")` |
| [`key_after(m, k)`](../std/collections.md#key_after), [`key_before(m, k)`](../std/collections.md#key_before) | the nearest key strictly above or below `k`; `err("none")` |
| [`keys_between(m, lo, hi)`](../std/collections.md#keys_between) | the keys from `lo` to `hi`, both included, ascending |
| [`copy_map(m)`](../std/collections.md#copy_map) | an independent map with the same entries and order |

`k` does not have to be a key of the map: `key_at_or_below(on_call, 1415)` asks for the last shift
that started at or before 14:15. A map is not iterable itself: `for k in m` is
[E0301](../errors/E0301.md), so loop over `keys(m)` as with a `map`. Reading a missing key is
[R0004](../errors/R0004.md), as for a `map`.

## `ordered_set[T]`: distinct values in order

An `ordered_set` holds each element once, ascending. `push` adds an element (adding one that is already
there changes nothing), `has` tests membership, `remove` deletes, and `for` visits the elements in
order.

```vo
import std.collections.after;
import std.collections.difference;
import std.collections.first;
import std.collections.union;

fn show(s: ordered_set[str]) -> str {
    let parts = new list[str];
    for x in s {
        push(parts, x);
    }
    return join(parts, ",");
}

fn main() -> result[void] uses console {
    let rota = new ordered_set[str];
    for name in ["dave", "ana", "cy", "bo"] {
        push(rota, name);
    }
    let paged = new ordered_set[str];
    for name in ["cy", "ana", "cy"] {
        push(paged, name);
    }
    print(show(paged) + " (" + str(len(paged)) + ")");
    print("never paged: " + show(difference(rota, paged)));
    print("first: " + try first(rota) + ", after bo: " + try after(rota, "bo"));
    let extra = new ordered_set[str];
    push(extra, "eve");
    print(show(union(rota, extra)) + " " + str(has(rota, "eve")));
    return ok();
}
```

```output
ana,cy (2)
never paged: bo,dave
first: ana, after bo: cy
ana,bo,cy,dave,eve false
```

| Function | Result |
|---|---|
| [`first(s)`](../std/collections.md#first), [`last(s)`](../std/collections.md#last) | the smallest or largest element; `err("empty")` |
| [`at_or_above(s, x)`](../std/collections.md#at_or_above), [`at_or_below(s, x)`](../std/collections.md#at_or_below) | the nearest element on that side, `x` itself included; `err("none")` |
| [`after(s, x)`](../std/collections.md#after), [`before(s, x)`](../std/collections.md#before) | the nearest element strictly above or below `x`; `err("none")` |
| [`between(s, lo, hi)`](../std/collections.md#between) | the elements from `lo` to `hi`, both included, ascending |
| [`union(a, b)`](../std/collections.md#union), [`intersection(a, b)`](../std/collections.md#intersection), [`difference(a, b)`](../std/collections.md#difference) | a new set, ordered like `a`; `a` and `b` are unchanged |
| [`copy_set(s)`](../std/collections.md#copy_set) | an independent set with the same elements and order |

## `priority_queue[T]`: smallest first

A `priority_queue` gives back its items smallest first, however they were pushed. `push` adds an item
(duplicates are kept), `pop` removes and returns the smallest as a `result`, and
[`peek(q)`](../std/collections.md#peek) returns it without removing it. Items that compare equal leave
in the order they were pushed, so a queue of equal priorities behaves like a plain first-in, first-out
queue.

A record of ordered fields is ordered field by field, in declaration order, so the record's
declaration is the priority rule. Here severity decides first, then the minute the ticket arrived:

```vo
import std.collections.peek;

record Ticket {
    severity: int;
    minute: int;
    title: str;
}

fn main() -> result[void] uses console {
    let q = new priority_queue[Ticket];
    push(q, new Ticket { severity: 2, minute: 5, title: "printer out of toner" });
    push(q, new Ticket { severity: 1, minute: 12, title: "VPN refuses logins" });
    push(q, new Ticket { severity: 1, minute: 7, title: "email down" });
    push(q, new Ticket { severity: 3, minute: 1, title: "new monitor" });
    print("next: " + (try peek(q)).title);
    while len(q) > 0 {
        let t = try pop(q);
        print(str(t.severity) + " " + str(t.minute) + " " + t.title);
    }
    return ok();
}
```

```output
next: email down
1 7 email down
1 12 VPN refuses logins
2 5 printer out of toner
3 1 new monitor
```

`for t in q` visits the items in the order `pop` would give them, without removing any.
[`copy_queue(q)`](../std/collections.md#copy_queue) makes an independent copy. For the largest first,
give the queue a comparison function (see [Other orders](#other-orders-the-_by-functions)).

## `deque[T]`: both ends

A `deque` (double-ended queue) is a sequence that is cheap to change at both ends. `push` adds at the
back and `pop` removes from the back, like a list; the library adds the front. `d[i]` reads by
position from the front ([R0003](../errors/R0003.md) outside the deque), and `for` goes front to back.

| Function | Result |
|---|---|
| [`push_front(d, x)`](../std/collections.md#push_front) | adds `x` at the front |
| [`pop_front(d)`](../std/collections.md#pop_front) | removes and returns the front element; `err("empty")` |
| [`front(d)`](../std/collections.md#front), [`back(d)`](../std/collections.md#back) | the element at that end, not removed; `err("empty")` |
| [`copy_deque(d)`](../std/collections.md#copy_deque) | an independent deque with the same elements |

Keeping only the last three items is `push` at the back and `pop_front` when it is full:

```vo
import std.collections.pop_front;
import std.collections.front;

fn main() -> result[void] uses console {
    let recent = new deque[str];
    for page in ["home", "docs", "guide", "errors", "std"] {
        push(recent, page);
        if len(recent) > 3 {
            let _ = try pop_front(recent);
        }
    }
    let parts = new list[str];
    for p in recent {
        push(parts, p);
    }
    print(join(parts, " > ") + " (oldest " + try front(recent) + ", newest " + recent[len(recent) - 1] + ")");
    return ok();
}
```

```output
guide > errors > std (oldest guide, newest std)
```

A breadth-first search uses a deque as its queue: `push` new work at the back, `pop_front` the next.

## Other orders: the `_by` functions

`new ordered_map[K, V]`, `new ordered_set[T]` and `new priority_queue[T]` use the built-in order of the
key or element type. For any other order, or for a type that has none, create the collection with a
comparison function:

| Function | Creates |
|---|---|
| [`ordered_map_by(cmp)`](../std/collections.md#ordered_map_by) | an empty `ordered_map[K, V]` ordered by `cmp: fn(K, K) -> int` |
| [`ordered_set_by(cmp)`](../std/collections.md#ordered_set_by) | an empty `ordered_set[T]` ordered by `cmp: fn(T, T) -> int` |
| [`priority_queue_by(cmp)`](../std/collections.md#priority_queue_by) | an empty `priority_queue[T]`; `pop` gives first what `cmp` puts first |

`cmp(a, b)` returns a negative number when `a` comes before `b`, zero when they are the same key, and a
positive number when `a` comes after `b`. Two elements for which `cmp` returns zero are the same
element of a set or the same key of a map, and a tie in a queue.

Write the comparison with `<` and `>`, not with subtraction. `a - b` looks right but overflows for
values far apart, and the overflow is a runtime error ([R0001](../errors/R0001.md)), not a wrong
order:

```vo
import std.collections.ordered_set_by;

fn by_value(a: int, b: int) -> int {
    return a - b;
}

fn main() uses console {
    let s = ordered_set_by(by_value);
    push(s, 9223372036854775807);
    push(s, -2);
    print(str(len(s)));
}
```

```error R0001
```

A safe comparison returns -1, 0 or 1. This queue gives the largest amount first, and the earliest
order among equal amounts (the ties keep push order):

```vo
import std.collections.priority_queue_by;

record Order {
    id: str;
    amount: int;
}

fn larger_first(a: Order, b: Order) -> int {
    if a.amount > b.amount {
        return -1;
    }
    if a.amount < b.amount {
        return 1;
    }
    return 0;
}

fn main() -> result[void] uses console {
    let q = priority_queue_by(larger_first);
    push(q, new Order { id: "a", amount: 30 });
    push(q, new Order { id: "b", amount: 9223372036854775807 });
    push(q, new Order { id: "c", amount: 30 });
    push(q, new Order { id: "d", amount: -5 });
    let ids = new list[str];
    while len(q) > 0 {
        push(ids, (try pop(q)).id);
    }
    print(join(ids, " "));
    return ok();
}
```

```output
b a c d
```

The element type comes from `cmp`, so `priority_queue_by(larger_first)` and `ordered_set_by(cmp)` need
no annotation. An `ordered_map_by(cmp)` cannot tell the value type from `cmp`, so annotate the
variable, `let m: ordered_map[str, int] = ordered_map_by(by_length);`, or the checker reports
[E0310](../errors/E0310.md).

The comparison function is part of the collection: `union`, `intersection` and `difference` order
their result like their first argument, and the `copy_` functions keep it. `==` compares contents in
order, not comparison functions. A comparison function must be pure, since its type is
`fn(T, T) -> int` without `uses`.

## Records as keys and elements

A record of ordered fields can be a key of an `ordered_map`, an element of an `ordered_set` or an item
of a `priority_queue` without a comparison function. It is ordered field by field, in declaration
order: the first field that differs decides. A record with a field that has no order (a list, a map, a
variant, a `bool`) is [E0313](../errors/E0313.md), and the message names that field:

```vo
record Task {
    title: str;
    done: bool;
}

fn main() uses console {
    let tasks = new ordered_set[Task];
    print(str(len(tasks)));
}
```

```error E0313
```

Use `ordered_set_by` with a comparison on the fields that matter, or leave the unordered field out of
the record you use as a key.

Records are references, so the collection holds the same record you pushed. **Changing a record while
it is a key or an element breaks the order**: the collection does not know the record changed, so
lookups and loops can go wrong. Remove it, change it, and add it again.

```vo
record Point {
    x: int;
    y: int;
}

fn show(s: ordered_set[Point]) -> str {
    let parts = new list[str];
    for p in s {
        push(parts, str(p.x) + "," + str(p.y));
    }
    return join(parts, " ");
}

fn main() uses console {
    let seen = new ordered_set[Point];
    let a = new Point { x: 1, y: 5 };
    push(seen, a);
    push(seen, new Point { x: 2, y: 0 });
    push(seen, new Point { x: 1, y: 2 });
    print(show(seen));
    set a.x = 9;
    print(show(seen));
    remove(seen, a);
    push(seen, a);
    print(show(seen));
}
```

```output
1,2 1,5 2,0
1,2 9,5 2,0
1,2 2,0 9,5
```

The second line is out of order: `a` moved without the set knowing. Here `remove` still found it; in a
larger set it may not. Change keys only while they are out of the collection.

## Loops copy the collection

`for x in c` over an `ordered_set`, a `priority_queue` or a `deque` first copies the whole collection,
then loops over the copy. The loop body can change the collection freely (push, pop, remove) without
affecting the loop, and the copy is made in full even if the loop stops early with `break`.

```vo
import std.collections.copy_set;

fn main() uses console {
    let s = new ordered_set[int];
    push(s, 1);
    push(s, 2);
    for x in s {
        push(s, x * 10);
        remove(s, x);
    }
    let parts = new list[str];
    for x in copy_set(s) {
        push(parts, str(x));
    }
    print(join(parts, " "));
}
```

```output
10 20
```

For a large collection where you only need a few elements, walk it without a copy, one neighbor at a
time, with `after` (sets) or `key_after` (maps):

```vo
import std.collections.first;
import std.collections.after;

fn main() uses console {
    let s = new ordered_set[int];
    for x in [40, 10, 30, 20] {
        push(s, x);
    }
    var cur = first(s);
    var shown = 0;
    while shown < 2 {
        match cur {
            case ok(x) {
                print(str(x));
                set cur = after(s, x);
                set shown = shown + 1;
            }
            case err(_) {
                break;
            }
        }
    }
}
```

```output
10
20
```

## Complexity

| Operation | Cost |
|---|---|
| map and set: insert, lookup (`m[k]`, `has`), `remove`, neighbors (`key_after`, `at_or_above`, ...) | `O(log n)` |
| queue: `push`, `pop` | `O(log n)` |
| deque: both ends, `d[i]` | `O(1)` amortized |
| `keys_between`, `between` with `k` results | `O(log n + k)` |
| `for` (the copy), `keys`, the `copy_` functions | `O(n)` |
| `union`, `intersection`, `difference` | `O(n log n)` |

## Choosing a collection

| You need | Use |
|---|---|
| items in the order you add them, by index | `list[T]` |
| a lookup by `int` or `str` key, in insertion order | `map[K, V]` |
| keys sorted, the nearest key, a range of keys, any ordered key type | `ordered_map[K, V]` |
| distinct values, membership, sorted order, set algebra | `ordered_set[T]` |
| the smallest (or, with a comparison, the most urgent) item next | `priority_queue[T]` |
| adding and removing at both ends | `deque[T]` |

A `map` is the simplest choice when you only look keys up; its keys must be `int` or `str`. An
`ordered_map` also takes `float` and record keys, and any type with `ordered_map_by`. A sorted `list`
is fine for data that is built once and then only read.

## Error codes

| Code | Meaning |
|---|---|
| [E0301](../errors/E0301.md) | `for k in m` over an `ordered_map`: loop over `keys(m)` |
| [E0310](../errors/E0310.md) | the type of `ordered_map_by(cmp)` cannot be inferred: annotate it |
| [E0313](../errors/E0313.md) | a key or element type without an order in `new`: use the `_by` function |
| [R0001](../errors/R0001.md) | integer overflow, for example in `a - b` inside a comparison |
| [R0003](../errors/R0003.md) | `d[i]` outside a deque |
| [R0004](../errors/R0004.md) | `m[k]` for a key the map does not have |

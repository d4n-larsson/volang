# Lists and maps

`list[T]` is a growable sequence and `map[K, V]` a table from keys to values in insertion order; both are references, changed in place by builtins such as `push` and `set m[k] = v;`.

## Two collections for most programs

Most programs need two shapes of data: things in order, and things looked up by a key. vo has one
built-in type for each, `list[T]` and `map[K, V]`, and a small set of builtins that work on them. There
are no methods (`len(xs)`, never `xs.len()`), no iterators and no slices that share storage: a
`slice` is a new list.

When you need data kept sorted, a set, a priority queue or a double-ended queue, use the built-in
[collections](collections.md), which are created and used the same way.

The rules are in the [Lists and maps](../reference/lists-and-maps.md) and
[Builtins](../reference/builtins.md) sections of the reference.

## Lists

A list literal is `[a, b, c]`: at least one element, all of one type. An empty list has no element to
take a type from, so it is written `new list[int]`; `[]` is [E0306](../errors/E0306.md).

```vo
fn main() uses console {
    let primes = [2, 3, 5, 7];
    let names = new list[str];
    push(names, "ana");
    push(names, "bo");
    print(str(len(primes)) + " " + str(primes[0]) + " " + str(primes[len(primes) - 1]));
    set primes[0] = 1;
    print(join(names, ",") + " " + str(primes[0]));
    match pop(names) {
        case ok(last) {
            print("popped " + last + ", " + str(len(names)) + " left");
        }
        case err(e) {
            print(e);
        }
    }
    let grid = [[1, 2], [3, 4]];
    print(str(grid[1][0]));
}
```

```output
4 2 7
ana,bo 1
popped bo, 1 left
3
```

| Operation | Meaning |
|---|---|
| `xs[i]` | the element at index `i`, from 0; outside `0..len(xs)` is [R0003](../errors/R0003.md) |
| `set xs[i] = v;` | replaces an existing element (it never grows the list) |
| `push(xs, v)` | appends `v` |
| `pop(xs)` | removes the last element and returns it as a `result` (`err("empty")`) |
| `len(xs)` | the number of elements |
| `slice(xs, a, b)` | a new list with elements `a` to `b - 1`; needs `0 <= a <= b <= len(xs)` |
| `sort(xs)` | sorts in place, ascending, stable |
| `sort_by(xs, key)` | sorts in place by `key(x)`, stable |

There is no negative indexing: `xs[-1]` is [R0003](../errors/R0003.md), and the last element is `xs[len(xs) - 1]`. There is
no insert-in-the-middle or remove-at-index builtin; build a new list with `slice` and `push`, or use a
[`deque`](collections.md) when you need both ends.

```vo
fn main() uses console {
    let xs = [10, 20, 30];
    print(str(xs[3]));
}
```

```error R0003
```

## Sorting

`sort(xs)` sorts a list of an ordered type in place: `int`, `float`, `str` (by bytes, so `"B"` comes
before `"a"`), or a record whose fields are all ordered, compared field by field (see
[Generics](generics.md#ordered-type-parameters)). `sort_by(xs, key)` sorts by a value computed from
each element; the key must be ordered too ([E0313](../errors/E0313.md) otherwise). Both are stable:
elements that compare equal keep their order, so sorting by a second key and then by a first key sorts
by both.

```vo
record Person {
    name: str;
    team: str;
    age: int;
}

fn main() uses console {
    let people = [
        new Person { name: "cy", team: "ops", age: 30 },
        new Person { name: "al", team: "dev", age: 41 },
        new Person { name: "bo", team: "ops", age: 25 }
    ];
    sort_by(people, fn(p: Person) -> int {
        return p.age;
    });
    sort_by(people, fn(p: Person) -> str {
        return p.team;
    });
    for p in people {
        print(p.team + " " + p.name + " " + str(p.age));
    }
    let words = ["pear", "Fig", "apple"];
    sort(words);
    print(join(words, ","));
}
```

```output
dev al 41
ops bo 25
ops cy 30
Fig,apple,pear
```

## `map`, `filter`, `fold`, `any`, `all`

Five builtins take a function and apply it to every element. None of them changes its input list.

| Builtin | Result |
|---|---|
| `map(xs, f)` | a new list of `f(x)` for each `x` |
| `filter(xs, f)` | a new list of the `x` for which `f(x)` is `true` |
| `fold(xs, init, f)` | `f(...f(f(init, x0), x1)..., xn)`: one value built from all elements |
| `any(xs, f)` | `true` if `f(x)` holds for some `x`; stops at the first that does |
| `all(xs, f)` | `true` if `f(x)` holds for every `x`; stops at the first that does not |

```vo
fn main() uses console {
    let prices = [120, 45, 300, 80];
    let cheap = filter(prices, fn(p: int) -> bool {
        return p < 100;
    });
    let labels = map(cheap, fn(p: int) -> str {
        return "$" + str(p);
    });
    let total = fold(prices, 0, fn(acc: int, p: int) -> int {
        return acc + p;
    });
    print(join(labels, " ") + " total " + str(total));
    print(str(any(prices, fn(p: int) -> bool {
        return p > 250;
    })) + " " + str(all(prices, fn(p: int) -> bool {
        return p > 50;
    })));
}
```

```output
$45 $80 total 545
true false
```

The function can be a named function (`map(xs, double)`) or an anonymous one. Builtins are not
values, so `map(words, len)` does not work: wrap it, `fn(s: str) -> int { return len(s); }`. If the
function has effects, the caller needs them: `map` with a function that prints needs `uses console`
(see [Functions](functions.md)).

## Lists are references

A list value is a reference to the list. Assigning it, passing it to a function or storing it in
another collection shares the same list; nothing is copied. `let` only stops the name from being bound
to another list: the contents can still change.

```vo
fn add_default(xs: list[str]) {
    push(xs, "default");
}

fn main() uses console {
    let tags = ["a"];
    add_default(tags);
    let alias = tags;
    set alias[0] = "z";
    print(join(tags, ","));
    let copy = slice(tags, 0, len(tags));
    push(copy, "extra");
    print(str(len(tags)) + " " + str(len(copy)));
    print(str(tags == ["z", "default"]));
}
```

```output
z,default
2 3
true
```

To keep a list safe from later changes, copy it with `slice(xs, 0, len(xs))`. The copy is shallow: if
the elements are lists, maps or records, the copy points at the same ones.

`==` compares lists by content, element by element, not by identity.

## Maps

A map is created empty with `new map[K, V]`; there is no map literal. Keys are `int`,
`bool`, `str`, `bytes`, or records and variants made of those ([E0309](../errors/E0309.md) for a
`float`, a list, a map, a function or a `result`, alone or inside a record); values can be any type,
including lists and other maps.

```vo
fn main() uses console {
    let counts = new map[str, int];
    for w in split("b a b c b", " ") {
        set counts[w] = get_or(counts, w, 0) + 1;
    }
    print(str(counts["b"]) + " " + str(len(counts)));
    print(str(has(counts, "c")) + " " + str(get_or(counts, "z", 0)) + " " + str(has(counts, "z")));
    remove(counts, "c");
    remove(counts, "not there");
    for k, n in counts {
        print(k + "=" + str(n));
    }
}
```

```output
3 3
true 0 false
b=3
a=1
```

| Operation | Meaning |
|---|---|
| `set m[k] = v;` | inserts `k`, or updates it if it is already there |
| `m[k]` | the value for `k`; a missing key is [R0004](../errors/R0004.md) |
| `has(m, k)` | whether `k` is a key |
| `get_or(m, k, d)` | the value for `k`, or `d` if it is missing; never adds `k` |
| `remove(m, k)` | deletes `k`; nothing happens if it is missing |
| `keys(m)` | a new list of the keys, in insertion order |
| `len(m)` | the number of entries |

Reading a key that is not there stops the program. Check with `has` first, or use `get_or`:

```vo
fn main() uses console {
    let ages = new map[str, int];
    set ages["ana"] = 31;
    print(str(ages["bo"]));
}
```

```error R0004
```

### Insertion order

`keys(m)` lists the keys in the order they were first inserted. Updating an existing key keeps its
place; removing a key and inserting it again moves it to the end. Output that loops over `keys(m)` is
therefore the same on every run.

```vo
fn main() uses console {
    let m = new map[int, str];
    set m[3] = "c";
    set m[1] = "a";
    set m[2] = "b";
    set m[3] = "C";
    remove(m, 1);
    set m[1] = "A";
    let parts = new list[str];
    for k, v in m {
        push(parts, str(k) + v);
    }
    print(join(parts, " "));
}
```

```output
3C 2b 1A
```

For keys in sorted order, sort the keys (`let ks = keys(m); sort(ks);`) or use an
[`ordered_map`](collections.md). Two maps are `==` when they have the same entries, whatever order they
were inserted in.

### Maps of lists

A map's values are references too, so a list stored in a map can be changed through `m[k]`:

```vo
fn main() uses console {
    let groups = new map[str, list[int]];
    for n in [1, 2, 3, 4, 5] {
        var key = "odd";
        if n % 2 == 0 {
            set key = "even";
        }
        if not has(groups, key) {
            set groups[key] = new list[int];
        }
        push(groups[key], n);
    }
    for k, group in groups {
        print(k + ": " + str(len(group)));
    }
}
```

```output
odd: 3
even: 2
```

Maps are references in the same way as lists: a map passed to a function is the caller's map. There is
no builtin to copy a `map`; build a new one from `keys(m)`.

### Keys made of several values

A key that combines several values is a record. Two keys are the same entry when `==` says they are
equal, so a fresh record with the same fields finds the entry:

```vo
record Visit {
    user: str;
    day: int;
}

fn main() uses console {
    let pages = new map[Visit, int];
    for user in ["ada", "bo", "ada"] {
        let v = new Visit { user: user, day: 3 };
        set pages[v] = get_or(pages, v, 0) + 1;
    }
    print(str(len(pages)) + " " + str(pages[new Visit { user: "ada", day: 3 }]));
}
```

```output
2 2
```

Building the key as text instead, `user + "," + str(day)`, formats and allocates on every lookup,
loses the key's type (`keys(m)` gives text to parse back) and merges two keys when a field contains the
separator. Do not change a record while it is a key: the map keeps it under its old value, so remove it,
change it and add it again.

A generic function that keys a map by its type parameter declares it `K: key`:
`fn count[K: key](xs: list[K]) -> map[K, int]`.

## Iteration

`for x in xs` visits the elements of a list in order. With two variables a `for` gives the position
too: `for i, x in xs` is each index with its element, and `for k, v in m` each key of a map with its
value, in the map's order. Write `_` for the one you do not need (`for _, v in m`).

A loop with two variables visits what was there when it began:

- over a list, it runs once for each index the list had, with the element that is there at its turn.
  Elements pushed in the body are not visited, and a list that has become shorter ends the loop.
  (`for x in xs`, with one variable, follows the list instead: it also visits what the body pushes.)
- over a map, it goes through the keys the map had and visits each one the map still has at its
  turn, with the value it has then. A key removed in the body is skipped; a key added in the body is
  not visited. Changing values while looping is fine.

The loops this replaces, `for k in keys(m)` followed by `let v = m[k];` and `for i in 0..len(xs)`
followed by `let x = xs[i];`, still work; [W0403](../errors/W0403.md) points them to the shorter form
and its fix rewrites them. A range is still the way to count without a list.

```vo
fn main() uses console {
    let names = ["ana", "bo", "cy"];
    for i, name in names {
        print(str(i) + ": " + name);
    }
    let scores = new map[str, int];
    set scores["ana"] = 3;
    set scores["bo"] = 5;
    var best = "";
    for name, score in scores {
        if best == "" or score > scores[best] {
            set best = name;
        }
    }
    print(best);
}
```

```output
0: ana
1: bo
2: cy
bo
```

Changing a collection while looping over it needs care:

- `keys(m)` is a new list made when the loop starts. Removing a key during the loop does not stop the
  loop from reaching it, so `m[k]` on a removed key is [R0004](../errors/R0004.md): check with `has(m, k)`.
- A `for` loop over a list reads the list as it goes: an element pushed during the loop is visited
  too, and `set xs[i] = v;` on a later element is seen. To loop over the list as it was, loop over a
  copy: `for x in slice(xs, 0, len(xs))`.

## Error codes

| Code | Meaning |
|---|---|
| [E0306](../errors/E0306.md) | `[]` has no element type: write `new list[T]` |
| [E0309](../errors/E0309.md) | a map key type contains a `float`, a collection, a function or a `result` |
| [E0313](../errors/E0313.md) | `sort` or a `sort_by` key on a type without an order |
| [R0003](../errors/R0003.md) | an index or `slice` bound outside the list |
| [R0004](../errors/R0004.md) | `m[k]` for a missing key |

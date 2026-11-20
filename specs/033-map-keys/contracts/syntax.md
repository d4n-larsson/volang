# Contract: Syntax and Behavior (spec 033)

## Key types

```text
record Point {
    x: int;
    y: int;
}

variant Status {
    open;
    closed(str);
}

fn main() uses console {
    let seen = new map[Point, int];
    set seen[new Point { x: 1, y: 2 }] = 1;
    set seen[new Point { x: 1, y: 2 }] = get_or(seen, new Point { x: 1, y: 2 }, 0) + 1;
    print(str(len(seen)) + " " + str(seen[new Point { x: 1, y: 2 }]));   # 1 2

    let by_status = new map[Status, int];
    set by_status[Status.open] = 3;
    set by_status[Status.closed("late")] = 1;
    print(str(has(by_status, Status.closed("late"))));                     # true
}
```

Every operation on maps accepts every key type: `m[k]`, `set m[k] = v;`, `has`, `get_or`, `remove`,
`keys` (insertion order, so `for k in keys(m)`), `len`. Two keys are the same entry when `==`
says so.

## Generic code

```text
fn count[K: key](xs: list[K]) -> map[K, int] {
    let m = new map[K, int];
    for x in xs {
        set m[x] = get_or(m, x, 0) + 1;
    }
    return m;
}

record Index[K: key, V] {
    by: map[K, V];
}
```

`key` is a constraint word like `ordered`, `json` and `effects`: one per parameter. `ordered` does not
make a parameter a key.

## Unchanged

- `ordered_map`, `ordered_set`, `priority_queue`: `ordered` keys.
- JSON: `map[str, V]` only (E0328 otherwise).
- `vo fmt` prints `K: key` as written; `vo doc` shows it in signatures.

# Contract: additions to `std.time` (spec 065)

Everything here is exported from `std/time.vo` and written `time.NAME` after `import std.time;`.
Nothing existing changes. No item has an effect. The parameters are `start` and `at`, not `now`: `std.time` has a private function `now`, and vo has no shadowing. Field meanings and the error text:
[data-model.md](../data-model.md).

## Items

```vo
export record Expiring[T] {
    expires_at: int;
    value_at: fn(int) -> result[T];
}

export fn expiring[T](value: T, expires_at: int) -> Expiring[T]
export fn expiring_for[T](value: T, start: int, ms: int) -> Expiring[T]
export fn left_ms[T](e: Expiring[T], at: int) -> int
```

| Item | Behavior |
|---|---|
| `expiring(value, expires_at)` | An `Expiring` whose `value_at(now)` is `ok(value)` when `now < expires_at`, else `err("expired at <expires_at>, read at <now>")`. |
| `expiring_for(value, start, ms)` | `expiring(value, start + ms)`. `ms` of 0 or less: expired from the start. `start + ms` outside the int range: R0001. |
| `left_ms(e, at)` | `e.expires_at - at` when `at < e.expires_at`, else 0. |
| `e.value_at(now)` | The read. The only way to the value. |
| `e.expires_at` | The expiry moment. |

## Examples in the documentation comments (executed by `tests/doc_conformance.py`)

```text
expiring("t", 1000).value_at(999) == ok("t") is true
expiring("t", 1000).value_at(1000) == err("expired at 1000, read at 1000") is true
expiring_for("t", 1000, 500).expires_at is 1500
left_ms(expiring("t", 1000), 400) is 600
```

## What `vo check` says about misuse (existing codes, no new ones)

| Program text | Code |
|---|---|
| an `Expiring[str]` where a `str` is expected | E0301 |
| `e.value_at(now);` as a statement | E0404, fix: insert `try ` |
| `e == e`, or `==` on a record that holds one | E0308 |
| `str(e)`, `format("{}", e)` | E0301 |
| `json.to_json(e)`, or a `T: json` record that holds one | E0328 |
| returned from the function given to `concurrent_map` | E0314 (confirmed by `tests/cases/time/expiring_misuse`) |

## Reference

The `std.time` row of the library table in `docs/reference.md` gains, at its end (7 words; the reference was 2 tokens under its budget, so four sentences near the table lost 7 words):

```text
; `expiring(value, until)`: read only by `.value_at(now)`
```

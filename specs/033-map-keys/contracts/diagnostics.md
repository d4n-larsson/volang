# Contract: Diagnostics (spec 033)

## E0309 invalid_map_key (reworded; Fixable: sometimes)

At the key type of `map[K, V]`:

| Written | Message |
|---|---|
| `map[float, int]` | `` `float` cannot be a map key: key by `str(x)` or a scaled `int`. `` |
| `map[list[int], int]` | `` `list[int]` cannot be a map key: it is a list, which can change. `` |
| `map[map[str, int], int]` | `` `map[str, int]` cannot be a map key: it is a map, which can change. `` |
| `map[Order, int]`, `Order.items: list[str]` | `` `Order` cannot be a map key: `items` is a list, which can change. `` |
| `map[Reading, int]`, `Reading.value: float` | `` `Reading` cannot be a map key: `value` is `float`. `` |
| `map[result[int], int]` | `` `result[int]` cannot be a map key: it is a result. `` |
| `map[fn(int) -> int, int]` | `` `fn(int) -> int` cannot be a map key: it is a function. `` |
| `map[K, V]`, `K` bare | `` `K` cannot be a map key: declare it as `K: key`. `` + fix appending `: key` to `K`'s declaration |
| `map[K, V]`, `K: ordered` | `` `K` cannot be a map key: it is declared `ordered`, not `key`. `` (no fix) |

At a call or instance whose `K: key` parameter receives a type that is not a key, at the argument or type
argument: the same message for that type.

Span: the key type node; at a call, the call, and at an instance, the type argument (as E0313 and
E0328 do).

## Unchanged

- E0328 for `to_json`/`T: json` on a map whose key is not `str`.
- E0311/E0313 for ordered collections.
- R0004 prints the missing key in vo syntax (`Key Point { x: 1, y: 2 } is not in the map.`).

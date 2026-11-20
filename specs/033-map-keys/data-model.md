# Data Model: Structural Map Keys

## Key type (checker)

| Type | Key? |
|---|---|
| `int`, `bool`, `str`, `bytes` | yes |
| record | when every field's type is a key (generic fields with the instance's arguments) |
| variant | when every case's value type is a key; cases without a value always are |
| type parameter | when declared `K: key` |
| `float`, `void`, `result[...]`, `list`, `map`, `ordered_map`, `ordered_set`, `priority_queue`, `deque`, function | no |

Recursive records and variants: each type is looked at once (R2).

## Type parameter constraint

`TPC_KEY` (4), beside `TPC_ORDERED` (1), `TPC_EFFECTS` (2), `TPC_JSON` (3). One constraint per
parameter. Written, formatted and documented as `K: key`.

## Map entry (runtime, unchanged layout)

| Field | Meaning |
|---|---|
| `key` | the key: an `int`/`bool` word or a pointer to a `str`, `bytes`, record or variant |
| `val` | the value |
| `hash` | 32 bits of `key_hash(key, key type)`; stored once at insert, reused when the map grows |
| `live` | 0 after `remove` |

`key_hash`: `int`, `bool`, `str` as today's `map_hash`; everything else FNV-1a over the byte stream of
R4. Equal keys (`==`) have equal hashes.

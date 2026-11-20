# Research: Structural Map Keys

## R1. What a key is

**Decision**: `int`, `bool`, `str`, `bytes`; a record whose fields are all keys; a variant whose cases'
values are all keys; a generic record or variant when its type arguments make its members keys; a type
parameter declared `K: key`. Nothing else: not `float`, `void`, `result`, lists, maps, the other
collections, or functions. A recursive record or variant is a key when its other members are (each type
is looked at once).

**Rationale**: these are exactly the types whose `==` is an equivalence relation that does not change
while nothing is `set` (records are the documented exception, R7). `float` breaks reflexivity (NaN) and
`==` treats `-0.0` and `0.0` as one value with different bits. `result` is left out to keep the rule
short; nothing needs it, and a program's own variant replaces it.

**Alternatives considered**: everything with `==` (lists included): a list changes through `push` with no
`set` on the key, a hazard much easier to hit than a record's. Records only, no variants: variants are
the natural key for a state or a kind (`Status.closed`), and are immutable.

## R2. One function decides it

**Decision**: `key_why(t, path)` in `src/check_generic.c`, written like `json_why` (spec 029): 0 when `t`
is a key, otherwise the reason for the first part that is not, with its path from the top
(`` `items` is a list ``). A seen-list ends recursive types; generic members are substituted with the
instance's arguments, as `json_why` does.

**Rationale**: the same shape already gives E0328 its precise message; one function serves the type
check and the call check.

**Alternatives considered**: a boolean like `ty_ordered`: the diagnostic could not say which field is to
blame.

## R3. Diagnostics: E0309 everywhere

**Decision**: E0309 `invalid_map_key` keeps its code and name. Message: "`$1` cannot be a map key: $2."
with `$2` from R2 and, when the whole type is `float`, "key by `str(x)` or a scaled `int`". Reported:

- at the key type of `map[K, V]` wherever it is written (`resolve_type`), including record fields of
  generic records;
- at a call or instance whose `K: key` parameter receives a non-key (`check_targ`, beside the `ordered`
  and `json` branches).

A bare type parameter (`K`, no constraint) gets the fix that appends `: key` to its declaration. A
parameter with another constraint gets no fix (one constraint per parameter, spec assumption); the
message says what it needs. The catalog entry becomes "Fixable: sometimes", with a `float` example.

**Rationale**: one code for "not a key", as the spec assumes; E0311 and E0313 are about order and would
mislead. The code is not redefined: its meaning stays "this type is not a map key", only the set of
key types grows (as spec 021 reworded it for `bytes`).

## R4. Hashing

**Decision**: `key_hash(key, type)` in `src/rt_map.c`. `int`, `bool` and `str` take today's `map_hash`
path unchanged (same instructions, same values). Every other key folds FNV-1a over a fixed byte stream:

- `int`, `bool`: the 8 bytes, low first (shifts, not memory order: the same on every platform);
- `str`, `bytes`: the length's 8 bytes, then the bytes (so field boundaries cannot shift);
- record: each field in declaration order;
- variant: the tag's 8 bytes, then the case's value if it has one;
- a type parameter: its argument in the running frame (`rt_concrete`), as `values_equal` does.

The result keeps 32 bits as today. It depends only on the value, so equal keys hash equally (R5) and the
hash never changes iteration order (insertion order).

**Rationale**: one hash, the existing algorithm, no allocation, deterministic across platforms (FR-009 of
spec 008).

**Alternatives considered**: hashing the rendered text: allocates on every lookup and is slower than the
hand-built key it replaces.

## R5. Equality

**Decision**: `map_find` compares keys with today's code for `int`, `bool` (integer compare) and `str`,
and with `values_equal` for every other key type. The comparison is only reached after the stored 32-bit
hash matched.

**Rationale**: `values_equal` is `==`; the spec requires the two to agree (FR-003). It already handles
generic members, depth (R0005) and the same-object shortcut.

`map_find` is also called outside running vo code: JSON (`str` keys only, today's path), task decoding
and property generation (concrete key types, so `rt_concrete` needs no frame). `values_equal`'s timeout
poll and stack check are safe there, as they are for `==` on the same values in those paths; the
`tasks` and `prop` cases (T016) confirm it.

## R6. Generic code

**Decision**: inside a function generic in `K: key`, the map's key type is a type parameter at run time;
`key_hash` and `values_equal` resolve it through the running frame's type arguments (`rt_concrete`), the
mechanism generic `==` already uses. The `int`/`str` fast path checks the kind before resolving, so
non-generic code pays nothing.

## R7. A record changed while it is a key

**Decision**: not detected. The entry keeps the hash of the old value: lookups with either value may
miss, `remove` of a missing key does nothing, and `map_grow` reuses the stored hash. No memory is read
out of bounds in any of these paths (`map_remove` only walks the probe sequence after `map_find` found the
entry). Documented in the reference with the `ordered_map` sentence, which becomes one sentence for both.

**Alternatives considered**: copying the key on insert: every insert allocates, and `keys(m)` would
return copies, not the values put in. Freezing records: a new language concept.

## R8. Reports, tasks, properties, JSON

**Decision**: no code expected. Verified by reading: `render_value` (test failures, traces, R0004 through
`rt_key_error`, `vo query`), `enc_value`/`dec_value` (tasks), `prop_gen`/shrink (properties) and
`rt_test.c`'s map diff all recurse with the key type and call `map_set`/`map_find` with it. JSON's check
(`json_why`) already refuses a map whose key is not `str`, so `map[Point, V]` stays E0328. Each gets a
golden case (R12); code changes only if a case fails.

## R9. What stays

**Decision**: `ordered_map`, `ordered_set` and `priority_queue` keep the `ordered` rule; `std.collections`
has no function taking a plain `map`, so nothing there changes. Recordings carry no maps. Tuples are not
added.

## R10. Performance

**Decision**: `tests/map_keys_bench.py OLD NEW` (timing-sensitive, not part of `make test`, like
`structures_bench.py`). OLD is today's binary saved as `build/vo-before` before the runtime changes.

- SC-002: 1,000,000 inserts, lookups and removes with `int` keys and with `str` keys: NEW within 5% of OLD,
  paired medians of five.
- SC-003: the same with a two-`int`-field record key against a `str` key built as
  `str(x) + "," + str(y)`, both on NEW: the record at most as slow.

## R11. Documentation

**Decision**:

- Reference: the `map` row ("`K` is a key: `int`, `bool`, `str`, `bytes`, records and variants of those
  (E0309)"); `K: key` in the type-parameter paragraph beside `ordered` and `json`; the maps paragraph's
  "(keys are `int` or `str`)" removed; the hazard sentence covers maps; one runnable example with a
  record key. Budget +≤ 150 tokens.
- Catalog: E0309 rewritten (R3).
- Guide: `site/docs/guide/lists-and-maps.md` gains a short "Keys made of several values" section with a
  runnable example.
- Roadmap: a row and section for 033; the "Tuples, more map keys" row records keys done and tuples
  declined (records cover them).
- `docs/review-2026-11/README.md` "Fixed since the review" and the README feature list.

**FR-016 / SC-004**: a search of `examples/` and `std/` finds no map key built by joining values into a
`str` (`examples/expenses` keys `totals` by one caller-chosen label). Nothing to convert; the search is
recorded in results. The benchmark tasks under `bench2/` are fixtures and are not edited.

## R12. Tests

**Decision**: `tests/cases/map_keys/`:

- `record`, `variant`, `nested`, `bytes_bool`, `generic_record` (`Option[int]`), `recursive`: each keys a
  map, reads it with fresh equal values, prints `keys(m)`, `has`, `get_or`, `remove`, `len`, `for k in keys(m)`;
- `separator`: two keys a hand-built `str` would merge stay distinct;
- `mutated`: a record changed while a key; output documented, no crash;
- `bad_float`, `bad_list`, `bad_field` (a record with a `float` field: the path), `bad_result`,
  `bad_func`: E0309;
- `generic`, `generic_bare` (fix `: key`), `generic_ordered` (no fix), `generic_call` (a call infers
  `float`), `generic_record_decl` (`record Index[K: key, V]`);
- `report` (a failing `expect_eq` of two record-keyed maps), `missing` (R0004 prints the key), `prop`,
  `tasks`, `json` (E0328 unchanged);
- `fmt` case for `K: key`; `vo doc` of a function with `K: key`.

`tests/cases/check/map_key` moves to a `float` key (a `bool` key is valid now) and
`tests/cases/bytes/type_errors` loses its `map[bytes, ...]` line's E0309: the two changed expectations.

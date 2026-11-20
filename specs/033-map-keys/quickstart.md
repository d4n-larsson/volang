# Quickstart: validating spec 033

Prerequisite: `make`. Details: [contracts/](contracts/).

1. **Record and variant keys.** The example in [contracts/syntax.md](contracts/syntax.md) prints `1 2`
   and `true` (`tests/cases/map_keys/record`, `variant`, `nested`, `separator`).
2. **Not keys.** `map[float, int]`, a record with a `float` field, a list, a result and a function are
   E0309 naming the part (`tests/cases/map_keys/bad_*`).
3. **Generic.** `count[K: key]` runs with `int`, `str` and a record; a bare `K` is E0309 with the `: key`
   fix; a call inferring `float` is E0309 at the call (`tests/cases/map_keys/generic*`).
4. **Tools.** A failing `expect_eq` of record-keyed maps and R0004 show keys in vo syntax; `prop` and
   `concurrent_map` handle them; `to_json` of `map[Point, int]` is E0328 (`tests/cases/map_keys/report`,
   `missing`, `prop`, `tasks`, `json`).
5. **Speed.** `python3 tests/map_keys_bench.py build/vo-before build/vo`: `int` and `str` keys within 5%;
   the record key no slower than the joined `str` key.
6. **Unchanged.** `make test`; `python3 tests/ref_examples.py` reports the reference under 15,000 tokens.

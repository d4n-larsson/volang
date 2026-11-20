# Results: Structural Map Keys

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `make test` passes (37 steps: golden and step cases, scripts, examples with their replays, `diff_engines.py` 1000 programs); the only changed expectations are listed under Deviations |
| SC-002 | met | `tests/map_keys_bench.py build/vo-before build/vo`, 1,000,000 inserts, lookups and removes, paired ratio over nine rounds: `int` keys 1.029 and 0.985, `str` keys 0.983 and 1.014 (two runs); limit 1.05 |
| SC-003 | met | same script: a two-`int`-field record key 1.003 s and 0.974 s against a `str` key joined from the two fields 1.093 s and 1.050 s (record at 0.92 and 0.93 of the joined key) |
| SC-004 | met | a search of `examples/` and `std/` for map keys joined into a `str` finds none (`examples/expenses` keys `totals` by one caller-chosen label); nothing converted; `bench2/` fixtures not edited |
| SC-005 | met | E0309 at check time naming the part, at the type (`map_keys/bad_*`, `check/map_key`), at a call or instance (`generic_call`), with the `: key` fix for a bare parameter (`generic_bare`; `apply_fixes.py` 118/118) |
| SC-006 | met | reference ~14,622 tokens of 15,000 (+77), 40 examples run; guide 212 examples run |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed, 37 steps, 7m50s |
| ARM64, RISC-V (qemu) | `map_keys`, `check`, `bytes`, `prop`, `tasks`, `fmt/map_keys` cases 187/187 and the 5 `map_keys` step cases on each |
| `catalog.py`, `apply_fixes.py`, `ref_examples.py`, `site_examples.py`, `doc_conformance.py` | passed |
| `make gate` | not run |

## Deviations

- **Key checks wait for the item pass**: record fields are resolved one item at a time, so a map type
  written before its key record (`fn f(m: map[Point, int])` above `record Point`) would see no fields.
  E0309 checks made while items' types are resolved run once every item is resolved
  (`key_checks_begin`/`key_checks_end`); inside function bodies they run at once.
- **Structural lookups out of line**: the first version shared the lookup loop between `int`/`str` and
  structural keys and measured `str` keys 4% to 8% slower (paired). Structural keys now take their own
  out-of-line loop and `int`/`str` keys run the old loop unchanged.
- **The benchmark compares in pairs**: separate medians swung by up to 17% between runs on this machine;
  the script takes the median of per-round ratios with the order alternating.
- **E0309 at a call spans the call**, as E0313 and E0328 do (contracts/diagnostics.md updated).
- **`apply_fixes.py` gained `map_keys`** in its list of case folders, so the new fix is applied and
  re-checked.
- **Changed expectations**: `check/map_key` (now a `float` key: `bool` keys are valid) and
  `bytes/type_errors` (its `map[bytes, int]` is valid: one error fewer).

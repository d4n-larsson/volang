# Results: Constants

Status: implemented and merged without the gate and without the full suite (stopped at the user's
request after the golden cases); see Verification for what ran.

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | expenses, helpdesk, notes-api and release-check declare each directory once (`data_dir`, `out_dir`, `report_file`); `tests/const_records.py` changes each to `"./"` in a recorded copy and gets E0504 for the functions it reaches |
| SC-002 | partly verified | all 970 golden cases and 229 stress cases pass; the changed expectations are listed under Deviations. The scripts after `apply_fixes.py` in `make test` (including `gc/baseline.py`) were not run on the final tree |
| SC-003 | met | `tests/const_records.py`: for the four examples, `vo effects`, `vo check` and `vo test` are identical with the constants and with their values written out |
| SC-004 | met | `err_all.vo` reports E0212, E0213, E0333 twice, E0332, E0207, W0202, E0122 and E0208 in one pass; `apply_fixes.py` fix coverage unchanged at 92% of fixable codes |
| SC-005 | met | `tests/const_equiv.py --count 200`: 0 differences between constants and literals (94 programs check clean, the rest with the same diagnostic codes) |
| SC-006 | not measured | a use compiles to the instruction a literal compiles to (no run-time cost by construction); the check-time comparison of `tests/perf.sh` was not run |
| SC-007 | met | `ref_examples.py`: 39 examples, ~14,255 tokens of 15,000 (+238) |
| SC-008 | deferred | needs isolated agent runs scheduled with the user |

## Verification

| Check | Result |
|---|---|
| Golden cases (x86-64) | 970 passed, 0 failed |
| TLS and math self-tests | passed |
| Stress build (`run`, `effects`, `func`, `generic`, `prop`) | 229 passed |
| `fuzz.py --iterations 200 --seed 1` (with the new `const` noise) | passed |
| `apply_fixes.py` | 92% of fixable codes, unchanged |
| `catalog.py`, `ref_examples.py`, `site_examples.py`, `doc_conformance.py` | passed (run on their own during the work) |
| Remaining `make test` scripts, `tests/perf.sh`, `structure_speed.py` | not run on the final tree |
| `make ARCH=aarch64`, `make ARCH=riscv64` | not run |
| `make gate` | not run (the last gate passed on the commit before this spec) |

## Deviations

- **E0122, not E0121**: E0121 was already `invalid_allow`.
- **Cycles** are reported at the use that closes them ("a -> b -> a"), as effect-set cycles (E0513) are,
  not at the first constant with related notes.
- **Formatting**: consecutive constants are grouped without blank lines (as imports and effect sets); a
  clause with a constant scope is printed as written, not sorted, since its value is not known to `vo fmt`.
- **A scope that names something that is not a constant** gets E0212 (or E0201 when the name is not
  defined), not a reworded E0506; an invalid value is E0506 naming the value.
- **W0202's fix** removes the declaration only, as W0505 does, not the comment above it.
- **Constants in `replace_all` replacements** are not checked (they still work at run time).
- **No run-time R0010 step case** for a constant scope: the run-time check sees the value, as for a literal.
- **E0207's conversion** now gives `max_items` for `MAX_ITEMS` and `parse_json` for `parseJSON` (it gave
  `m_a_x_i_t_e_m_s`), for every name.
- **Changed expectations**: E0315, E0321, E0324 and E0330 say "a string literal or a constant"; the
  `export` error lists `const`; `effects/env_denied` reads the variable name from a function, because
  `"PA" + "TH"` is now a constant expression checked by `vo check` (E0503).

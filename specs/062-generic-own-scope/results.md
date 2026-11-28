# Results: A Generic Function's Own Scope

Measured on 2026-11-27 on x86-64. Baseline: `ab41b22` (vo 0.7.0 plus the bug sweep fixes), kept as a copy
outside the repository.

## Measured before

The cases of this spec on the baseline (`|` separates output lines):

| Case | Baseline | Now |
|---|---|---|
| `generic_own_unscoped_caller` | `inside -`, `secret -` | `inside -`, R0010 `g` |
| `generic_own_scoped_caller` | `inside -`, `other -` | `inside -`, R0010 `g` |
| `generic_own_lambda_handed_on` (P3) | `inside -`, `other -` | `inside -`, R0010 `g` |
| `generic_own_same_as_plain` | `g` reads `b/`; `h` is R0010 | both R0010 |
| `generic_own_builtin` | third line `other` | third line R0010 `each_own` |
| `generic_own_env` | `one two`, `two -` | `one two`, R0010 `ge` |
| `generic_own_recursive` | second line `other inside+inside+-` | second line R0010 `deep` |
| `generic_own_report` | `other other` | R0010 `g`, `allowed` `["a/"]`, `declared_by` `g` |
| `generic_own_caller_scope` | R0010 naming `g` with `main`'s scopes in the text | R0010 naming `main` |
| `generic_own_p1`, `p2`, `p4`, `p5`, `p6` (first two lines), `record_of_functions`, `no_scopes` | pass | unchanged |
| `generic_scope_l1`, `l2` | R0010 `h_scoped` | unchanged |
| `generic_scope_l3` | `- other` (the leak of research L3, there before any rule) | R0010 `g3` |

## Cases

- 20 step cases under `tests/cases/effect_sets/` (`generic_own_*`), 3 under `tests/cases/security/`
  (`generic_scope_l1` to `l3`), `tests/cases/replay/generic_own_scope`,
  `tests/cases/serve_concurrent/generic_own_scope`.
- Each rule is needed by a case. With rule 3 (the exempt activation) switched off: `p4`, `p5` and
  `recursive` refuse. With rule 2 (transparent calls) off as well: `p1`, `p2`, `builtin` and
  `record_of_functions` refuse too.
- The first form of rule 2 refused `generic_own_p6` (a callback handed on between two generic functions
  that both have scopes); research R2 has the correction.
- `tests/scope_calls.py`: 300 programs with seed 1 and 2,000 each with seeds 2, 3 and 4, 1,000 with seed
  9: 0 mismatches. On the baseline binary: 65 of 300.
- `tests/security/nested_race.py` with a generic helper: 0 reads outside the outer scope in 300 runs.

## Suite

- `make test`: 1,347 golden cases, 369 step cases, 62 scripts, 0 failed.
- `make lint`: clean on three architectures. `make security-check`: all 8 scripts pass.
- The stress build at its usual interval over `effect_sets`, `generic`, `lambda_types`, `func` and
  `security`: 107 passed.
- Not run: `make gate` (the cross builds under qemu, the sanitizer build, the RISC-V speed limit).

## Speed

`tests/speed.py` against the baseline, three runs: `closures_generics` 0.99 to 1.05, `fib` 0.98 to 1.01,
`int_loop` 0.97 to 1.00, `list_work` 0.88 to 0.97, `map_count` 1.02, `str_build` 1.02 to 1.05. Within the
run-to-run spread on this machine.

## Open points

- Research R7a, closed afterwards (case `generic_own_library`): a standard-library function performing an
  effect directly under its effect parameter is no longer bound by a generic caller's own scopes. Left:
  an anonymous function inside such a library function that performs the effect itself.
- No case for `process` and `net` scopes, nor inside `std.workers`.
- Found on the way, outside this spec (docs/bug-sweep-2026-11-27.md, findings 39 to 48): with the stress
  build collecting at every allocation, ten uses of collected objects were found and fixed. None came
  from this spec or from the bug sweep.

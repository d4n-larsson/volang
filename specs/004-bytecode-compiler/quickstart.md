# Quickstart: validating vo-lang bytecode execution

Every command runs from the repository root on the development host (x86-64 with `qemu-riscv64`). Contracts: [contracts/equivalence.md](contracts/equivalence.md). Layouts: [data-model.md](data-model.md).

## 0. Build both engines

```sh
make                # bin/vo: the bytecode engine
make oracle         # build-oracle/vo: commit 1fff558, the tree walker (built once)
make STRESS=1       # build-stress/vo: the collector stress build (spec 003)
```

Expected: three binaries. `build-oracle/vo run` and `bin/vo run` print the same output for any program in `tests/cases/run/`.

## 1. Nothing visible changed (US2, SC-003)

```sh
make test                           # golden cases, repeats, fuzz, catalog, ref_examples, fmt_corpus,
                                    # migrate, record_changes, scope_fuzz, gc_bounds, baseline, timeout_check
python3 tests/gc/baseline.py        # 703/703 bench2 reference runs unchanged
```

Expected: everything passes. The only changed golden expectations are listed in `results.md` as intended changes:
- the memory figures in `tests/cases/cli/mem_stats_*`;
- the in-use figure in the R0006 cases (`run/oom`, `gc/edge/grow_forever`, `gc/edge/tiny_limit`, `gc/edge/oom_in_test`), if it moved;
- `run/vm_recursion_200k`, which fails on the old engine.

## 2. It is faster (US1, SC-001, SC-002)

```sh
make speed          # python3 tests/speed.py: oracle vs bin/vo vs python3, medians of 5
```

Expected: a table of six programs with the oracle time, the new time, the speedup and the ratio to Python, followed by:
- the geometric mean, at least 4.0×;
- every program at least 2.0×;
- `int_loop` (10 million iterations) at most 1.35 s.

## 3. The engines agree on generated programs (US4, SC-005)

```sh
python3 tests/diff_engines.py --count 10000 --seed 1
```

Expected: `10000 programs, 0 disagreements`, with the number that stopped on a runtime error reported. Any disagreement is written to `build/diff-failures/` with both outputs.

## 4. The collector still sees everything (US4, SC-004)

```sh
make gc-check       # the stress pass over the corpus, bench2 under stress, the fault matrix
```

Expected: the corpus is identical under stress. Every fault kind is caught, including by the new targeted programs for temporaries, staged builtin arguments and callback frames.

## 5. Limits (US3, SC-007, SC-008)

```sh
python3 tests/gc_bounds.py                       # the five spec 003 workloads, peak < 64 MiB
python3 tests/gc/timeout_check.py                # builtins and tight loops stop within 1 s of a 2 s limit
```

Recursion check (also a golden case, `run/vm_deep_recursion`):

```sh
printf 'fn down(n: int) -> int {\n    if n <= 0 {\n        return 0;\n    }\n    return down(n - 1) + 1;\n}\n\nfn main() uses console {\n    print(str(down(100000)));\n}\n' > build/tmp/rec.vo
bin/vo run build/tmp/rec.vo     # prints 100000
```

## 6. Budget (SC-006, SC-009)

```sh
sh tests/perf.sh                 # 1,000-line programs: check + run under 1 s
python3 tests/ref_examples.py    # the reference examples, unchanged reference
```

# Quickstart: Validate vo-lang v3 Memory

Prerequisites and the build are as in `specs/001-vo-lang-core/quickstart.md`. `make test` runs the gate, including the new memory checks; `make gc-check` builds the stress binary (`make STRESS=1`) and runs the slow stress and fault-injection suites.

## US1: Allocation-heavy programs run in bounded memory

```sh
bin/vo run --mem-stats tests/gc/workloads/string_build.vo
python3 tests/gc_bounds.py
tests/run.sh --stress 4 tests/gc/edge
```
- **`string_build.vo`** builds a 200,000-character string by repeated `+` (about 2 GB copied). The `memory:` line on stderr should show a peak live size under 64 MiB, with gigabytes allocated.
- **`gc_bounds.py`** runs all five workloads in `tests/gc/workloads` (string building, list churn, map churn, closures, generic containers). It checks each one's output, `peak_live < 64 MiB` and `allocated >= 1 GiB` (SC-001).
- **`tests/gc/edge`** holds the structure cases (deep lists and variant chains, shared values, builtin temporaries, captures), normally and under stress.

## US2: Nothing else changes

```sh
make test                                    # the existing gate, byte-identical (SC-002)
python3 bench2/validate.py                   # benchmark reference solutions unchanged
sh tests/perf.sh                             # under 1 s; at most 20% slower than the v2 numbers (SC-006)
```

## US3: Out of memory is a clear, deterministic error

```sh
bin/vo run --max-memory 16 --json tests/gc/edge/grow_forever.vo
```
- **Expected:** R0006, with `in_use` within 10% of `limit` (16 MiB).
- **Repeats:** `tests/gc_bounds.py` runs it 20 times and requires identical reports (SC-005).

## US4: The time limit stops everything

```sh
bin/vo run --timeout 2 tests/cases/run/timeout_in_sort.vo
python3 tests/gc/timeout_check.py
```
- **Expected:** R0009 (exit 124) within 3 s.
- **`timeout_check.py`** runs all five `timeout_in_*` cases (sort, equality, render, find, split with collections) and requires each to stop within 3 s (SC-007).

## US5: Generic code is not slower in hot loops

```sh
sh tests/perf.sh
```
- **Expected:** the generic/specific pairs report a ratio of at most 1.5 (SC-008).

## US6: Confidence that nothing in use is reclaimed

```sh
make STRESS=1                                # build-stress/vo: the maintainers' stress build
tests/run.sh --stress                        # the whole corpus collecting every 64th allocation, compared with normal mode
python3 tests/gc/baseline.py --stress        # every benchmark reference solution in stress mode (SC-003)
python3 tests/fuzz.py --stress --iterations 3000 --seed 9
python3 tests/gc_faults.py                   # 9 fault builds, each must be caught (SC-004)
```

# Quickstart: Validating Complex Data Structures

Prerequisites: `make` (x86-64), cross builds as in spec 008; Python 3.

## 1. The module (US1-US3)

```sh
bin/vo doc std.collections            # every type and function, each with a checked example
make test                             # golden cases under tests/cases/collections/, property tests
python3 tests/structures_props.py     # 1,000 generated operation sequences per structure (SC-001)
```

Expected: the ordered map and set agree with a sorted-list model and stay within the AVL height bound;
queue pops agree with a stable sort; the deque agrees with a list; every "nothing found" is an `err`.

## 2. Speed (SC-002, SC-003)

```sh
python3 tests/structures_bench.py     # 100,000 inserts + lookups, pushes + pops; doubling runs
```

Expected: each under 5 s; doubling the size at most about doubles the time (sorted keys included); the
ordered map within 5x of the built-in `map`.

## 3. Language changes (US4, US5)

```sh
tests/run.sh tests/cases/generic tests/cases/ordered
```

Expected: `new Node { ... }` and `insert(Tree.leaf, 5)` check without type arguments; a record of ordered
fields sorts and keys a map field by field; a record with a list field is E0313 naming the field; `new
ordered_map[list[int], str]` is E0313 naming `ordered_map_by`.

## 4. Unchanged behavior (FR-017)

```sh
make gate && make ARCH=aarch64 test && make ARCH=riscv64 test && make instrumented
make diff-asm COUNT=10000
```

Expected: every existing case byte-identical; 0 disagreements with the previous release.

## 5. Evaluation (US6, SC-004, SC-005)

```sh
python3 tests/structures_eval.py                  # with.vo and without.vo pass; naive.vo times out
python3 tests/structures_eval.py --lines          # SC-004: median line counts, with vs without
python3 tests/structures_eval.py --solve          # isolated agents, module documented
python3 tests/structures_eval.py --solve --without-module
```

Expected: `with.vo` and `without.vo` pass all visible and hidden tests; `naive.vo` times out on a large
hidden input of every task; `with.vo` is at least 30% shorter (median); agent results recorded in
`results.md`.

# Results: Complex Data Structures

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/structures_props.py --stress`: the property tests of every structure (agreement with list and map models over generated operation sequences, neighbors and strict neighbors against scans, tie order, deque against a list, generated values valid) under 10 seeds, 100 cases each, on the normal and the stress build: 16,000 generated cases, 0 failures. The stress build checks AVL heights, balance, key order and size, and heap order, after every change. |
| SC-002 | met | `tests/structures_bench.py`: 100,000 inserts then 100,000 lookups (random and ascending keys) in about 0.03 s for the ordered map and set, 100,000 pushes then pops in the priority queue about 0.03 s (limit 5 s); doubling to 200,000 multiplies the time by 1.95-2.27 (limit 2.5). |
| SC-003 | met | The ordered map and set take 2.5-3.1x the built-in `map` for the same inserts and lookups (limit 5x). |
| SC-004 | met | `tests/structures_eval.py --lines`: `with.vo` is a median 51.1% shorter than the efficient hand-written `without.vo` (limit 30%); s04 (sliding window) is the exception (-8%: a list with a head index is as short as a deque). `tests/structures_eval.py`: `with.vo` and `without.vo` pass every test of all 8 tasks, and `naive.vo` times out on a large hidden input of every task. |
| SC-005 | not run | The isolated agent run (`structures_eval.py --solve` with and without `--without-module`) costs agent time and runs when the user asks (spec: recorded, not gated). |
| SC-006 | met | The AVL tree of User Story 4 checks and runs with no type arguments written (`tests/cases/generic/infer_avl.vo`, height 10 for 1,000 sorted inserts). Reference: about 11,107 tokens, +259 (limit +300, budget 15,000). The full gate passes on all three platforms, and existing programs give byte-identical results (the changed expectations are listed under Behavior changes). |

## Verification

| Check | Result |
|---|---|
| `make gate` (x86-64) | passed (second run): lint, golden 530, step 205 (139 + 35 repeated + the server, HTTP and replay sets), catalog, reference, doc conformance, fuzz, examples, diff-engines 1,000 (0 disagreements); ubtrap; diff-asm 2,000 (0 disagreements); gc-check (stress golden, fuzz 3,000, fault injection, stress diff-engines 1,000: 0 disagreements); speed geometric mean 0.986 (limit 1.10). The first run failed in the stress fuzzer (see Deviations) |
| `make ARCH=aarch64 test`, `make ARCH=riscv64 test` | passed on both: golden 530, step 205 / 139 / 35 |
| `make instrumented` | 530 of 530 |
| `make diff-asm COUNT=10000` | 10,000 programs, 0 disagreements |
| `tests/doc_conformance.py` | 80 library items, 454 checked calls, 80 examples, 0 failures |
| `tests/catalog.py` | 101 sections, 0 problems (no new codes) |

## Behavior changes

- **New types** `ordered_map[K, V]`, `ordered_set[T]`, `priority_queue[T]`, `deque[T]`: recognized only in
  a type position and after `new` when `[` follows, so they are not reserved words (variables and
  functions with those names keep working, `tests/cases/collections`). Values print as
  `ordered_map{1: "a"}`, `deque{0, 1}`.
- **Existing builtins** accept the collections: `len`, `has`, `remove`, `get_or`, `keys`, `push`, `pop`,
  indexing, `set c[k] = v;`, and `for` (a copy of the collection, so a loop may change it).
- **Records of ordered fields are ordered** (field by field): `<`, `sort`, `min`, `max`, `sort_by` keys,
  `T: ordered`. `tests/cases/generic/not_ordered` used a one-`int` record to show E0313; it now has a list
  field. The E0313 catalog example changed the same way.
- **E0313's message** names the first field without an order for a record, and names the `_by`
  constructor for `new ordered_set[list[int]]`.
- **Inference:** `new Pair { ... }` and `Tree.leaf` as an argument infer their type arguments; E0310 and
  E0312 keep their messages where nothing determines them.
- **New std module** `std.collections` (25 functions); `vo doc` now prints the type parameters of
  generic records (it dropped them before: no standard module had one).
- **`--timeout`** is charged for work inside collection operations (found by the evaluation: a loop's
  copy of a large set ran 2.7x past the limit before).

## Deviations

- **The types are lowercase built-ins created with `new` (user's decisions during implementation).** The
  planned design declared PascalCase types (`OrderedMap`) as field-less records in `std/collections.vo`,
  created by constructor functions (`ordered_map()`), with a new code E0314 for `new`. The user found
  both the constructors and the casing inconsistent with `new map[str, int]`. The revision removed the
  constructors, E0314, the record mapping and the empty-record literal they needed; no new code remains.
- **Built in C, not written in vo** (user's decision in planning, research R1): equality, printing,
  generation and hidden internals by contents needed it.
- **Two functions added after the evaluation:** strict neighbors `after`/`before` (sets) and
  `key_after`/`key_before` (maps). Without them "the k best" needed a full copy (`for` with `break`) or
  remove-and-re-add.
- **Tests first:** the four structures share one runtime file, so the golden cases of the foundation,
  US1-US3 and US5 were written after it; they fail on the previous release (the types do not exist).
  US4's cases were written first and failed as expected.
- **Property-test harness:** `tests/doc_conformance.py` learned setup statements in examples
  (`let s = ...; push(s, 1); EXPR is VALUE`), annotated probes for type parameters only a result mentions,
  and `new` values for the collection types.
- **Stress fuzzer:** the first gate run failed: `tests/fuzz.py --stress` mutated a comment of
  `run/reentry_fold.vo`, a case the stress build already skips (its deep recursion does not fit that
  build's smaller root stack: T0003), picked for the first time because the new cases changed seed 9's
  choices. The fuzzer now leaves out the cases `tests/run.sh --stress` skips; seed 9 passes.
- **Stress build:** the 100,000-element cases (`omap_sorted_100k`, `pq_100k`) are skipped there
  (`stress-every`: whole-structure checks after every change are quadratic); the smaller cases and the
  properties run with the checks.

## Follow-ups

- **SC-005**: run `tests/structures_eval.py --solve` and `--solve --without-module`.
- **Changing a record that is a key** breaks the order (the collections hold references, as `list` and
  `map` do); documented in the reference. Freezing or copying keys would prevent it.
- **Loops copy the collection** (O(n) even with `break`), documented; a lazy iterator that fails fast on
  change would avoid the copy.
- **Comparison functions written `a - b`** overflow near the integer limits (R0001); a `compare` builtin
  would give agents a safe idiom.
- **W0504 on single-file programs that import a std module** (pre-existing, unrelated): the warning asks
  for `vo.effects`, which library functions never appear in.
- `pop` on a list returns a `result`, so dropping the last element needs `match` or `try`.

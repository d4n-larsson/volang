---

description: "Task list for vo-lang spec 014: complex data structures"
---

# Tasks: Complex Data Structures

**Input**: Design documents from `specs/014-complex-data-structures/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R10), data-model.md, contracts/api.md,
contracts/language.md, contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution IV, FR-010): golden cases and property tests land first and fail before
each piece.

**Organization**: setup and the shared foundation, then one phase per user story (US1, US2, US4 are P1;
US3, US5, US6 are P2), then the gate.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- Golden cases: `tests/cases/collections/` (the module), `tests/cases/ordered/` (record order, comparison
  functions), `tests/cases/generic/` (inference). Property tests are vo `prop` cases in
  `tests/cases/collections/props_*.vo`.
- **Regression rule:** programs that use neither the module nor the new inference keep byte-identical
  results; `make test` after every task.
- The type-kind inventory (plan.md, Structure Decision) lists every switch a new kind must reach;
  closed-set defaults (`ty_name_into`, `ty_unify`, `values_equal`, `render_value`, `prop_gen`) MUST be
  extended explicitly.

---

## Phase 1: Setup

- [X] T001 `specs/014-complex-data-structures/results.md` (SC table, verification table); add
  `014-complex-data-structures` to `tests/catalog.py`; `std/collections.vo` stub (module doc comment) added
  to `STD_FILES` in `Makefile` and to `doc_modules` in `src/doc.c`; create `tests/cases/collections/`,
  `tests/cases/ordered/`.

---

## Phase 2: Foundational (blocks US1, US2, US3, US5)

- [X] T002 [P] Foundation cases in `tests/cases/collections/`: importing the four types and the
  constructors; `let m = new ordered_map[int, str];` and `len(m)` is 0; `new ordered_set[list[int]]` is
  E0313 naming `ordered_set_by`; `vo doc std.collections` lists the functions; `==` on two empty maps is
  true. Confirm they fail. (Revised during implementation: lowercase built-in types created with `new`;
  the first version's constructor functions and E0314 were removed.)
- [X] T003 Type kinds (research R2): `TY_OMAP`, `TY_OSET`, `TY_PQUEUE`, `TY_DEQUE` numbered above
  `TY_EFFSET` in `src/vo.h` (rt_concrete's `<= TY_VOID` shortcut); `resolve_type` in `src/check.c` maps the
  four empty records of `std/collections.vo` (by file and name) to them, `a` = key or element type, `b` =
  value type; `ty_name_into` prints `ordered_map[K, V]` etc.; `ty_has_params`, `ty_subst`, `ty_unify` and
  the memo in `src/types.c`; `ty_has_func` in `src/check_generic.c`; `export_type` in `src/check.c`; `==`
  allowed in `check_binary` (`src/check_expr.c`).
- [X] T004 Runtime object and primitive (research R4, R5): `OBJ_COLL` in `src/vo.h` with the count at
  offset 8 (OPC_LEN reads `len` there for every kind; add a CHECK_OFFSET), kind, element type, comparison
  closure, root/array, capacity, sequence counter; allocation in new `src/rt_coll.c`; the std-only
  primitive `__coll(OP, a, b, c)` as builtin id 61 (`B_COUNT`/`B_LAST`, `builtin_names`, the `BU_*` enum
  and `_Static_assert` in `src/rt_call.c`, `builtin_effects` 0), typed loosely inside FILE_STD only (its
  result takes the expected type), with a `ponytail:` comment; the constructors `ordered_map`,
  `ordered_set`, `priority_queue`, `deque` in `std/collections.vo` (the runtime stores
  `rt_concrete` of the returned type); E0314 in `check_new_record` (`src/check_expr.c`) with the
  `sometimes` fix, its entry in `docs/errors.md`.
- [X] T005 Order and snapshots: `value_cmp(a, b, Type *)` in `src/rt_coll.c` for `int`, `float`, `str`
  (records in US5), and calls of a comparison closure through `call_value` (as `sort_by` calls its key);
  a guard that raises R0007 ("the collection changed while it was being ordered") when a collection is
  changed during one of its own operations; `coll_snapshot` returning a `list` of the elements in
  iteration order (used by `for`, `keys`, rendering, equality, set algebra).

**Checkpoint**: T002 passes; nothing else changes (`make test`).

---

## Phase 3: User Story 1 - Keys in sorted order (Priority: P1) 🎯 MVP

**Goal**: ordered map and set with neighbors, ranges and set algebra (FR-002, FR-005, FR-006).

**Independent Test**: golden cases for every map and set operation; property tests against a sorted-list
model; the AVL invariant checked after every operation in the stress build.

- [X] T006 [P] [US1] Cases in `tests/cases/collections/omap_*.vo` and `oset_*.vo` for every row of
  contracts/api.md for the map and set: insert order vs key order, replace keeps the size, removing a
  missing key, `m[k]` R0004, every "nothing found" an `err`, `keys_between`/`between` bounds inclusive,
  union/intersection/difference, `copy_map` independence, `for x in s` over a snapshot while the loop
  changes `s`, 100,000 ascending inserts; properties in `tests/cases/collections/props_ordered.vo`
  (agreement with a sorted-list model over generated operation lists; size). Confirm they fail.
- [X] T007 [US1] AVL tree in `src/rt_coll.c` (research R5): nodes on the collected heap (key, value, left,
  right, height); iterative insert and delete with an explicit path stack bounded by the AVL height (no
  native recursion); lookup, first/last, at-or-above/at-or-below, range collection, in-order snapshot,
  copy, linear-merge union/intersection/difference "ordered like `a`". In stress builds (`VO_STRESS`),
  check after every operation that in-order keys strictly increase, every height is 1 + the larger child
  height, children differ by at most 1, and size equals the node count (T0003 on a violation).
- [X] T008 [US1] Builtins on map and set (contracts/api.md): `len`, `has`, `remove`, `get_or`, `keys`,
  `m[k]` and `set m[k] = v;` (`check_index`, `check_set` in `src/check_expr.c`/`src/check.c`; the index
  and set-index paths in `src/compile.c`, `src/vm.c` or through `builtin_call` in `src/rt_call.c`), `push`
  on a set, `for x in s` (`check_for` in `src/check.c`; `src/compile.c` iterates `coll_snapshot`); the
  library functions `first_key`, `last_key`, `key_at_or_above`, `key_at_or_below`, `keys_between`,
  `first`, `last`, `at_or_above`, `at_or_below`, `between`, `union`, `intersection`, `difference`,
  `copy_map`, `copy_set` in `std/collections.vo`, each with a doc comment and an `EXPR is VALUE` example.
- [X] T009 [US1] Contents semantics for map and set (research R7): `values_equal` in `src/interp.c`
  (same type, same entries in order; comparison functions not compared); `render_value` in
  `src/rt_render.c` (`ordered_map{1: "a", 2: "b"}`, `ordered_set{1, 2}`, the 200-character cut and depth
  limit); `pg_find`, `prop_gen`, `prop_copy`, `prop_shrinks` in `src/prop.c` (a generated list of
  elements inserted through the operations; shrink the list); E0602 for an element type with no order.

**Checkpoint**: US1 cases and properties pass; `make test`.

---

## Phase 4: User Story 2 - The next most urgent item (Priority: P1)

**Goal**: priority queue, ties in push order (FR-003, FR-005, FR-006).

**Independent Test**: golden cases; properties against a stable sort.

- [X] T010 [P] [US2] Cases in `tests/cases/collections/pq_*.vo`: pops in order, ties in push order,
  `pop`/`peek` on empty give `err("empty")`, `for x in q` in pop order without removing, `copy_queue`,
  equality and rendering, 100,000 pushes then pops; properties in `props_queue.vo` (pops equal the stable
  sort of the pushes). Confirm they fail.
- [X] T011 [US2] Binary heap of (item, sequence) in `src/rt_coll.c` (ties by sequence); `push`, `pop`,
  `len`, `for` (snapshot sorted by (order, sequence)) through the builtins; `peek`, `copy_queue` in
  `std/collections.vo`; equality, rendering (`priority_queue{1, 3}`) and generation in `src/interp.c`,
  `src/rt_render.c`, `src/prop.c`; stress-build heap-order check after every operation.

---

## Phase 5: User Story 4 - Writing your own generic structures is easier (Priority: P1)

**Goal**: inference for `new` on generic records and for cases without a value passed to generic
functions (FR-011). Independent of the collections: may run in parallel with Phases 2-4.

**Independent Test**: golden cases in `tests/cases/generic/`.

- [X] T012 [P] [US4] Cases `tests/cases/generic/infer_new_*.vo` and `infer_case_arg_*.vo`: `return new
  Node { ... }` with the type from the return type and from field values; `insert(Tree.leaf, 5)`;
  `make(Tree.leaf, x, Tree.leaf)` with `x: T`; a `new` inside a list literal with an annotated `let`;
  explicit type arguments still valid; `let t = Tree.leaf;` still E0310 with its message; a wrong count
  of written arguments still E0312; the AVL tree from the roadmap written without type arguments checks
  and runs. Confirm they fail.
- [X] T013 [US4] `check_new_record` in `src/check_expr.c`: with no type arguments written on a generic
  record, bind them from the expected type when it is an instance of the same record, then by unifying
  each field's declared type with its value's type (as `vcase_generic` does for payloads), then
  `bind_rest` (E0310); `check_generic_call` in `src/check_generic.c`: check arguments that are a generic
  case without a value or such a `new` after the others, with the parameter type substituted by the
  bindings so far as their expected type.

---

## Phase 6: User Story 3 - Both ends of a queue (Priority: P2)

**Goal**: deque (FR-004, FR-005, FR-006).

- [X] T014 [P] [US3] Cases in `tests/cases/collections/deque_*.vo`: `push`/`push_front`, `pop`/`pop_front`,
  `front`/`back`, `d[i]` and `set d[i] = v;` (R0003 out of range), `for` front to back, empty gives
  `err("empty")`, `copy_deque`, equality and rendering, 100,000 operations at both ends; properties in
  `props_deque.vo` against a list model. Confirm they fail.
- [X] T015 [US3] Ring buffer (capacity a power of two, doubling when full) in `src/rt_coll.c`; builtins
  `len`, `push`, `pop`, indexing, `for`; `push_front`, `pop_front`, `front`, `back`, `copy_deque` in
  `std/collections.vo`; equality, rendering (`Deque{0, 1, 2}`), generation.

---

## Phase 7: User Story 5 - Ordering by user types (Priority: P2)

**Goal**: records of ordered fields are ordered (FR-012); comparison functions (FR-013).

- [X] T016 [P] [US5] Cases in `tests/cases/ordered/`: `<`, `sort`, `min`, `max` on `record Slot { date:
  str; room: int; }`; a record as an ordered-map key and a queue priority; a generic record ordered per
  instance; a record with a `list` field used where an order is needed is E0313 naming the field
  ("`Slot` is not ordered: its field `tags` (`list[str]`) has no order."); `priority_queue_by` with a
  descending comparison; `ordered_map_by` on a record without an order; a comparison function with
  effects is E0301; a contradictory comparison function (always -1) finishes with a consistent size; a
  comparison function that changes the collection is R0007; `==` stays structural. Confirm they fail.
- [X] T017 [US5] `ty_ordered` and `check_targ` in `src/check_generic.c` (records whose fields are all
  ordered, recursively, per instance) and the E0313 message naming the first field without an order;
  `is_ordered_kind` and the `sort_by` key check in `src/check_expr.c`, `src/check_func.c`; at run time
  `value_cmp` for records (field by field in declaration order) in `src/rt_coll.c`, used by
  `op_cmp_param` (`src/vm.c`) when the concrete kind is a record, by `sort` (`list_sort`/`elem_less` in
  `src/rt_list.c`) and `min`/`max` (`src/rt_call.c`); `ordered_map_by`, `ordered_set_by`,
  `priority_queue_by` in `std/collections.vo` (the closure stored in the object, pure function type
  required); every tree and heap operation bounded by height and size whatever the function answers.

---

## Phase 8: User Story 6 - Does it help agents? (Priority: P2)

**Goal**: the evaluation (FR-015, FR-016) and the measured targets (SC-001 to SC-005).

- [X] T018 [P] [US6] `tests/structures_eval/` (research R9): 8 tasks (a booking calendar with range
  queries, top-k leaderboard updates, shortest paths, sliding-window maximum, an event simulator, merging
  sorted streams, an interval scheduler, a running median), each with `task.md`, `tests.json` (visible
  and hidden, hidden inputs sized so a sort-every-step solution exceeds `--timeout`), and three
  reference solutions: `with.vo` (uses `std.collections`), `without.vo` (correct and efficient, with
  `list`/`map` and a structure written by hand) and `naive.vo` (sorts or scans on every step);
  `tests/structures_eval.py` checks that `with.vo` and `without.vo` pass every test and `naive.vo` fails a
  large hidden input of every task; `--lines` compares `with.vo` and `without.vo` (SC-004); `--solve
  [--without-module]` runs the isolated agent protocol of `tests/stdlib_eval.py`.
- [X] T019 [P] [US6] `tests/structures_bench.py` (SC-002, SC-003: 100,000 inserts plus lookups with random
  and sorted keys, 100,000 pushes plus pops, doubling runs, the built-in `map` as the baseline) and
  `tests/structures_props.py` (SC-001: the `props_*.vo` files under 10 seeds, at least 1,000 generated
  sequences per structure, run in the stress build too).

---

## Phase 9: Polish & Cross-Cutting Concerns

- [X] T020 Docs: `docs/reference.md` (ordered records and inference paragraph, the builtin rows gain the
  collection types with their meaning per type where it differs: `push` on a set adds, `pop` on a queue
  removes the smallest and on a deque the back; one line to `vo doc std.collections`; at most 300
  tokens, stated in results);
  `docs/errors.md` E0313 text; `contracts/` final; `docs/roadmap.md` (014 status) and `README.md`.
- [X] T021 Gate: `make test`, `make gate`, `make ARCH=aarch64 test`, `make ARCH=riscv64 test`, `make
  instrumented`, `make diff-asm COUNT=10000`, `tests/structures_props.py`, `tests/structures_bench.py`,
  `tests/structures_eval.py` (and `--lines`); fill `results.md` (SC-001 to SC-006, behavior changes,
  deviations). The isolated agent run (SC-005) is run and recorded when the user asks (it costs agent
  time).

---

## Dependencies & Execution Order

- Setup → Foundational (T002-T005) → US1 → US2 → US3 → US5 (US5's record order also reaches the
  collections' default order) → US6 (needs all structures) → Polish.
- US4 (T012-T013) touches only the checker's inference and can run any time after Setup.
- Within a story: cases first (failing), then the implementation.

## Parallel Example

T006, T010, T012, T014 and T016 (cases) can be written together; T018 and T019 together.

## Implementation Strategy

MVP: Foundation + US1 (ordered map and set) + US4 (inference). Then the queue and the deque, record
ordering and comparison functions, the evaluation, and the gate.

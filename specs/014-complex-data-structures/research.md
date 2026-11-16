# Research: Complex Data Structures

## R1: Built in C, exposed through `std.collections` (clarified)

- **Decision:** four new built-in generic types, `ordered_map[K, V]`, `ordered_set[T]`, `priority_queue[T]`
  and `deque[T]`, are runtime objects implemented in C (like `list` and `map`), each a new type kind.
  Programs write them like `list` and `map` (R2), and the module's exported functions (vo code
  wrapping a std-only `__` primitive, as `std.http` wraps `__http_exchange`) give the operations that
  have no existing builtin (R2 revised: the types are lowercase built-ins needing no import).
- **Rationale (user's choice, 2026-11-16):** equality by contents, rendering by contents, property-test
  generation, iteration snapshots, collector tracing and hidden internals all come from the same code
  paths as `list` and `map`, instead of new opaque-type machinery; speed meets SC-002/003 with margin.
  Spec FR-001's "written in vo" becomes "built in, exposed as a standard module"; the spec is updated.
- **Alternatives:** the algorithms in vo plus opaque records and std-only hooks for `==`, rendering and
  generation (most new machinery, and 100,000 operations in under 5 s is tight for bytecode); plain vo
  records with `==` refused (weaker guarantees, invalid trees constructible by hand).

## R2: Naming and creating the types: like `list` and `map` (revised during implementation)

- **Decision (user, 2026-11-16):** the types are lowercase built-ins, `ordered_map[K, V]`, `ordered_set[T]`,
  `priority_queue[T]`, `deque[T]`, created with `new` like maps: `new ordered_map[str, int]`. The names
  are recognized only in a type position and after `new` when `[` follows, so they are not reserved:
  variables and functions with those names keep working. No import is needed for the types; a key or
  element type with no order in `new ordered_map[...]` is E0313 naming the `_by` function. Other orders:
  `ordered_map_by(cmp)`, `ordered_set_by(cmp)`, `priority_queue_by(cmp)` in `std.collections`
  (`new` has no place for a function).
- **First design, replaced:** PascalCase types declared as field-less records in `std/collections.vo`
  and created by constructor functions (`ordered_map()`), with E0314 for `new`. The user found the
  constructor functions inconsistent with `new map[...]`, and the casing inconsistent with `list`/`map`.
  The revision removed E0314, the record mapping and the empty-record literal it needed.
- **Rationale:** one way to create every collection, built-in or not; a type's casing tells built-in
  (lowercase) from user-defined (PascalCase).

## R4: Operations: existing builtins first, then a few named functions

- **Decision:** the existing builtins extend to the new types where they mean the same thing, so agents
  use words they already know:
  - `len(c)` (all four); `has(c, k)` and `remove(c, k)` (map, set); `get_or(m, k, d)`, `keys(m)` (in key
    order), `m[k]` (R0004 when missing) and `set m[k] = v;` (map);
  - `push(c, x)` (set: add; queue: add; deque: add at the back) and `pop(c)` (queue: the smallest;
    deque: the back), both `pop`s returning `result[T]` with `err("empty")` like `pop` on a list;
  - `d[i]` and `set d[i] = v;` (deque, R0003 out of range); `for x in c` (set in order, deque front to
    back, queue in priority order; a map keeps `for k in keys(m)`, as today).
  
  `std.collections` exports the rest, each name used by one type only (no overloading):
  - map: `first_key`, `last_key`, `key_at_or_above`, `key_at_or_below`, `keys_between(m, lo, hi)`;
  - set: `first`, `last`, `at_or_above`, `at_or_below`, `between(s, lo, hi)`, `union`, `intersection`,
    `difference`;
  - queue: `peek`;
  - deque: `push_front`, `pop_front`, `front`, `back`;
  - all: `copy_map`, `copy_set`, `copy_queue`, `copy_deque`; and `ordered_map_by`, `ordered_set_by`,
    `priority_queue_by` (R2).

  Every operation that can find nothing returns a `result` (`err("empty")`, `err("none")`).
- **How the library functions reach C:** each is a one-line vo wrapper in `std/collections.vo` calling
  one std-only primitive, `__coll(OP, a, b, c)` (builtin id 61), typed loosely inside the standard
  library only (its result takes the expected type); the wrapper's declared signature is the public type
  that programs are checked against. `new` and the `_by` constructors give the runtime the concrete collection
  type (via `rt_concrete`), stored for the built-in order.
- **Rationale:** Principle V (minimal surface): builtins already documented in the reference keep their
  meaning; the 20 new names live in `vo doc std.collections`, not in the reference. Principle I (no
  overloading) holds: builtins were already polymorphic over `str`, `list` and `map`; library functions
  are not.
- **Alternatives:** all operations as polymorphic builtins (global names such as `first` and `union`
  would break programs that define them, and the reference grows by about 500 tokens); separate modules
  per structure (the same names would clash when two are imported).

## R5: Representation and complexity

- **Decision:**
  - `ordered_map`/`ordered_set`: an AVL tree of nodes allocated on the collected heap (key, value, left,
    right, height), iterative insert and delete with an explicit path stack (bounded by the AVL height,
    at most 1.44 log2(n + 2) + 1, so no native recursion), size kept.
  - `priority_queue`: a binary heap in a growable array of (item, sequence number); ties order by the
    sequence number, so equal priorities leave in insertion order (FR-003) whatever the heap layout.
  - `deque`: a ring buffer that doubles when full.
  - Each object stores its element type (for the built-in order) or its comparison function.
- **Rationale:** the classic structures with known bounds (FR-006); iterative code keeps the native
  stack flat (spec 013's L10 lesson).
- **Alternatives:** red-black trees (the user named AVL, which is also more tightly balanced, so faster
  lookups); a pairing heap (no gain at these sizes).

## R6: Ordering of records and comparison functions

- **Decision:** a record type is ordered when every field type is ordered (`int`, `float`, `str`, or a
  record ordered by this rule, recursively; generic records per instance). `ty_ordered` gains that rule;
  `<`, `<=`, `>`, `>=`, `min`, `max`, `sort` and the default order of the new structures use one C
  comparison (`value_cmp`) that compares field by field in declaration order. A record with a field of
  another kind used where an order is needed is E0313, whose message names the field (no new code).
  Comparison functions are `fn(T, T) -> int` and must be pure (a function type with effects is E0301 at
  the constructor); the runtime calls them like `sort_by` calls its key function.
- **Inconsistent comparison functions (FR-013):** every tree operation is bounded by the tree height and
  every heap operation by log2(n), whatever the function answers, so a contradictory comparison gives a
  wrong order, never a loop or a corrupt size; a comparison function that raises (R0007, R0009) stops
  the program like any other runtime error, after the structure is left consistent.
- **Mutation during a comparison or iteration:** the comparison function cannot reach the collection
  being changed unless it captured it; any change to a collection while one of its own operations runs
  is R0007 ("the collection changed while it was being ordered"), the same rule as spec 013's `sort_by`
  snapshot. Iteration copies the elements first (FR-007).
- **Alternatives:** ordering variants too (not needed by the stories; later if asked); `-> bool` "less
  than" functions (a three-way result tells equality from order, which a map needs for keys).

## R7: Equality, rendering, generation

- **Decision:**
  - `==`: two collections of the same type are equal when they hold the same elements (map: entries) in
    the same order; a collection created with a comparison function compares contents only (the
    function is not compared). Queues compare their elements in priority order, deques front to back.
  - Rendering (`expect_eq` reports, runtime-error locals, traces, the `--json` value form):
    `ordered_map{1: "a", 2: "b"}`, `ordered_set{1, 2}`, `priority_queue{1, 3}`, `deque{0, 1, 2}`, with the
    usual 200-character cut and the spec 013 depth limit.
  - Property tests: a parameter of one of these types is generated as a list of elements (as lists are
    generated today) inserted through the public operations, and shrunk by shrinking that list, so
    every generated value satisfies the invariants. Generated collections use the built-in order (a
    comparison function is not part of the type); an element or key type with no built-in order cannot
    be generated: the existing E0602 (`ungeneratable_type`).
- **Rationale:** FR-008 and FR-009 with the existing equality, render and generation code paths.

## R8: Type-argument inference for `new` and for cases without a value (US4)

- **Decision:**
  - `new Node { ... }` of a generic record with no type arguments written: the arguments come from the
    expected type when it is an instance of the same record, then from the field values (unifying each
    field's declared type with its value's type), as `vcase_generic` already does for payloads. What
    stays unbound is E0310 with the existing message (`give the variable a type`). Writing the arguments
    keeps working.
  - A generic case without a value (`Tree.leaf`) or a `new` without arguments, passed as an argument to a
    generic function: `check_generic_call` checks such arguments after the others, with the parameter
    type substituted by the bindings found so far as the expected type.
- **Rationale:** the two failures seen when writing an AVL tree in vo; small, local changes to
  `check_new_record` and `check_generic_call`.

## R9: Evaluation

- **Decision:** `tests/structures_eval/` in the style of `tests/stdlib_eval/` (statement, visible tests,
  hidden tests, fixtures, reference solution), 8 tasks; hidden inputs sized so a solution that sorts or
  scans on every step exceeds `--timeout` while a logarithmic one finishes in under a second. Each task
  has three reference solutions: `with.vo` (using `std.collections`), `without.vo` (correct and
  efficient with `list`/`map` and a hand-written structure: the fair line-count comparison of SC-004),
  and `naive.vo` (sorting or scanning on every step: shows the large hidden inputs separate the two). `tests/structures_eval.py --solve [--without-module]` runs the isolated agent protocol of
  spec 007 (no tools, empty working directory, no user settings), with and without
  `vo doc std.collections` in the prompt; results go to `results.md` (SC-005).
- **Rationale:** reuses a protocol and harness that already exist; the run itself is not part of the gate.

## R10: Performance targets

- **Decision:** `tests/structures_bench.py` measures 100,000 inserts plus 100,000 lookups (map, sorted and
  random keys) and 100,000 pushes plus pops (queue) against the built-in `map` and a size-doubling run;
  SC-002 and SC-003 thresholds are checked there, outside `make test` (timing-sensitive), and run by the
  gate's speed step.

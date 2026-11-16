# Feature Specification: Complex Data Structures

**Feature Branch**: `014-complex-data-structures`

**Created**: 2026-11-16

**Status**: Draft

**Input**: User description: "Complex data structures (roadmap "Next: complex data structures", spec 014): a standard-library module (std.collections) written in vo with an ordered map and an ordered set on a balanced AVL tree, a binary heap (priority queue) and a deque, each with property tests for its invariants (balance, ordering, size) and vo doc examples; decide persistent vs mutable (likely persistent by default, measured against list and map); the small language changes the module needs: inference of type arguments for `new` on generic records and for cases without a value (e.g. an empty `Tree.leaf` passed as an argument, today E0310), and ordering for user types (records ordered field by field, or a compare function); plus an agent evaluation: benchmark tasks that need one of these structures, with and without the module."

Background: vo has two collections, `list` and `map` (keys `int` or `str`, insertion order). A program
that needs keys in sorted order, the smallest pending item, or both ends of a queue builds the structure
itself or falls back to sorting a list on every step. Writing a generic AVL tree in vo today works, but
costs more than it should: `new Node[T] { ... }` needs its type argument written out, an empty case
passed as an argument cannot be inferred (E0310), and `ordered` covers only `int`, `float` and `str`.
Hand-written balanced trees are also where agents make quiet mistakes: a wrong rotation still returns
plausible answers on small inputs.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–013: AI coding agents that write vo programs from the reference and
`vo doc`, and the people who review and run those programs.

### User Story 1 - Keys in sorted order (Priority: P1)

An agent writes a scheduler that keeps bookings by start time and asks "which booking starts at or
after 10:30?" and "list everything between 9:00 and 12:00, in order". It imports an ordered map from the
standard library, reads the exact operations from `vo doc`, and gets each answer in logarithmic time,
with no sorting in its own code.

**Why this priority**: sorted keys with range queries are the most common need that `list` and `map`
do not cover; the ordered set comes with it.

**Independent Test**: golden cases using every operation of the ordered map and set; property tests
that compare each operation against a sorted-list model on generated operation sequences, and check the
balance invariant after every step.

**Acceptance Scenarios**:

1. **Given** an empty ordered map of `int` to `str`, **When** keys are inserted in any order, **Then**
   iterating it yields the keys in ascending order, each once, and its size is the number of distinct
   keys.
2. **Given** a map with keys 10, 20, 30, **When** asked for the first key at or above 15, the last key at
   or below 25, the smallest and the largest key, **Then** it answers 20, 20, 10 and 30; asked the same
   on an empty map, it answers "none" (a `result` error), never a runtime error.
3. **Given** a map, **When** a key is inserted again, **Then** its value is replaced and the size is
   unchanged; **When** a missing key is removed, **Then** nothing changes.
4. **Given** keys inserted in ascending order (the worst case for an unbalanced tree), **When** 100,000
   are inserted, **Then** every lookup still takes logarithmic time (the tree height stays within the
   AVL bound).
5. **Given** an ordered set, **When** the same operations are used without values, **Then** it behaves
   as the map's key set, and union, intersection and difference of two sets give their sorted results.

---

### User Story 2 - The next most urgent item (Priority: P1)

An agent writes a shortest-path search and an event simulator. Each needs "take the smallest pending
item" repeatedly while adding new ones. It uses a priority queue from the standard library.

**Why this priority**: the second most common structure agents reimplement (often as "sort the list
every time", which is quadratic).

**Independent Test**: golden cases; property tests that compare pops against a sorted model and check
that equal priorities come out in insertion order.

**Acceptance Scenarios**:

1. **Given** items pushed with priorities 5, 1, 3, **When** popped three times, **Then** they come out
   with priorities 1, 3, 5; a fourth pop on the empty queue gives "none", never a runtime error.
2. **Given** two items with the same priority pushed in order A then B, **When** popped, **Then** A comes
   before B (ties keep insertion order, so results do not depend on internal layout).
3. **Given** 100,000 pushes and pops, **When** run, **Then** each operation takes logarithmic time.

---

### User Story 3 - Both ends of a queue (Priority: P2)

An agent writes a sliding-window computation and a breadth-first search: it adds and removes at both
ends. It uses a deque instead of slicing a list.

**Why this priority**: common, but a list with an index can stand in for it; less risk of subtle bugs.

**Independent Test**: golden cases; property tests against a list model.

**Acceptance Scenarios**:

1. **Given** an empty deque, **When** 1, 2 are pushed at the back and 0 at the front, **Then** it reads
   0, 1, 2 from front to back, and indexed access by position returns those values.
2. **Given** a deque, **When** values are popped from either end until it is empty and once more,
   **Then** the last pop gives "none", never a runtime error.
3. **Given** 100,000 pushes and pops at either end, **When** run, **Then** each operation takes constant
   time on average.

---

### User Story 4 - Writing your own generic structures is easier (Priority: P1)

An agent writes its own generic tree or linked structure (the library cannot cover everything). It writes
`new Node { left: l, value: v, right: r }` and passes `Tree.leaf` as an argument, and the checker infers
the type arguments from the context instead of reporting E0312 or E0310.

**Why this priority**: the standard library itself is written in vo and needs this; so does every user
structure the library does not provide.

**Independent Test**: golden cases where type arguments of `new` and of cases without a value are
inferred from the expected type, an argument's parameter type, or the other fields; cases where they
cannot be inferred keep a clear error saying what to write.

**Acceptance Scenarios**:

1. **Given** a generic record `Node[T]` and a function returning `Node[T]`, **When** it writes
   `return new Node { value: v, ... };` with `v: T`, **Then** the type argument is inferred and the
   program checks.
2. **Given** `insert(Tree.leaf, x)` where the parameter is `Tree[T]` and `x: int`, **When** checked,
   **Then** `Tree.leaf` is inferred as `Tree[int]`.
3. **Given** `let t = Tree.leaf;` with nothing to infer from, **When** checked, **Then** it is still
   E0310 with today's message ("give the variable a type"), so existing reports stay unchanged.
4. **Given** a program that writes the type arguments explicitly, **When** checked, **Then** it checks as
   before (explicit arguments stay valid).

---

### User Story 5 - Ordering by user types (Priority: P2)

An agent keeps tasks in a priority queue ordered by deadline then name, or uses an ordered map keyed by
a (date, room) pair. Today `ordered` allows only `int`, `float` and `str`.

**Why this priority**: real keys are often composite; without this, agents encode them into strings
(`"2026-11-16|room3"`), which sorts wrongly for numbers.

**Independent Test**: golden cases with composite keys; property tests checking that the order is a
total order consistent with `==`.

**Acceptance Scenarios**:

1. **Given** `record Slot { date: str; room: int; }`, **When** it is used as an ordered-map key, a
   priority, with `sort`, `min`, `max` or `<`, **Then** slots compare field by field in declaration order
   (date first, then room), with no extra code (clarified: records of ordered fields are ordered).
2. **Given** a record with a field that has no order (a list, a map, a function, a variant), **When** it
   is used where an order is needed, **Then** it is a check error naming that field.
3. **Given** a priority queue of tasks that should come out latest deadline first, **When** the agent
   creates it with a comparison function, **Then** that function decides the order instead of the
   built-in one (clarified: both field-by-field order and an optional comparison function).
4. **Given** a comparison function that is not a consistent order (for example it says a < b and b < a),
   **When** the structure is used, **Then** results may be wrong but the program never crashes and the
   structure never becomes corrupt (every operation still terminates and keeps its size consistent).

---

### User Story 6 - Does it help agents? (Priority: P2)

The maintainer runs benchmark tasks that need one of these structures, once with agents told about the
module and once without, and compares how many tasks are solved and how many hidden large inputs finish
within the time limit.

**Why this priority**: vo's goal is to be the language agents do best in; a module nobody uses well is
surface without benefit.

**Independent Test**: a task set in the style of `tests/stdlib_eval/` (statement, visible tests, hidden
tests including inputs large enough that a quadratic solution exceeds the time limit) with saved
reference solutions that pass; the isolated agent run is a separate step.

**Acceptance Scenarios**:

1. **Given** at least 8 tasks (for example: a booking calendar with range queries, top-k leaderboard
   updates, shortest paths, a sliding-window maximum, an event simulator, merging sorted streams), **When**
   the reference solutions run, **Then** they pass every visible and hidden test within the time limit.
2. **Given** isolated agent runs with and without the module documented, **When** compared, **Then** the
   results (solved, timed out, code length) are recorded in the spec's results.

---

### Edge Cases

- **Empty structures**: every "smallest", "largest", "first at or above", "pop" and "peek" on an empty
  structure returns a `result` error, never a runtime error.
- **Equality and printing**: two ordered maps with the same entries built in different orders are equal
  under `==`, and print (in `str`, `expect_eq` reports and runtime-error locals) as their contents in key
  order, never as an internal tree shape.
- **Generated test inputs**: property tests with a parameter of a collection type get valid collections
  (built through the public operations), never an unbalanced or unordered internal structure.
- **Invalid internal states cannot be built**: a program cannot construct or modify a collection's
  internals except through its operations.
- **Floats as keys**: ordered by the language's existing `<` on floats; equal values are the same key.
- **Iterating while changing**: the collections are mutable (clarified), so a loop may change the
  collection it iterates; iteration sees the collection as it was when the loop began, and a change
  during iteration never corrupts the structure or raises (the lesson of review 1's C4).
- **Aliasing**: like `list` and `map`, a collection is a reference: passing it to a function or storing
  it in a record shares it; a copy operation gives an independent collection.
- **Very large structures**: bounded by `--max-memory` like every other value (R0006); deep structures
  never exhaust the native stack (operations do not recurse deeper than the logarithmic height).
- **Determinism**: the same operations in the same order give the same results and the same iteration
  order on every run and platform.

## Requirements *(mandatory)*

### Functional Requirements

**The module**

- **FR-001**: The standard library MUST provide a collections module with an ordered map, an ordered set,
  a priority queue and a deque, each generic over its element (and key) types and pure (no effects).
  The structures are built into the toolchain like `list` and `map` (clarified in planning) and reached
  through the module.
- **FR-002**: The ordered map and set MUST keep their keys in ascending order in a balanced binary search
  tree whose height stays within the AVL bound (at most about 1.44 log2(n + 2)), and MUST offer: insert
  (replacing an existing key's value), lookup, membership, remove, size, emptiness, smallest and largest
  key, first key at or above and last key at or below a given key, the keys between two keys in order
  (their values by lookup: the language has no tuple type), and iteration in key order. The set MUST also offer union, intersection and difference.
- **FR-003**: The priority queue MUST offer push, pop of the smallest item, peek, size and emptiness;
  items with equal priority MUST come out in the order they were pushed.
- **FR-004**: The deque MUST offer push and pop at both ends, peek at both ends, access by position,
  size, emptiness and iteration from front to back.
- **FR-005**: Every operation that can find nothing (empty structure, missing key, no key at or above)
  MUST return a `result` (or offer a `get_or`-style default), never stop the program.
- **FR-006**: Lookups, inserts and removals MUST take logarithmic time in the ordered map, set and
  priority queue, and constant amortized time at the ends of the deque.
- **FR-007**: The collections MUST be mutable reference values, like `list` and `map` (clarified):
  operations change the collection in place, sharing follows the language's existing reference rules,
  and each structure MUST offer a copy that gives an independent collection. Iteration MUST see the
  collection as it was when it began.
- **FR-008**: A program MUST NOT be able to construct or change a collection's internal representation
  except through its operations; `==` on two collections MUST compare their contents (in order), and
  every rendering of a collection (`str` where defined, test reports, runtime-error locals, traces) MUST
  show its contents in order, not its internal shape.
- **FR-009**: Property tests MUST be able to take collection-typed parameters, generated through the
  public operations, so every generated value satisfies its invariants, with shrinking.
- **FR-010**: Each operation MUST have a `vo doc` entry with its signature and a checked example; each
  structure MUST have property tests for its invariants (ordering, balance, size, agreement with a list
  model, tie order) run by the quality gate.

**Language changes the module needs**

- **FR-011**: The checker MUST infer the type arguments of `new` on a generic record and of a variant case
  without a value from the expected type (an annotated variable, a return type, a parameter type, a field
  type) or from the field values; when nothing determines them, the existing error (E0310) MUST remain
  with its current message.
- **FR-012**: A record whose fields are all ordered MUST itself be ordered (`T: ordered` accepts it;
  `<`, `<=`, `>`, `>=`, `min`, `max`, `sort` and the new structures compare it field by field in
  declaration order). A record with a field that has no order used where an order is needed MUST be a
  check error naming the field.
- **FR-013**: The ordered map, ordered set and priority queue MUST also accept an optional comparison
  function (pure: no effects) that replaces the built-in order for that collection, so any element type
  can be ordered and any order (descending, by one field) expressed. An inconsistent comparison function
  MUST NOT crash the program or corrupt the structure.
- **FR-014**: The language changes MUST keep every existing program checking and running byte-identically
  (explicit type arguments stay valid), and the reference MUST describe them in at most 300 more tokens
  while staying under 15,000.

**Evaluation**

- **FR-015**: The benchmark MUST gain at least 8 tasks that need one of these structures, each with a
  statement, visible tests, hidden tests that include inputs large enough to separate logarithmic from
  quadratic solutions under the time limit, and a reference solution that passes.
- **FR-016**: The maintainer MUST be able to run the isolated agent protocol (as in spec 007's stdlib
  evaluation) on those tasks with and without the module documented, and record the comparison.

**Quality**

- **FR-017**: Every new error code MUST have a catalog entry and a golden case; the toolchain MUST keep
  passing its full quality gate on x86-64, ARM64 and RISC-V; programs that do not use the module or the
  new inference MUST behave byte-identically to the previous release.

### Key Entities

- **Ordered map**: keys of an ordered type mapped to values, kept in ascending key order; size, smallest
  and largest key, neighbors of any key.
- **Ordered set**: the key part of an ordered map, with set algebra.
- **Priority queue**: items with priorities; the smallest priority first, ties in insertion order.
- **Deque**: a sequence with cheap insertion and removal at both ends and access by position.
- **Ordering**: what makes a type usable as a key or priority: the built-in order of `int`, `float` and
  `str`, extended to composite types (see User Story 5).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Each structure's operations agree with a simple list-based model on 100% of at least 1,000
  generated operation sequences per structure (the property tests), and the ordered map's height never
  exceeds the AVL bound in any of them.
- **SC-002**: 100,000 inserts followed by 100,000 lookups in the ordered map, and 100,000 pushes followed
  by 100,000 pops in the priority queue, each finish in under 5 seconds on the gate machine; doubling the
  size at most about doubles the time (no quadratic behavior), including for keys inserted in sorted
  order.
- **SC-003**: The ordered map and set take at most 5 times as long as the built-in `map` for the same
  inserts and lookups at 100,000 keys (measured and recorded; the built-in map has no order).
- **SC-004**: With the module, the reference solutions of the new benchmark tasks are at least 30%
  shorter (median, in lines) than correct solutions without it (which build the structure they need by
  hand), and both pass every hidden test; a naive solution (sorting or scanning on every step) fails the
  large hidden inputs of every task.
- **SC-005**: In isolated agent runs, agents with the module documented solve at least as many of the new
  tasks as agents without it, and time out on fewer hidden large inputs (recorded per task).
- **SC-006**: The examples from User Story 4 (a generic tree with `new` and empty cases passed as
  arguments) check with no type arguments written; the reference grows by at most 300 tokens and stays
  under 15,000; the full gate passes on all three platforms, and existing programs give byte-identical
  results.

## Assumptions

- **Built in, reached through a standard module (clarified 2026-11-16, in planning)**: the structures
  are runtime types implemented in the toolchain like `list` and `map`, so equality, printing,
  generated test inputs and hidden internals work the same way as for those; the module declares the
  types and the operations that have no existing builtin. Writing them in vo would have needed new
  opaque-type machinery for FR-008 and FR-009. The language features of User Story 4 are still delivered
  and tested with user-written trees.
- **AVL for ordered structures, a binary heap for the priority queue**: named by the user; the spec
  fixes their guarantees (balance bound, logarithmic operations, tie order), not their code.
- **Determinism (constitution Principle III)**: no structure depends on hashing, addresses or randomness;
  iteration order is defined (key order, priority then insertion, front to back).
- **No new effects**: the structures are pure values; nothing here touches the security boundary of
  spec 013.
- **Mutable, like `list` and `map` (clarified 2026-11-16)**: one style across vo; persistent
  structures were considered (safer sharing, a fit for replay and concurrency) and left out to keep one
  API and agents' familiar habits.
- **Both orderings (clarified 2026-11-16)**: records of ordered fields are ordered field by field (a
  language change), and the ordered structures also take an optional comparison function. Variants are
  not ordered by this spec.
- **The built-in `map` and `list` stay as they are**: `map` keeps insertion order and `int`/`str` keys;
  the new structures are additions, not replacements.
- **The evaluation follows the existing isolated protocol** (no tools, empty working directory, no user
  settings), and its results are recorded, not gated: SC-005 is reported as measured.
- **The previous release is the oracle** for "behaves exactly as before".

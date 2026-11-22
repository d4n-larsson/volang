# Feature Specification: Iteration (two-variable `for`, list helpers)

**Feature Branch**: `046-iteration`

**Created**: 2026-11-22

**Status**: Implemented ([results](results.md))

**Input**: User description: "continue with the next thing on the review" (docs/review-2026-11/language.md
item 7, "Iteration": "`for k, v in m { ... }` today: `for k in keys(m) { let v = m[k]; ... }`;
`for i, x in xs { ... }` today: a var counter, or `for i in 0..len(xs)` with `xs[i]`;
`for i in 0..n by 2 { ... }` today: a while loop. Also worth having as builtins or in a `std.list`:
`zip`, `reverse`, `contains`, `index_of`, `first`, `last`, `sum`, `min_of`/`max_of`, `group_by`,
`sort_by` with a descending flag (the README's own example sorts descending by negating the key, which
does not work for strings).")

Background: a `for` loop in vo has one variable. Visiting a map's entries is written
`for k in keys(m) { let v = m[k]; ... }`, and visiting a list with positions is
`for i in 0..len(xs) { let x = xs[i]; ... }`. Both work, and both repeat a lookup the loop already did.
Counted in `std/`, `examples/` and `tests/cases/` today: 17 loops over `keys(m)`, 13 of which read
`m[k]` in the body; 29 loops over `0..len(xs)`, 8 of which begin by reading `xs[i]`. Small list
operations are written by hand where needed (`has_item` in `std.web`, descending order by negating a
sort key, which cannot be done for text).

The same count found no loop that steps by a constant other than 1, so the review's third form
(`for i in 0..n by 2`) is left out: it would add a keyword for a pattern the code does not have.

The reference has 95 tokens left of its 15,000. That is the size of this feature in the language.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Visit a map's keys and values together (Priority: P1)

A developer writes `for name, count in counts { ... }` and gets each key with its value, in the map's
order.

**Why this priority**: it is the more common of the two patterns, and the one where today's form
does a second lookup per entry.

**Independent Test**: a word count printed with the new loop gives the same output as with
`for k in keys(m)` and `m[k]`.

**Acceptance Scenarios**:

1. **Given** a `map[str, int]`, **When** looped with two variables, **Then** the body runs once per
   entry, in insertion order, with the key in the first variable and the value in the second.
2. **Given** an `ordered_map`, **When** looped with two variables, **Then** the entries come in
   ascending key order.
3. **Given** an empty map, **When** looped, **Then** the body does not run.
4. **Given** `for _, v in m` or `for k, _ in m`, **When** checked, **Then** it is accepted and the
   ignored part has no name.
5. **Given** a body that changes the value of a key (`set m[k] = v + 1`), **When** the loop runs,
   **Then** every entry present at the start is visited once, and the loop ends.
6. **Given** a body that removes or adds entries, **When** the loop runs, **Then** the result is
   defined: the entries present when the loop began are visited in their order, except those removed
   before their turn; entries added during the loop are not visited.
7. **Given** `break`, `continue` or `return` in the body, **When** run, **Then** they behave as in
   every other loop.

---

### User Story 2 - Visit a list's elements with their positions (Priority: P1)

A developer writes `for i, line in lines { ... }` and gets each element with its index from 0.

**Why this priority**: numbered output and "compare with the previous element" need the index, and
today that means a counter or an index loop that reads the element first thing.

**Independent Test**: numbering the lines of a text gives the same output as the index loop.

**Acceptance Scenarios**:

1. **Given** a list, **When** looped with two variables, **Then** the first is the index (0, 1, ...)
   and the second the element.
2. **Given** a `deque`, **When** looped with two variables, **Then** the first is the position from
   the front.
3. **Given** a body that pushes to or pops from the list, **When** the loop runs, **Then** the result
   is defined: the body runs once for each position the list had when the loop began, with the
   element that is there at its turn; elements pushed during the loop are not visited, and the loop
   ends early when the list has become too short. (The one-variable `for x in xs` is unchanged: it
   follows the list.)
4. **Given** two variables over a text, a range (`0..n`), an `ordered_set` or a `priority_queue`,
   **When** checked, **Then** it is an error that says what a two-variable loop goes over, with the
   one-variable form as its fix where there is one.
5. **Given** the two names equal (`for x, x in xs`) or a name that is a builtin's, **When** checked,
   **Then** the existing errors for those are reported.

---

### User Story 3 - The old patterns point to the new form (Priority: P2)

A developer (or an agent) who writes the old pattern is told about the loop that replaces it, with a
fix that rewrites it, so that programs converge on one way of writing each loop.

**Why this priority**: vo's rule is one form per construct. A second spelling without a pointer from
the first would leave both in use.

**Independent Test**: applying the fixes to `std/` and `examples/` leaves none of the two patterns,
and every test still passes with the same output.

**Acceptance Scenarios**:

1. **Given** `for k in keys(m) {` whose first statement is `let v = m[k];`, **When** checked, **Then**
   a warning proposes `for k, v in m {` and its fix makes exactly that change.
2. **Given** `for i in 0..len(xs) {` whose first statement is `let x = xs[i];`, **When** checked,
   **Then** a warning proposes `for i, x in xs {` with the fix. The two mean the same (story 2,
   scenario 3), except that the old form stops the program when the body has shortened the list and
   the new one ends the loop.
3. **Given** either loop without that first statement, or with `m`/`xs` anything but a variable that
   cannot be reassigned (a `let` or a parameter), **When** checked, **Then** there is no warning: the
   fix is only offered where the two forms mean the same.
4. **Given** a loop over `keys(m)` of an `ordered_set`, or any loop that does not match exactly,
   **When** checked, **Then** nothing changes for it.

---

### User Story 4 - Common list operations exist once (Priority: P2)

A developer asks whether a list contains a value, where it is, reverses it, sorts it in descending
order, finds its smallest or largest element by a key, sums it, or groups it, without writing the
loop.

**Why this priority**: each is a few lines, and each few lines is written again per program; the
descending sort has no correct form today for text keys.

**Independent Test**: each function agrees with the obvious loop on thousands of random lists.

**Acceptance Scenarios**:

1. **Given** a list and a value, **When** asked whether it is there and where, **Then** the answers
   are a `bool` and the first index (or the "not found" value the plan chooses, consistent with
   `find` on text).
2. **Given** a list, **When** its reverse is asked for, **Then** a new list comes back and the
   original is unchanged.
3. **Given** a list and a key function, **When** sorted in descending order, **Then** the order is by
   key from largest to smallest, elements with equal keys keep their relative order, and it works for
   `int`, `float` and `str` keys.
4. **Given** a list and a key function, **When** the element with the smallest or largest key is
   asked for, **Then** it is the first such element; for an empty list the answer is an error value,
   not a stop.
5. **Given** a list of `int`, **When** summed, **Then** the result is the sum (0 for an empty list;
   overflow stops with R0001 as `+` does).
6. **Given** a list and a key function, **When** grouped, **Then** the result is a map from each key
   to the list of its elements, keys in order of first appearance, elements in list order.

---

### User Story 5 - Tools, documentation, budget (Priority: P2)

Everything that reads vo source understands the new loop, and the reference stays within its budget.

**Acceptance Scenarios**:

1. **Given** a file using the two-variable loop, **When** formatted, queried, renamed (either
   variable), hovered, mutated or measured for coverage, **Then** each tool treats it as it treats the
   one-variable loop.
2. **Given** the editor grammars (TextMate, tree-sitter, Vim) and the site's highlighter, **When**
   checked against every `.vo` file in the repository, **Then** they pass.
3. **Given** the reference, **When** counted, **Then** it is under 15,000 tokens, and it shows the
   two-variable loop instead of `for k in keys(m)` as the way to loop over a map.

### Edge Cases

- A map whose value type is a record: the second variable is the record itself (a reference), as
  `m[k]` gives.
- Nested two-variable loops over the same map.
- A loop variable assigned in the body is the existing error for loop variables.
- An unused first or second variable gets the existing warning for unused names, whose fix is `_`.
- `for _, _ in m` is accepted and runs the body once per entry.
- A very large map (a million entries): the loop costs no more than the `keys` form.
- The loop over a map inside a function with a contract, a property or a test: no difference.
- `group_by` with a key type that cannot be a map key is the existing error for map keys.
- Descending sort of a list with one element or none.

## Requirements *(mandatory)*

### Functional Requirements

**The loop**

- **FR-001**: `for A, B in X { ... }` MUST be accepted where `X` is a `map` or an `ordered_map` (A is
  the key, B the value) or a `list` or a `deque` (A is the index from 0, B the element). `_` MUST be
  accepted for either.
- **FR-002**: Over a map the loop MUST go through the keys the map had when the loop began, in the
  map's order, and visit each key the map has at its turn, with the value stored then. So a key
  removed before its turn is skipped, a key removed and put back before its turn is visited with its
  new value, and a key that was not in the map at the start is not visited. The same program MUST behave the same on every
  run and architecture.
- **FR-003**: Over a list the loop MUST run once for each index the list had when the loop began,
  with the element at that index at its turn, and MUST end when the index is no longer in the list.
  Over a deque it MUST visit the elements the deque had when the loop began, as the one-variable loop
  does. Every two-variable loop thus visits what was there when it began.
- **FR-004**: A two-variable loop over anything else MUST be a check error naming what such a loop
  goes over. No error code is added unless no existing one fits; if one is added it MUST be in the
  catalog with an example and, where a rewrite exists, a fix.
- **FR-005**: The loop over a map MUST NOT take more time per entry or more memory than
  `for k in keys(m)` with `m[k]` takes today. (It may hold the list of keys the loop began with, as
  that form does; a loop that holds nothing is left for when it is measured to matter.)
- **FR-006**: The one-variable forms MUST keep their meaning. No existing program changes behaviour.
  `for i in a..b by n` is not added.

**Converging on one form**

- **FR-007**: A warning with a fix MUST be given for exactly the two patterns of story 3, and only
  when the rewrite cannot change what a program that ran to its end did (the collection is a `let` or
  a parameter; the first statement is the plain read).
- **FR-008**: `std/`, `examples/`, the site and the reference MUST be free of the two patterns after
  this feature, by applying the fix. Golden test cases MAY keep them where they test the old form.

**List helpers**

- **FR-009**: The standard library MUST provide, written in vo unless the plan measures a need
  otherwise: membership and first index of a value in a list; a reversed copy; a stable descending
  sort by a key of `int`, `float` or `str`; the element with the smallest and with the largest key;
  the sum of a list of `int`; grouping by a key into a map. Names and the module are chosen in the
  plan so that they clash with no builtin or existing library function.
- **FR-010**: Each helper MUST be pure, MUST leave its argument unchanged unless its name and
  documentation say it sorts in place, and MUST have a doc comment with examples that run.
- **FR-011**: `zip`, `first` and `last` are not added: vo has no tuples for `zip` to return, and
  `xs[0]` and `xs[len(xs) - 1]` exist.

**Tools and documents**

- **FR-012**: `vo fmt`, `vo query`, `vo edit` (rename), `vo lsp`, `vo test --coverage` and
  `--mutate`, the three editor grammars and the site's highlighter MUST handle the two-variable loop.
- **FR-013**: The reference MUST stay under 15,000 tokens (14,905 now), if necessary by replacing
  text the feature makes obsolete. The guide on the site documents the loop and the helpers; the site
  mentions no development material.
- **FR-014**: The feature MUST work the same on x86-64, ARM64 and RISC-V.

### Key Entities

- **Two-variable loop**: a `for` with two names before `in`, over a map, ordered map, list or deque.
- **Replaced pattern**: a one-variable loop over `keys(m)` or `0..len(xs)` whose first statement
  reads the entry.
- **List helper**: one of the library functions of FR-009.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For 1,000 generated programs that loop over random maps and lists with bodies that
  read, update, add and remove, the two-variable loop prints what the specified meaning says, on the
  three architectures.
- **SC-002**: Looping over a map of a million entries with two variables is at least as fast as the
  `keys` form (within 5%, the noise of the measurement) and completes under the smallest memory limit
  under which the `keys` form completes: both are measured and reported.
- **SC-003**: After the fixes are applied, `std/` and `examples/` contain none of the two replaced
  patterns, and the full test suite passes with no golden output changed except cases that show the
  changed source lines or the new warning.
- **SC-004**: Each list helper agrees with a hand-written loop on 10,000 random lists (including
  empty lists, equal keys and text keys).
- **SC-005**: The grammars and the highlighter pass their checks on every `.vo` file; rename,
  format, coverage and mutation have a test each on a two-variable loop.
- **SC-006**: The reference is under 15,000 tokens; every doc example passes; the full test suite
  passes on x86-64.

## Assumptions

- **Two names before `in` is the whole syntax.** No general destructuring, no tuples (declined in
  spec 033), no third variable.
- **The first variable is the position, the second the thing**, for maps (key, value) and lists
  (index, element) alike, as in Go and Python's `enumerate`.
- **Text is left out**: an index into text could mean bytes or characters, and `for c in s` with a
  counter is clear about which.
- **`by` is left out** for lack of use (Background). A descending or stepping loop stays a `while`.
- **Mutation during a map loop is defined, not an error** (FR-002), because today's `keys` form
  allows it and programs rely on updating values while looping.
- **A warning, not an error, for the old patterns**: existing programs keep working; the fix moves
  them.
- **Helpers are library functions, not builtins**: they cost one line of the reference instead of
  seven. The one change to the toolchain for them is that `sort_by` accepts a key of an `ordered`
  type parameter.
- Out of scope: iterators, generators and lazy sequences, comprehensions, `zip`, loop labels,
  `for` with a step, a two-variable loop over text or ranges, a descending flag on the builtin
  `sort_by` (the builtin only learns to take a key of an `ordered` type parameter, so that the
  library's descending sort can use it).

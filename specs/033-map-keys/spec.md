# Feature Specification: Structural Map Keys

**Feature Branch**: `033-map-keys`

**Created**: 2026-11-20

**Status**: Implemented (gate not run)

**Input**: User description: "Structural map keys: `map[K, V]` accepts any key type built from `int`,
`bool`, `str`, `bytes`, and records and variants whose fields are themselves keys, hashed and compared
structurally (`==` is already structural on every type). Today keys are `int` or `str` only (E0309), so
programs build compound keys by hand (`str(x) + "," + str(y)`), which is slow and a wrong separator is a
real bug class. A generic function that uses its type parameter as a map key needs a constraint like the
existing `T: ordered` (candidate: `T: key`). `float` keys stay out (NaN and -0.0), as do lists, maps and
functions (mutable or without equality). Mutating a record while it is a key is the same documented
hazard `ordered_map` already has. Everything that takes a map keeps working with the new keys: `has`,
`get_or`, `remove`, `keys`, `for`, `len`, the collections in std.collections that take maps, JSON (only
`map[str, V]` is JSON, unchanged), reports and traces print keys in vo syntax, `prop` generates and
shrinks maps with such keys, record/replay unchanged. Tuples are out of scope (the review recommends
records for multiple returns and saying no to a second way). Source: docs/review-2026-11/language.md
section 3, roadmap row "Tuples, more map keys". Must keep: one way to write each thing, diagnostics with
fixes, vo fmt canonical form, vo query/edit, insertion order of maps, map performance for int and str
keys (no regression), the reference under 15,000 tokens (about 14,545 now)."

Background: a vo `map` is keyed by `int` or `str` only. A program that counts moves on a grid, caches a
result per (user, day) or groups orders by (customer, status) has two choices: an `ordered_map`, which
keeps keys sorted it does not need sorted and refuses variants, or a `str` key built by hand,
`str(x) + "," + str(y)`. The second is what programs do. It is slower than it looks (every lookup
formats and allocates), it loses the key's type (`keys(m)` gives strings to parse back), and a separator
that can occur in a field (`"a,b" + "," + "c"` equals `"a" + "," + "b,c"`) silently merges two keys.
`==` is already structural on every type, so the language already knows when two such keys are the same.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A record or variant is a map key (Priority: P1)

A developer keys a map by a record or a variant: `map[Point, int]`, `map[Status, list[Order]]`,
`map[Visit, int]` where `Visit` holds a user id and a day. Lookups find the entry whose key is `==` to the
one given, and `keys(m)` gives the keys back with their type.

**Why this priority**: it is the feature and removes the hand-built key.

**Independent Test**: a program declares `record Point { x: int; y: int; }`, fills a `map[Point, int]`,
reads it with a fresh `new Point { x: 1, y: 2 }`, and prints `keys(m)` in insertion order; it checks and
prints the expected output.

**Acceptance Scenarios**:

1. **Given** `new map[Point, int]` with `Point` a record of `int` fields, **When** `set m[p] = 1;` and
   later `m[q]` with `q` a different record value `==` to `p`, **Then** it reads `1`; `has`, `get_or` and
   `remove` agree.
2. **Given** a variant `Status { open; closed(str); }`, **When** used as a key, **Then** `Status.open` and
   `Status.closed("x")` are distinct keys and two `Status.closed("x")` values are the same key.
3. **Given** a key type made of `int`, `bool`, `str`, `bytes`, records and variants of those, nested to
   any depth, **When** used as `K`, **Then** it checks; a generic record or variant (`Option[int]`) is a
   key when its type arguments are.
4. **Given** a key type that contains `float`, a `list`, a map or collection, a function, or a `result`,
   **When** written as `K`, **Then** it is E0309 naming the part that is not a key, at the type.
5. **Given** a map with record keys, **When** iterated with `for k in keys(m)`, **Then** the order is
   insertion order, and updating an existing key keeps its position, as for `int` and `str` keys.
6. **Given** a record key changed with `set` while it is in a map, **When** looked up, **Then** the
   behavior is the documented hazard (as for `ordered_map` today), never a crash or memory error.

---

### User Story 2 - Generic code keyed by a type parameter (Priority: P2)

A library author writes `fn group_by[T, K: key](xs: list[T], f: fn(T) -> K) -> map[K, list[T]]`, and
callers pass any key type.

**Why this priority**: without it, every generic helper that builds a map is limited to one key type, and
`std.collections` cannot offer one for records.

**Independent Test**: a generic `count[K: key](xs: list[K]) -> map[K, int]` is called with `int`, `str`
and a record; all three check and run; calling it with a `float` list is the constraint error naming the
type.

**Acceptance Scenarios**:

1. **Given** `fn f[K: key](...)` that writes `map[K, V]`, **When** checked, **Then** it checks; the same
   function with a plain `K` is E0309 with the fix that adds `: key`.
2. **Given** a call that infers `K` as a type that is not a key, **When** checked, **Then** it is an error
   at the call naming the type and the part that is not a key.
3. **Given** a record or variant declared with a type parameter used as a map key inside it, **When**
   checked, **Then** the parameter needs `: key` in the same way.

---

### User Story 3 - Everything that shows or tests maps handles the new keys (Priority: P2)

Test failures, runtime reports, traces, `vo query` and property tests show and generate maps with record
and variant keys the same way they show those values elsewhere.

**Why this priority**: a key type the tools cannot print or generate would make the feature a trap.

**Independent Test**: a failing `expect_eq` between two `map[Point, int]` prints both maps in vo syntax
with the first differing key; a `prop` over `map[Point, int]` generates and shrinks.

**Acceptance Scenarios**:

1. **Given** a failing comparison of maps with record keys, **When** reported, **Then** keys print in vo
   syntax (`Point { x: 1, y: 2 }`), as values do today.
2. **Given** R0004 (missing key) with a record key, **When** reported, **Then** the key prints in vo
   syntax.
3. **Given** `prop` over a type containing `map[K, V]` with a record or variant `K`, **When** run,
   **Then** it generates such maps and shrinks them.
4. **Given** `to_json` or `T: json` with `map[Point, V]`, **When** checked, **Then** it is refused as today
   for non-`str` keys (E0328); only `map[str, V]` is JSON.
5. **Given** a recording made before this change, **When** replayed, **Then** it replays identically.

---

### User Story 4 - Programs stop building keys by hand (Priority: P3)

The reference documents the rule in one place, and the examples and standard library that build `str`
keys out of several values use a record key instead.

**Why this priority**: agents learn from the reference and the examples; the old pattern must not be the
one they see.

**Independent Test**: the reference's map row and constraint paragraph state the rule with a runnable
example; searching the examples and the standard library finds no map key built by joining values into a
`str`.

**Acceptance Scenarios**:

1. **Given** the reference, **When** read, **Then** the `map` row says which types are keys and `K: key`
   is described next to `T: ordered`, with a runnable example; the reference stays under 15,000 tokens.
2. **Given** E0309, **When** a program hits it, **Then** the catalog entry has a broken and a fixed
   example and the diagnostic says what a key may contain.

---

### Edge Cases

- A key that holds a record that holds a record: the whole structure is the key; changing any nested
  field while it is a key is the same hazard as changing a top-level one.
- A variant case without fields used as a key: works like any value; value-less cases are shared values
  today and compare equal.
- `bytes` keys: compared byte by byte, as `==` does.
- `bool` keys: two entries at most.
- A record key with a `str` field holding characters a hand-built key would use as a separator: distinct
  keys stay distinct.
- A recursive variant (a tree) as a key: allowed if all its fields are keys; hashing a deep value costs
  time proportional to its size, as `==` does.
- `ordered_map`, `ordered_set` and `priority_queue`: unchanged; they still need `ordered` keys.
- `map[K, V]` with `K` a type parameter declared `ordered`: E0309; `ordered` does not imply `key` because
  `float` is ordered.
- `concurrent_map` returning a map with record keys: allowed when the map can come back from a task
  (no functions, E0314), as for other maps.
- Large maps of record keys: lookups stay constant time on average; no key type makes a map degrade to
  scanning.

## Requirements *(mandatory)*

### Functional Requirements

**The rule**

- **FR-001**: A type MUST be a key when it is `int`, `bool`, `str`, `bytes`, a record whose fields are all
  keys, or a variant whose cases' values are all keys (generic ones when their type arguments are keys);
  nothing else is a key.
- **FR-002**: `map[K, V]` MUST accept every key type and refuse every other `K` with E0309 at the type,
  naming the part that is not a key.
- **FR-003**: Two keys MUST be the same entry exactly when `==` says they are equal.
- **FR-004**: Every existing valid program MUST check, run, format and report byte-identically. The only
  changed expectations are programs that become valid (`bool` and `bytes` keys) and E0309's wording.

**Generic code**

- **FR-005**: A type parameter MUST be usable as a map key only when declared `K: key`; a missing
  constraint MUST be E0309 with a fix that adds it, and a call that infers a non-key MUST be an error at
  the call naming the type.
- **FR-006**: `key` MUST be written and formatted like `ordered`; there MUST be no other way to declare
  it.

**Operations**

- **FR-007**: Indexing, `set m[k]`, `has`, `get_or`, `remove`, `keys` (and so `for k in keys(m)`), `len` and every function of
  `std.collections` that takes a `map` MUST work with every key type, keeping insertion order.
- **FR-008**: Lookups, inserts and removes MUST take constant time on average for every key type.
- **FR-009**: Changing a record while it is a key MUST never crash or corrupt memory; the reference MUST
  document it with the `ordered_map` hazard.

**Tools**

- **FR-010**: Every report that shows values (test failures, runtime errors, traces, `vo query`) MUST
  show keys in vo syntax.
- **FR-011**: Property tests MUST generate and shrink maps with every key type they can generate.
- **FR-012**: JSON MUST be unchanged: only `map[str, V]` is a JSON value.
- **FR-013**: Recordings and replay MUST be unchanged.
- **FR-014**: `vo fmt`, `vo query` and `vo edit` MUST handle `K: key` like `T: ordered`.

**Documentation**

- **FR-015**: The reference MUST state the rule and `K: key` with a runnable example and stay under
  15,000 tokens; E0309's catalog entry MUST be updated.
- **FR-016**: Examples and standard library code that build a map key by joining values into a `str`
  MUST use a record key instead, with unchanged output.

### Key Entities

- **Key type**: a type whose values can identify a map entry: built from `int`, `bool`, `str`, `bytes`,
  records and variants of those.
- **`key` constraint**: a type parameter's promise that it is a key type; like `ordered`, it is checked
  at each call.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every program in the test corpus, the examples and the standard library checks, runs and
  prints exactly as before.
- **SC-002**: Maps keyed by `int` and by `str` are not measurably slower: insert, lookup and remove
  benchmarks within 5% of today.
- **SC-003**: A map keyed by a two-field record is at least as fast as the same map keyed by a `str`
  built from the two fields, measured on insert plus lookup of 1,000,000 entries.
- **SC-004**: No example or standard library module builds a map key by joining values into a `str`.
- **SC-005**: Every key-type mistake is reported at check time, with the part that is not a key named.
- **SC-006**: The reference stays under 15,000 tokens with the new rule, and all its examples run.

## Assumptions

- `float` is not a key: NaN is not equal to itself and `-0.0 == 0.0`; a program that needs one can key by
  `str(x)` or by a scaled `int`.
- `result` is not a key: nothing in the examples needs one, and keeping it out keeps the rule short.
  A variant written by the program covers the case.
- Lists, maps, the other collections and functions are not keys: collections change in place and
  functions have no `==`.
- `ordered` does not imply `key` (`float` is ordered). A type parameter takes one constraint, as today;
  no code in the examples or the standard library needs a parameter that is both, so combining
  constraints is left for when one does.
- `ordered_map`, `ordered_set` and `priority_queue` keep their `ordered` rule; this spec does not widen
  it to variants.
- Tuples are out of scope: records cover compound keys and multiple returns, and a tuple would be a
  second way to write a record.
- A changed record key is not detected; the documented hazard matches `ordered_map`, and copying keys on
  insert would make every insert slower and change what `keys(m)` returns.
- E0309 keeps its code and name with a wider message; `key` constraint errors reuse it so there is one
  code for "not a key".

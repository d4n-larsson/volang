# Feature Specification: Readable Signatures and Named Effect Sets

**Feature Branch**: `030-readable-signatures`

**Created**: 2026-11-20

**Status**: Implemented (ARM64 and RISC-V runs pending)

**Input**: User description: "Readable function signatures and effect clauses. Long signatures are hard to
read because the effect clause (`uses file.read("data/"), file.write("data/")`) sits between the return
type and the body, and API code repeats the same long scoped effect lists on every operation. Two parts,
keeping one syntactic form for effects everywhere (declarations, function types, type effect arguments):
(1) vo fmt wraps function signatures that are over 100 characters: the effect clause moves to its own
indented line, and when the parameters are still too long they go one per line with `) -> Type uses ... {`
after them; also for function types in long parameter lists and anonymous functions. (2) Named effect
sets: a top-level declaration such as `effects Storage = file.read("data/"), file.write("data/");` that
`uses Storage` (and other places effects are written) can refer to, combinable with other effects
(`uses console, Storage`), exportable and importable like other items, expanded everywhere the toolchain
reports or records effects (vo.effects, E0501/E0503 messages and fixes, vo query, vo doc) so reviewing
stays exact; with checks for unknown or cyclic names, unused sets, and sets whose scopes are invalid.
Replace the roadmap's 'Effects as an annotation' row (rejected: two spellings for one concept) with this.
Measure: no line over 100 characters in the examples' signatures after vo fmt; the notes-api and an API
with 10 operations declare their storage effects once; agents' first-try results on the effect-related
benchmark tasks do not get worse."

Background: a function declares what it may touch with a `uses` clause after its return type, and the
same clause appears in function types (`fn(Request) -> Response uses E`) and in the effect arguments of
types (`Database[uses file.read, file.write]`). The clause is one form everywhere, which agents learn once.
Two things make it hard to read in practice: since the formatter only breaks long statements, a long
signature stays one line (`fn handle(db: Database[uses file.read, file.write], r: Request) -> Response
uses file.read("data/"), file.write("data/") {` is 120 characters); and an API, a service or a tool
repeats the same scoped effects on every function that touches its storage, so a change of directory is an
edit in twenty places and a reader compares long lists to see that they are the same. Moving the clause
into an annotation was considered and rejected: types keep `uses`, so the same concept would have two
spellings.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Long signatures are laid out by the formatter (Priority: P1)

A developer (or agent) writes a function whose signature is longer than a line. `vo fmt` lays it out
readably and the same way every time: first the effect clause on its own indented line; if the line is
still too long, the parameters one per line.

**Why this priority**: it fixes the readability complaint for every program at no cost to the language:
no new syntax, nothing to learn.

**Independent Test**: format a file with a 120-character signature, a signature with long parameters,
a long function type parameter and a long anonymous function: every line is at most 100 characters (where
any layout can make it so), the result formats to itself, and the program behaves the same.

**Acceptance Scenarios**:

1. **Given** a function whose signature line is over 100 characters only because of its effect clause,
   **When** formatted, **Then** the clause moves to the next line, indented one level, followed by ` {`, and
   the body keeps its indentation.
2. **Given** a signature still over 100 characters without its effect clause, **When** formatted, **Then**
   each parameter goes on its own line, one level deeper, and `) -> Type uses ... {` follows on its own
   line (the effect clause on yet another line if that line is still too long).
3. **Given** a signature of at most 100 characters, **When** formatted, **Then** it stays on one line,
   exactly as today.
4. **Given** a parameter whose type is a long function type, or an anonymous function whose header is too
   long, **When** formatted, **Then** the parameter goes on its own line by the second rule, and the
   anonymous function's parameters follow it too; no type is broken inside.
5. **Given** any formatted file, **When** formatted again, **Then** nothing changes, and every program
   behaves as before formatting.

---

### User Story 2 - Effects a program repeats are named once (Priority: P1)

A developer declares a named set of effects once (`effects Storage = file.read("data/"),
file.write("data/");`) and uses the name wherever effects are written: `uses Storage`, `uses console,
Storage`, in function types and in effect arguments of types. Changing the directory is one edit.

**Why this priority**: it removes the repetition that makes clauses long in the first place, and makes
"these functions touch the storage" visible as a name.

**Independent Test**: the notes-api declares its storage effects once and every function that touches the
database names the set; it checks, runs, passes its tests, and its effects record lists the same effects
for every function as before.

**Acceptance Scenarios**:

1. **Given** `effects Storage = file.read("data/"), file.write("data/");`, **When** a function declares
   `uses Storage`, **Then** it may do exactly what `uses file.read("data/"), file.write("data/")` allows,
   no more and no less.
2. **Given** a set, **When** combined with other effects or sets (`uses console, Storage, Network`),
   **Then** the clause means their union.
3. **Given** a set that includes another set (`effects App = console, Storage;`), **When** used, **Then**
   it means the union of both; a set that includes itself, directly or through others, is an error naming
   the cycle.
4. **Given** a set used in a function type (`fn(Request) -> Response uses Storage`) or an effect argument
   of a type (`Database[uses Storage]`), **When** checked, **Then** it means the same effects; in a type,
   where effects carry no scopes, the set's scopes are dropped without an error.
5. **Given** a set declared with `export` in one file, **When** another file imports it like any other item,
   **Then** it can use the name; an unexported set is private to its file.
6. **Given** an unknown name in an effect clause, **When** checked, **Then** the error says it is neither
   an effect nor a set in scope and offers the nearest known name as a fix.
7. **Given** a set not exported and never used, **When** checked, **Then** there is a warning (as for
   unused imports).
8. **Given** a set whose effect list is invalid (an unknown effect, a scope that is empty, absolute or uses
   `..`, a missing scope for `process`), **When** checked, **Then** the same errors as for a `uses` clause
   are reported at the set's declaration, once.

---

### User Story 3 - Reviewing stays exact (Priority: P1)

A reviewer (or agent) reading a change sees what each function may touch. Everywhere the toolchain reports
or records effects, it shows the expanded effects, so a named set never hides a capability: the effects
record (`vo.effects`), diagnostics about missing or too narrow effects and their fixes, `vo effects`,
`vo query` and `vo doc`.

**Why this priority**: effects are vo's security story; a name must not make a new file write invisible.

**Independent Test**: changing a set's definition from `file.read("data/")` to `file.read("data/"),
file.write("data/")` makes `vo check` report every function using the set as gaining `file.write` (E0504),
and `vo effects --update` records each of them.

**Acceptance Scenarios**:

1. **Given** a function declaring `uses Storage`, **When** the effects record is written, **Then** it lists
   the expanded effects (`file.read("data/"), file.write("data/")`), not the name.
2. **Given** a function missing an effect it needs, **When** checked, **Then** the message and its fix name
   effects as today; a fix that adds an effect to a clause that names a set adds it to the clause in
   canonical order (`uses console, Storage`), never inside the set's declaration.
3. **Given** `vo doc` or `vo query` showing a function's signature, **When** the function uses a set,
   **Then** the signature shows the name as written and the expanded effects next to it.
4. **Given** a set's definition gains an effect, **When** checked, **Then** every function using it is
   reported as gaining the effect (E0504), exactly as if its clause had been edited.

---

### User Story 4 - The examples and the guide use them (Priority: P2)

The examples whose functions repeat effects use a named set, and every example's signatures are laid out by
the formatter; the guide's chapter on effects explains named sets and when to use them (repetition of the
same scoped effects), and the reference documents both changes.

**Why this priority**: how agents learn the form.

**Independent Test**: after `vo fmt`, no signature line in the examples is over 100 characters; the
notes-api and a 10-operation API declare their storage effects once.

**Acceptance Scenarios**:

1. **Given** the examples, **When** formatted, **Then** no line of a signature is over 100 characters.
2. **Given** the guide and reference, **When** read, **Then** they show the layout rule and named sets with
   checked examples.

### Edge Cases

- A set with the same name as an effect (`effects console = ...`) or as another item: an error (names of
  sets are capitalized like effect parameters, so they cannot be effect names).
- A set with no effects (`effects Nothing = ;`): an error; a pure function declares no clause.
- An effect parameter named like a set in scope: an error (E0203, as for any name already defined in an
  outer scope); vo has no shadowing.
- A set used inside a `requires`/`ensures` or a test's clause: tests declare effects the same way, so sets
  work there too.
- `vo edit rename` of a set renames every use, as for other items.
- A signature that cannot fit even one parameter per line (a single very long type): it stays long; the rule
  never breaks inside a type.
- Comments between parameters: kept, each on its own line.
- A clause naming both a set and an effect the set already contains: allowed (the union), with the existing
  warning for a redundant effect if there is one.

## Requirements *(mandatory)*

### Functional Requirements

**Signature layout**

- **FR-001**: The formatter MUST keep every function, test and anonymous-function header of at most 100
  characters on one line, unchanged.
- **FR-002**: For a longer header, the formatter MUST first move the effect clause to its own line, indented
  one level deeper than the header, ending with ` {`.
- **FR-003**: If the header without its effect clause is still over 100 characters, the formatter MUST put
  each parameter on its own line one level deeper and close with `) -> Type` (and the effect clause by the
  rule of FR-002) on the following line.
- **FR-004**: Function types MUST never be broken: a parameter whose type is long goes on its own line by
  FR-003; record fields and return types are not wrapped (a line that stays long because of one type is
  allowed, SC-001).
- **FR-005**: Formatting MUST be idempotent and MUST NOT change any program's behavior.

**Named effect sets**

- **FR-006**: A program MUST be able to declare `effects Name = effect, effect, ...;` at the top level of a
  file, with the effects written exactly as in a `uses` clause (scopes included) and other set names.
- **FR-007**: A set name MUST be usable wherever an effect is written: function `uses` clauses, test clauses,
  function types, and effect arguments of types; it means the union of its effects.
- **FR-008**: In types, where effects carry no scopes, a set's scopes MUST be dropped silently (the type
  means the set's effects in any scope).
- **FR-009**: Sets MUST be exportable and importable like other items (`export effects Storage = ...;`,
  `import notes.Storage;`).
- **FR-010**: Unknown names, cycles, empty sets, names that clash with effects or items, and invalid effect
  lists MUST be errors with their position and, where one correction is unambiguous, a fix; an unused set
  that is not exported MUST be a warning.
- **FR-011**: Everywhere the toolchain reports or records effects — the effects record, missing/narrow
  effect diagnostics and their fixes, `vo effects`, `vo query`, `vo doc` — the expanded effects MUST be
  shown; a set name MAY be shown in addition, never instead.
- **FR-012**: A change to a set's definition MUST be reported for every function using it as if each clause
  had changed (gained or lost effects).
- **FR-013**: Fixes that add effects MUST add them to the clause being fixed, next to any set name, never
  to a set's declaration.

**Documentation and examples**

- **FR-014**: The reference, the error catalog, the guide's effects chapter and `vo explain` MUST document
  the layout rule and named sets within the reference's token budget.
- **FR-015**: The examples MUST follow the layout rule, and those that repeat the same scoped effects in
  three or more functions MUST use a set.
- **FR-016**: The roadmap's "Effects as an annotation" row MUST be replaced by this spec, with the reason it
  was rejected.

### Key Entities

- **Effect set**: a named, top-level declaration of effects (with scopes), possibly including other sets;
  exported or private; expanded to a plain set of effects wherever effects are compared, reported or
  recorded.
- **Signature layout**: the formatter's rule for headers over 100 characters: effect clause on its own
  line, then parameters one per line.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After formatting, no signature line in the examples, the standard library or the site's
  examples is over 100 characters, except where a single type alone exceeds it.
- **SC-002**: The notes-api and a 10-operation API example declare their storage effects once each; changing
  the storage directory is a one-line edit, and the effects record then shows the change for every affected
  function.
- **SC-003**: For every program in the test corpus and examples, formatting is idempotent and behavior is
  unchanged; every effects record is identical before and after replacing repeated clauses with sets.
- **SC-004**: Agents' first-try pass rate on the existing benchmark tasks that involve effects does not drop
  (measured on the effect-related tasks of benchmark v2, before and after).
- **SC-005**: Existing programs check and behave exactly as before; programs that do not use sets see no
  difference except the formatter's layout of long signatures.

## Assumptions

- Set names are capitalized (like effect parameters and types), so they never collide with effect names,
  which are lower case.
- A set cannot take parameters and cannot name an effect parameter (it is declared at the top level, where
  no parameter is in scope); generic code keeps using effect parameters.
- The effects record keeps listing expanded effects, so records written before this spec stay valid.
- The layout rule extends the 100-column rule `vo fmt` already applies to statements; the same limit and
  counting (characters, not bytes) apply.
- The agent benchmark of SC-004 reuses benchmark v2's effect-related tasks and protocol (isolated agents);
  its run may be scheduled separately, as spec 029's was.

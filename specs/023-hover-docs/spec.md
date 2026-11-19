# Feature Specification: Documentation on hover

**Feature Branch**: `023-hover-docs`

**Created**: 2026-11-19

**Status**: Implemented

**Input**: User description: "Hover documentation for editors: `vo query type-at --json` gains an optional
`doc` object (signature, description, example, effects) when the name at the position refers to a
documented item — user functions, records, variants and cases, std and package items, and builtins —
from the existing doc-comment convention (# lines above an item; `#     EXPR is VALUE` example). Same
JSON schema version (field added). The human output shows the signature and description. Locals and
parameters keep today's answer. The doc syntax is not extended. Optionally `vo doc ENTRY NAME` for user
items. Motivation: the IntelliJ plugin uses vo query and wants Java-like hover docs."

Background: the IntelliJ plugin for vo answers "what is this?" with `vo query type-at`, which today gives
the type of the name or expression at a position and where it is declared. Hovering a call such as
`dist2(p, q)` shows `float` (the call's result) and `declared at geo.vo:9:11`; it shows neither the
function's signature nor what it does. In Java, hovering a method shows its signature and its Javadoc.

vo already has a documentation convention: the comment lines directly above an item describe it, and a
line `#     EXPR is VALUE` gives a checked example. The standard library uses it, and `vo doc` prints it
for builtins, `std` and packages, but not for the program's own items (`vo doc geo` is T0010), and nothing
returns it for a position in a file.

What this spec must keep:
- **Principle II**: the machine-readable answer changes only by an added field, within schema version 1.
- **Principle V**: no new doc syntax (no tags, no markup), and the reference grows by a sentence or two.
- **One source**: the documentation an editor shows is the same that `vo doc` prints.

## User Scenarios & Testing *(mandatory)*

The users: people reading vo code in an editor (through the IntelliJ plugin, or any editor that calls
`vo query`), and AI coding agents, which use the same queries to understand code they did not write.

### User Story 1 - Hover a call to see what it does (Priority: P1)

A developer hovers `dist2` in `print(str(dist2(p, q)));`. The editor shows the function's signature
`fn dist2(a: Point, b: Point) -> float`, its description "The squared distance between a and b; cheaper
than the distance.", its example, and its effects.

**Why this priority**: this is the request; it is what Java's hover gives, and the most common thing a
reader wants to know about a call.

**Independent Test**: golden cases running `vo query type-at --json` at calls of documented functions in
another file, the same file, `std`, a package and a builtin, comparing the `doc` object.

**Acceptance Scenarios**:

1. **Given** a function in the program with a comment directly above it, **When** `type-at` is asked at
   a call of it (or at its name in its declaration, or in an import), **Then** the answer has a `doc`
   object with its signature, description, example (when the comment has one) and effects.
2. **Given** `sqrt` imported from `std.math`, **When** `type-at` is asked at a call of it, **Then** the
   `doc` object is the one `vo doc --json std.math.sqrt` prints.
3. **Given** a builtin such as `len`, **When** `type-at` is asked at a call of it, **Then** the `doc`
   object is the one `vo doc --json len` prints.
4. **Given** a function with no comment above it, **When** `type-at` is asked at a call of it, **Then**
   the `doc` object still has the signature and effects, and no description.

---

### User Story 2 - Hover a type, a field or a case (Priority: P2)

A developer hovers `Point` in `new Point { x: 1.0, y: 2.0 }`, the field `x` in `p.x`, or the case
`circle` in `Shape.circle(1.0)`. The editor shows the record's or variant's declaration and comment, the
field's or case's type and comment.

**Why this priority**: types and their parts are the second most hovered names.

**Independent Test**: golden cases at a record name, a variant name, a field use and a case use.

**Acceptance Scenarios**:

1. **Given** a documented record, **When** `type-at` is asked at its name in a type or a `new`
   expression, **Then** the `doc` object has the record's signature (its fields) and description.
2. **Given** a field with a comment directly above it in the record, **When** `type-at` is asked at a use
   of the field, **Then** the `doc` object has the field's signature (`x: float`), its description, and
   names the record it belongs to.
3. **Given** a variant case, **When** `type-at` is asked at a use of it, **Then** the `doc` object has the
   case's signature and the comment above it, if any.

---

### User Story 3 - The same in the terminal (Priority: P2)

An agent or a person without an editor runs `vo query type-at main.vo main.vo:7:20` and sees the
signature and the description under today's line; `vo doc main.vo dist2` prints the program's own
item as `vo doc` prints a library item.

**Why this priority**: the human output and `vo doc` keep the CLI and the editor in step, and give agents
the same answer without JSON.

**Independent Test**: golden cases of the human output and of `vo doc ENTRY NAME`.

**Acceptance Scenarios**:

1. **Given** the call of a documented function, **When** `type-at` runs without `--json`, **Then** it
   prints today's line, then the signature and the description, indented.
2. **Given** a program and the name of one of its documented items, **When** `vo doc ENTRY NAME` runs,
   **Then** it prints the item as `vo doc` prints library items (signature, description, example), and
   `--json` gives the same record as the `doc` object.
3. **Given** a name the program does not declare, **When** `vo doc ENTRY NAME` runs, **Then** it is T0010
   with a suggestion, as for library names.

---

### User Story 4 - Locals and parameters stay as they are (Priority: P3)

A developer hovers a local variable or a parameter: the editor shows its type and declaration, as today.

**Why this priority**: it must not get worse; locals have no doc comments.

**Independent Test**: the existing `type-at` cases are unchanged; new cases at a local and a parameter
show no `doc` object.

**Acceptance Scenarios**:

1. **Given** a local or a parameter, **When** `type-at` is asked at it, **Then** the answer is exactly
   today's, with no `doc` object.

### Edge Cases

- **A comment that is not directly above**: a blank line between the comment and the item means the
  comment does not document it (as `vo doc` treats `std`).
- **Several comment lines**: the description is the comment's lines joined, as `vo doc` does; the
  example line is not part of the description.
- **Generic functions**: the signature shows the type parameters as declared (`fn first[T](xs: list[T])
  -> result[T]`), not the types at the call.
- **A call through a function value** (`f(x)` where `f` is a parameter): a parameter, so no `doc`.
- **Imported items**: the doc comes from the file that declares the item; `export` is shown in the
  signature only as `vo doc` shows it.
- **A program with errors**: `type-at` answers where it can today; the `doc` object needs only the
  declaration, not a clean check.
- **Positions on keywords, literals or operators**: unchanged (no name, no `doc`).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `vo query type-at --json` MUST add a `doc` object to its answer when the name at the position
  refers to a function, record, variant, variant case, record field, standard-library or package item, or
  builtin.
- **FR-002**: The `doc` object MUST be the same record `vo doc --json` gives for that item (the fields
  `item`, `module`, `name`, `signature`, `description`, `example`, `effects` and the item's own fields),
  so a client parses one format; fields with nothing to say are left out, as in `vo doc`.
- **FR-003**: For the program's own items, the description and example MUST come from the comment lines
  directly above the item, read by the same rules `vo doc` uses for `std`; an item with no such comment
  gets a `doc` object without `description`.
- **FR-004**: Record fields and variant cases MUST get a `doc` object with their signature and the comment
  directly above them in the declaration, and the name of their record or variant.
- **FR-005**: Locals, parameters, keywords, literals and operators MUST get exactly today's answer.
- **FR-006**: Without `--json`, `type-at` MUST print today's line followed by the signature and the
  description (when there is one), each on its own indented line.
- **FR-007**: `vo doc ENTRY NAME` MUST document an item of the program (functions, records, variants) the
  way it documents library items, text and `--json`; an unknown name is T0010 with a suggestion.
- **FR-008**: The JSON schema version MUST stay 1; the change only adds fields.
- **FR-009**: The reference MUST state the doc-comment convention for programs (comment lines directly
  above an item; an example line `#     EXPR is VALUE`) and that `type-at` and `vo doc ENTRY NAME` return
  it, within a few sentences; the doc syntax itself does not change.

### Key Entities

- **Doc record**: the record `vo doc --json` prints for an item: item kind, module (or file), name,
  signature, description, example, effects, and kind-specific fields (parameters, fields, cases).
- **Doc comment**: the comment lines directly above an item; the description, and an optional example
  line.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every documented function, record, variant, case, field, `std` item and builtin used in
  the golden programs, hovering any use of it returns the same documentation `vo doc` prints for it.
- **SC-002**: Every existing `type-at` golden case keeps its output except for the added `doc` objects.
- **SC-003**: Answering `type-at` with documentation on a 2,000-line program takes under 100 ms, so an
  editor can ask on every hover.
- **SC-004**: The reference grows by at most 80 tokens.

## Assumptions

- The IntelliJ plugin renders the `doc` object itself (signature as code, description as text, example as
  code); this spec only provides the data.
- Doc comments are plain text: no markdown, tags or parameter sections. A richer syntax would cost
  reference tokens on every agent task and is out of scope.
- Comments above record fields and variant cases are allowed today (comments may stand on their own
  line anywhere); they are just not shown anywhere yet.
- `vo doc ENTRY NAME` covers top-level items only; fields and cases are reached through `type-at`.
- Language-server support (completion, a standing server) stays a separate roadmap item.

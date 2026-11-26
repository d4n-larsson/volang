# Feature Specification: Variant Cases With Fields, and `match` on Values

**Feature Branch**: `060-case-fields-value-match`

**Created**: 2026-11-25

**Status**: Draft

**Input**: User description: a variant case carries at most one value, so every case with several values
needs a record that exists only to hold them (`Shape.rect(new Size { ... })`). Allow named fields in a
case, and `match` on `int` and `str` with a required `else` arm, which replaces `if`/`else if` chains
where a forgotten value is silent.

Background: the November review (`docs/review-2026-11/language.md`, item 10). Errors, events, commands
and messages are variants in vo, and most of their cases carry more than one value: a typed error with a
path and a reason, a worker message with an id and a body. Each needs a payload record, declared
elsewhere, exported if the variant is, named, and built with `new`. The reference's own variant example
declares `Size` for that reason only. Separately, `match` works on variants and `result` only, so
dispatch on a status code, a command name or a header value is an `if`/`else if` chain: the checker
cannot see a repeated value, and the chain has no visible "everything else" arm.

What this spec must keep:
- **Principle I, one form:** a case with one value keeps `circle(float)`; a case with two or more values
  uses fields; a case with no value is unchanged. Each situation has one form, decided by the count.
  `match` on a value has one shape: literal arms and one `else`.
- **Exhaustiveness:** variant matches stay exhaustive with no default arm; value matches always end in
  `else`.
- **JSON, equality, order, keys** keep their meaning: a case with fields behaves as the case carrying a
  record of those fields does today, so moving from a payload record to fields changes no wire format.
- **Principle V:** the reference stays within 15,000 tokens.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A case with several values (Priority: P1)

An agent declares the ways a load can fail:

```text
variant LoadError {
    missing(str);
    malformed { path: str; line: int; reason: str; }
    empty;
}
```

builds `LoadError.malformed { path: p, line: 3, reason: "bad number" }`, and handles it with
`case malformed(m) { print(format("{}:{}: {}", m.path, m.line, m.reason)); }`.

**Why this priority**: typed errors (spec 032) made variants the normal way to report failure, and almost
every non-trivial error case carries two or more values.

**Independent Test**: the reference's `Shape` example without the `Size` record, and the std error
variants that carry payload records, rewritten with fields, check, run and give identical output and
identical JSON.

**Acceptance Scenarios**:

1. **Given** the variant above, **When** a program builds a `malformed` value with every field, **Then**
   it checks; a missing field is E0305 and an unknown field E0304, as for records.
2. **Given** `case malformed(m)`, **When** the arm reads `m.line`, **Then** it is an `int`; `set m.line
   = 4;` is an error (case fields are read-only).
3. **Given** two `malformed` values with equal fields, **When** compared with `==`, used as map keys,
   sorted, or written with `to_json`, **Then** they behave as two cases carrying equal records do today.
4. **Given** a case declared with one field (`missing { path: str; }`), **When** checked, **Then** a
   diagnostic's fix rewrites it to `missing(str)` and its uses accordingly.

### User Story 2 - `match` on an `int` or `str` (Priority: P1)

An agent maps a status code:

```text
match status {
    case 200 { return "ok"; }
    case 404 { return "not found"; }
    case max_status { return "limit"; }
    else { return "other"; }
}
```

**Why this priority**: dispatch on values is in every service (status codes, commands, content types,
header values); a repeated value or a forgotten default in an `if` chain is a bug the checker cannot see.

**Independent Test**: a program matching on `int` and on `str`, with literal and constant arms, gives the
same output as the `if` chain it replaces; golden cases cover each new diagnostic.

**Acceptance Scenarios**:

1. **Given** a `match` on an `int` with no `else`, **When** checked, **Then** a diagnostic's fix inserts
   an empty `else { }` arm, as E0401's fix inserts missing cases.
2. **Given** two arms with the same value (`case 200` twice, or a constant equal to a literal arm),
   **When** checked, **Then** E0402 reports the repeat.
3. **Given** an arm whose value has another type (`case "200"` on an `int`), **When** checked,
   **Then** E0301 reports it.
4. **Given** a `match` on a `float`, `bool`, `bytes`, list, map or record, **When** checked, **Then** an
   error says which types `match` accepts.

### User Story 3 - Old chains point to `match` (dropped)

Dropped on 2026-11-26 (plan research R8): no `if`/`else if` chain in `std/` or `examples/` compares one
local or parameter with three or more values, so the warning would rewrite nothing; the value `match` is
the one form for new code.

### Edge Cases

- A case with fields of a generic variant (`node { left: Tree[T]; value: T; right: Tree[T]; }`): type
  arguments are inferred from the fields and the context, as for a generic record today.
- Annotations on case fields (`@size`, `@range`, `@pattern`, `@default`) limit `from_json` and std.web
  input as on record fields.
- Binding a case with fields: `case malformed(m)` binds the whole set of fields; `case malformed(_)`
  ignores it. No destructuring form (`case malformed { path, line }`) is added.
- Passing the bound value on: its type is written `LoadError.malformed` (for a generic variant,
  `Tree[int].node`) and may appear wherever a type may; it cannot be built with `new`, only through the
  case.
- A record-payload case today (`rect(Size)`) remains valid: it carries one value. Migrating it to
  fields is a refactoring the program's owner chooses, not a required change.
- `match` on a `str` compares bytes, as `==` does; no case folding.
- `match` on an `int` with arms `-1`, `0x10` and constants: all literal forms and constants of the type
  are accepted; ranges and several values per arm are not.
- `return` in every arm including `else` ends the path, so a function returning from such a `match`
  needs no `return` after it, as with an exhaustive variant match.
- Coverage and mutation testing treat each value arm and the `else` arm as branches.

## Requirements *(mandatory)*

### Functional Requirements

**Case fields**
- **FR-001**: A variant case may declare two or more fields in braces, each `name: type;` with optional
  annotations, instead of one value in parentheses.
- **FR-002**: A case with fields is built `Variant.case { field: value, ... }`, every field exactly once
  in any order (E0305 for a missing one, the record codes for unknown or repeated fields); no `new`.
- **FR-003**: `case name(b)` binds the fields as one read-only value `b`; `b.field` reads a field;
  assigning to a field is an error with a stable code.
- **FR-004**: The fields of a case form a type, written `Variant.case`, usable anywhere a type is. It is
  equal, ordered, a map key and JSON-convertible exactly when a record with the same fields would be.
- **FR-005**: `to_json` and `from_json` encode a case with fields exactly as the same case carrying a
  record with those fields; `vo` prints it in its construction syntax (`LoadError.malformed { path: "a",
  line: 3, reason: "x" }`; amended by plan research R4: the record's name no longer exists). A golden case proves both for every std
  error variant that is migrated.
- **FR-006**: A case declared with exactly one field is a diagnostic with a fix that rewrites it to the
  one-value form and its constructions and bindings accordingly.
- **FR-007**: Generated test inputs (`prop`), equality, ordering, hashing and printing cover cases with
  fields.

**`match` on values**
- **FR-008**: `match` accepts an `int` or `str` scrutinee. Each arm is `case V { ... }` where `V` is a
  literal or a constant of the scrutinee's type, and the match ends with exactly one `else { ... }` arm.
- **FR-009**: A value match without `else` is a new diagnostic with a fix inserting an empty `else`; an
  `else` on a variant or `result` match is an error with a fix removing it (those stay exhaustive with
  no default arm).
- **FR-010**: Two arms with equal values are E0402. An arm of the wrong type is E0301. A scrutinee of
  another type is a new diagnostic naming the accepted types.
- **FR-011**: The reference's sentence "`match` works on variant and `result` values only" is replaced
  by the value form; the statement table shows both shapes.

**Chains and migration**
- **FR-012**: Dropped with User Story 3 (plan research R8).
- **FR-013**: The std library and examples are reviewed: payload records whose only use is one case
  become fields where the JSON and the exported API are unchanged.
  The changelog lists every exported type that disappears this way.

**Tools and docs**
- **FR-014**: `vo fmt`, the editor grammars, `vo query` (TARGET `Variant.case.field`), `vo edit rename`
  and `add-field` (on a case with fields), hover and `vo doc` support case fields.
- **FR-015**: The reference's variant example drops `Size`; its size before and after is stated and stays
  within 15,000 tokens.

### Key Entities

- **Case with fields**: a variant case carrying two or more named, read-only values; its fields form the
  type `Variant.case`.
- **Value match**: a `match` on an `int` or `str` with literal or constant arms and a required `else`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In `std/` and `examples/`, at least 80% of the records used only as the payload of a single
  case are removed, with byte-identical JSON output and test results.
- **SC-002**: Dropped with User Story 3; every migrated program gives identical output on x86-64, ARM64
  and RISC-V (as SC-001 requires).
- **SC-003**: Every new diagnostic has a golden case and a catalog entry with a broken and a fixed
  example; each fix's result checks clean.
- **SC-004**: Property tests over a variant with field cases generate, shrink and print those cases,
  deterministically for a `--seed`.
- **SC-005**: The reference stays at or under 15,000 tokens, with the change in size stated.

## Assumptions

- No existing program breaks: one-value cases, record payloads and variant matches keep their meaning.
  Only FR-006 and FR-009's second half reject something, and neither is valid code today.
- Case fields are read-only because variant values are values; a mutable payload stays a record.
- `match` on `float` is excluded on purpose (equality on floats is a trap); `bool` is excluded because
  `if` covers it in one form.
- Lands as a minor version on its own; it does not depend on specs 058 or 059.

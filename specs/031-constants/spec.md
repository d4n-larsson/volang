# Feature Specification: Constants

**Feature Branch**: `031-constants`

**Created**: 2026-11-20

**Status**: Implemented (full suite, three platforms and gate not run)

**Input**: User description: "Constants: named compile-time values declared at the top level of a file.
`const max_items = 100;` and `export const default_port = "8080";` (optionally with a type: `const rate:
float = 0.25;`). A constant's value is a literal or a pure expression over literals and other constants,
evaluated by the checker; it has no effects and cannot be changed. Constants can be used wherever an
expression of their type can, are imported and exported like other items, and can be used where vo today
requires a literal that the checker reads: effect scopes (`uses file.write(out_dir)`), field annotations
(`@size(1, max_items)`), and `format` templates are candidates to decide. Today vo has no global variables
or constants, so limits, table sizes, default paths and messages are repeated literals or zero-argument
functions, and a scope and the path used under it are two string literals that must agree. Source:
docs/review-2026-11/language.md section 2. Must keep: one way to write each thing, every statement and
item starting with a keyword, diagnostics with fixes, vo fmt canonical form, vo query/edit (rename, refs,
type-at) working on constants, the reference under 15,000 tokens (currently about 14,017)."

Background: a vo file holds functions, records, variants, effect sets and tests, and nothing else: "there
are no global variables or constants". No globals is deliberate (a function's result depends only on its
arguments and its effects). No constants is a gap. A limit, a default port, a directory or a message that
a program uses in several places is written as the same literal each time, or as a function with no
parameters that returns it. Both hide that the places must agree, and neither can be used where the
checker itself reads a literal: the directory in `uses file.write("out/")` and the path
`write_file("out/report.txt", ...)` are two string literals whose agreement only E0503 or a run-time
R0010 checks, and a field's `@size(1, 100)` cannot name the limit the rest of the program uses.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A value used in several places is named once (Priority: P1)

A developer (or agent) declares `const max_items = 100;` at the top level of a file and uses `max_items`
wherever an `int` is expected; `const greeting = "hello";` wherever a `str` is. Changing the value is one
edit, and the name says what the number means.

**Why this priority**: it is the whole feature for most programs, and everything else builds on it.

**Independent Test**: a program with a limit, a rate and a message declared as constants and used in
several functions and tests checks, runs and prints the same as with the literals written out; changing a
constant changes every use.

**Acceptance Scenarios**:

1. **Given** `const max_items = 100;`, **When** a function uses `max_items` in an expression, **Then** it
   has the type `int` and the value 100, in every function and test of the file.
2. **Given** `const rate: float = 0.25;`, **When** checked, **Then** the stated type must be the type of
   the value (E0301 otherwise), exactly as for `let rate: float = 0.25;`.
3. **Given** `const total = max_items * 2 + 1;` and `const banner = greeting + ", world";`, **When**
   checked, **Then** each has the value of its expression (201, `"hello, world"`), worked out once, at
   check time, with vo's own arithmetic.
4. **Given** a constant used before its declaration in the file, **When** checked, **Then** it works: items
   can use items declared later, constants included.
5. **Given** a function that uses constants and declares no `uses` clause, **When** checked, **Then** it is
   still pure: reading a constant is not an effect.
6. **Given** `set max_items = 5;`, **When** checked, **Then** it is an error at that statement saying a
   constant cannot be changed, with no fix.
7. **Given** a program that declares no constant, **When** checked, run and formatted, **Then** every
   output is byte-identical to before this feature.

---

### User Story 2 - A mistake in a constant is reported where it is written (Priority: P1)

A constant's value must be something the checker can work out by itself. Anything else, and anything that
would be a runtime error if a function did it, is a check error at the constant, with a fix where one
correction is unambiguous.

**Why this priority**: an agent meets the rules through the diagnostics; a constant that fails only when
the program runs would be worse than a literal.

**Independent Test**: a file with one of each mistake reports each at its own position with its code, in
one pass, and the rest of the program is still checked.

**Acceptance Scenarios**:

1. **Given** `const now = now_ms();` or `const n = len(items());`, **When** checked, **Then** the error
   says a constant's value cannot call a function, at the call.
2. **Given** `const a = b + 1;` and `const b = a + 1;`, **When** checked, **Then** one error names the
   cycle (`a` → `b` → `a`), as import cycles and effect-set cycles are reported.
3. **Given** `const big = 9223372036854775807 + 1;`, `const bad = 10 / 0;` or a `%` by zero, **When**
   checked, **Then** the error says the value overflows or divides by zero, at the operator: what would be
   R0001 or R0002 at run time is a check error here.
4. **Given** `const xs = [1, 2, 3];`, `const p = new Point { x: 1, y: 2 };` or a constant of any type
   other than `int`, `float`, `bool` and `str`, **When** checked, **Then** the error says which types a
   constant can have.
5. **Given** `const MaxItems = 100;` or `const MAX_ITEMS = 100;`, **When** checked, **Then** E0207 offers
   `max_items` as the fix: constants are named like functions and variables.
6. **Given** a constant with the name of another item or a builtin, **When** checked, **Then**
   it is E0202; a local, a parameter or a `for` variable with the name of a constant in scope is E0203 (vo
   has no shadowing).
7. **Given** a private constant that nothing uses, **When** checked, **Then** there is a warning with a fix
   that removes it, as for an unused private effect set.
8. **Given** `const x = 1;` written as a statement inside a function, **When** checked, **Then** the error
   says constants are declared at the top level and offers `let` as the fix.

---

### User Story 3 - Constants cross files like other items (Priority: P1)

A developer writes `export const default_port = "8080";` in one file and `import config.default_port;`
in another. A constant without `export` is private to its file.

**Why this priority**: shared limits and defaults are the common case in programs of more than one file.

**Independent Test**: a two-file program where one file exports two constants and the other imports one
checks and runs; importing the unexported one is E0209 with the fix that adds `export`.

**Acceptance Scenarios**:

1. **Given** an exported constant, **When** another file imports it by name, **Then** it can use it as if
   declared there.
2. **Given** a private constant, **When** another file imports it, **Then** it is E0209, whose fix adds
   `export`, as for functions.
3. **Given** a name used in a file that a sibling file declares as an exported constant, **When** checked,
   **Then** E0204 offers the `import` line, as for functions.
4. **Given** a package pinned in `vo.packages`, **When** it exports constants, **Then** programs import
   them like its functions, and `vo doc NAME.module` lists them.
5. **Given** constants whose values depend on constants of other files, **When** checked, **Then** they
   are worked out in dependency order; a cycle through files is already an import cycle (E0206).

---

### User Story 4 - A constant can stand where the checker reads a literal (Priority: P2)

Some places take a literal because the checker reads its text: the scopes of effects, the limits in field
annotations, and the templates and patterns of `format`, `regex`, `sql` and `std.web` routes. A constant
of the right type can be written in each of these, and means exactly what its value written there would.

**Why this priority**: it is what a function returning a literal can never do, and it is where two
literals that must agree cause real errors. It comes after P1 because P1 is useful alone.

**Independent Test**: a program declares `const out_dir = "out/";`, uses it in `uses
file.write(out_dir)` and in `write_file(out_dir + "report.txt", ...)`, and in a field's `@size(1,
max_name)`: it checks and runs as with the literals; `vo.effects` shows `file.write("out/")`; changing
the constant changes the scope and the path together and is reported as a capability change.

**Acceptance Scenarios**:

1. **Given** `const out_dir = "out/";`, **When** a function declares `uses file.write(out_dir)`, **Then**
   it may write exactly where `uses file.write("out/")` allows; an invalid scope value (empty, absolute,
   with `..`) is reported at the constant's use with the constant's value in the message.
2. **Given** an effect set `effects Storage = file.read(data_dir), file.write(data_dir);`, **When** used,
   **Then** it means the same as with the literal.
3. **Given** a field annotated `@size(1, max_name)`, `@range(0, max_items)`, `@default(default_port)` or
   `@pattern(code_pattern)`, **When** checked and when `from_json` or `std.web` validates a value, **Then**
   the limits are the constants' values; a constant of the wrong type is E0329 as a literal would be.
4. **Given** `format(row_template, name, total)`, `regex(code_pattern)`, `sql(insert_note)` or
   `on_get(note_path, show)` with `str` constants, **When** checked, **Then** the template, pattern,
   statement or route is checked as if the constant's value were written there, and a problem inside it
   is reported at the use, quoting the value.
5. **Given** a call whose literal target is checked against the caller's scopes (`write_file("tmp/x",
   ...)` under `uses file.write("out/")` is E0503), **When** the target is a constant or a constant
   expression (`out_dir + "report.txt"`), **Then** it is checked the same way.
6. **Given** a name that is not a constant in one of these places (a parameter, a local), **When**
   checked, **Then** the existing error for "a literal is needed here" now says "a literal or a constant".

---

### User Story 5 - Reviewing, querying and editing know constants (Priority: P2)

Everything the toolchain reports shows what a constant stands for where that matters, so a name never
hides a capability, and the tools that answer from the checker answer for constants too.

**Why this priority**: effects are vo's security story, and `vo query`/`vo edit` are how agents work on a
codebase; a new kind of item that they do not know would be a step back.

**Independent Test**: changing `const out_dir = "out/";` to `"./"` makes `vo check` report every function
whose scope used it as gaining a wider scope (E0504); `vo query refs`, `type-at` and `vo edit rename` work
on a constant across two files.

**Acceptance Scenarios**:

1. **Given** functions whose `uses` clauses name a constant in a scope, **When** the effects record is
   written or compared, **Then** it holds the scope's value (`file.write("out/")`), never the name; a
   record written before a literal was replaced by a constant of the same value stays valid, unchanged.
2. **Given** a constant's value changes so that a scope becomes wider, **When** checked, **Then** every
   function using it is E0504, with a note naming the constant; narrower is W0502.
3. **Given** `vo query refs ENTRY max_items` or a position on a use, **When** run, **Then** it lists the
   declaration, the imports and every use, including uses in scopes and annotations; `callers` does not
   apply to constants and says so.
4. **Given** `vo query type-at` on a constant's use or declaration, **When** run, **Then** it shows the
   type, the value, where it is declared and its documentation comment.
5. **Given** `vo edit rename ENTRY max_items item_limit`, **When** run, **Then** every reference in every
   file changes, the result is checked first, and all files are written or none.
6. **Given** `vo impact ENTRY --diff FILE` for a diff that changes a constant's value, **When** run,
   **Then** the functions and tests that use the constant (directly or through other constants) are listed
   as affected.
7. **Given** `vo doc ENTRY` or `vo doc NAME.module`, **When** run, **Then** exported constants are listed
   with their type, value and comment; `--json` gives records.
8. **Given** a runtime error or a failing `expect` whose expression uses a constant, **When** reported,
   **Then** the report shows the constant's value among the operands, as it does for locals.

---

### User Story 6 - One canonical form, documented where agents read (Priority: P3)

`vo fmt` writes a constant one way; the reference, the guide, the error catalog and the examples show it.

**Why this priority**: how agents learn the form; it follows the behaviour.

**Independent Test**: formatting a file with oddly spaced constants gives the canonical text and formats to
itself; every new reference and guide example is run by the test suite; the reference stays inside its
budget.

**Acceptance Scenarios**:

1. **Given** `const   max_items=100 ;` or `export  const rate : float=0.25;`, **When** formatted, **Then**
   the result is `const max_items = 100;` and `export const rate: float = 0.25;`, with a comment above it
   kept as its documentation and one blank line between items, as for other items.
2. **Given** a constant whose expression is over 100 characters, **When** formatted, **Then** it is laid
   out by the rule statements already follow.
3. **Given** the reference, **When** read, **Then** it says where constants are declared, what a value may
   be, where a constant may stand for a literal, and shows a runnable example, within 15,000 tokens.
4. **Given** the examples, **When** read, **Then** each literal that the same example repeated three or
   more times with one meaning, and each scope repeated as a path, is a constant.

### Edge Cases

- `const` is not a new reserved word: it is recognized only where an item starts (as `effects` is), so a
  program that uses `const` as a name keeps working. No program that checked before changes.
- A constant named like an effect set or a type cannot exist: constants are lower case, those are
  capitalized, and E0207 fixes the case first.
- `const x: int = todo("...");`: a hole is not a value the checker can work out: the error of scenario
  2.1 (a hole is a call).
- A `float` constant that is `inf` or `nan` (`1.0 / 0.0`): allowed, as the same expression is in a
  function; it prints as `inf` or `nan`.
- A `str` constant built with `+` from other constants; a constant with escapes (`"a\tb"`): the value is
  the string after escapes, everywhere it is shown.
- A constant used only in a scope or an annotation counts as used.
- A constant used in `requires`/`ensures`, in a test's clause, in a `prop`, in an anonymous function: all
  are expressions or clauses like any other.
- Anonymous functions capture `let` bindings by value; a constant needs no capture and is always visible.
- `==` and `<` between constants in a constant (`const wide = max_items > 50;`): a `bool` constant.
- A very long constant chain (a constant depending on a thousand others): worked out without deep
  recursion limits surprising the user; a limit, if any, is an error naming it.
- Two files exporting constants with the same name imported into a third: E0202 at the second import, as
  for functions.
- Mutation testing changes code inside functions; a constant's value is not a mutation site in this
  feature. Coverage has no points in constants.
- A constant in a `@spec("...")` or `@allow("W0703", "...")` annotation: these take literals only; they
  name requirements and codes, not values of the program.
- The structure warnings (file and function sizes, repeated statements) count a constant as one item line.

## Requirements *(mandatory)*

### Functional Requirements

**Declaring and using**

- **FR-001**: A file MUST be able to declare `const NAME = EXPR;` and `const NAME: TYPE = EXPR;` among its
  top-level items, in any order with the others, each optionally after `export`. This is the only form.
- **FR-002**: A constant's type MUST be `int`, `float`, `bool` or `str`: inferred from the value, or
  checked against the stated type.
- **FR-003**: A constant's value MUST be an expression made only of literals, other constants, parentheses
  and the operators `+ - * / %`, unary `-`, the comparisons, and `and`, `or`, `not`, with the typing rules
  expressions already have (no implicit conversions; `+` joins two `str`).
- **FR-004**: The checker MUST work out every constant's value once, with exactly the results the same
  expression has at run time (integer division truncating toward zero, IEEE floats, byte order for `str`
  comparisons), identically on every platform.
- **FR-005**: A constant MUST be usable wherever an expression of its type is, in every function, test,
  property, contract and anonymous function that can see its name, and MUST never be assignable.
- **FR-006**: Using a constant MUST NOT be an effect and MUST NOT change any function's effects.
- **FR-007**: Constants MUST be exported, imported, found missing (E0204) and refused when private (E0209)
  by the same rules and fixes as functions, including through packages.

**Mistakes**

- **FR-008**: Each of these MUST be a check error at the constant, with a stable code, a catalog entry and
  a golden case: a value that calls a function or builtin, names something that is not a constant, or has
  a type a constant cannot have; a cycle; an overflow or a division by zero in the value; assigning to a
  constant; a constant declared inside a function.
- **FR-009**: Naming (E0207, with the converted name as the fix), duplicates (E0202) and shadowing (E0203)
  MUST apply to constants as to other names; an unused private constant MUST be a warning with a fix.
- **FR-010**: All independent mistakes in constants MUST be reported in one pass; a constant whose value is
  in error MUST NOT cause further errors at its uses.

**Standing for a literal**

- **FR-011**: A `str` constant MUST be accepted as the scope of an effect, in `uses` clauses, test clauses
  and effect sets, and mean its value written there.
- **FR-012**: A constant of the right type MUST be accepted as an argument of the field annotations
  `@size`, `@range`, `@pattern`, `@default` and `@header`.
- **FR-013**: A `str` constant MUST be accepted as the template of `format`, the pattern of
  `std.regex.regex`, the statement of `std.sqlite.sql` and the pattern of a `std.web` route, and be checked
  as its value would be.
- **FR-014**: Wherever the checker compares a literal argument with the caller's scopes (E0503), it MUST
  do the same for a constant and for a constant expression.
- **FR-015**: In all of FR-011 to FR-014, a problem inside the value MUST be reported at the use, and say
  the value.

**Reviewing and tools**

- **FR-016**: The effects record, `vo effects`, and every diagnostic or fix that shows effects MUST show a
  scope's value, never a constant's name; a change of a constant that changes a scope MUST be reported
  exactly as an edit of the clause would be (E0504, W0502).
- **FR-017**: `vo query refs` and `type-at`, `vo edit rename`, `vo impact` and `vo doc` MUST handle
  constants: references in expressions, scopes, annotations and imports; type, value and documentation;
  the functions and tests a changed constant affects.
- **FR-018**: Reports that show the values of operands or locals (failing `expect`, runtime error traces)
  MUST show a constant's value where its name appears in the reported expression.
- **FR-019**: `vo fmt` MUST have one canonical form for a constant, idempotent, with its comment kept as
  documentation.
- **FR-020**: Every new report MUST also be available with `--json` in the existing schema (version 1):
  new codes and, where a value is shown, new optional fields only.

**Documentation and examples**

- **FR-021**: The reference MUST document constants with a runnable example and stay under 15,000 tokens;
  the error catalog MUST have an entry with a broken and a fixed example for every new code; the guide
  MUST explain when to use a constant (a value with one meaning used in more than one place, or a value a
  scope or annotation shares with code) and when not (a value computed from input: a function).
- **FR-022**: The examples MUST use constants where they repeat a literal with one meaning three or more
  times, and where a scope and a path are the same directory.
- **FR-023**: The roadmap and the README MUST list the feature; "no global variables or constants" in the
  reference becomes "no global variables".

### Key Entities

- **Constant**: a named, top-level, immutable value of type `int`, `float`, `bool` or `str`; exported or
  private; its value known to the checker; documented by the comment above it.
- **Constant expression**: literals and constants combined with operators; what a constant's value may
  be, and what may stand where the checker needs a value it can read.
- **Literal position**: a place where the checker reads a value's text: an effect scope, a field
  annotation's argument, a checked template, pattern, statement or route.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In the examples, every directory that appears both as an effect scope and as a path is
  written once; changing it is a one-line edit that `vo check` reports as a capability change for every
  function it reaches.
- **SC-002**: Every program in the test corpus, the examples, the standard library and the benchmark
  reference solutions checks, runs and formats byte-identically before and after the feature (731 of 731
  reference runs unchanged), and their effects records are unchanged.
- **SC-003**: Replacing a repeated literal by a constant changes no output: for the examples rewritten
  under FR-022, output, exit status and effects record are identical before and after.
- **SC-004**: Every mistake of User Story 2 is reported in one `vo check`, each with its own code and
  position; the ones with a single correction carry a fix that, applied, resolves the diagnostic (fix
  coverage does not drop below today's 92% of fixable codes).
- **SC-005**: A constant and the same value written as a literal give the same result in every position
  of User Story 4, for at least 1,000 generated programs compared both ways.
- **SC-006**: Checking a 5,000-line program takes no more than 5% longer than before, and a program that
  uses a constant in a loop runs no slower than with the literal.
- **SC-007**: The reference stays under 15,000 tokens with the new section, and all its examples run.
- **SC-008**: An agent given only `vo reference` uses constants correctly: on tasks that state a shared
  limit or directory, first-try results are no worse than before the feature (the benchmark's isolated
  protocol; the run may be scheduled separately, as for specs 029 and 030).

## Assumptions

- Constants are named in `snake_case`, like functions and variables. vo has one rule for lower-case names
  and E0207 enforces it; a third convention (`MAX_ITEMS`) would be a second way to see a name.
- The types are the four that have literals and cannot be changed in place. Lists, maps, records and
  variants are references that code could modify, so a constant of those types would be a global
  variable; they stay out. `bytes` has no literal and stays out.
- A value cannot call anything, not even pure builtins such as `len`, `str` or `format`. Operators cover
  the sizes, sums and joined texts that programs need; allowing calls would make the checker an
  interpreter and the rule ("what counts as pure enough") something to learn. A later spec can widen it.
- A constant can stand in every literal position the checker reads except `@spec` and `@allow`, whose
  arguments are identifiers of requirements and warning codes. One rule ("a literal or a constant") is
  easier to learn than a list of places; the places are delivered in priority order (P1 expressions, P2
  scopes, annotations and checked templates), so the first slice is useful alone.
- Constants cannot be declared inside functions: `let` already names a value there. One way to write each
  thing.
- A constant has no generic form and no effect; it is not a function value and cannot be called.
- The standard library keeps its interface: functions such as `std.math.pi()` stay functions in this
  feature.
- The effects record keeps holding values, so records written before this feature stay valid and a
  literal replaced by an equal constant changes nothing in review.
- New diagnostics take the next free codes in their ranges (E02xx for names, E03xx for types, W02xx for
  the unused warning); none is reused.
- The benchmark run of SC-008 reuses the isolated agent protocol and may run after the implementation, as
  in earlier specs.

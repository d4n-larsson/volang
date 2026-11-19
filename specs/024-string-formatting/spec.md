# Feature Specification: String Formatting

**Feature Branch**: `024-string-formatting`

**Created**: 2026-11-19

**Status**: Implemented

**Input**: User description: "String formatting, allow formatting like sprintf. Look how other popular
languages have solved this and have a coding agents perspective"

Background: a gap of the roadmap's "Planned after 017" table ("String formatting: only `+`, `str()` and
padding; one formatting builtin within the reference budget"). Today an agent builds text with `+` and
`str(...)`, pads with `std.text.pad_left` / `align_right`, and has no way at all to print a float with a
fixed number of decimals (`3.14` from `pi()`), an int in hex, or a number with thousands separators. Every
program that prints money, measurements, percentages or a table writes its own rounding helper, and agents
get those helpers wrong (rounding, negative numbers, `-0.00`).

### How other languages do it

| Language | Form | Checked when | What agents get wrong |
|---|---|---|---|
| C | `printf("%5.2f %d", x, n)` | compiler warning only | verb for the wrong type (`%d` with a double), argument count, format-string injection |
| Go | `fmt.Sprintf("%.2f %v", x, v)` | `go vet` (not the compiler) | `%d` with a float prints `%!d(float64=...)` at run time instead of failing |
| Python | `f"{x:.2f} {n:>5}"`, `"{}".format(x)`, `"%d" % n` | run time | three spellings; `{:.2}` means two *significant* digits, not decimals; errors only when that line runs |
| Rust | `format!("{:.2} {name}", x)` | compile time (macro) | few: the template must be a literal, so mismatches are compile errors |
| Zig | `std.fmt.allocPrint(a, "{d:.2}", .{x})` | compile time | same as Rust |
| JavaScript | `` `${x.toFixed(2)}` ``, `padStart` | never | `toFixed` rounding surprises, locale-dependent `toLocaleString` |
| Swift / Kotlin | `"\(x)"` / `"$x"` interpolation, plus `String(format: "%.2f", x)` | interpolation at compile time, `%` formats at run time | two systems for one job |
| C# | `$"{x,8:F2}"` | compile time (holes), run time (spec) | culture-dependent output |
| Java | `String.format("%.2f", x)` (string templates were previewed and withdrawn) | run time | as C |

From an agent's point of view:
- **The Python/Rust brace mini-language is what agents write most** (f-strings dominate their training
  data), with C's `%` verbs second. Both are well known; a new syntax would be guessed wrong.
- **Type letters are redundant when the checker knows the type** (`%d` vs `%s`) and are the most common
  source of mismatches. Only letters that change the output (`f` for fixed decimals, `x` for hex) carry
  information.
- **The languages where agents make the fewest formatting mistakes check the template at compile time**
  (Rust, Zig, C#'s holes): the template is a literal, so every count or type mismatch is a compile error
  pointing at the placeholder. That fits vo's loop: every mistake becomes a diagnostic, most with a fix.
- **Interpolation** (`f"..."`, `` `${}` ``) puts expressions inside strings: it needs a second lexer mode,
  spans inside string literals, and changes to `vo fmt`, `vo query` and `vo edit rename`. vo's reference
  already says "no interpolation", and the roadmap asks for one builtin. A call with a literal template
  gives the same checking with no new syntax.
- **Habits from other languages should be recognized, not just rejected**: `%d`, `{0}`, `{name}`,
  `{:.2}` and `f"..."` are each one mechanical fix away from the vo form.

What this spec must keep:
- **Principle I**: one way to write each format (no alternative spellings of the same placeholder).
- **Principle II**: every mistake in a template is a check-time diagnostic with a stable code, the span of
  the placeholder, and a fix wherever one correction is unambiguous.
- **Principle III**: the same values give the same bytes on every platform; no locale, no environment.
- **Principle V**: the reference budget (about 12,700 of 15,000 tokens used today).

## Clarifications

### Session 2026-11-19

- Q: Where does `format` live? → A: A builtin (no import); programs defining their own `fn format` rename
  it (E0202).
- Q: Placeholder syntax? → A: Python/Rust braces (`{}`, `{:.2f}`); C `%` verbs get a check error whose fix
  rewrites them to braces.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–023: AI coding agents that write vo programs from the reference and
`vo doc`, and the people who review and run those programs.

### User Story 1 - Fixed decimals and columns (Priority: P1)

An agent writes a program that prints a receipt: item names left-aligned in a 12-character column, prices
right-aligned with exactly two decimals, and a total. It writes one call per line, with the values next to
their placeholders, instead of a chain of `+`, `str` and padding helpers, and without writing its own
rounding.

**Why this priority**: fixed decimals is the one formatting need vo cannot meet at all today, and columns
are the most common reason for padding chains.

**Independent Test**: golden cases that format floats with 0–6 decimals and strings and numbers in
columns, compared byte for byte with expected output.

**Acceptance Scenarios**:

1. **Given** `format("{:.2f}", 3.14159)`, **When** it runs, **Then** the result is `"3.14"`.
2. **Given** `format("{:<12}|{:>8.2f}", "coffee", 4.5)`, **When** it runs, **Then** the result is
   `"coffee      |    4.50"`.
3. **Given** `format("{} of {} done", 3, 10)`, **When** it runs, **Then** the result is `"3 of 10 done"`.
4. **Given** `format("{:.2f}", 2.675)`, **When** it runs, **Then** the result is `"2.67"` (the double
   nearest to 2.675 is below it), the same as Python, C and Rust.
5. **Given** `format("{:.2f}", -0.001)`, **When** it runs, **Then** the result is `"-0.00"`, as in Python
   and C.
6. **Given** `format("{{}} {}", 1)`, **When** it runs, **Then** the result is `"{} 1"`.

---

### User Story 2 - Mistakes are caught before the program runs (Priority: P1)

An agent writes a template with one placeholder too many, passes an int where it asked for decimals, or
builds the template from a variable. `vo check` reports each mistake at the placeholder or argument, with a
fix where the correction is clear, and the program never prints a half-formatted line at run time.

**Why this priority**: in C, Go and Python these mistakes surface only at run time, if at all; catching
them at check time is the main reason to prefer a checked builtin over a library function, and it is what
keeps the agent's loop short.

**Independent Test**: golden check cases, one per kind of mistake, each with the expected JSON diagnostic
(code, span, message, fix).

**Acceptance Scenarios**:

1. **Given** `format("{} and {}", a)`, **When** checked, **Then** a diagnostic points at the template,
   says it has 2 placeholders and 1 argument, and has no fix.
2. **Given** `format("{:.2f}", n)` with `n: int`, **When** checked, **Then** a diagnostic points at `n`
   with the fix `float(n)`.
3. **Given** `format(template, x)` where `template` is a variable, **When** checked, **Then** a diagnostic
   says the template must be a string literal.
4. **Given** `format("{:x}", 2.5)`, **When** checked, **Then** a diagnostic says `x` applies to `int` only.
5. **Given** an argument that is a list, record or other type without a text form, **When** checked,
   **Then** a type error lists the accepted types (`int`, `float`, `bool`, `str`).
6. **Given** several independent mistakes in one file, **When** checked, **Then** all are reported in one
   pass.

---

### User Story 3 - Habits from other languages get a fix (Priority: P2)

An agent that has written mostly Python, C or Go writes `format("%d items, %.2f each", n, p)`,
`format("{0} {1}", a, b)`, `format("{:.2}", x)` or `f"hello {name}"`. Each gets a diagnostic whose fix is
the vo form, so `vo check --json` plus the fix applier turns it into a working line without a second
guess.

**Why this priority**: agents learn vo from the reference only; recognized habits cost one round trip
instead of several.

**Independent Test**: golden check cases for each habit, with the fix applied by the existing fix applier
and the fixed program then checked and run.

**Acceptance Scenarios**:

1. **Given** `format("%d items, %.2f each", n, p)` (no `{}` placeholders, `%` verbs matching the
   arguments), **When** checked, **Then** the fix is `format("{} items, {:.2f} each", n, p)`.
2. **Given** `format("{0} and {1}", a, b)` (indexes in order), **When** checked, **Then** the fix is
   `format("{} and {}", a, b)`; out-of-order or repeated indexes get no fix.
3. **Given** `format("{:.2}", x)` with `x: float`, **When** checked, **Then** the diagnostic explains that
   precision needs `f` and the fix is `{:.2f}`.
4. **Given** `format("{:d}", n)` or `format("{:s}", s)`, **When** checked, **Then** the fix removes the
   redundant letter (`{}`).
5. **Given** `f"hi {name}, you are {age}"` where every hole is a name or a field access, **When** checked,
   **Then** the diagnostic's fix is `format("hi {}, you are {}", name, age)`.
6. **Given** a `%` in a template that already has `{}` placeholders (`format("{}%", pct)`), **When**
   checked, **Then** there is no diagnostic: `%` is plain text.

---

### User Story 4 - Numbers for people and for machines (Priority: P3)

An agent prints a population with thousands separators, a signed temperature change, a zero-padded
invoice number, and an int as hex or binary for a checksum or bit-mask puzzle.

**Why this priority**: common but each has a workaround today (`pad_left` for zeros, a loop for hex);
cheap to add in the same mini-language.

**Independent Test**: golden cases per option, and a differential test against Python's `format()` for
random values and every supported spec.

**Acceptance Scenarios**:

1. **Given** `format("{:,}", 1234567)`, **Then** `"1,234,567"`; `format("{:,.2f}", 1234567.891)`
   gives `"1,234,567.89"`.
2. **Given** `format("{:+}", 5)` and `format("{:+.1f}", -0.25)`, **Then** `"+5"` and `"-0.2"`.
3. **Given** `format("{:05}", 42)` and `format("{:05}", -42)`, **Then** `"00042"` and `"-0042"`.
4. **Given** `format("{:x}", 255)`, `format("{:08b}", 5)` and `format("{:x}", -255)`, **Then** `"ff"`,
   `"00000101"` and `"-ff"`.

### Edge Cases

- **Default alignment**: as in Python and Rust, strings and bools align left in a width, numbers align
  right; `<`, `>` and `^` override it (`^` puts the odd extra space on the right, as Python does).
- **Width counts characters** (code points), like `pad_left`, not bytes; a value longer than the width is
  never cut.
- **`{}` with no spec** gives exactly what `str(x)` gives for `int`, `float` and `bool`, and the string
  itself for `str`; `true`/`false` for bools (not Python's `1`/`True`).
- **NaN and infinities** print as `str` prints them (`nan`, `inf`, `-inf`), whatever the precision; the
  `0` flag pads them with spaces, not zeros.
- **Rounding** is of the exact binary value to the nearest decimal, ties to even (the C and Python rule),
  identical on every platform; no locale, no environment variable changes any output.
- **Large precision and width**: precision above 100 or width above 1000 is a check error, so no template
  can ask for an unbounded result; within the bounds, the digits are the exact decimal expansion
  (`format("{:.20f}", 0.1)` is `"0.10000000000000000555"`).
- **Braces**: `{{` and `}}` are literal braces; an unmatched `{` or `}` is a check error pointing at it.
- **Unsupported options** (a fill character, `#`, `e`, `%`, `_`, a space sign, precision on a string or
  int): a check error naming the supported options; with a fix where one is unambiguous (`{:0>5}` on an
  int is `{:05}`).
- **Combinations Python handles surprisingly** (`0` with an alignment, where Python's `{:<05}` gives
  `50000`; `0` with `,`, where Python groups the padding zeros) are check errors naming the form to use.
- **No arguments**: `format("plain")` with no placeholders is allowed, but a template with no
  placeholders and at least one argument is an error (or the `%`-habit fix of Story 3).
- **Effects**: `format` is pure; it needs no effect and may be called anywhere, including contracts and
  property tests.
- **Memory**: the result counts toward `--max-memory` like any string.
- **Existing programs named `format`**: `format` is a builtin, so a program that defines its own
  `fn format` now gets E0202 and must rename it. In this repository that is the `h07-word-wrap` property
  fixtures (`tests/prop_eval/h07-word-wrap/`), renamed in this change.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: vo MUST provide `format(template, args...)` returning a `str`: the template with each `{}`
  placeholder, in order, replaced by the text of the matching argument. Where it is declared is
  FR-016.
- **FR-002**: The template MUST be a string literal; anything else is a check error. Arguments MAY be any
  expressions of type `int`, `float`, `bool` or `str`; any other type is a check error listing those four.
- **FR-003**: The number of placeholders MUST equal the number of arguments, or the check fails with a
  diagnostic giving both counts.
- **FR-004**: A placeholder is `{}` or `{:spec}`, where spec is, in this order, all optional: an
  alignment (`<`, `>`, `^`), a sign (`+`), a zero flag (`0`), a width (digits), a grouping (`,`), a
  precision (`.` digits, followed by `f`), and a type letter for ints (`x` lowercase hex, `b` binary).
  This is a subset of the Python and Rust format mini-language, with the same meaning for every option it
  keeps.
- **FR-005**: For every supported spec, `int` values, and finite `float` values with a precision, MUST
  format to exactly what Python's `format(value, spec)` gives for the same spec. (Without a precision a
  float prints `str(x)`'s digits, which differ from Python's shortest round-trip text.)
- **FR-006**: Each option MUST apply only to the types where it means something (`.Nf` to `float`; `x`
  and `b` to `int`; `+`, `0` and `,` to numbers; width and alignment to all four); any other use is a
  check error at the placeholder.
- **FR-007**: `{{` and `}}` MUST produce `{` and `}`; an unmatched brace is a check error.
- **FR-008**: The type letters `d` and `s`, the precision form without `f` (`{:.2}`), and indexed or
  named placeholders (`{0}`, `{name}`) MUST be check errors, each with a fix to the vo form when it is
  unambiguous (Story 3).
- **FR-009**: A template with no `{}` placeholders, at least one argument, and C-style `%` verbs whose
  count matches the arguments MUST get a diagnostic with a fix translating the verbs (`%d`, `%s`, `%v` to
  `{}`; `%.Nf` to `{:.Nf}`; `%x` to `{:x}`; `%5d` to `{:5}`; `%-5s` to `{:<5}`). Otherwise `%` is plain
  text.
- **FR-010**: A string literal written with an `f` prefix MUST get a diagnostic (replacing today's generic
  parse error) whose fix is the equivalent `format` call when every hole is a name or a field access.
- **FR-011**: Every diagnostic MUST have a stable code in the error catalog with an example and a fix
  (`vo explain`), MUST point at the placeholder or argument concerned, and MUST be reported together with
  all other independent errors in one pass.
- **FR-012**: A valid `format` call MUST NOT be able to fail at run time, except by running out of memory
  under `--max-memory` like any other allocation.
- **FR-013**: Output MUST be byte-identical on x86-64, ARM64 and RISC-V 64, and independent of locale and
  environment.
- **FR-014**: Precision MUST be at most 100 and width at most 1000; larger values are check errors.
- **FR-015**: The reference MUST document `format` (the placeholder grammar, a table of options, and one
  runnable example), and hover (`vo query type-at`) on a `format` call MUST show that documentation.
- **FR-016**: `format` MUST be a builtin (no import), listed in the reference's builtins table; a user
  definition named `format` is E0202 like any other builtin name.

### Key Entities

- **Template**: a string literal holding plain text, literal braces (`{{`, `}}`) and placeholders.
- **Placeholder**: `{}` or `{:spec}`; matched to arguments by position, left to right.
- **Spec**: alignment, sign, zero flag, width, grouping, precision with `f`, type letter (`x`, `b`); each
  part optional, in that fixed order.
- **Argument**: an expression of type `int`, `float`, `bool` or `str`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For 100,000 random `int` and finite `float` values over every supported spec combination
  (widths 0–12, precisions 0–10), the output is byte-identical to Python's `format(value, spec)`, and
  identical on all three platforms.
- **SC-002**: Every kind of template mistake listed in Stories 2 and 3 is reported at check time with its
  own stable code; for each habit in Story 3, applying the fix gives a program that checks and prints the
  intended text.
- **SC-003**: Every new diagnostic code has a catalog entry and at least one golden case; every example in
  this spec's acceptance scenarios is a golden case.
- **SC-004**: A program that formats 1,000,000 values with `format` runs no slower than the same program
  built with `+` and `str(...)` (median of 5 runs).
- **SC-005**: The reference grows by at most 350 tokens and stays within the 15,000-token budget.
- **SC-006**: In a benchmark run, agents given the updated reference solve the formatting tasks (receipt,
  table, percentages, hex dump) on the first attempt at least as often as before, with fewer
  formatting-related check errors per task.

## Assumptions

- `format` takes its arguments directly (variadic), not as a list: a `list[str]` would force `str(...)` on
  every value and lose the types the checker needs to validate the spec. It is the only variadic call in
  the language, which is why it is checked specially.
- Placeholders are positional `{}` only. Indexed (`{0}`) and named (`{name}`) forms are left out: they are
  a second and third spelling of the same thing, and named holes amount to interpolation.
- The Python/Rust brace mini-language is chosen over C's `%` verbs because the checker already knows each
  argument's type (so `%d` versus `%s` adds nothing but a way to be wrong), because it is what agents
  write most, and because `%` habits get a mechanical fix (FR-009).
- Precision requires `f` (`{:.2f}`, as in Python) rather than accepting Rust's `{:.2}`: in Python `{:.2}`
  means two significant digits, so accepting it would silently mean different things to agents from the
  two languages. `{:.2}` gets a fix instead.
- String interpolation stays out of the language (the reference's "no interpolation" stands); `f"..."` is
  only recognized to give a fix.
- Out of scope: scientific notation (`e`), percentages (`%`), custom fill characters, `#` prefixes,
  formatting lists, maps or records (no `%v`-style debug printing), locale-aware output, and a `printf`
  that writes directly (use `print(format(...))`). Each can be added later in the same mini-language.
- Existing uses of `+` and `str(...)` stay valid and are not rewritten; the reference's examples change
  only where `format` is the subject.
- `print` and `str` are unchanged.

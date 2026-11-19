# Research: String Formatting

The language-level choices (braces, builtin, literal template, the option set) are in the spec and its
Clarifications; this file records how they are built and the alternatives rejected.

## R1. Parse the template from its source text, in the checker

**Decision**: `check_format.c` reads the template's *source* bytes (between the quotes of the `N_STR`
node), decoding the four escapes (`\n`, `\t`, `\\`, `\"`) as it goes. Every piece keeps the byte offsets
of its source, so a diagnostic points exactly at `{:.2}` inside the literal (`diag_add` takes a file and
a byte range). The parsed pieces (literal text with `{{`/`}}` already collapsed, and placeholders with
their packed spec) are stored on the call (`call->e.p`, unused by builtin calls) for the compiler.

**Rationale**: the checker is where types are known and where every diagnostic must come from (one pass,
cascades suppressed). Parsing the unescaped value instead would lose the mapping to source columns.

**Alternatives considered**: parsing in the lexer (no types; the lexer does not know which strings are
templates); parsing at run time (mistakes would be runtime errors, against Principle II and the spec).

## R2. Lower to existing instructions; one new internal builtin per placeholder

**Decision**: `cformat` in `src/compile.c` emits, left to right:
- `LOADK` of a string constant for each literal piece (the `Str` made by the checker, like any literal);
- for each placeholder, `CALL_BUILTIN` on a synthetic call node with id `B_FMT_PIECE` (83, unnamed, as
  ids 50 and 51 are), one argument (the user's expression, compiled in place), and the packed spec in
  `e.i`;
- `OP_CONCAT` (the string `+`) to join them. `format("plain")` is just the constant.

`builtin_call` reads the id from the node (`call->d.i`), so the synthetic node is the natural carrier;
`c_expect_part` already makes such copies. The value's type comes from `arg0_type(call)`, as for `str`.

**Rationale**: no VM change, no new opcode, nothing to collect specially (each value travels in its own
typed slot, exactly as for `str(x)`). The result is the same instruction sequence as the hand-written
`+`/`str` chain, which is what SC-004 measures against.

**Alternatives considered**:
- One runtime builtin taking all values in a list: list elements of mixed kinds (ints, float bits, string
  pointers) would break the collector's view of the list. Rejected.
- Raising the three-argument limit of `CALL_BUILTIN`: changes the VM for one builtin. Rejected.
- Rewriting the tree in the checker into `+` and calls: the rewritten nodes would be seen by `vo query`,
  coverage and mutation testing as if the user had written them. Rejected; the compiler's copies are
  invisible to them (mutation testing only mutates binary operators, booleans, `not` and int literals,
  and the copies are a call and string constants).

ponytail: concatenation copies the growing string once per piece (quadratic in the number of pieces);
templates have a handful. Upgrade: a single join instruction if a measurement shows long templates.

## R3. Fixed decimals: exact digits from the existing bignums

**Decision**: `exact_fixed(bits, prec, out)` in `src/stdprim.c`, next to `exact_digits` and sharing its
2560-bit numbers: scale `m · 2^e` to N/D as `exact_digits` does, take the integer part and then `prec`
fraction digits by repeated `·10`, and round on the remainder against half (ties to even on the last
kept digit, carrying into the integer part). Values below half a unit of the last place give zero with
the sign kept (`-0.00`, as Python and C).

**Rationale**: correct rounding of the exact binary value is what C, Python and Rust print, so the
differential test against Python is exact. It reuses code that already guarantees correct digits for
`str` and JSON (spec 020 replaced a floating-point scaling that was wrong in about 4% of last digits).
The bignums are large enough: N stays below 10·D, and D is at most 2^1074 · 10^0 or 10^308.

**Cost**: the bignum path is about 30 times slower than the 128-bit fast path of `float_to_text`. A
128-bit fast path (as `digits15_u128`) covers `|x| < 2^64` with `prec ≤ 19` and is added only if SC-004
fails without it. ponytail: bignum only; fast path when measured.

**Alternatives considered**: formatting with `str` and rounding the text (double rounding: `2.675` would
print `2.68`); floating-point scaling `x · 10^prec` (rounding errors, and the constitution's
bit-identical rule makes every such shortcut a risk).

## R4. Integers, padding and grouping

**Decision**: `fmt_piece(value, kind, spec)` in `src/num.c` builds: sign (`-`, or `+` with the `+` flag),
digits (decimal via the existing conversion, hex and binary by shifting the magnitude as an unsigned
value so the minimum int works), grouping (`,` every three digits of the integer part, decimal only),
then width: zero flag pads digits after the sign; otherwise spaces by alignment, with the default right
for numbers and left for `str` and `bool`, `^` putting the extra space on the right. Width counts code
points (`cp_count`), like `pad_left`.

Floats without a precision use `float_to_text` (the `str` digits) and accept only width, alignment, `+`,
`0` and `,` around them. NaN and infinities use `str`'s text and are padded with spaces even with `0`.

**Combinations that are check errors (E0317)** because Python gives surprising output for them and they
add a second way to write something: `0` together with an alignment (`{:<05}` is `50000` in Python),
`0` together with `,` (Python groups the padding zeros: `0,001,234`). Each message names the form to use.

## R5. Codes and fixes

**Decision**: five new codes (full text in [contracts/diagnostics.md](contracts/diagnostics.md)):

| Code | Name | Fix |
|---|---|---|
| E0120 | `f_string` | `format("...", holes)` when every hole is a name or field access (with an optional `:spec`) |
| E0315 | `format_template_not_literal` | none |
| E0316 | `format_argument_count` | translation of C `%` verbs when the template has no `{}` and the verbs match the arguments |
| E0317 | `format_placeholder_invalid` | `{0}…{n}` in order → `{}`; all `{name}` and no arguments → moved out; `{:.N}` → `{:.Nf}`; `{:d}`/`{:s}` → letter dropped; `{:0>N}` on an int → `{:0N}` |
| E0318 | `format_option_type` | `.Nf` on an `int` argument → `float(arg)` |

A wrong argument type (a list, a record) is E0301 "Expected `int`, `float`, `bool` or `str`, found `T`."
with the existing conversion fixes where they apply. All diagnostics of one call are reported together;
after E0315, E0316 or a bad placeholder, the arguments are still checked as expressions but not against
specs (no cascade).

**Rationale**: separate codes for separate fixes keep `vo explain` short and precise; E0302 (function
arity) would mislead, since its explanation is about parameters.

**Alternatives considered**: one code for all template mistakes (one long `vo explain` entry, and an
agent cannot tell a count problem from a type problem by code); runtime errors (rejected in the spec).

## R6. `f"..."` in the lexer

**Decision**: when the lexer reads the identifier `f` immediately followed by `"`, it reports E0120 over
`f"..."`, drops the `f`, and lexes the string normally. The fix rewrites holes `{expr}` to `{}` (or
`{:spec}` for `{expr:spec}`) and appends the hole texts as arguments, when every hole is
`name(.name)*`; `{{` and `}}` are kept. With other hole contents the diagnostic has no fix.

**Rationale**: today the habit gives a misleading E0110 ("Expected `)`, found a string"). Lexing the rest
as a plain string means the parser and checker continue with a `str`, so nothing cascades.

## R7. Python as the oracle for digits

**Decision**: `tests/format_diff.py` generates vo programs of 5,000 lines each, `print(format("{:SPEC}",
EXPR));`, runs them, and compares line by line with Python's `format(value, SPEC)`. Values come from
expressions both languages evaluate identically under IEEE 754: int literals; `float(a) / float(b)` with
`|a|, |b| < 2^53` (one correctly rounded division in both); repeated `* 10.0` / `/ 10.0` from those for
extremes; known ties (`0.125`, `2.5`, `2.675`, `1.005`); powers of two; the largest and smallest normal
and subnormal values (built by repeated halving). vo float literals cannot hold an exponent or more than
15 digits, so random doubles are built, not written. Specs: every combination of the supported options
with widths 0–12 and precisions 0–10, restricted to the combinations FR-006 allows. Floats are compared
only for specs with a precision (without one, vo prints `str`'s 15 digits and Python its shortest
round trip; FR-005).

The same programs' outputs are compared across `build/`, `build-aarch64/` and `build-riscv64/` (through
`qemu-user`) for SC-001's cross-platform half.

## R8. Reference text and budget

**Decision**: one builtin-table row, and a "Formatting" paragraph after the strings paragraph with a
four-row option table and one runnable example (drafted in [contracts/language.md](contracts/language.md)).
The strings bullet's "no interpolation: build strings with `+` and `str(...)`" becomes "no interpolation:
use `format` or `+`". The catalog sentence becomes "E0101 to E0120". Estimated +230 words (≈ 300 tokens),
under the 350 of SC-005. `vo doc format` and hover come from the table row (spec 023), with no other
change.

## R9. The name clash in the repository

**Decision**: the h07 word-wrap property fixtures define and import `format`; they are renamed to
`reflow` (definition, calls, `props.vo`'s import). Nothing else in `std/`, `examples/`, `bench/`,
`bench2/` or `site/` defines `format`. Programs outside the repository get E0202 ("`format` is a builtin
and cannot be redefined"), whose message already says what to do.

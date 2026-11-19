# Implementation Plan: String Formatting

**Branch**: `024-string-formatting` (work on `main`, as for specs 001–023) | **Date**: 2026-11-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/024-string-formatting/spec.md`

## Summary

A builtin `format(template, args...)` with Python/Rust brace placeholders (`{}`, `{:<12}`, `{:>8.2f}`,
`{:,}`, `{:05}`, `{:x}`). The checker parses the template literal once, checks every placeholder against
its argument's type, and attaches the parsed pieces to the call. The compiler lowers the call to existing
instructions: string constants for the literal pieces, one internal builtin per placeholder that formats a
single value, and string concatenation. Nothing new at run time can fail, so there is no runtime code.

**Where each part lives** (research R1–R4):

| Part | File | What |
|---|---|---|
| Template parsing and checks | `src/check_format.c` (new) | parse the literal's source text into pieces; E0315–E0318 with fixes; `%`-habit translation; attaches pieces to `call->e.p` |
| `f"..."` habit | `src/lex.c` | E0120 with the `format(...)` fix; the token is then lexed as a plain string (no cascade) |
| Builtin ids | `src/check_expr.c`, `src/vo.h`, `src/rt_call.c`, `src/check_effects.c` | `format` (82, public) and `__fmt_piece` (83, unnamed); both pure |
| Lowering | `src/compile.c` | `cformat`: constants, one `CALL_BUILTIN __fmt_piece` per placeholder, `OP_CONCAT` |
| Value formatting | `src/num.c` | `fmt_piece(value, kind, spec)`: ints (decimal, hex, binary, grouping), floats with `.Nf` (exact digits via `exact_digits`), padding and alignment by code points |
| Exact fixed digits | `src/stdprim.c` | `exact_fixed(bits, prec, out)`: the exact decimal expansion rounded at 10^-prec, ties to even (a sibling of `exact_digits` on the same bignums) |

**Diagnostics** (research R5, [contracts/diagnostics.md](contracts/diagnostics.md)):
E0120 `f_string`, E0315 `format_template_not_literal`, E0316 `format_argument_count` (its fix translates
C `%` verbs), E0317 `format_placeholder_invalid` (fixes for `{0}`, `{name}`, `{:.2}`, `{:d}`, `{:0>5}`),
E0318 `format_option_type` (fix `float(n)` for `.Nf` on an int). Wrong argument types use E0301.

## Technical Context

**Language/Version**: C (C11, freestanding), vo for golden cases.

**Primary Dependencies**: none new. Reuses the 2560-bit bignums of `src/stdprim.c`.

**Storage**: none (pure; nothing recorded or replayed).

**Testing**:
- golden cases `tests/cases/format/`: every acceptance scenario of the spec (run cases with `.out`; check
  cases with JSONL and fixes);
- `tests/apply_fixes.py` gains the `format` directory, so every fix is applied and re-checked (SC-002);
- `tests/format_diff.py`: 100,000 values against Python's `format()` (SC-001), plus the cross-platform
  comparison under `make ARCH=aarch64|riscv64`;
- `tests/ref_examples.py` runs the new reference example and checks the budget (SC-005);
- `tests/catalog.py` requires a catalog entry and a golden case per new code (SC-003);
- `make quick AREA=format` between edits (`tools/quick.sh` gains the area); `make test` once per change.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: SC-004: formatting 1,000,000 values no slower than the equivalent `+`/`str` chain
(the lowering is that chain, with one call per placeholder instead of `str` plus padding helpers).

**Constraints**:
- Principle III: integer arithmetic and bignums only; no floating-point operation formats a float, so
  output is bit-identical on every platform.
- Principle V: the reference grows by at most 350 tokens (about 12,700 of 15,000 used).
- No runtime error is possible for a checked call; precision ≤ 100 and width ≤ 1000 bound every result.

**Scale/Scope**:
- C: about 450 lines (`check_format.c` about 280 with the `%` translator and fixes; `fmt_piece` and
  `exact_fixed` about 130; lowering about 30; lexer about 40);
- reference: one builtin row and a short "Formatting" subsection with an option table and one example;
- catalog: 5 entries; about 30 golden cases; 1 script; renaming `format` in the h07 property fixtures.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate | Status |
|---|---|
| I. Agent-first | PASS. One placeholder form (`{}` positional, one fixed order of options); no indexed or named placeholders, no `%` verbs, no interpolation. Habits from other languages are diagnostics with fixes, not alternative spellings. No implicit conversion: a `.2f` on an int is an error whose fix is `float(n)`. |
| II. Machine-readable feedback | PASS. Five new stable codes, each with a span inside the template (the exact placeholder), a one-sentence message and a fix where one correction is unambiguous; all mistakes of a template are reported in one pass. |
| III. Determinism and defined behavior | PASS. Formatting uses integers and bignums only; no locale or environment. A checked call cannot fail at run time (bounded width and precision); out of memory is the existing R0006. |
| IV. Test-first | PASS. Golden cases first; every new code has a case; the reference example runs; the differential test against Python is the oracle for digits. |
| V. Minimal surface | PASS with the note below. One builtin name and about 300 reference tokens. YAGNI: no `e`, `%`, fill characters, `#`, debug printing of records or lists, or a writing `printf`. |
| Implementation constraints | PASS. C only, no assembly, no vendored code, no floating point in formatting. |

Note on V: `format` is the language's only variadic call, and its first argument must be a literal. Both
are special cases in the checker; see Complexity Tracking.

Post-design re-check: unchanged (all PASS). The design adds no instruction to the VM and no runtime error.

## Project Structure

### Documentation (this feature)

```text
specs/024-string-formatting/
├── plan.md              # this file
├── research.md          # R1–R9: decisions and rejected alternatives
├── data-model.md        # template pieces, the packed spec, the lowering
├── quickstart.md        # validation scenarios
├── contracts/
│   ├── language.md      # the builtin, the placeholder grammar, the output rules, the reference text
│   └── diagnostics.md   # E0120, E0315–E0318: messages, spans, fixes; the `%` translation table
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/check_format.c       # new: template parser, checks, fixes, `%` and `{name}` translation
src/check_expr.c         # builtin names/ids 82 format, 83 (unnamed piece); B_FORMAT → check_format_call
src/lex.c                # E0120 for f"..."
src/compile.c            # cformat lowering
src/num.c                # fmt_piece: one value by its packed spec
src/stdprim.c            # exact_fixed: digits to a fixed decimal position
src/rt_call.c            # BU_FORMAT (unreachable), BU_FMT_PIECE
src/vo.h                 # ids, B_LAST, prototypes, FmtPiece
src/check_effects.c      # builtin_effects entries (none)
Makefile                 # check_format.c in the source list
docs/reference.md        # builtin row, "Formatting" subsection, E0101–E0120 range sentence
docs/errors.md           # E0120, E0315–E0318
docs/roadmap.md, README.md   # gap row → done; status line
tests/cases/format/      # golden cases (run and check)
tests/format_diff.py     # SC-001 differential test against Python
tests/apply_fixes.py     # + the format directory
tools/quick.sh           # area `format`
tests/prop_eval/h07-word-wrap/   # rename fn format → reflow (ref, bugs, props.vo)
```

**Structure Decision**: one new checker file, because the template parser and its fixes are about as large
as `check_generic.c` and unrelated to the rest of `check_expr.c`. Everything else is a small addition to
the file that owns that stage.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| The only variadic builtin, with a literal-only first argument | Each argument keeps its own static type, so a spec can be checked against it and every mistake is a check error | `format(t, [str(a), str(b)])`: a `list[str]` forces `str(...)` on every value (no `.2f` possible on a string) and loses the types; one builtin per type (`fmt_float(x, 2)`) is several names and no columns |
| A new checker file (`check_format.c`) | The parser, five codes and three fix translators are about 280 lines | Inside `check_expr.c`: that file is already the largest checker file and the code shares nothing with it |

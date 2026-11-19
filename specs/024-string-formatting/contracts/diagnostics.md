# Contract: formatting diagnostics

All are errors (exit 65), reported in one pass with the rest of the file. Spans are byte ranges in the
source; "placeholder" means `{` to `}` inclusive inside the template literal. `$n` are filled in.

## E0120 `f_string`

- **Span**: `f"..."` (the `f` and the whole literal).
- **Message**: "vo has no f-strings: use `format` with `{}` placeholders and the values as arguments."
- **Fix** (when every hole is `name(.name)*`, optionally followed by `:spec`): replace with
  `format("TEXT", HOLE1, HOLE2, ...)`, each hole becoming `{}` or `{:spec}`; `{{`/`}}` kept.
  `f"hi {user.name}, {n:>3}"` → `format("hi {}, {:>3}", user.name, n)`.
- **After**: the literal is lexed as an ordinary string, so the rest of the file checks normally.

## E0315 `format_template_not_literal`

- **Span**: the first argument.
- **Message**: "The template of `format` must be a string literal, so its placeholders can be checked."
- **Fix**: none. (`format` with no arguments at all is E0302: "`format` takes a template and the values
  for its placeholders, got no arguments.")

## E0316 `format_argument_count`

- **Span**: the template literal. A template with no `{}` or `{:...}` placeholder whose `%` verbs all
  translate gets E0316 with the fix even when it has braces (`"{%d}"` → `"{{{}}}"`), instead of E0317
  for the braces.
- **Message**: "The template has $1 placeholder(s) and $2 argument(s) follow it."
- **Fix**: only for the C habit: the template has no `{}` placeholders, and its `%` verbs (below) are as
  many as the arguments and all translatable. Replaces the template literal.

| C / Go verb | Argument | Becomes |
|---|---|---|
| `%d` `%i` `%s` `%v` `%t` | any | `{}` |
| `%5d`, `%5s` | number / `str` or `bool` | `{:5}` / `{:>5}` (C right-aligns strings) |
| `%-5d`, `%-5s` | number / `str` or `bool` | `{:<5}` / `{:5}` |
| `%05d` | number | `{:05}` |
| `%+d` | number | `{:+}` |
| `%.2f`, `%8.2f`, `%f` | `float` | `{:.2f}`, `{:8.2f}`, `{:.6f}` (C's default precision) |
| `%x`, `%b` | `int` | `{:x}`, `{:b}` |
| `%%` | — | `%` |
| `{`, `}` in the text | — | `{{`, `}}` |

Any other verb (`%X`, `%e`, `%g`, `%q`, `%c`, `%p`, `*` widths), or a verb that does not fit its
argument's type: no fix.

## E0317 `format_placeholder_invalid`

- **Span**: the placeholder, or the lone brace.
- **Messages and fixes**:

| Case | Message | Fix |
|---|---|---|
| lone `{` or `}` | "Unmatched `$1` in the template: write `{{` or `}}` for a brace." | none |
| `{0}`, `{1}`, ... | "Placeholders take the arguments in order: write `{}` (no index)." | when all indexes are 0, 1, 2, ... in order: each → `{}` |
| `{name}`, `{a.b}` | "Placeholders take the arguments in order: write `{}` and pass `$1` as an argument." | when every placeholder is named and no argument follows: each → `{}`, names appended as arguments |
| `{:.2}` | "Precision needs `f`: `{:.2f}` is 2 decimals." | `.N` → `.Nf` |
| `{:d}`, `{:s}` | "`$1` is not needed: the argument's type decides how it prints. Write `{}`." | letter removed |
| `{:0>5}` (fill character) | "Fill characters are not supported: use `0` for zeros or spaces by alignment." | on an `int` or `float` with fill `0` and `>`: `{:05}` |
| `0` with alignment, `0` with `,` | "`0` pads numbers by itself: drop the $1." | none |
| `,` or a precision with `x`/`b` | "`x` cannot be combined with `,`." (or "with a precision") | none |
| width > 1000, precision > 100 | "Width is at most 1000 and precision at most 100." | none |
| anything else (`e`, `%`, `#`, `_`, `X`, a space sign, unknown order) | "Unsupported option `$1`: the options are `<` `>` `^` `+` `0` width `,` `.Nf` `x` `b`, in that order." | none |

## E0318 `format_option_type`

- **Span**: the placeholder; for the fix, the argument.
- **Message**: "`$1` applies to $2, but the argument is `$3`."
- **Fix**: `.Nf` with an `int` argument → wrap the argument in `float(...)`. Otherwise none.

## E0301 (existing) for argument types

"Expected `int`, `float`, `bool` or `str`, found `$1`." at the argument, with the existing conversion
fixes where they apply.

## Catalog

Each code gets an entry in `docs/errors.md` (name, one paragraph, Fixable, Example, Fixed) and at least
one golden case in `tests/cases/format/` (`tests/catalog.py`). E0120's example is `f"..."`; E0316's
fixed example shows the `%` translation.

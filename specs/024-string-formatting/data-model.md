# Data Model: String Formatting

Nothing here is a runtime value a program can see: these are the checker's and compiler's views of one
`format` call. The program sees only the resulting `str`.

## Template (check time)

The source bytes between the quotes of the first argument, an `N_STR` literal. Decoded with the four
string escapes while parsing, so every piece knows its source byte range.

| Piece | Holds | Source range |
|---|---|---|
| literal | the text, with `{{` → `{` and `}}` → `}` and escapes decoded (a `Str`, made in the checker like any literal) | the run of plain text |
| placeholder | the argument index (its position among placeholders) and the packed spec | `{` to `}` inclusive |

Stored as a `Vec` of `FmtPiece` on the call node (`call->e.p`); empty literal pieces are dropped.

Validation (each a diagnostic, [contracts/diagnostics.md](contracts/diagnostics.md)):
- the first argument is a string literal (E0315);
- every `{` starts `{{` or a placeholder that closes on the same literal; every `}` is `}}` or closes a
  placeholder (E0317);
- the placeholder matches the grammar of [contracts/language.md](contracts/language.md) (E0317);
- width ≤ 1000, precision ≤ 100 (E0317);
- placeholders = arguments after the template (E0316);
- each option fits its argument's type (E0318); each argument is `int`, `float`, `bool` or `str` (E0301).

## Packed spec (an `int` on the synthetic piece node, `e.i`)

| Bits | Field | Values |
|---|---|---|
| 0–1 | align | 0 default, 1 `<`, 2 `>`, 3 `^` |
| 2 | plus | `+` given |
| 3 | zero | `0` given |
| 4–13 | width | 0–1000 (0: none) |
| 14 | group | `,` given |
| 15 | has precision | `.Nf` given |
| 16–22 | precision | 0–100 |
| 23–24 | radix letter | 0 none, 1 `x`, 2 `b` |

The value's kind (`int`, `float`, `bool`, `str`) is not packed: it is the static type of the piece's
argument (`arg0_type`), as `str(x)` reads it.

## Lowering (compile time)

`format(T, a1, ..., an)` with pieces p1..pk compiles to

```text
r = LOADK p1            (literal)   |   r = CALL_BUILTIN __fmt_piece(a_i)   (placeholder, spec in the node)
r = CONCAT r, <next piece>          (k - 1 times)
```

| Builtin id | Name | Called by | Effects |
|---|---|---|---|
| 82 | `format` | user code; never reaches `builtin_call` (lowered) | none |
| 83 | (unnamed) | the lowering only, one synthetic node per placeholder | none |

## Formatting one value (run time, `fmt_piece`)

| Kind | Digits | Then |
|---|---|---|
| `int` | decimal (`,` grouping allowed), or `x` / `b` of the magnitude, sign in front | `+` sign, then zero padding after the sign or space padding by alignment (default right) |
| `float`, precision | `exact_fixed`: the exact value rounded at 10^-N, ties to even; `,` groups the integer part | as `int`; NaN and infinities: `str`'s text, space padding only |
| `float`, no precision | `str`'s text (15 significant digits); `,` groups the integer part | as `int` |
| `bool` | `true` / `false` | space padding by alignment (default left) |
| `str` | the string | space padding by alignment (default left) |

Width is in code points; a longer value is never cut.

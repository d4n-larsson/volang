# Contract: `format`

## Builtin

| Builtin | Signature | Notes |
|---|---|---|
| format | `(template: str, args...) -> str` | `template` a string literal; each `{}` or `{:spec}` takes the next argument (`int`, `float`, `bool` or `str`) |

Pure (no effect). Never fails at run time except out of memory (R0006). `format` is a builtin name:
defining a function `format` is E0202.

## Template grammar

```text
template     = { text | "{{" | "}}" | placeholder }
placeholder  = "{" [ ":" spec ] "}"
spec         = [ align ] [ "+" ] [ "0" ] [ width ] [ "," ] [ "." precision "f" ] [ radix ]
align        = "<" | ">" | "^"
width        = digit { digit }          (1–1000; a leading 0 is the zero flag)
precision    = digit { digit }          (0–100)
radix        = "x" | "b"
```

`text` is any characters other than `{` and `}`; string escapes (`\n`, `\t`, `\\`, `\"`) mean what they
mean in any string. Placeholders take the arguments after the template in order; their numbers must be
equal.

## Options by argument type

| Option | `int` | `float` | `bool` | `str` |
|---|---|---|---|---|
| width, `<` `>` `^` | yes | yes | yes | yes |
| `+`, `0`, `,` | yes | yes | — | — |
| `.Nf` | — (fix: `float(n)`) | yes | — | — |
| `x`, `b` | yes | — | — | — |

Not allowed together: `0` with an alignment; `0` with `,`; `,` with `x` or `b`; `.Nf` with `x` or `b`.

## Output

| Rule | Example |
|---|---|
| `{}` is `str(x)` for numbers and bools, the string itself for `str` | `format("{} {}", 2.5, true)` is `"2.5 true"` |
| `.Nf`: exactly N decimals, the exact binary value rounded, ties to even | `{:.2f}` of `2.675` is `"2.67"`; of `-0.001` is `"-0.00"`; `{:.0f}` of `2.5` is `"2"` |
| default alignment: numbers right, `str` and `bool` left | `format("[{:5}][{:5}]", 42, "ab")` is `"[   42][ab   ]"` |
| `^` centers, the extra space on the right | `{:^5}` of `"ab"` is `" ab  "` |
| `0` pads with zeros after the sign | `{:05}` of `-42` is `"-0042"` |
| `+` shows the sign of non-negative numbers | `{:+}` of `5` is `"+5"`; `{:+.1f}` of `-0.25` is `"-0.2"` |
| `,` groups the integer part by three | `{:,.2f}` of `1234567.891` is `"1,234,567.89"` |
| `x`, `b`: the magnitude in hex (lowercase) or binary, with `-` for negatives | `{:x}` of `-255` is `"-ff"`; `{:08b}` of `5` is `"00000101"` |
| width counts characters (code points), never cuts | `{:3}` of `"héllo"` is `"héllo"` |
| NaN and infinities print as `str` prints them, space-padded even with `0` | `{:06.2f}` of `inf` is `"   inf"` |
| `{{` and `}}` are braces | `format("{{}} {}", 1)` is `"{} 1"` |

For `int` values and for `float` values with a precision, the output equals Python's
`format(value, spec)` for the same spec. No locale or environment variable changes any output.

## Reference text (draft, counted against the budget)

Strings bullet, last sentence: "There are no single-quoted strings, no character type, no raw or
multi-line strings and no interpolation: use `format` or `+`."

New paragraph after the scalar types table:

> **Formatting**: `format("{} of {} done", 3, 10)` is `"3 of 10 done"`. The template must be a string
> literal; each `{}` takes the next argument (`int`, `float`, `bool` or `str`), and `{{`/`}}` are braces.
> Options go after a colon, in this order: alignment `<` `>` `^`, `+`, `0` (zero-pad), width, `,`
> (thousands), `.Nf` (N decimals, floats only), `x` or `b` (hex or binary, ints only). Numbers align
> right, text left. `vo check` reports a wrong count or option with a fix (also for `%d` and `f"..."`).
>
> ```vo
> fn main() uses console {
>     print(format("{:<8}|{:>8.2f}|{:05}|{:x}", "tea", 3.14159, 42, 255));
>     print(format("{:,} {:+}", 1234567, 5));
> }
> ```
>
> ```text
> tea     |    3.14|00042|ff
> 1,234,567 +5
> ```

Catalog sentence: "E01xx syntax (E0101 to E0120)".

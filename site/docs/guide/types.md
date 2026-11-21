# Types

The built-in types `int`, `float`, `bool` and `str`, how numbers overflow and print, why strings are bytes, and how to convert between types explicitly.

vo is statically typed: every expression has one type, known before the program runs. Types are inferred
inside functions and written out in signatures. There are no implicit conversions of any kind, so a
mismatch is always a check error that points at the exact place, never a silent change of value. The
rules are in the reference section [Types](../reference/types.md).

## The built-in types

| Type | Values | Literals |
|---|---|---|
| `int` | 64-bit signed integers, -9223372036854775808 to 9223372036854775807 | `0`, `42`, `-5` |
| `float` | 64-bit IEEE 754 floating point | `1.0`, `0.5`, `-3.25` |
| `bool` | `true`, `false` | `true`, `false` |
| `str` | immutable UTF-8 text | `"hello"` |

Types build on these: `list[int]`, `map[str, list[float]]`, `result[str]`, records and variants you
declare, and function types. They are covered in [Lists and maps](lists-and-maps.md),
[Collections](collections.md), [Results and errors](results-and-errors.md),
[Records and variants](records-and-variants.md) and [Functions](functions.md). There is no `null`, no
`any`, no tuple type and no character type.

## Inference and annotations

A `let` or `var` takes the type of its initializer. Add `: type` after the name to state it:

```vo
fn main() uses console {
    let count = 3;
    let ratio: float = 0.75;
    var label: str = "items";
    set label = label + ": " + str(count);
    print(label + " at " + str(ratio));
}
```

```output
items: 3 at 0.75
```

An annotation never converts: it is a check that the initializer has that type. `let x: float = 1;` is
[E0301](../errors/E0301.md), because `1` is an `int` (the fix rewrites it to `float(1)`; writing `1.0`
is simpler).

Annotations are needed where the initializer alone does not say enough: an empty list
(`let xs: list[int] = new list[int];` or simply `new list[int]`, since `[]` is
[E0306](../errors/E0306.md)), a result built with `ok(...)` ([E0307](../errors/E0307.md)) and a typed hole
(`todo`). Function parameters and return types are always written.

## int

`int` is a 64-bit signed integer. Arithmetic that leaves the range does not wrap around: it stops the
program with runtime error [R0001](../errors/R0001.md) (exit status 70), and the report shows the values
involved.

```vo
fn main() uses console {
    let big = 3037000500;
    print(str(big * big));
}
```

```error R0001
```

The same holds for `-x`, `abs(x)` and `x / -1` when `x` is the smallest `int`. A literal larger than the
maximum is a check error, [E0105](../errors/E0105.md); the smallest `int` can only be written as an
expression, `-9223372036854775807 - 1`. Integer division truncates toward zero, and division by zero is
[R0002](../errors/R0002.md) (see [Basics](basics.md#expressions-and-operators)).

## float

`float` is an IEEE 754 double. Arithmetic follows IEEE: `1.0 / 0.0` is `inf`, `0.0 / 0.0` is `nan`, and
`nan` is not equal to anything, itself included.

`str` of a float is the shortest text that reads back as the same value (`parse_float(str(x))` is
`ok(x)`), at most 17 significant digits. It always shows a `.`, so a float never prints like an integer. There is no exponent notation: large and small values are written out in full.

```vo
fn main() uses console {
    print(str(0.1 + 0.2) + " " + str(1.0 / 3.0) + " " + str(2.0 / 3.0));
    print(str(100.0) + " " + str(2.5) + " " + str(-0.0));
    print(str(1000000.0 * 1000000.0 * 1000000.0) + " " + str(1.0 / 1000000.0 / 10.0));
    print(str(1.0 / 0.0) + " " + str(-1.0 / 0.0) + " " + str(0.0 / 0.0));
    print(str(0.1 + 0.2 == 0.3));
}
```

```output
0.30000000000000004 0.3333333333333333 0.6666666666666666
100.0 2.5 -0.0
1000000000000000000.0 0.0000001
inf -inf nan
false
```

The first and last lines show the same thing: `0.1 + 0.2` is not exactly `0.3`, and its text says so.
For a fixed number of decimals use `format("{:.2f}", x)`. Compare floats against a tolerance,
`abs(a - b) < 0.000001`, when exact equality is not what you mean.

A float literal has at most 17 significant digits ([E0105](../errors/E0105.md) otherwise), needs digits on
both sides of the `.`, and may have an exponent: `1e9`, `1.5e-3`.

## bool

`bool` is `true` or `false`. Conditions in `if` and `while` must be `bool`; nothing else is "truthy"
([E0301](../errors/E0301.md)). `and`, `or` and `not` work only on `bool`, and `<`, `>` do not apply to it.

## str

A `str` is immutable UTF-8 text. Strings are compared and measured as bytes:

- `len(s)` is the number of bytes, not characters.
- `slice(s, a, b)` takes bytes `a` to `b - 1`. A boundary that falls inside a multi-byte character is
  runtime error [R0007](../errors/R0007.md); one beyond the end of the string is [R0003](../errors/R0003.md).
- `<`, `<=`, `>`, `>=` compare bytes, so `"Zebra" < "apple"`.
- Strings cannot be indexed: `s[0]` is [E0308](../errors/E0308.md).

To work with characters (Unicode code points), loop over the string with `for c in s`, or split it with
`chars(s)`, which returns a `list[str]` of one-character strings. `ord(c)` gives the code point of a one-character string and
`chr(i)` the reverse. To scan a long text, read it in place: `byte_at(s, i)` is byte `i` as an `int`
(0 to 255), and makes no list.

```vo
fn main() uses console {
    let word = "héllo";
    print(str(len(word)) + " bytes, " + str(len(chars(word))) + " characters");
    print(chars(word)[1] + " " + str(ord("é")) + " " + chr(233));
    print(slice(word, 0, 3) + "|" + str(find(word, "llo")));
    print(upper(word));
}
```

```output
6 bytes, 5 characters
é 233 é
hé|3
HÉLLO
```

`upper` and `lower` change Latin, Greek and Cyrillic letters; a letter whose other case is several
characters, such as `ß`, stays as it is. Operations that need byte
offsets, like `find` and `slice`, agree with each other, so `slice(s, find(s, x), len(s))` is always
safe when `find` succeeds. The string builtins (`split`, `join`, `trim`, `find`, ...) are listed in
[Builtins](../reference/builtins.md), and [`std.text`](../std/text.md) has more, such as
[`pad_left`](../std/text.md#pad_left).

Strings are built with `+` and `str(...)`, or with `format`, which puts values into a template. There is
no interpolation: the template is a string literal, each `{}` takes the next argument, and options after
a colon set the width, the alignment, the number of decimals and more:

```vo
fn main() uses console {
    let name = "tea";
    let price = 3.14159;
    print(format("{:<8}{:>6.2f}", name, price));
    print(format("{} items, {:+} since {:,}", 3, -2, 1250000));
    print(format("{:05} {:x} {{}}", 42, 255));
}
```

```output
tea       3.14
3 items, -2 since 1,250,000
00042 ff {}
```

`vo check` checks the template against the arguments: a missing argument, `{:.2f}` on an `int` or a
`%d` from C each get a diagnostic, most with a fix. The options are listed in the
[reference](../reference/types.md).

## Conversions

vo converts nothing implicitly: not `int` to `float`, and not anything to `str`. Each conversion is a
function call you can see:

| Call | From | To | Notes |
|---|---|---|---|
| `str(x)` | `int`, `float`, `bool` | `str` | the shortest text that reads back as the same float |
| `float(i)` | `int` | `float` | |
| `int(f)` | `float` | `int` | truncates toward zero; [R0007](../errors/R0007.md) for `nan`, `inf` or out of range |
| `parse_int(s)` | `str` | `result[int]` | the whole string must be the number |
| `parse_float(s)` | `str` | `result[float]` | the whole string must be the number |

`str` takes only `int`, `float` and `bool`: `str("x")` is an error, since the value is already a string.
To show a list or a record, build the text yourself, for example with `join(map(xs, ...), ", ")` (see
[Functions](functions.md)).

Mixing `int` and `float` in arithmetic is the most common type error. Convert one side:

```vo
fn main() uses console {
    let total = 7;
    let count = 2;
    let mean = total / count;
    print(str(mean));
}
```

```output
3
```

```vo
fn main() uses console {
    let total = 7;
    let count = 2;
    let mean = float(total) / float(count);
    print(str(mean) + " " + str(int(mean)) + " " + str(int(-3.9)));
}
```

```output
3.5 3 -3
```

```vo
fn main() uses console {
    let count = 2;
    print(str(7.0 / count));
}
```

```error E0301
```

### Parsing text

`parse_int` and `parse_float` return a `result`, because text may not be a number. The whole string must
be the number: no spaces (use `trim` first) and no `+` sign; `parse_float` accepts an exponent (`"1e9"`).
`parse_float("2")` succeeds with
`2.0`.

```vo
fn describe(text: str) -> str {
    match parse_int(trim(text)) {
        case ok(n) {
            return str(n * 2);
        }
        case err(e) {
            return e;
        }
    }
}

fn main() uses console {
    print(describe("21"));
    print(describe(" 42 "));
    print(describe("+5"));
    print(describe("3.0"));
}
```

```output
42
84
invalid int: +5
invalid int: 3.0
```

How to handle and pass on a `result` is the subject of [Results and errors](results-and-errors.md).

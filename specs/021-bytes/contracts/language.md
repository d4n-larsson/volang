# Contract: the `bytes` type

## Type

`bytes`: an immutable sequence of integers 0–255. Keyword and type name. No literal.

## Operations

| Form | Type | Error |
|---|---|---|
| `len(b)` | `int` | — |
| `b[i]` | `int` (0–255) | R0003 "Index i is out of range for bytes of length n." |
| `slice(b, a, c)` | `bytes` (bytes `a` to `c - 1`) | R0003 |
| `b + c` | `bytes` | — |
| `b == c`, `b != c` | `bool` | — |
| `b < c` (and `<=`, `>`, `>=`) | `bool`, byte by byte, prefix first | — |
| `sort(xs: list[bytes])`, `min`, `max`, `T: ordered`, ordered collections | as `str` | — |
| `expect_eq(b, c)` | — | the report gives the first differing byte offset |

Type errors (E0301/E0302), each with a fix where one conversion fits:
- `b + "x"`, `"x" + b`: fix `std.bytes.from_text("x")`;
- `print(b)`, `str(b)`: fix `std.bytes.to_hex(b)` (`str(b)` names both `to_text` and `to_hex`; no fix,
  since two fit);
- `for x in b`: message names `std.bytes.to_ints(b)`;
- `map[bytes, T]`: key type error, message names `to_hex`.

## Strict text

Every `str` is well-formed UTF-8 (Unicode Table 3-7). The reference sentence:

> `str` is always valid UTF-8: text from files, standard input, arguments, the environment, programs and
> HTTP is checked when it is read (an `err`, or R0007 where the function returns no `result`, naming the
> byte offset). Read data that may not be text as `bytes`.

## Rendering

`<N bytes: HEX>` with HEX the lowercase hex of the first 32 bytes, followed by `...` when N > 32;
`<0 bytes>` for empty. Used in traces, locals, test reports and JSON value fields.

# Contract: vo-lang Grammar and Builtins (v1)

This is the authoritative syntax for v1. `docs/reference.md` teaches the same rules with examples.

## Lexical rules

- Source files are UTF-8. Whitespace (space, tab, newline) only separates tokens. Indentation means nothing.
- **Comments**: A comment is a line whose first non-blank character is `#`. The comment runs to the end of that line. A `#` after code on the same line is an error (E0101).
- **Identifiers**: `[a-z][a-z0-9_]*` for functions, variables, fields and variant cases. `[A-Z][A-Za-z0-9]*` for records and variants. Breaking these rules is E0207, which comes with the converted name as a fix. A lone `_` is the discard name: it is allowed only as a `let` name, a `case` binding or a `for` variable (`for _ in 0..n` repeats n times), and it is never bound.
- **Integers**: `0` or `[1-9][0-9]*`. Values above 2^63−1 are E0105. Negative numbers are written with unary `-`.
- **Floats**: `[0-9]+ "." [0-9]+`, with at most 15 significant digits and no exponent.
- **Strings**: `"…"` on a single line. The only escapes are `\n \t \\ \"`. Any other escape is E0103.
- **Keywords**: `fn record variant test import let var set if else while for in match case return break continue try new and or not true false int float bool str list map result void`

## Syntax (EBNF)

```ebnf
file      = { import } { item } ;
import    = "import" ident { "." ident } ";" ;
item      = func | record | variant | test ;
func      = "fn" ident "(" [ param { "," param } ] ")" [ "->" type ] block ;
param     = ident ":" type ;
record    = "record" Type "{" field { field } "}" ;
field     = ident ":" type ";" ;
variant   = "variant" Type "{" case { case } "}" ;
case      = ident [ "(" type ")" ] ";" ;
test      = "test" string block ;

type      = "int" | "float" | "bool" | "str" | Type
          | "list" "[" type "]"
          | "map" "[" type "," type "]"
          | "result" "[" ( type | "void" ) "]" ;

block     = "{" { stmt } "}" ;
stmt      = "let" ( ident | "_" ) [ ":" type ] "=" expr ";"
          | "var" ident [ ":" type ] "=" expr ";"
          | "set" place "=" expr ";"
          | "if" expr block [ "else" ( block | ifstmt ) ]
          | "while" expr block
          | "for" ( ident | "_" ) "in" expr [ ".." expr ] block
          | "match" expr "{" arm { arm } "}"
          | "return" [ expr ] ";"
          | "break" ";" | "continue" ";"
          | call ";"
          | "try" call ";" ;
arm       = "case" ident [ "(" ( ident | "_" ) ")" ] block ;
place     = ident { "." ident | "[" expr "]" } ;

expr      = and { "or" and } ;
and       = not { "and" not } ;
not       = "not" not | cmp ;
cmp       = sum [ ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) sum ] ;   (* no chaining *)
sum       = prod { ( "+" | "-" ) prod } ;
prod      = unary { ( "*" | "/" | "%" ) unary } ;
unary     = "-" unary | "try" unary | postfix ;
postfix   = primary { "." ident | "[" expr "]" } ;
primary   = int | float | string | "true" | "false"
          | call | ident | Type "." ident [ "(" expr ")" ]          (* variant case *)
          | ( "str" | "int" | "float" ) "(" expr ")"              (* conversions: these names are keywords *)
          | "(" expr ")"
          | "[" expr { "," expr } "]"
          | "new" ( Type "{" finit { "," finit } "}"
                  | "list" "[" type "]"
                  | "map" "[" type "," type "]" ) ;
call      = ident "(" [ expr { "," expr } ] ")" ;
finit     = ident ":" expr ;
```

Each statement kind can be identified from its first token (FR-002). There are no trailing commas and no empty `[]` (use `new list[T]`, E0306). A `cmp` cannot be chained (`a < b < c` is E0110).

## Semantics that the grammar does not show

- **Types**: There are no implicit conversions. Both operands of an arithmetic operator must have the same type (`int` or `float`). `+` also concatenates two `str` values. `%` works on `int` only. `/` on `int` truncates toward zero. `and`, `or` and `not` take `bool` only and short-circuit.
- **Equality**: `==` and `!=` compare structurally for every type. Float comparison follows IEEE, so `nan != nan`.
- **Bindings**: `let` cannot be reassigned. `var` can be reassigned with `set`. Lists, maps and records are references: `let` only stops rebinding, it does not stop mutation. Shadowing is E0203. Locals that are never used are W0201.
- **for**: `for x in xs` iterates over a list. `for i in a..b` iterates over the ints from `a` up to but not including `b`. To iterate over a map, use `for k in keys(m)`.
- **match**: Works on variant and result values. The arms must cover every case exactly once (E0401, E0402). A case with a payload binds it with `case name(binding)`. `result[T]` has the cases `ok(T)` (`ok` with no binding for `result[void]`) and `err(str)`.
- **result**: Build one with `ok(v)`, `ok()` or `err(msg)`. The expected type must be known from a declared return type or type annotation (E0307 otherwise). `try e` evaluates to the `ok` payload, or returns the `err` from the enclosing function, which must itself return a `result` (E0406). Calling a function that returns a `result` as a statement, or binding its value to `_`, is E0404.
- **`try` statements**: `try f();` propagates an `err` and discards the `ok` payload. It is allowed only when that payload is `void`; otherwise it is E0405, with the fix `let _ = try f();`.
- **Unused values**: A non-void call used as a statement is E0405. Discard the value with `let _ = f();`. This form is not allowed for `result`.
- **Functions**: Every path through a function with a return type must reach a `return` (E0403). There is no overloading and there are no first-class functions.
- **Entry point**: `fn main()` or `fn main() -> result[void]`. When `main` returns an `err`, its message is printed to stderr as `error: <msg>` and the program exits with status 1.
- **Imports**: `import a.b.name;` loads `a/b.vo`, resolved relative to the importing file's directory, and binds `name`, which must be a top-level item of that file. Every top-level item is importable. Paths cannot use `..`. Import cycles are E0206.

## Builtins

`T` means any type. `K` must be `int` or `str`. `C` must be `int`, `float` or `str`.

| Builtin | Signature | Notes |
|---|---|---|
| print | `(s: str)` | stdout, appends `\n` |
| write | `(s: str)` | stdout, no newline |
| eprint | `(s: str)` | stderr, appends `\n` |
| read_line | `() -> result[str]` | removes the trailing `\n` (and a `\r` before it); at EOF returns `err("eof")` |
| read_all | `() -> str` | the rest of stdin |
| read_file | `(path: str) -> result[str]` | |
| write_file | `(path: str, s: str) -> result[void]` | |
| args | `() -> list[str]` | the arguments after `--` |
| exit | `(code: int)` | 0–63, otherwise R0008 |
| now_ms | `() -> int` | the only source of nondeterminism |
| str | `(x: int \| float \| bool) -> str` | floats keep at most 15 significant digits |
| int | `(f: float) -> int` | truncates; R0007 for NaN or out of range |
| float | `(i: int) -> float` | |
| parse_int, parse_float | `(s: str) -> result[int]` / `result[float]` | the whole text must be the number (no spaces or `+`; `trim` first) |
| len | `(x: str \| list[T] \| map[K,T]) -> int` | for `str`: bytes |
| slice | `(s: str, a: int, b: int) -> str`, `(xs: list[T], a: int, b: int) -> list[T]` | half-open range; R0003 if out of range, R0007 if it splits a UTF-8 character |
| find | `(s: str, sub: str) -> int` | byte offset of the first match, or −1 |
| split | `(s: str, sep: str) -> list[str]` | an empty `sep` is R0007 |
| join | `(xs: list[str], sep: str) -> str` | |
| trim | `(s: str) -> str` | removes ASCII whitespace from both ends |
| chars | `(s: str) -> list[str]` | one string per code point |
| upper, lower | `(s: str) -> str` | ASCII letters only |
| ord | `(s: str) -> int` | code point of a one-character string; R0007 otherwise |
| chr | `(i: int) -> str` | the character for a code point; R0007 if invalid |
| abs | `(x: int \| float) -> same` | `abs` of the smallest int is R0001 |
| min, max | `(a: C, b: C) -> C` | `int`, `float` or `str` |
| push | `(xs: list[T], v: T)` | |
| pop | `(xs: list[T]) -> result[T]` | `err("empty")` |
| sort | `(xs: list[C])` | in place, stable |
| has | `(m: map[K,T], k: K) -> bool` | |
| remove | `(m: map[K,T], k: K)` | does nothing if the key is missing |
| keys | `(m: map[K,T]) -> list[K]` | in insertion order |
| expect | `(cond: bool)` | only allowed inside `test` (E0410) |
| expect_eq | `(actual: T, expected: T)` | only allowed inside `test` (E0410); reports both values |

Indexing: `xs[i]` gives R0003 when out of bounds. `m[k]` gives R0004 when the key is missing. `set xs[i] = v;` and `set m[k] = v;` write.

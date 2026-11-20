# Contract: Syntax and Meaning (spec 031)

## The item

```text
const NAME = EXPR;
const NAME: TYPE = EXPR;
export const NAME = EXPR;
export const NAME: TYPE = EXPR;
```

- Top level only, among the other items, in any order; usable before its declaration.
- `const` is a word only at the start of an item (after an optional `export`). It is not reserved.
- `NAME`: `[a-z][a-z0-9_]*`, the rule for functions and variables.
- `TYPE`: `int`, `float`, `bool` or `str`. Without it the type is the value's.
- `EXPR`: a constant expression.

```vo
const max_items = 100;
const rate: float = 0.25;
const out_dir = "out/";
const report_path = out_dir + "report.txt";
const wide = max_items * 2 > 150;

fn main() uses console, file.write(out_dir) {
    print(str(max_items) + " " + str(rate) + " " + report_path + " " + str(wide));
}
```

```output
100 0.25 out/report.txt true
```

## Constant expressions

```text
constexpr := INT | FLOAT | STRING | true | false
           | NAME                      a constant in scope
           | ( constexpr )
           | - constexpr | not constexpr
           | constexpr OP constexpr    OP: + - * / %  == != < <= > >=  and or
```

Typing is that of ordinary expressions: both operands of an arithmetic operator have the same type, `int`
or `float` (`%`: `int`); `+` also joins two `str`; comparisons give `bool`; `and`, `or`, `not` take
`bool`. No conversion functions: `str(max_items)` is not a constant expression.

The value is the one the same expression has at run time: `7 / 2` is `3`, `-7 % 3` is `-1`, floats follow
IEEE (`1.0 / 0.0` is `inf`), `str` comparisons use byte order. `and` and `or` are evaluated like the
others: both sides must be valid constant expressions.

## Using a constant

Wherever an expression of its type is allowed, in any function, test, property, contract or anonymous
function of a file that declares or imports it. Reading one is not an effect. It cannot be assigned.

## Literal positions

Each takes a constant expression where it took a literal:

| Position | Type |
|---|---|
| a scope of `file.read`, `file.write`, `file.delete`, `net`, `env`, `serve`, `process`, in `uses` clauses, test clauses and `effects` sets | `str` |
| `@size(min, max)` | `int` |
| `@range(min, max)` | `int` or `float`, as the field |
| `@pattern(re)`, `@header(name)` | `str` |
| `@default(v)` | the field's type |
| `format(TEMPLATE, ...)`, `regex(PATTERN)`, `sql(STATEMENT)`, the pattern of `on_get` and the other `std.web` routes | `str` |
| the target of a call the checker compares with the caller's scopes (E0503) | `str` |

The meaning is that of the value written there. `@spec` and `@allow` take string literals only.

## Import and export

`import config.max_items;` as for a function. Private unless declared with `export`.

## Canonical form (`vo fmt`)

- `const name = expr;` and `const name: type = expr;`, `export ` before it when exported; single spaces
  around `=` and after `:`; the expression in the canonical form of expressions.
- Comment lines directly above are its documentation and stay attached.
- One blank line between items; consecutive constants keep the blank lines the source has, at most one
  (none stays none), as imports and effect sets do.
- An expression that makes the line longer than 100 characters is broken by the rule for statements.
- Scopes of one effect are sorted by their source text: literals (which start with `"`) before names.

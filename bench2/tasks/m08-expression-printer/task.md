# Expression printer

Read integer expressions and assignments from standard input, one per line. For each, print the expression in a canonical form with the fewest parentheses that keep its meaning, followed by its value.

## Program structure

Split the program into at least three source files: the tokenizer, the parser (which builds an expression tree), and the printer and evaluator (these two may share a file or have one each), plus the entry file that reads standard input, keeps the variables, and prints the results.

## Tokens

Spaces separate tokens and are otherwise ignored. The tokens are:

- a *number*: a run of ASCII digits (`007` is a number with value 7);
- a *name*: a run of lowercase letters `a`-`z` (so `x1` is the name `x` followed by the number `1`);
- the symbols `+ - * / % ^ ( ) =`.

Any other character makes the line a syntax error. A line containing only spaces (or nothing) is ignored and prints nothing.

## Grammar

A line is either `<name> = <expr>` (an assignment) or `<expr>`. Expressions follow this grammar, where `{ ... }` means zero or more repetitions:

```
expr  := term { ("+" | "-") term }
term  := unary { ("*" | "/" | "%") unary }
unary := "-" unary | power
power := atom [ "^" unary ]
atom  := number | name | "(" expr ")"
```

So `+ - * / %` are left-associative, `^` is right-associative and binds tighter than unary minus (`-2 ^ 2` is `-(2 ^ 2)`), and the exponent may itself start with a minus (`2 ^ -1`). If the whole line cannot be parsed this way (including a stray `=`, missing operands, unbalanced parentheses or leftover tokens), print `error: syntax`.

## Printing

Print the parsed tree, not the input text:

- A number is printed in decimal without leading zeros (`007` prints as `7`, `000` as `0`). A name is printed as is.
- A binary operation is printed as `<left> <op> <right>` with one space on each side of the operator.
- A unary minus is printed as `-` immediately followed by its operand.
- A sub-expression is wrapped in `( )` exactly when needed, using these precedence levels: `+ -` 1, `* / %` 2, unary minus 3, `^` 4, numbers, names 5.
  - For `+ - * / %` with level `p`: the left operand needs parentheses if its level is below `p`; the right operand if its level is `p` or below.
  - For `^`: the left operand needs parentheses if its level is 4 or below; the right operand if its level is below 3.
  - For a unary minus: the operand needs parentheses if its level is below 3.

Examples: `((1)) + (2 * 3)` prints `1 + 2 * 3`; `1 - (2 - 3)` prints `1 - (2 - 3)`; `(2 ^ 3) ^ 2` prints `(2 ^ 3) ^ 2`; `(-2) ^ 2` prints `(-2) ^ 2`; `- - x` prints `--x`; `-(2 ^ 2)` prints `-2 ^ 2`.

## Evaluation

Values are signed 64-bit integers: from -9223372036854775808 to 9223372036854775807.

- Evaluation goes left operand first, then right operand; the first error stops it.
- A name evaluates to its variable's value; a name without a value is the error `undefined <name>`.
- A number larger than 9223372036854775807 is the error `overflow` (also when it appears under a unary minus).
- `+ - *` and unary minus: the error `overflow` if the exact result is outside the range.
- `/` divides and truncates toward zero (`-7 / 2` is `-3`). `%` gives the remainder with the sign of the left operand (`-7 % 2` is `-1`, `7 % -2` is `1`), so that `(a / b) * b + a % b` equals `a`. A right operand of 0 is the error `division by zero` (checked after both operands are evaluated). If the exact quotient is outside the range, the error is `overflow`; the remainder is always in range.
- `a ^ b`: if `b < 0`, the error `negative exponent`. Otherwise `a` multiplied by itself `b` times (`a ^ 0` is 1, also for `a = 0`), and the error `overflow` if the exact result is outside the range. Exponents can be very large (up to the 64-bit maximum).

## Output

- For an expression line: `<printed> => <value>`, or `<printed> => error: <message>`.
- For an assignment: `<name> = <printed> => <value>`, and the variable now has that value. If evaluation fails: `<name> = <printed> => error: <message>`, and the variable keeps its previous value (or stays without one).
- For a syntax error: `error: syntax`.

Every output line ends with a newline.

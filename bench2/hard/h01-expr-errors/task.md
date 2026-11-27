# Expression errors

Evaluate integer expressions, one per input line, and print each value or the first error.

Input: lines of text (each ends with `\n`; the last line may lack it). Every line, including an empty one, produces exactly one output line. Empty input produces no output. Lines contain no `\r`.

## Tokens

- Spaces and tabs separate tokens and are otherwise ignored.
- A number is a maximal run of ASCII digits `0`-`9` (leading zeros are allowed: `007` is 7). There is no sign in a number: `-` is an operator.
- A name is a maximal run of lowercase ASCII letters `a`-`z`. The function names are `min`, `max` and `abs`; any other name is an unknown name.
- The operators and punctuation are `+ - * / % ^ ( ) ,`.
- Any other character (an uppercase letter, `.`, `×`, ...) is an unexpected character.

## Grammar

```
expr    = term { ("+" | "-") term }
term    = unary { ("*" | "/" | "%") unary }
unary   = "-" unary | power
power   = primary [ "^" unary ]
primary = number | "(" expr ")" | ("min" | "max") "(" expr { "," expr } ")" | "abs" "(" expr ")"
```

A line must be exactly one `expr`. So `+ -` and `* / %` are left-associative, `^` is right-associative and binds tighter than unary minus (`-2^2` is -4, `2^3^2` is 512, `2^-1` is a negative exponent), `--5` is 5, and there is no unary `+`. `min` and `max` take one or more arguments; `abs` takes exactly one.

## Values

- `a / b` and `a % b` use Euclidean division: the remainder `a % b` is always between 0 and `|b| - 1`, and `a / b` is the integer `q` with `a = b * q + a % b`. For example `7 / 2` is 3, `-7 / 2` is -4, `7 / -2` is -3, `-7 / -2` is 4, and `7 % 2`, `-7 % 2`, `7 % -2`, `-7 % -2` are all 1.
- `a ^ b` is `a` raised to the power `b`; `0 ^ 0` is 1.
- `min`, `max` give the smallest and largest argument, `abs` the absolute value.
- Every number in the input, and the result of every operation that is carried out, lies between -10^18 and 10^18 inclusive, and every exponent `b` that is used is at most 64.

## Errors

Columns count characters (Unicode code points, not bytes) from 1; a tab is one character.

First, the line is checked against the grammar. Reading the line from left to right, the first problem is reported:
- an unexpected character: `error at column C: unexpected character X`, where `X` is the character;
- an unknown name: `error at column C: unknown function NAME`;
- the first token at which the tokens read so far are no longer the beginning of any valid line: `error at column C: unexpected T`, with `T` the token as written (`C` is the column of its first character);
- if all tokens together are the beginning of a valid line but not a whole one: `error at column C: unexpected end of line`, where `C` is the number of characters in the line plus 1.

A problem at a smaller column is always reported before one at a larger column.

Only if the line matches the grammar is it evaluated. The left operand of an operator is evaluated before its right operand, and the arguments of a function from left to right, each completely before the next. The first of these errors stops the evaluation:
- `a / b` or `a % b` with `b` = 0: `error at column C: division by zero`, `C` the column of the `/` or `%`;
- `a ^ b` with `b` < 0: `error at column C: negative exponent`, `C` the column of the `^`.

Output: the value as a decimal integer (with `-` if negative), or the error line. Every output line ends with a newline.

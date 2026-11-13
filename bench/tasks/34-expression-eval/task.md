# Arithmetic expressions

Evaluate arithmetic expressions.

## Input

Zero or more lines, each an expression made of non-negative integers, the operators `+`, `-` and `*`, and parentheses. Spaces may appear anywhere between tokens. There is no unary minus. Every expression is valid, and every intermediate value fits in a signed 64-bit integer.

## Output

For every line, the value of the expression. `*` binds tighter than `+` and `-`; operators of the same level are evaluated from left to right.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
1 + 2 * 3
(1 + 2) * 3
```

Output:

```
7
9
```

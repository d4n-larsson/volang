# Modular exponentiation

Compute large powers modulo a number.

## Input

Zero or more lines, each with three integers `a b m` separated by single spaces, where 0 <= a <= 1000000000000000000, 0 <= b <= 1000000000000000000 and 1 <= m <= 1000000000.

## Output

For every line, the remainder of `a` to the power `b` divided by `m` (a value from 0 to m-1). Any number to the power 0 is 1.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
2 10 1000
3 0 7
```

Output:

```
24
1
```

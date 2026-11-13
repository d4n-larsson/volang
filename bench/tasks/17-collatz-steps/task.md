# Collatz steps

Count the steps of the Collatz sequence.

## Input

Zero or more lines, each with an integer `n` (1 <= n <= 1000000).

## Output

For every line, the number of steps needed to reach 1, where one step replaces `n` by `n / 2` if `n` is even and by `3 * n + 1` if `n` is odd.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
6
1
```

Output:

```
8
0
```

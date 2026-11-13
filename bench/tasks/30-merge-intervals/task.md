# Merge intervals

Merge overlapping intervals.

## Input

Zero or more lines, each with two integers `a b` (a <= b) describing the closed interval from `a` to `b`.

## Output

The merged intervals, one per line as `a b`, sorted by start. Two intervals are merged when they overlap or touch, that is when the start of one is at most the end of the other (`[1, 5]` and `[5, 7]` merge into `[1, 7]`, but `[1, 2]` and `[3, 4]` stay separate).

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
1 3
2 6
8 10
15 18
```

Output:

```
1 6
8 10
15 18
```

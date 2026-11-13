# Duration sum

Convert durations to seconds and add them up.

## Input

Zero or more lines, each a duration such as `1h30m`, `45s` or `2h5s`: one or more parts, each a non-negative integer immediately followed by a unit `h` (hours), `m` (minutes) or `s` (seconds). Units appear in the order `h`, `m`, `s`, each at most once.

## Output

For every line, the duration in seconds. Then one final line `total N`, where `N` is the sum of all durations in seconds.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
1h30m
45s
```

Output:

```
5400
45
total 5445
```

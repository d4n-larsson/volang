# Two sum

Find two positions whose values add up to a target.

## Input

The first line holds the target integer `t`. The second line holds zero or more integers separated by single spaces (the line is empty when there are none).

## Output

One line with two 0-based positions `i j` (with `i < j`) such that the values at `i` and `j` sum to `t`. Choose the pair with the smallest `j`; if several pairs share that `j`, choose the smallest `i`. If there is no such pair, print `none`.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
9
2 7 11 15
```

Output:

```
0 1
```

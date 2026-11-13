# Tower of Hanoi

List the moves that solve the Tower of Hanoi.

## Input

One line with the number of disks `n` (0 <= n <= 12). The disks are numbered 1 (smallest) to `n` (largest) and start on peg `A`.

## Output

The moves of the standard minimal solution that moves all disks from peg `A` to peg `C`, one per line as `D X Y`: disk `D` moves from peg `X` to peg `Y`. The standard solution moves `n` disks from `X` to `Y` by moving `n-1` disks from `X` to the third peg, moving disk `n` from `X` to `Y`, then moving the `n-1` disks from the third peg to `Y`. For `n = 0` print nothing.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
2
```

Output:

```
1 A B
2 A C
1 B C
```

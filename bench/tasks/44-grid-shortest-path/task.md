# Shortest path in a grid

Find the length of the shortest path through a maze.

## Input

One or more lines of the same length forming a grid. `.` is an open cell, `#` is a wall, `S` is the start and `E` is the end (exactly one of each; both are open cells).

## Output

One line with the minimum number of moves from `S` to `E`, where a move goes to an adjacent open cell up, down, left or right and never leaves the grid. Print `-1` if `E` cannot be reached.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
S.#
..#
#.E
```

Output:

```
4
```

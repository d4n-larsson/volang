# Game of Life

Simulate Conway's Game of Life on a bounded grid.

## Input

The first line holds the number of generations `n` (n >= 0). The following lines form the grid: at least one row, all rows the same length, `#` for a live cell and `.` for a dead cell. Cells outside the grid are always dead.

## Output

The grid after `n` generations, in the same format. In each generation, all cells update at once using their 8 neighbours: a live cell with 2 or 3 live neighbours stays alive, a dead cell with exactly 3 live neighbours becomes alive, and every other cell is dead.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
1
.....
..#..
..#..
..#..
.....
```

Output:

```
.....
.....
.###.
.....
.....
```

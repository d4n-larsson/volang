# Grid route

Read a map grid from standard input and plan the cheapest route from the start through numbered checkpoints to the goal. Print its cost, its moves, and the map with the route drawn on it.

## Program structure

Split the program into at least three source files: grid parsing and validation, the path search (cheapest costs and the move choice), and the route rendering (drawing the route on the map and printing) each in their own file, plus the entry file that reads standard input and drives them.

## Input

Every non-empty input line is one grid row (empty lines are ignored). Cells:

| Cell | Meaning | Cost to enter |
|---|---|---|
| `#` | wall, never entered | - |
| `.` | plain ground | 1 |
| `:` | rough ground | 2 |
| `~` | swamp | 5 |
| `S` | the start | 1 |
| `G` | the goal | 1 |
| `1`-`9` | checkpoints | 1 |

The grid is *invalid* if it has no rows, if its rows differ in length, if it contains any other character, if it does not contain exactly one `S` and exactly one `G`, or if its checkpoint digits are not exactly `1` to `k` for some `k >= 0`, each appearing once (so `1 2 3` is fine, `2` alone or `1 1` is not). For an invalid grid print only `invalid grid`.

## Route

A move goes one cell up (`U`), right (`R`), down (`D`) or left (`L`) and must stay in the grid and not enter a wall. The cost of a move is the cost of the cell it enters. A path's cost is the sum of its moves' costs.

The route is made of legs: start to checkpoint 1, checkpoint 1 to 2, ..., checkpoint `k` to the goal (just start to goal when `k` is 0). Each leg is a cheapest path between its two cells; legs may pass through any non-wall cell, including the start, the goal and other checkpoints.

When several cheapest paths exist for a leg, choose the one whose move string is smallest when moves are compared in the order `U` < `R` < `D` < `L`, letter by letter from the first move. (Equivalently: standing on a cell of the leg, always take the first move in the order U, R, D, L that still lies on some cheapest path to the leg's end.)

## Output

If some leg has no path, print only `no route`. Otherwise print:

1. `cost <c>`: the total cost of all legs;
2. `moves <m>`: all legs' moves concatenated;
3. the grid rows, with every cell the route enters or leaves replaced by `*`, except cells holding `S`, `G` or a digit, which keep their character.

Every output line ends with a newline.

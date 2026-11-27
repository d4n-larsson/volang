# Spreadsheet: several sheets

## The existing program

The program is a spreadsheet with one sheet of integer cells. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. The input is ASCII.

A cell is named by a column letter `A`-`Z` and a row `1`-`99` without leading zeros (`A1`, `Z99`); letters may be lower-case and are printed upper-case. A cell is empty, or holds a number or a formula. A *range* is `CELL:CELL`, two corners in any order. Lists of cells are sorted by column, then row (`A1 A2 B1`).

`set CELL VALUE` joins all words after CELL without spaces: text starting with `=` is a formula, an optional `-` and digits is a number (`007` is 7), anything else is `error: bad value`. A formula has integer literals, cell references (`A1`; `$` before the letter and/or the row marks a part that `copy` keeps fixed: `$A1`, `A$1`, `$A$1`), `+ - * /` with the usual precedence, unary `-`, parentheses, and `SUM(RANGE)`, `MIN(RANGE)`, `MAX(RANGE)` (any letter case). Text that is not such a formula is `error: bad formula`.

Values are computed whenever they are needed. An empty cell counts as 0; `SUM` adds the range's cells, `MIN`/`MAX` skip its empty cells (0 if all are empty). Division truncates toward zero (`-7/2` is `-3`). A value may instead be an error: `#DIV0` (division by zero), `#CYCLE` (a cell refers back to itself through its formulas) or `#REF` (a reference lost by `copy`); a formula's value is the first error met evaluating it left to right (a range row by row, left to right within a row).

Commands (an error prints its message and changes nothing):

- `set CELL VALUE`: stores the number or formula and prints `CELL = V` with the cell's value.
- `get CELL`: `CELL = V`, or `CELL empty`.
- `show CELL`: `CELL: N` for a number, `CELL: =FORMULA` for a formula, `CELL: empty`. A formula is printed from what was parsed: references and function names upper-case, numbers as integers, no spaces, the parentheses that were written.
- `clear CELL`: empties the cell, `CELL cleared`; `error: CELL is empty` if it was empty.
- `copy SRC DST`: copies SRC's content to DST and prints `copied SRC to DST` (`error: SRC is empty` if SRC is empty). Each reference in a copied formula moves by DST's column and row offsets from SRC, except its `$` parts; a reference that leaves the grid is printed and evaluated as `#REF`, and a function whose range loses a corner as `SUM(#REF)`.
- `deps CELL`: `deps CELL: ...`, the cells the formula refers to (ranges cell by cell), or `none`.
- `users CELL`: `users CELL: ...`, the formula cells that refer to CELL (also through a range), or `none`.
- `list`: `CELL: N` or `CELL: =FORMULA = V` for every non-empty cell, sorted; `no cells` if there is none.
- `count`: `cells N, formulas F, errors E` (E counts cells whose value is an error).
- `find N` (N an optional `-` and digits): `found: ...`, the cells whose value is N, or `found: none`.
- `eval =FORMULA`: computes a formula without storing it, `= V`.
- `print RANGE`: a grid of the values (see `grid` in the code).

A wrong number of arguments (or a bad `find` number) prints `error: bad command`, a bad cell name `error: bad cell TEXT`, a bad range `error: bad range TEXT`, an unknown first word `error: unknown command WORD`.

## The change

The spreadsheet gets several **sheets**, each with its own A1-Z99 cells. At the start there is one sheet, `Sheet1`, and it is the *current sheet*. A sheet name is an ASCII letter followed by letters, digits and `_`; names are case-sensitive (`data` and `Data` are two sheets).

**New commands** (wrong number of arguments: `error: bad command`):

- `sheet NAME`: adds an empty sheet, after the existing ones; the current sheet stays. Prints `sheet NAME added`; `error: bad sheet name NAME` if NAME is not a valid name, else `error: sheet NAME exists` if a sheet has that name.
- `use NAME`: makes NAME the current sheet, `using NAME`; `error: unknown sheet NAME` if there is no sheet of that name.
- `sheets`: one line per sheet in the order they were added, `NAME: N cells` (N non-empty cells, always the word `cells`), with ` (current)` at the end of the current sheet's line.
- `rename OLD NEW`: checks in this order: `error: unknown sheet OLD`; `error: bad sheet name NEW`; `error: sheet NEW exists` (also when NEW is OLD). Then prints `renamed OLD to NEW`. The sheet keeps its place in `sheets`, its cells, and whether it is current; every reference to it now prints with the new name.
- `drop NAME`: `error: unknown sheet NAME`, then `error: cannot drop the current sheet`. Otherwise prints `dropped NAME` and deletes the sheet with its cells. References to it in other sheets' formulas become *dead*: they evaluate to `#REF` and print as `#REF!` followed by the reference (`#REF!A1`, `#REF!$B2`, `SUM(#REF!A1:B2)`). A sheet added later with the same name is a new sheet: dead references stay dead.

**References in formulas**: `NAME!A1`, `NAME!$A$1`, and in functions `SUM(NAME!A1:B3)` (the name qualifies the whole range; the second corner has no name of its own). The `!` comes right after the name; anything else with a `!` (for example `Data!SUM(A1:A2)`, `Data!5`, `!A1`, `9x!A1`) is `error: bad formula`. A sheet name may look like a cell or a function (`A1!B2`, `SUM!A1`). An unqualified reference means the formula's **own sheet** (the sheet of the cell that holds it), never the current sheet at the time it is evaluated. In `set` and `eval`, the formula is first checked as a formula (`error: bad formula`), then every name in it must be a sheet's: otherwise `error: unknown sheet NAME` for the first unknown name from the left, and nothing changes. A reference qualified with the formula's own sheet is stored as an unqualified one.

Evaluation follows references across sheets; `#CYCLE` is about a cell on a sheet referring back to that same cell on that same sheet (`Data!A1` referring to `Sheet1!A1` is not a cycle). `show` and `list` print a formula relative to its own sheet: references to its own sheet unqualified, references to other sheets as `NAME!...` with the sheet's current name, dead references as `#REF!...`. A reference lost by `copy` is `#REF` as before, without a name.

**Cell arguments**: the CELL arguments of `set`, `get`, `show`, `clear`, `copy`, `deps` and `users`, and the RANGE of `print`, may be qualified, `NAME!CELL` or `NAME!RANGE`; unqualified means the current sheet. Such an argument is split at its first `!`: if the part before it is not a valid sheet name or the part after it not a valid cell (range), it is `error: bad cell ARG` (`error: bad range ARG`) with the whole argument; then if there is no such sheet, `error: unknown sheet NAME`. The arguments are checked in order, each completely (`copy` checks SRC, then DST, then whether SRC is empty).

**Names in output**: wherever a command prints a cell's name (the `set`, `get`, `show`, `clear` and `copy` lines, `CELL is empty`, the `deps`/`users` title and lists, `found:`, `list`), it is `NAME!CELL` when the cell is not on the current sheet and just `CELL` when it is. Lists of cells are sorted by sheet (in `sheets` order), then column, then row.

**Commands across sheets**:

- `eval`: unqualified references mean the current sheet.
- `copy SRC DST` may copy between sheets. References move by DST's column and row offsets from SRC as before. An unqualified reference stays unqualified, so it now means DST's sheet. A qualified reference (also a dead one) keeps its sheet; if that sheet is DST's sheet, it is stored as unqualified.
- `deps`: the cells on live sheets the formula refers to (dead references are left out). `users`: the formula cells on every sheet that refer to the cell.
- `list` and `find` cover the current sheet only; `count` covers all sheets. `print` prints the given sheet's cells in the same grid as before.

Everything else stays exactly as it is.

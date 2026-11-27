# Spreadsheet

Write a small spreadsheet engine driven by commands read from standard input. Cells hold numbers, text or formulas; formulas refer to other cells, and every value always reflects the current contents of the whole sheet.

## Program structure

Split the program into at least three source files: the formula language (tokenizing, parsing, and rewriting references for `copy`), the sheet (cell contents, dependencies, cycle detection and evaluation), and the output formatting (`print`, `formula`, `deps` and the `show` grid) each in their own file, plus the entry file that reads standard input and runs the commands.

## Cells

The sheet has columns `A` to `Z` and rows `1` to `99`. A *cell name* is one uppercase letter followed by a row number from 1 to 99 written without leading zeros: `A1`, `Z99`, `C10` (not `a1`, `A0`, `A01`, `A100`, `$A1`).

Every cell is *empty* or holds *content* of one of three kinds:

- a **number**: content that is an optional `-` followed by one or more digits (`42`, `-7`, `007`). It is stored as its integer value (`007` is stored as `7`, `-0` as `0`).
- a **formula**: content that starts with `=`. The text after `=` is the formula's expression.
- **text**: any other content (`hello world`, `+5`, `1.5`, `#DIV/0!`).

## Values

Every cell has a *value*, which is one of: a number (an integer), a text, an error, or nothing for an empty cell. The errors are `#PARSE!`, `#REF!`, `#DIV/0!`, `#VALUE!`, `#NUM!` and `#CYCLE!`.

Numbers are limited to the range -999999999 to 999999999. Any number outside it — a stored number, a number literal in a formula, or the result of an operator or function — is replaced by the error `#NUM!`. (Inputs never contain a number with more than 15 digits.)

- A number cell's value is its number (or `#NUM!`); a text cell's value is its text.
- A formula cell's value is its expression's value (see Evaluation), except that every cell on a cycle has the value `#CYCLE!` and a formula whose expression is not valid has the value `#PARSE!`.

A text value is never an error, even if it looks like one: a text cell holding `#DIV/0!` is text.

## Formula syntax

Tokens may be separated by spaces; spaces are not allowed inside a token. The tokens are:

- a *number literal*: one or more digits (leading zeros allowed). A literal has no sign; `-5` is unary minus applied to `5`.
- a *reference*: an optional `$`, one uppercase letter, an optional `$`, and a row number from 1 to 99 without leading zeros (`B2`, `$B2`, `B$2`, `$B$2`). A `$` marks that part as fixed for `copy`; it does not change what the reference points to.
- a *range*: two references joined by `:` with no spaces (`A1:B3`, `$A$1:C2`). It covers every cell of the rectangle with those two corners, given in any order (`B3:A1` is the same as `A1:B3`).
- a *function name*: two or more uppercase letters. The functions are `SUM`, `MIN`, `MAX`, `COUNT`, `IF` and `ABS`.
- the error literal `#REF!`.
- the operators `+ - * / = <> < <= > >=`, parentheses and commas.

A run of `$` signs, uppercase letters and digits that starts with `$` or a letter is always one token, which must be exactly a reference or a function name (or a reference that starts a range); a run of digits that starts with a digit is a number literal. Anything that cannot be split into these tokens is invalid (for example `a1`, `A0`, `AB1`, `A01`, `A 1`, `$A`, `1.5`, `SUM` followed by digits, a stray `:` or `$`). Grammar, from loosest to tightest binding:

```
expression := sum [ compare sum ]          compare is one of = <> < <= > >=
sum        := product { ( + | - ) product }   left to right
product    := unary { ( * | / ) unary }       left to right
unary      := - unary | primary
primary    := number | reference | #REF! | ( expression ) | function ( arguments )
```

At most one comparison is allowed at each level (`1<2<3` is invalid; `(1<2)<3` is valid). Function names are followed by `(`. `IF` takes exactly 3 arguments, `ABS` exactly 1, and `SUM`, `MIN`, `MAX`, `COUNT` (the *aggregates*) one or more. Each argument is an expression, except that a whole argument of an aggregate may also be a range. A range anywhere else is invalid. An expression with any of these problems (bad token, grammar error, unknown function, wrong number of arguments, misplaced range, nothing after `=`) makes the formula invalid.

## Evaluation

Expressions are evaluated left to right. A reference evaluates to the value of the cell it points to, where an empty cell counts as the number 0.

- **Literal**: its number (or `#NUM!`). `#REF!` evaluates to the error `#REF!`.
- **Unary minus**: evaluate the operand; an error stays that error; a text gives `#VALUE!`; else the negated number.
- **Binary operators**: evaluate the left operand; if it is an error, that is the result and the right operand is not evaluated. Otherwise evaluate the right operand; if it is an error, that is the result. Otherwise, if either operand is a text, the result is `#VALUE!`. Otherwise compute:
  - `+ - *` as usual.
  - `/`: if the right operand is 0, `#DIV/0!`; otherwise the exact quotient rounded to the nearest integer, with halves rounded away from zero (`7/2` is 4, `-7/2` is -4, `5/3` is 2, `1/3` is 0, `-5/4` is -1).
  - comparisons give 1 if true and 0 if false.
  - then a result outside the number range becomes `#NUM!`.
- **IF(c, a, b)**: evaluate `c`; an error is the result; a text gives `#VALUE!`; a non-zero number evaluates and returns `a`, zero evaluates and returns `b`. The other branch is not evaluated. The result may be a text.
- **ABS(x)**: evaluate `x`; an error is the result; a text gives `#VALUE!`; else the absolute value.
- **Aggregates** go through their arguments left to right and collect numbers:
  - A range argument visits its cells row by row from the top row down, left to right within a row. An empty cell or a cell whose value is a text is skipped; a cell whose value is an error makes that error the result immediately; a number is collected.
  - An argument that is exactly one reference (with nothing else, not even parentheses) is treated as the one-cell range of that cell.
  - Any other argument is evaluated: an error is the result immediately; a text gives `#VALUE!` immediately; a number is collected.
  - `SUM` is the sum of the collected numbers (then the range check), `COUNT` how many were collected, `MIN` and `MAX` the smallest and largest, or 0 when nothing was collected.

A reference to a cell whose formula evaluates to a text gives that text: if `A1` holds `hi` and `B1` holds `=A1`, then `B1`'s value is the text `hi`.

## Dependencies and cycles

A cell holding a valid formula *depends directly* on every cell named by a reference in it and every cell inside each range in it, whether or not evaluation reaches that cell (both branches of an `IF` count). Number, text and empty cells and cells with an invalid formula depend on nothing.

A cell is *on a cycle* when following direct dependencies, one or more steps, from that cell leads back to the cell itself (a formula referring to its own cell is on a cycle). Every cell on a cycle has the value `#CYCLE!`. Other cells are evaluated normally; one that reaches a cell on a cycle gets the `#CYCLE!` value of that cell like any other error.

## Commands

The input is ASCII text. Each input line is one command. Leading and trailing spaces are ignored, and lines that are then empty are ignored. Lines are numbered from 1, counting every line. The first run of non-space characters is the command word, and fields are separated by runs of spaces.

- `set <cell> <content>`: the content is the rest of the line after the cell name, with leading and trailing spaces removed; it must not be empty. Spaces inside it are kept as written. The cell gets that content.
- `clear <cell>`: the cell becomes empty.
- `copy <from> <to>`: the target cell gets a copy of the source cell's content (an empty source makes the target empty). For a formula the copy is rewritten, unless the formula is invalid, in which case its text is copied unchanged. Let `dc` and `dr` be the column and row distance from the source to the target (target minus source). In every reference and in both ends of every range, a column without `$` moves by `dc` and a row without `$` moves by `dr`; parts with `$` stay. A moved reference keeps its `$` marks and is written in the same form. If a moved reference (or either end of a moved range) falls outside the sheet, that whole reference (or the whole range, both ends and the `:`) is replaced by the text `#REF!`. All other characters of the formula, including spaces, stay exactly as they were.
- `print <cell>`: prints `<cell> = <value>`, or `<cell> is empty` for an empty cell.
- `formula <cell>`: prints `<cell> := <content>`, or `<cell> is empty`. A number is written as its stored value (`-` and digits, no leading zeros); a formula and a text exactly as stored (for a formula, including the `=`).
- `deps <cell>`: prints `<cell> <- ` followed by every cell that depends on the given cell through one or more direct dependencies, in reading order (by row, then by column), separated by single spaces. The list includes the cell itself if it is on a cycle. If there are none, it prints `<cell> <- none`.
- `show <range>`: prints part of the sheet as a grid (see below). The range is two cell names joined by `:` (any corner order) or a single cell name.

Numbers are written in decimal with a leading `-` when negative; errors by their name; texts as they are.

Anything else is invalid and prints `line <N>: invalid command`, changing nothing: an unknown command word (command words are lowercase), a wrong number of fields (`set` needs at least 3, `copy` exactly 3, the others exactly 2), or a malformed cell name or range.

## The grid

For a `show` covering rows `r1` to `r2` and columns `c1` to `c2`:

- The label width `L` is the number of digits of `r2`. Each column's width is the largest length of the written values of its cells in the range, and at least 1; an empty cell's written value is the empty string.
- The first line is `L` spaces, then for each column ` | ` followed by the column letter padded with spaces on the right to the column's width.
- The second line is `L` dashes, then for each column `-+-` followed by the column's width in dashes.
- Then one line per row: the row number padded with spaces on the left to width `L`, then for each column ` | ` and the cell's written value padded with spaces to the column's width: a number is padded on the left (aligned right), a text or an error on the right (aligned left).
- Trailing spaces are removed from every line of the grid.

Every output line ends with a newline.

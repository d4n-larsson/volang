# Spreadsheet

Evaluate the cells of a small spreadsheet.

## Input

Every non-empty line is `REF CONTENT`: a cell reference, one space, and the cell's content (the rest of the line, non-empty). A cell reference is an uppercase letter `A`-`Z` (the column) followed by a row number from 1 to 9999 without leading zeros, such as `B12`. A later line for the same cell replaces the earlier one. There are at most 5000 lines.

A content is:
- a formula if it starts with `=`;
- otherwise a number if it is an optional `-` followed by 1 to 18 ASCII digits;
- otherwise a text, which is shown as it is.

Cells without a line are empty.

## Formulas

After the `=`, a formula is a sequence of tokens, which may be separated by spaces: the characters `+ - * ( ) , :`, and words. A word is a maximal run of ASCII letters and digits, so `SUM`, `B12`, `12` and `A1B` are one word each. A word is an integer if it is 1 to 18 digits, a reference if it is a cell reference as above, and a function name if it is `SUM`, `MIN` or `MAX`. The tokens must form an expression in this grammar:

```
expr    = term { ("+" | "-") term }
term    = unary { "*" unary }
unary   = "-" unary | primary
primary = integer | REF | FUNC "(" arg { "," arg } ")" | "(" expr ")"
arg     = REF ":" REF | expr
FUNC    = "SUM" | "MIN" | "MAX"
```

`REF:REF` is a range: all cells of the rectangle with these two corners (in any order), taken row by row from the top, each row from left to right. A formula that does not match the grammar (for example with a word such as `A0`, `a1`, `A01`, `AA1`, `A10000`, `2A` or `sum`, any other character, or a range outside a function's parentheses) has the value `#PARSE`. Formulas are at most 200 characters long and nest parentheses at most 10 deep.

## Values

The value of a cell is an integer, a text or an error: `#PARSE`, `#CYCLE` or `#VALUE`.

- A cell is on a cycle if, following references from its formula (a range refers to every cell in it, also empty ones), you can come back to the cell itself. Every cell on a cycle has the value `#CYCLE`.
- Otherwise a formula is evaluated from left to right, and the first error met becomes its value:
  - A reference used as a value (an operand, a whole formula such as `=B2`, or a function argument that is a single reference) gives the integer value of that cell; an empty cell gives 0; a cell with a text gives `#VALUE`; a cell with an error gives that error.
  - A range given to a function uses the integer values of its cells in order and skips empty cells and texts; a cell with an error in the range gives that error.
  - `SUM` adds all its integers (0 if there are none); `MIN` and `MAX` give the smallest and largest one, or `#VALUE` if there are none.
- The input guarantees that every integer computed lies between -10^18 and 10^18, that no path of references visits more than 60 different cells, and that the ranges of all formulas together hold at most 1000000 cells. A cell may be referenced by many formulas: a solution that evaluates a cell again for each use can be far too slow.

## Output

One line `REF VALUE` for every cell that has a line in the input, ordered by row, then by column. An integer is written in decimal without leading zeros, with a `-` if it is negative (so the content `-007` is shown as `-7`, and `-0` as `0`); a text as it is; an error as its name. Every output line ends with a newline.

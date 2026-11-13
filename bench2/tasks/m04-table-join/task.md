# Table join

Read two comma-separated tables from standard input, join them on a key column, and print the result as an aligned text table.

## Program structure

Split the program into at least three source files: CSV line parsing, the join itself, and the aligned table formatting each in their own file, plus the entry file that reads standard input, splits it into sections and drives them.

## Input

All input is ASCII.

- Line 1 is always `<kind> <column>`: `kind` is `inner`, `left` or `full`, followed by one space and the key column name (any text without commas or quotes).
- The following lines up to a line that is exactly `---` form table A; the lines after it form table B. The `---` line is always present.
- In each table, empty lines are ignored. The first non-empty line is the header; each later non-empty line is a data row. Each table always has a header. Header names are distinct within a table and never malformed.

## CSV rules

A line is split into fields at commas, with this quoting rule:

- A field that starts with `"` is *quoted*: it continues to the next `"` that is not doubled; inside it, `""` stands for one `"` and commas are ordinary characters. The closing `"` must be followed by a comma or the end of the line.
- A field that does not start with `"` is taken as written, up to the next comma (a `"` inside it is an ordinary character). Spaces are never trimmed.
- An empty line part gives an empty field: `a,,b` has 3 fields and `a,` has 2.

A data row is *malformed* if a quoted field has no closing quote, if a closing quote is followed by something other than a comma or the end of the line, or if its number of fields differs from its table's header. Malformed rows are skipped (they take no part in the join) and counted.

## Join

If the key column is missing from A's header, print `error: no column <column> in A` and nothing else; otherwise, if it is missing from B's header, print `error: no column <column> in B` and nothing else.

Two rows *match* when their key values are equal and not empty: a row whose key value is the empty string matches nothing.

Output columns: the key column (named as in the input), then A's other columns in header order, then B's other columns in header order. When a non-key column name occurs in both headers, its output names are `a.<name>` for A's column and `b.<name>` for B's.

Output rows, in this order:

1. For each A row in input order: one output row for each matching B row, in B's input order. If there is no matching B row, `left` and `full` produce one row with all B columns empty; `inner` produces nothing.
2. For `full` only: each B row that matched no A row, in input order, with the key taken from B and all A columns empty.

## Output format

- The width of a column is the largest length among its output name and its values in all output rows.
- The header line and every row line: each cell padded with spaces on the right to its column's width, cells joined by ` | ` (space, bar, space), and then trailing spaces removed from the end of the line.
- After the header line comes a separator line: for each column, a run of `-` as long as its width, joined by `-+-`.
- Then the row lines, and finally `rows: <n>, skipped: <k>` where `n` is the number of output rows and `k` the number of malformed rows in both tables together.

Every output line ends with a newline.

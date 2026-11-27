# CSV table

Read CSV text from standard input and print it as an aligned text table, or report the first error.

## CSV

The input is UTF-8 text without `\r`. It is a sequence of records:

- Records are separated by `\n`. A `\n` at the very end of the input ends the last record and does not start a new one. Empty input has no records. An empty line in the middle of the input (or a last line that is empty because the input ends with `\n\n`) is a record with one empty field.
- The fields of a record are separated by `,`.
- A field that starts with `"` is quoted: it ends at the next `"` that is not doubled. Inside it, `""` stands for one `"`, and `,` and `\n` are ordinary characters (so a quoted field can span lines). The closing `"` must be followed by `,`, `\n` or the end of the input.
- Any other field is unquoted: it runs up to the next `,`, `\n` or the end of the input, and must not contain `"`.

The first record is the header. Every record must have as many fields as the header.

## Errors

The input is read from the start, and the first problem stops it:

- A `"` in an unquoted field: `error: line L, column C: quote in unquoted field`, at that `"`.
- A closing quote followed by another character: `error: line L, column C: character after closing quote`, at that character.
- A quoted field that is still open at the end of the input: `error: line L, column C: unclosed quote`, at the opening `"`.
- A record with a different number of fields than the header, found when that record ends: `error: record R has N fields, expected M`, where records are numbered from 1 (the header is record 1).

Lines are the lines of the input text, numbered from 1 (a `\n` inside a quoted field also starts a new line); columns count characters (Unicode code points, not bytes) from 1.

When there is an error, print only the error line.

## Table

If there are no records, print nothing. Otherwise:

- A cell is shown with each `\` replaced by `\\` and each `\n` replaced by `\n` (a backslash and the letter n).
- A column's width is the largest number of characters of its shown cells, the header included (it is 0 if they are all empty).
- A column is numeric if there is at least one record after the header and the field of every such record in this column is an integer: an optional `-` followed by one or more ASCII digits, nothing else.
- Each table line is the shown cells of a record, joined with ` | `. Each cell is padded with spaces to its column's width: on the left in a numeric column (the header too), on the right in other columns. But if the last column is not numeric, its cells are not padded at all.
- The header line comes first, then a line of the column widths in `-` characters joined with `-+-`, then the other records in input order.

Every output line ends with a newline.

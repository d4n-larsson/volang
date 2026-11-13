# Bracket errors

Check the brackets on each line of standard input (lines end with `\n`; the last line may lack it). Each line is checked on its own and produces exactly one output line.

Rules:
- The brackets are `()`, `[]` and `{}`. Every other character is ignored, except `"`.
- A `"` starts a string, which ends at the next `"` on the same line. Everything inside a string, brackets included, is ignored. There are no escapes.
- Columns are 1-based byte positions in the line.

Scan the line from the left and report the first problem:
- A closing bracket when no bracket is open: `error at column C: unexpected X`, where `X` is the closing bracket and `C` its column.
- A closing bracket that does not match the most recently opened, still open bracket: `error at column C: expected Y but found X`, where `Y` is the closing bracket that would match that open bracket and `X`, `C` are the bracket found and its column.

If the scan reaches the end of the line without such a problem:
- If a string is still open: `error at column C: unclosed "`, with `C` the column of the `"` that opened it.
- Else, if brackets are still open: `error at column C: unclosed X`, where `X` is the leftmost bracket that was never closed and `C` its column (for `([{` that is the `(` at column 1).
- Else print `ok`.

An empty line prints `ok`. Empty input produces no output. Lines can be up to 20000 characters long. Every output line ends with a newline.

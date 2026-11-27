# Shell words

Split command lines into words the way a small shell does, with quoting, escapes, variables and comments.

## Input

The input is UTF-8 text in lines (each ends with `\n`; the last line may lack it; no `\r`). It starts with variable definitions, one per line, `NAME=VALUE`, followed by a line that is exactly `%%`. Every line after that is a command line. There may be no definitions; there is always a `%%` line; there may be no command lines.

A NAME is an ASCII letter or `_` followed by any number of ASCII letters, digits and `_`. VALUE is everything after the first `=`, taken literally (it may be empty and may contain spaces, tabs, quotes, `$` and so on). A later definition of the same NAME replaces an earlier one. A variable that is not defined has the empty value.

## Command lines

A command line is read from left to right. Outside quotes:

- Spaces and tabs separate words.
- `\` followed by any character gives that character literally (also a space, a tab, a quote, `$`, `#` or `\`). A `\` that is the last character of the line is an error: `dangling backslash`.
- `'` starts a single-quoted part that ends at the next `'`. Everything between is literal: no escapes, no variables.
- `"` starts a double-quoted part that ends at the next `"` that is not escaped. Inside it, `\` followed by `"`, `\` or `$` gives that character; a `\` before any other character is kept as it is, together with that character (`"a\nb"` gives `a\nb`). Variables are expanded inside double quotes.
- A `#` starts a comment, which runs to the end of the line and is ignored, if it is at the start of the line or right after a space or tab that separates words (that is, a space or tab that is neither quoted nor escaped). Any other `#` is an ordinary character, and so is a `#` inside quotes or after a `\`.

Quoted parts, escaped characters and ordinary characters that are not separated by spaces or tabs form one word: `a'b c'"d"\ e` is the single word `ab cd e`. A quoted part makes a word even when it is empty: `''` and `""` are each one empty word.

## Variables

- `$NAME` expands to the value of the variable, where NAME is the longest possible name after the `$`.
- `${NAME}` expands to the value of NAME. If `${` is not followed by a NAME and then immediately by `}`, that is an error: `bad substitution`.
- A `$` that is not followed by `{`, an ASCII letter or `_` is an ordinary character (`$1`, `$$`, `$` at the end of the line).
- The value is never read again as a command line: quotes, `\`, `$` and `#` in it are ordinary characters.
- Inside double quotes the value becomes part of the word as it is.
- Outside quotes the value is split at spaces and tabs: each run of spaces and tabs in the value ends the current word, and the text between runs goes into words. So with `X= a  b ` (a value with spaces at both ends), `1$X-2` gives the three words `1`, `a` and `b`, then `-2` starts a new word; a value of only spaces and tabs just separates words. An empty value adds nothing: on its own, `$E` (with E empty or undefined) makes no word, while `"$E"` and `$E''` each make one empty word.

## Errors

The line is read from left to right and the first problem stops it. A quote that is never closed is found only at the end of the line: `unclosed '` or `unclosed "`. The error is reported as `error at column C: MESSAGE`, where `C` is the column of the `\` (dangling backslash), of the `$` (bad substitution) or of the quote that opened the unclosed part. Columns count characters (Unicode code points, not bytes) from 1.

## Output

One line per command line: the number of words, then each word in square brackets, all separated by single spaces (`3 [ls] [-l] [my file]`; `0` for a line without words), or the error line. Words are printed exactly as they are, including any spaces, tabs or brackets in them. Every output line ends with a newline.

# Stack language

Write an interpreter for a tiny stack language read from standard input. There is one stack of integers, shared by all lines, and a dictionary of user-defined words.

## Program structure

Split the program into at least three source files: the tokenizer (splitting lines and recognizing number literals), the stack machine (the stack and the built-in words, including the arithmetic rules), and the dictionary of user-defined words each in their own file, plus the entry file that reads standard input and drives them.

## Tokens

Each input line is processed on its own, left to right. A line is split into tokens at runs of spaces; leading and trailing spaces are ignored. Tokens are case-sensitive.

A token is a *number literal* if it is an optional `-` followed by one or more ASCII digits (leading zeros allowed), and nothing else. `-` alone is not a number, nor are `+5`, `5-`, `--5` or `1.5`.

## Values and arithmetic

All values must lie in the range from -1000000000000000000 to 1000000000000000000 (minus and plus 10^18), inclusive. A number literal outside that range, or an arithmetic result outside it, is the error `overflow`.

## Built-in words

In the table the stack is written bottom to top, with the top on the right. `a b` means `b` is on top.

| Word | Stack before -> after | Notes |
|---|---|---|
| number literal | -> n | pushes its value |
| `+` `-` `*` | a b -> (a op b) | |
| `/` | a b -> q | `q` is `a / b` truncated toward zero (`-7 3 /` gives `-2`) |
| `%` | a b -> r | `r = a - b * q` with `q` as for `/` (`-7 3 %` gives `-1`, `7 -3 %` gives `1`) |
| `neg` | a -> -a | |
| `abs` | a -> \|a\| | |
| `dup` | a -> a a | |
| `drop` | a -> | |
| `swap` | a b -> b a | |
| `over` | a b -> a b a | |
| `rot` | a b c -> b c a | |
| `clear` | ... -> | empties the stack |
| `.` | a -> | prints `a` on its own line |
| `.s` | unchanged | prints `<n>` where `n` is the stack size, followed by a space and each value from bottom to top, separated by single spaces; an empty stack prints just `<0>` |

A word that needs more values than the stack holds is the error `stack underflow` (nothing is popped). `/` and `%` with `b` equal to 0 are the error `division by zero`.

## User-defined words

The token `:` starts a definition: the next token is the word's *name* and the tokens up to the next `;` token on the same line are its *body*. Execution continues after the `;`.

- If there is no name token, or no `;` token after the name on the same line, the error is `unfinished definition`.
- The name must consist of the lowercase letters `a`-`z` only and must not be a built-in word name (`neg abs dup drop swap over rot clear`); otherwise the error is `bad name <name>`.
- Each body token must be a number literal (within range, else `overflow`), a built-in word, or a word that is defined at this moment; otherwise the error is `unknown word <token>` for the first such token. (`:` cannot appear in a body: it is not a built-in word.)
- The definition records the meaning of its body *now*: a body token that names a user word stands for that word's current meaning, even if the word is redefined later. A word can be redefined; its new body may use its previous meaning. A body may be empty.
- A successful definition prints nothing and does not touch the stack.

Calling a user word runs its recorded body as if those tokens appeared in place of the call.

Checks happen in this order for a definition: unfinished definition, bad name, then the body tokens left to right.

## Errors

Any other token that is not a number literal, a built-in word or a defined user word is the error `unknown word <token>` (this includes a stray `;`).

When an error happens, print `error: <message>`, empty the stack, and skip the rest of the current line. Output printed earlier on that line stays. Processing continues with the next line; user-defined words are kept.

## End

After the last input line, print the stack once more in the `.s` format. Every output line ends with a newline.

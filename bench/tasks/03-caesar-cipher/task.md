# Caesar cipher

Shift every letter of a text by a fixed number of positions in the alphabet.

## Input

The first line holds an integer `k` (between -1000 and 1000). Every following line is text to encode. There may be no text lines.

## Output

Each text line, with every ASCII letter moved `k` positions forward in the alphabet (backward when `k` is negative), wrapping around from `z` to `a`. Uppercase letters stay uppercase and lowercase letters stay lowercase. All other characters are unchanged.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
3
Hello, World!
xyz
```

Output:

```
Khoor, Zruog!
abc
```

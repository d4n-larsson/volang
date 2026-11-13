# Run-length decoding

Decode run-length encoded lines.

Each line of standard input (lines end with `\n`; the last line may lack it) is decoded on its own and produces exactly one output line. Lines are not trimmed: spaces are ordinary characters.

An encoded line is a sequence of zero or more runs. A run is a count followed by exactly one character:
- The count is one or more decimal digits with value at least 1 and no leading zero (`0` and `03` are not valid counts). A count has at most 9 digits.
- The character is any single character that is not a digit (a space is allowed).
- The run decodes to the character repeated count times, so `3a12b1 ` decodes to `aaa`, twelve `b`, then one space. Runs are concatenated in order.

Errors: scan the line from the left; the first problem found decides the output, which is `error at column C` with `C` the 1-based byte position:
- a character that is not preceded by a count (for example the first `a` of `a3b`, or the `b` of `2ab`): `C` is the position of that character;
- a count that is `0` or starts with `0`: `C` is the position of its first digit;
- a count at the end of the line with no character after it: `C` is the length of the line plus 1.

If the line has none of these problems but decodes to more than 1000 characters in total, print `error: too long` instead of the decoded text.

An empty input line decodes to an empty output line. Empty input produces no output. Every output line ends with a newline.

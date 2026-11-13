# Run-length encoding

Compress each line with run-length encoding.

## Input

Zero or more lines, each made of lowercase letters `a`-`z` only. A line may be empty.

## Output

For every input line, its encoding: each maximal run of the same letter becomes the length of the run followed by the letter. For example `aaabcc` becomes `3a1b2c`. An empty line gives an empty line.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
aaabccdddd
abc
```

Output:

```
3a1b2c4d
1a1b1c
```

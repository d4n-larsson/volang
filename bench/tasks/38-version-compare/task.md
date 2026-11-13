# Version comparison

Compare version numbers.

## Input

Zero or more lines, each with two versions `A B` separated by a single space. A version is one or more non-negative integers separated by dots, such as `1.2.10`. Parts may have leading zeros.

## Output

For every line, `<` if A is lower than B, `>` if A is higher, and `=` if they are equal. Compare the parts numerically from left to right; a missing part counts as 0, so `1.0` equals `1`.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
1.2.10 1.10
1.0 1
```

Output:

```
<
=
```

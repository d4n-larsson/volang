# Remove duplicate lines

Remove repeated lines while keeping the original order.

## Input

Zero or more lines of text.

## Output

Every distinct input line once, in the order of its first appearance. Lines are compared exactly (case-sensitive); empty lines count as lines too.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
apple
banana
apple
cherry
banana
```

Output:

```
apple
banana
cherry
```

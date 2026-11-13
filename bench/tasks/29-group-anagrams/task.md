# Group anagrams

Group words that are anagrams of each other.

## Input

Zero or more lines, each a non-empty word of lowercase letters.

## Output

One line per group of words that use exactly the same letters the same number of times. Groups appear in the order of their first word in the input; the words in a group appear in input order, separated by single spaces. Repeated words are listed every time they appear.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
eat
tea
tan
ate
nat
bat
```

Output:

```
eat tea ate
tan nat
bat
```

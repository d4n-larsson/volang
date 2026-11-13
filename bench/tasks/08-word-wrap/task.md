# Word wrap

Re-flow a text so that no line is longer than a given width.

## Input

The first line holds the width `w` (at least 1). The following lines hold the text. A word is a maximal run of characters other than spaces and newlines; line breaks in the text count as spaces.

## Output

The words in their original order, placed greedily: a line takes as many words as fit, separated by single spaces, with the line length (in characters) at most `w`. A word longer than `w` goes alone on its own line. If there are no words, print nothing.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
10
the quick brown fox jumps over the lazy dog
```

Output:

```
the quick
brown fox
jumps over
the lazy
dog
```

# Word frequency

Count how often each word occurs.

## Input

Zero or more lines of ASCII text. A word is a maximal run of letters (`a`-`z`, `A`-`Z`); every other character separates words. Words are compared after converting them to lowercase.

## Output

One line per distinct word: the lowercase word, a single space, and its count. Sort the lines by count, highest first; words with the same count are sorted alphabetically. If there are no words, print nothing.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
the cat and the hat
The end
```

Output:

```
the 3
and 1
cat 1
end 1
hat 1
```

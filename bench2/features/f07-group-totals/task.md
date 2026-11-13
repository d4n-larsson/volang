# Group totals

Each line of standard input is `CATEGORY AMOUNT`: a category of lowercase letters and an integer amount (it may be negative, absolute value at most 1000000). The input may be empty.

For each distinct category, in order of first appearance, print

```
CATEGORY count=C total=T max=M
```

with the number of lines of that category, the sum of their amounts and the largest amount. Finally print `grand G`, the sum of all amounts (0 for empty input).

Group the rows by key and compute each group's figures with `filter` and `fold`.

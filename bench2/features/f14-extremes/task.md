# Extremes

Each line of standard input is `int` or `str` followed by zero or more values, all separated by single spaces: integers after `int`, words of ASCII letters after `str`.

For each line print `empty` if it has no values, otherwise

```
min=MIN max=MAX median=MED distinct=D
```

where `MED` is the element at 0-based position `(n - 1) / 2` (integer division) of the values sorted ascending, and `D` is the number of different values. Strings compare by byte order.

Write the computation once as generic functions over an ordered element type and use them for both kinds of line; the function computing min and max returns a generic variant that is empty for an empty list.

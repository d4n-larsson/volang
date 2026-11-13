# Grade report

Each line of standard input is `NAME SCORE`: a name of lowercase letters and an integer score from 0 to 100. The input may be empty.

Print exactly these six lines:

```
count N
passed NAMES
total T
average A
any_perfect YES_NO
all_passed YES_NO
```

- `N` is the number of students.
- `NAMES` are the names of the students with score at least 50, in input order, separated by single spaces; `-` if there are none.
- `T` is the sum of all scores.
- `A` is `T / N` rounded down (integer division); `-` if there are no students.
- `any_perfect` is `yes` if some student has 100, else `no`.
- `all_passed` is `yes` if every student has at least 50 (also when there are no students), else `no`.

Compute these with `filter`, `map`, `fold`, `any` and `all` rather than hand-written loops.

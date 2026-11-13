# Top earners per department

The first line of standard input is an integer `K >= 0`. Each following line is `DEPT NAME SALARY`: department and name of lowercase letters, an integer salary (it may be negative). There may be no such lines.

For each department, in ascending byte order of its name, print

```
DEPT total=T top=LIST
```

where `T` is the sum of the department's salaries and `LIST` is the department's first `K` employees (all, if it has fewer) ordered by salary descending, then by name ascending, written `NAME:SALARY` and separated by `,`; `LIST` is `-` when it is empty.

Use `filter`, `fold`, `map` and sort-by-key.

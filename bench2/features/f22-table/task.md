# Score table

Each line of standard input is `NAME,SCORE`: a name of lowercase letters (at most 12 characters) and an integer score from 0 to 9999999. There is at least one line, and the scores add up to more than 0.

Print a table with three columns separated by `|`: the name left-aligned in 12 characters, the score right-aligned in 10 with `,` between groups of three digits, and the share of the total as a percentage with one decimal and a `%` sign, right-aligned in 7 (the `%` included).

- The first line is the header: `name`, `score` and `share`, aligned like the columns.
- The second line is `------------+----------+-------`.
- Then one line per input line, in input order. The share is `float(score) * 100.0 / float(total)`.
- Then the separator line again, and a last line `total` with the sum of the scores and `100.0%`.

Example: `ana,1500` with a total of 3000 prints `ana         |     1,500|  50.0%`.

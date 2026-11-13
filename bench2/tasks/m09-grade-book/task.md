# Grade book

Read a course's assignments, students and scores from standard input, compute each student's weighted final grade with integer arithmetic, and print the grades, class statistics, and per-assignment averages.

## Program structure

Split the program into at least three source files: line parsing, the grade book (stored data and the per-student grade computation including dropped assignments), and the statistics/report output each in their own file, plus the entry file that reads standard input and drives them.

## Input

Lines are numbered from 1; every line counts. A line is split into fields at runs of spaces; leading and trailing spaces are ignored; lines without fields are ignored. Below, `digits(a-b)` means `a` to `b` ASCII digits (leading zeros allowed). Names are any fields.

| Line | Rules |
|---|---|
| `assignment <name> <weight> <max>` | weight is digits(1-3) and at least 1; max is digits(1-4) and at least 1; the name must not be an already declared assignment |
| `student <name>` | the name must not be an already declared student |
| `score <student> <assignment> <points>` | both must be declared on earlier lines; points is digits(1-4) and at most the assignment's max. A later score for the same student and assignment replaces the earlier one. |
| `drop <n>` | n is exactly one digit; sets the number of dropped assignments (the last `drop` line wins; the default is 0) |

Any other line, or one breaking these rules, prints `invalid line <N>` and changes nothing. These lines are printed first, in input order.

## Grades

All values are integers. *Basis points* (bp) are hundredths of a percent: 10000 bp is 100%. `round(x / y)` below means `x / y` rounded to the nearest integer with halves rounded up (all values involved are non-negative).

- A student's score on an assignment, in bp, is `points * 10000 / max` rounded **down**. A missing score is 0 bp.
- With `n` declared assignments and `d` the drop count, each student drops `k = min(d, n - 1)` assignments (none if `n` is 0): those with the lowest bp; among equal bp, the assignment declared earlier is dropped first.
- The student's *final* grade is `round(sum(weight * bp) / sum(weight))` over the assignments they keep, or 0 if there are no assignments.
- The letter is `A` for a final of at least 9000, `B` for at least 8000, `C` for at least 7000, `D` for at least 6000, and `F` otherwise.

A bp value is printed as a percentage with two decimals: `bp / 100`, `.`, then `bp % 100` as two digits (`8753` is `87.53`, `10000` is `100.00`, `5` is `0.05`).

## Output

After the invalid lines:

1. `students:` and one line per student, ordered by final grade (highest first), then by name in byte order (ASCII order): `<name> <final> <letter>`.
2. `stats:` then, with `c` the number of students:
   - `count <c>`; if `c` is 0, nothing more in this section;
   - `mean <m>` with `m = round(sum of finals / c)`;
   - `median <m>`: the middle final of the sorted finals if `c` is odd; otherwise `round((a + b) / 2)` of the two middle finals;
   - `top <final> <names>`: the highest final and every student who has it, in byte order, joined by `,`;
   - `letters A:<n> B:<n> C:<n> D:<n> F:<n>`: how many students got each letter.
3. `assignments:` and one line per assignment in declaration order: `<name> avg <a> missing <k>`, where `a = round(sum of all students' bp on it / c)` (missing scores count as 0; `a` is 0 if there are no students) and `k` is the number of students with no score on it. Dropping does not affect this section.

Every output line ends with a newline.

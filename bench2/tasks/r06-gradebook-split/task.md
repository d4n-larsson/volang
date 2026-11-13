# Gradebook: split the grading module, letter grade modifiers

## The existing program

The program is a gradebook. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9`; inputs keep every number below 1000000.

All averages and percentages are integers counting **tenths of a percent** (875 means 87.5%) and are printed as `<x / 10>.<x % 10>` (`875` prints `87.5`, `1000` prints `100.0`, `0` prints `0.0`). `rdiv(a, b)` below means a / b rounded half up, computed as `(a + b / 2) / b` with integer division (all values are non-negative).

Commands (`NAME`, `STUDENT` and `ASSIGNMENT` are single words):

- `assignment NAME WEIGHT MAX` (WEIGHT is a number from 1 to 100, MAX a number of at least 1): adds an assignment worth MAX points. Prints `assignment NAME added`, or `error: assignment NAME exists` (nothing changes).
- `enroll NAME`: adds a student. Prints `student NAME enrolled`, or `error: student NAME exists`.
- `score STUDENT ASSIGNMENT POINTS` (POINTS is a number): records (or replaces) the student's points for the assignment. Checks in this order: `error: unknown student STUDENT`, `error: unknown assignment ASSIGNMENT`, then if POINTS is above the assignment's MAX, `error: POINTS above max MAX for ASSIGNMENT` (nothing changes). Prints `STUDENT ASSIGNMENT POINTS/MAX`.
- `card STUDENT`: the student's report card. Unknown student: `error: unknown student STUDENT`. The first line is `STUDENT average AVG LETTER`, or `STUDENT no scores` when the student has no score at all. Then one line per assignment in the order the assignments were added: `  ASSIGNMENT POINTS/MAX PCT` (two leading spaces) or `  ASSIGNMENT missing` when the student has no score for it.
- `stats ASSIGNMENT`: unknown assignment: `error: unknown assignment ASSIGNMENT`. With no scores: `ASSIGNMENT no scores`. Otherwise, over the N students that have a score for it, `ASSIGNMENT count N min LOW max HIGH mean MEAN` where LOW and HIGH are points (plain integers) and MEAN is `rdiv(10 * (sum of points), N)` printed as tenths (so `mean 15.3` means 15.3 points).
- `report`: the class report. Students with at least one score, sorted by average, highest first, ties by name (ascending byte order), each as `RANK. NAME AVG LETTER`, where RANK is 1 + the number of students with a strictly higher average (so tied students share a rank: `1.`, `2.`, `2.`, `4.`). Then each student without scores, by name (ascending byte order), as `-. NAME no scores`. Last, `class average X` with X = `rdiv(sum of the listed averages, their count)`, or `class average none` when no student has a score.

The percentage for one score is PCT = `rdiv(POINTS * 1000, MAX)`. A student's average AVG uses only the assignments the student has a score for: `rdiv(sum of WEIGHT * PCT, sum of WEIGHT)`.

The letter grade for an average is `A` from 900 up, `B` from 800, `C` from 700, `D` from 600, else `F`.

A command with the wrong number of arguments or with a value that is not as described prints `error: bad command` and changes nothing. A first word that is not a command prints `error: unknown command WORD`.

## The change

All the statistics and all the letter-grade logic currently live in one module (`grading`). Split it: put the numeric statistics (rounding, percentages, averages, assignment statistics) in one module, the letter grades in another, and the report output (card, report, distribution) in a third, so that the letter function is defined once and used by every command that shows a letter. Then make these two changes:

1. **Letter grades get `+` and `-` modifiers.** For an average in the `A`, `B`, `C` or `D` band, let R = average − the band's lower limit (900, 800, 700 or 600). If R ≥ 70 the letter gets `+`; if R < 30 it gets `-`; otherwise it has no modifier. So 97.0 to 100.0 is `A+`, 93.0 to 96.9 `A`, 90.0 to 92.9 `A-`, 87.0 to 89.9 `B+`, and so on down to 60.0 to 62.9 `D-`. `F` never has a modifier. This changes the letters shown by `card` and `report`.
2. **New command `distribution`** (no arguments, else `error: bad command`): counts the students with at least one score by letter grade (with modifiers). For each letter in the order `A+ A A- B+ B B- C+ C C- D+ D D- F` that at least one student has, it prints `LETTER N NAMES`, where N is the number of those students and NAMES their names in ascending byte order separated by single spaces. If no student has a score, it prints `no grades`.

Everything else stays exactly as it is: the `assignment`, `enroll`, `score` and `stats` outputs are byte-identical, and `card` and `report` differ only in the letters.

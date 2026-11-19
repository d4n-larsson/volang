# Yearly growth

Each line of standard input is `YEAR VALUE`: a year and a positive integer value below 10^15, with the years increasing. The input may be empty.

For each line print the year, a space, the value right-aligned in 18 characters with `,` between groups of three digits, a space, and the change from the previous line's value right-aligned in 9 characters: `-` for the first line, otherwise the percentage `float(value - previous) * 100.0 / float(previous)` with one decimal, always with its sign (`+` or `-`), followed by `%`.

Example: `2020 1000` then `2021 1250` print `2020              1,000         -` and `2021              1,250    +25.0%`.

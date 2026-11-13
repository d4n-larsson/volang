# Day of the year

Validate dates and compute their day of the year.

## Input

Zero or more lines, each a candidate date.

## Output

For every line: if it is a valid date written exactly as `YYYY-MM-DD` (4 digits, `-`, 2 digits, `-`, 2 digits), with a year from 1 to 9999, a month from 1 to 12 and a day that exists in that month, print its day of the year (January 1 is 1). Otherwise print `invalid`. February has 29 days in leap years: years divisible by 4, except years divisible by 100 that are not divisible by 400.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
2024-03-01
2023-03-01
```

Output:

```
61
60
```

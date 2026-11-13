# Date weekday

For each line of standard input (lines end with `\n`; the last line may lack it), validate a date and print its weekday and day of the year, or `invalid`. Lines are not trimmed.

A valid line is exactly `YYYY-MM-DD`: 10 characters, four digits, `-`, two digits, `-`, two digits, nothing before or after (no spaces). It must also be a real date in the proleptic Gregorian calendar (the Gregorian rules applied to every year, including years before 1582):
- the year is from 0001 to 9999 (0000 is invalid);
- the month is from 01 to 12;
- the day is from 01 to the length of the month: 31 for January, March, May, July, August, October and December, 30 for April, June, September and November, and for February 29 in a leap year and 28 otherwise;
- a year is a leap year when it is divisible by 4, except years divisible by 100, which are leap years only when also divisible by 400 (2000 is a leap year, 1900 is not).

Output for a valid date: the English weekday name (`Monday`, `Tuesday`, `Wednesday`, `Thursday`, `Friday`, `Saturday`, `Sunday`), one space, and the day of the year without leading zeros (January 1st is 1). Example: `2024-03-01` gives `Friday 61`. For anything else print `invalid` (including an empty line).

One output line per input line; empty input produces no output. Every output line ends with a newline.

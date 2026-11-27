# Recurring events

Expand recurring calendar events and list the days in a date range on which they occur.

Dates are written `YYYY-MM-DD` and use the Gregorian calendar: a year is a leap year if it is divisible by 4, except years divisible by 100 that are not divisible by 400 (2000 is a leap year, 1900 and 2100 are not). Every date in the input lies between 1900-01-01 and 2200-12-31. Weeks run from Monday to Sunday.

## Input

The input is well-formed. Tokens are separated by single spaces; lines have no leading or trailing spaces. The first line is `FROM TO`, two dates with FROM ≤ TO: the range to report, both days included. Every following non-empty line is an event line or an exception line (empty lines are ignored):

- `event NAME RULE`: NAME is lowercase ASCII letters, digits and `-`; no two event lines have the same NAME.
- `except NAME DATE`: the event NAME does not take place on DATE. An exception line may come before or after its event's line; one for a name that has no event line, or for a date that is not an occurrence of the event, has no effect.

A RULE is one of the forms below, where `N` is an integer from 1 to 1000 and `D` is the event's start date. An event never occurs before `D`.

- `every N days from D`: on `D`, `D + N` days, `D + 2N` days, ...
- `every N weeks on DAYS from D`: DAYS is a comma-separated list of weekdays `mon tue wed thu fri sat sun` (in any order, possibly repeated). The event occurs on each listed weekday of the week that contains `D`, and of every N-th week after it (`N` = 2: every other week), except on days before `D`.
- `every N months on day K from D`: `K` is 1 to 31, or -1 to -31 to count from the end of the month (-1 is the last day, -2 the day before it). The event occurs on day K of the month of `D` and of every N-th month after it. A month without such a day (day 31 of April, day -30 of February) has no occurrence; the date is never moved to another day.
- `every N months on ORD WEEKDAY from D`: ORD is `1st`, `2nd`, `3rd`, `4th`, `5th` or `last`, WEEKDAY one of the weekdays above: the 1st, ..., 5th or last such weekday of the month of `D` and of every N-th month after it. A month without a 5th such weekday has no occurrence.
- `every N years on MM-DD from D`: on month MM, day DD of the year of `D` and of every N-th year after it. In a year without that day (02-29 outside leap years) there is no occurrence.

A rule may end with ` until U` (the event never occurs after the date U), then with ` count C` (C ≥ 1: the event has only its first C occurrences, counted in date order from `D`), or both, in that order. Every day the rule gives counts towards `count`, including days before FROM and days removed by an exception line.

## Output

One line for each day from FROM to TO on which at least one event occurs, in date order: the date, the weekday as `Mon`, `Tue`, `Wed`, `Thu`, `Fri`, `Sat` or `Sun`, and the names of the events on that day in the order of their event lines, all separated by single spaces. If no event occurs in the range, print the single line `no events`. Every output line ends with a newline.

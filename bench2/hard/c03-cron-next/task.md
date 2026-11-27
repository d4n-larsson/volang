# Cron schedule

Read a crontab from the directory `jobs/` and answer questions about when its jobs run.

All times are UTC calendar times written `YYYY-MM-DD HH:MM`, with minute precision. The current time comes from the input, never from the system clock.

## Input

Standard input, line 1: `now <time>`. Line 2: `crontab <name>`. Every further non-empty line is a query. Lines 1 and 2 are always present and well-formed, `now` is a valid time between the years 2000 and 2098.

If the crontab name is not 1 to 30 characters, each a lowercase ASCII letter, a digit or `-`, print `error: bad crontab name` and stop. Otherwise the crontab is the file `jobs/<name>.cron`; if it does not exist, print `error: crontab not found` and stop.

## Crontab

Ignore lines that are empty or contain only spaces, and lines whose first character other than a space is `#`. Line numbers count every line of the file, starting at 1. Every other line is split into fields at runs of one or more spaces (spaces at both ends are ignored) and is checked; the first failing check gives the error `line <n>: <message>` and the line is skipped:

1. `expected 6 fields`: there are not exactly six fields: `<minute> <hour> <day> <month> <weekday> <job>`.
2. `bad minute field`, `bad hour field`, `bad day field`, `bad month field`, `bad weekday field`: the first of these five fields, in this order, that is not valid (below).
3. `bad job name`: the job name is not 1 to 20 characters, each a lowercase ASCII letter, a digit or `-`.
4. `duplicate job <job>`: an earlier line that passed all checks has the same job name.

**Time fields.** The allowed values are minute 0–59, hour 0–23, day of month 1–31, month 1–12, weekday 0–7 (0 and 7 both mean Sunday, 1 is Monday). A field is a list of one or more items separated by `,`; it is valid when every item is valid, and it stands for the union of its items' values. An item is one of:

- `*`: every allowed value;
- `<a>`: the value `a`;
- `<a>-<b>`: the values `a` to `b`, with `a <= b`;
- any of the two previous forms but with `*` or a range, followed by `/<s>`: `*/<s>` and `<a>-<b>/<s>` stand for the first value of the range (the lowest allowed value for `*`) and then every `s`-th value after it within the range (`10-30/15` is 10, 25). `s` is at least 1 and may be larger than the range (`*/90` in the minute field is just 0). A single value with a step (`5/2`) is not valid.

Numbers are one or more ASCII digits; `a` and `b` must lie within the allowed values. Nothing else is valid: no names, no spaces, no empty items, no signs.

**When a job runs.** A job runs at every minute whose minute, hour and month are in its fields and whose date matches the day rule: if the day field or the weekday field is exactly the text `*`, the date must match both fields (a `*` matches every date); otherwise (both are something other than exactly `*`, even `*/1`) the date must match at least one of them. A date matches the day field when its day of month is in it, and the weekday field when its weekday is in it. Dates that do not exist (April 31) never occur.

**Runs after now.** Queries only look at runs strictly after `now` and at most 366 days after it (`now` plus 366 × 24 hours, that minute included).

## Queries

Print each query line as `> <query>` before its answer. A query is one of the following, with single spaces; anything else (an unknown word, a wrong number of words, a count that is not ASCII digits or is outside 1–50, a time or date that is not valid) is answered `bad query`. A valid time or date is written exactly in its form (four, two, two, then two and two ASCII digits), is a real date of a year from 2000 to 2099, and has hours 00–23 and minutes 00–59.

- `next <job> <k>`: the first `k` runs of the job after now, one line each, `<time> <Mon|Tue|Wed|Thu|Fri|Sat|Sun>`. If there are fewer than `k`, print those, then `no more runs`. A job that is not in the crontab (or whose line had an error): `unknown job <job>`.
- `timeline <k>`: the first `k` runs of all jobs together, after now, sorted by time and then by job name (byte order), one line each, `<time> <job>`. If there are fewer than `k`, print those, then `no more runs`.
- `at <time>`: the jobs that run at that minute (whether or not it is after now), sorted by name and joined with `,`, as `<time> <jobs>`, or `<time> none`.
- `day <YYYY-MM-DD>`: for each job that runs on that date (whether or not it is after now), in crontab order, `<job> <number of runs on that date>`; then `total <sum>`.

## Output

First print the crontab errors in file order, then `jobs <number of lines that passed all checks>`, then the answers to the queries in input order.

## Allowed effects

The program may print, read standard input, and read files under `jobs/`. Nothing else: no clock (the current time is line 1 of the input), no reading outside `jobs/`, no writing files, no environment variables, no randomness, no network.

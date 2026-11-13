# Log analyzer: extract the log-line parser and fix a timestamp bug

## The existing program

The program analyzes a log. Standard input holds log lines, then a line that is exactly `---`, then query commands, one per line. Lines that are empty or contain only spaces are ignored everywhere (they are not log lines and not commands). Only the first `---` line separates the sections; a later `---` line is a command. If there is no `---` line, every line is a log line and nothing is printed.

Words on a line are separated by one or more spaces.

### Log lines

A log line is *valid* when all of these hold (otherwise it is *invalid* and only counts in `stats`):

- It has at least 4 words: `DATE TIME LEVEL COMPONENT: [MESSAGE...]`.
- DATE is `YYYY-MM-DD`: exactly 10 characters, `-` at positions 5 and 8, digits elsewhere; the year is at least 1 (`0000` is invalid), the month 01-12, the day from 01 to the length of that month (Gregorian calendar: February has 29 days when the year is divisible by 4 and not by 100, or divisible by 400).
- TIME is `HH:MM:SS`: exactly 8 characters, `:` at positions 3 and 6, digits elsewhere; HH is 00-23, MM and SS are 00-59.
- LEVEL is exactly one of `DEBUG`, `INFO`, `WARN`, `ERROR` (upper case).
- COMPONENT is the fourth word; it must end with `:` and be at least 2 characters long. The component's name is the word without the final `:`.
- The message is the remaining words (possibly none) joined with single spaces.

An entry is *printed* as its words joined with single spaces (for example `2024-03-05   14:07:09 ERROR auth: x` prints as `2024-03-05 14:07:09 ERROR auth: x`; with no message, `... DEBUG web:`).

The *timestamp* of a DATE and TIME is the number of seconds since `0001-01-01 00:00:00` (days × 86400 + HH × 3600 + MM × 60 + SS, where days is the number of days from 0001-01-01 to DATE in the Gregorian calendar).

### Commands

- `stats`: `lines N valid V invalid I`, where N is the number of (non-blank) log lines.
- `levels`: four lines `DEBUG n`, `INFO n`, `WARN n`, `ERROR n` counting the valid entries of each level.
- `errors`: one line `NAME n` per component name with at least one `ERROR` entry, sorted by n descending, ties by name ascending (byte order, so `Alpha` comes before `ab`). If there are none: `no errors`.
- `busiest`: `busiest hour HH with n entries`, the hour of the day (00-23, always two digits, from the TIME field) with the most valid entries; on a tie the earliest hour. With no valid entries: `no entries`.
- `range DATE TIME DATE TIME`: if either DATE/TIME pair is not valid (same rules as in log lines), prints `error: bad time`. Otherwise prints every valid entry whose timestamp t satisfies from ≤ t ≤ to, in input order, then `K entries in range` (K may be 0, e.g. when from > to).
- `span`: `span S seconds` where S is the latest timestamp minus the earliest one among the valid entries (0 for one entry). With no valid entries: `no entries`.
- `grep WORD`: prints every valid entry whose message contains WORD (case-sensitive substring; the date, time, level and component are not searched), in input order, then `K matches`.

Any of these commands with the wrong number of words after it prints `error: bad command`. A first word that is not a command (commands are lower case) prints `error: unknown command WORD`.

## The change

The code that splits a log line and checks it (date, time, level, component) is written out separately in three modules: once for `stats`/`levels`/`errors`, once for `busiest`/`range`/`span`, and once for `grep`. Extract it into one shared function (in its own module) that parses a line into an entry record (level, component name, message, hour, timestamp, printed text), plus one shared DATE/TIME-to-seconds function, and make every command use them. Remove the duplicated copies.

One of the copies has a bug: the timestamp computation used by `range` and `span` ignores the seconds field, both for log lines and for the `range` bounds. Currently `range 2024-03-05 14:00:00 2024-03-05 14:30:00` includes an entry at `14:30:59`, and `span` over entries at `09:00:00` and `23:59:59` prints `span 53940 seconds`. After the change, timestamps include the seconds exactly as specified above: that `range` excludes the `14:30:59` entry and the `span` is `53999 seconds`.

Everything else (validation rules, every message, ordering, the other commands) stays exactly as it is.

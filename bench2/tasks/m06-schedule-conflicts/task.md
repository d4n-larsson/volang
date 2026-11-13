# Schedule conflicts

Read a week's meetings and free-time queries from standard input. Report invalid lines, conflicting meeting pairs, how long each room is booked, and the earliest free slot for each query.

## Program structure

Split the program into at least three source files: line and time parsing, the calendar (stored meetings, conflict detection and room usage), and the free-slot search each in their own file, plus the entry file that reads standard input, drives them and prints the sections.

## Input

Lines are numbered from 1; every line counts. A line is split into fields at runs of spaces; leading and trailing spaces are ignored; lines without fields are ignored.

- A *day* is one of `Mon Tue Wed Thu Fri`.
- A *time* is `HH:MM` with exactly two digits each, `HH` from `00` to `23` and `MM` from `00` to `59`. It stands for `HH * 60 + MM` minutes.
- A *people list* is one or more names separated by single commas, with no empty name and no name repeated (`ana,bo`; `ana,,bo`, `ana,` and `ana,ana` are not people lists). A name is any text without commas.

Lines have one of two forms:

- `meeting <id> <room> <day> <start>-<end> <people>`: the fourth field is two times joined by `-`, and `end` must be later than `start`. The meeting occupies the minutes from `start` up to but not including `end`. `id` and `room` are any fields. The id must differ from the ids of all earlier valid meetings.
- `free <day> <minutes> <people>`: `minutes` is 1 to 3 digits and greater than zero (leading zeros allowed).

Any other line (unknown first word, wrong number of fields, a malformed value, a repeated meeting id) is invalid and takes no further part.

## Output

Print four sections, in this order:

1. For each invalid line, in input order: `invalid line <N>`.
2. `conflicts: <count>` and then one line per conflicting pair. Two valid meetings *conflict* when they are on the same day, their time ranges overlap (`start1 < end2` and `start2 < end1`; touching ranges do not overlap), and they use the same room or share at least one person. For a pair whose meetings come at input positions `i < j`, print `<id_i> <id_j>: <reasons>`, where the reasons are `room <room>` if the rooms are equal, then `person <name>` for each shared person in byte order (ASCII order), separated by `, `. Pairs are ordered by `i`, then by `j`.
3. `rooms:` and then, for each room used by a valid meeting, in byte order: `<room> <minutes>`, the number of minutes in the week during which the room is booked by at least one meeting (overlapping meetings are counted once).
4. For each valid `free` line, in input order: `free line <N>: <answer>`. The answer is the earliest start time `t` such that `t >= 08:00`, `t + minutes <= 18:00`, and no valid meeting on that day with any of the listed people overlaps the range from `t` up to `t + minutes`. Meetings anywhere in the input count, also those after the `free` line. Print `t` as `HH:MM` (two digits each), or `none` if there is no such time.

Every output line ends with a newline.

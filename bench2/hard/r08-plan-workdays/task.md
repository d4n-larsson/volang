# Project planner: working days

## The existing program

The program plans a project made of tasks. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9`; inputs keep every number below 10000. Days are plain numbers: the project starts on day 0.

A task has an ID (a word of lowercase letters, digits and `-` that starts with a letter), a name, a duration in days (0 makes it a *milestone*), an optional "not before" day, a percent done, and finish-to-start dependencies on other tasks, each with a lag: `b` after `a+2` means `b` may start 2 days after `a` finishes. Tasks are kept in creation order (removing a task and adding it again puts it last).

**The schedule** is computed whenever a command needs it:

- *Schedule order*: repeatedly take, among the tasks not yet taken whose predecessors have all been taken, the one created first.
- *Forward pass*, in schedule order: a task's `start` is the largest of its "not before" day (0 if not set) and, for each dependency, the predecessor's `finish` plus the lag. Its `finish` is `start + duration`: the first day after its work. A task with duration 3 starting on day 5 works on days 5, 6 and 7 and has finish 8; a milestone has finish = start.
- The project's *end* is the largest `finish` of all tasks.
- *Backward pass*, in reverse schedule order: a task's late finish `lfinish` is the smallest of the project end and, for each task that depends on it, that task's `lstart` minus the dependency's lag. Its late start `lstart` is `lfinish - duration`, and its `slack` is `lstart - start`.

Commands (`ID` and `PRED` are task IDs; `NAME...` is one or more words, joined with single spaces):

- `task ID DURATION [NAME...]`: adds a task (the name defaults to the ID). Prints `added task ID`, or `added milestone ID` for duration 0. Errors: `error: bad task id ID`, `error: task ID exists`.
- `rename ID NAME...`: prints `task ID renamed`.
- `dep ID PRED` or `dep ID PRED+LAG` (LAG a number): `ID` depends on `PRED`. Prints `ID after PRED` (or `ID after PRED+LAG` when LAG is not 0). Errors, checked in this order: `error: unknown task ID`, `error: unknown task PRED`, `error: task ID cannot depend on itself`, `error: ID already depends on PRED`, and `error: cycle ID -> PRED -> ... -> ID` when PRED already depends (directly or not) on ID; the chain is the first one found by a depth-first search from PRED that tries each task's predecessors in the order they were added.
- `undep ID PRED`: prints `ID no longer after PRED`; errors for unknown tasks, then `error: ID does not depend on PRED`.
- `remove ID`: prints `removed ID`; `error: ID is needed by A, B` (the tasks depending on it, in byte order) leaves it in place.
- `duration ID N`, `notbefore ID DAY`, `progress ID PCT` (PCT from 0 to 100; `error: ID is a milestone`): change the task and confirm.
- `list`: one line per task in creation order, `ID 5d NAME` (`ID ms NAME` for a milestone).
- `show ID`: the task's details, including `  start S finish F slack K` from the schedule.
- `schedule`: a table with the columns `task start finish lstart lfinish slack`, one row per task in schedule order, then `end E`.
- `critical`: `critical: ` followed by the IDs of the tasks with slack 0 in schedule order, then ` (L days)` with L the length of the project, its end.
- `gantt` or `gantt FROM TO`: a ruler line (the last digit of each day) and one row per task in schedule order, one character per day: `#` on the days the task works (`start` to `finish - 1`), `*` on a milestone's day, `.` otherwise. Without arguments the days are 0 to the project end (at most 60 days: 0 to 59); `error: bad range` if FROM > TO, `error: range too wide` for more than 60 days.
- `status DAY`: `status on day DAY`, then for each task that is not a milestone, in schedule order, `  ID: planned P done D WORD`, where P is how many of the task's days come before day DAY, D is duration × percent / 100 rounded down and WORD is `ahead`, `behind` or `on track` (D > P, D < P, equal), then `total planned P done D X%` with the sums and X = D × 100 / P rounded down (`-` instead of `X%` when P is 0).
- `end`: `project finishes on day E`.

With no tasks, `list`, `schedule`, `critical`, `gantt`, `status` and `end` print `no tasks`. Wrong argument counts or a non-number where a number is required print `error: bad command`; an unknown first word prints `error: unknown command WORD`. The code shows every message exactly.

## The change

The project now follows a **working-day calendar**. Day numbers stay calendar days, but durations, lags and slack count **working days** only.

**The calendar.** Day 0 is a Monday, so day D is a Monday when D % 7 is 0, ..., a Saturday when D % 7 is 5 and a Sunday when D % 7 is 6. Saturdays and Sundays are off; every other day is a working day. Two new commands change single days (D a number; wrong argument counts or a non-number print `error: bad command`):

- `holiday D`: makes day D a day off. Prints `day D is now off`, or `error: day D is already off` if it is not a working day.
- `workday D`: makes day D a working day (a Saturday or Sunday, or a day made a holiday). Prints `day D is now a working day`, or `error: day D is already a working day`.

Calendar changes can come at any time, before or after the tasks: every command uses the calendar as it is at that moment.

A third new command, `cal FROM TO` (two numbers, else `error: bad command`; `error: bad range` if FROM > TO), prints `days FROM-TO: W working, O off`, counting the days FROM to TO (both included), and then, only when O > 0, a line `off: ` followed by those days off in increasing order, separated by single spaces.

**The schedule** (all its days remain calendar day numbers):

- *Forward pass*: for each dependency, the predecessor's `finish` moved forward by LAG working days: the LAG-th working day after it (the `finish` itself when LAG is 0). The task's earliest day is the largest of these and its "not before" day (0 if not set), and its `start` is the first working day on or after that earliest day. A task with duration N works on N working days: `start` and the next N − 1 working days. Its `finish` is the first working day after the last of them. A milestone's `finish` is its `start`.
- The project end is still the largest `finish`.
- *Backward pass*: for each task depending on it, that task's `lstart` moved back by LAG working days: the LAG-th working day before it (itself when LAG is 0); `lfinish` is the smallest of these and the project end. `lstart` is the N-th working day before `lfinish` (counting back over the working days before `lfinish`; a milestone's `lstart` is its `lfinish`).
- `slack` is the number of working days from `start` up to, not including, `lstart`.

Example: tasks `a` (duration 3) and `b` (duration 2) with `dep b a+1`, and day 4 (a Friday) a holiday. `a` works on days 0, 1, 2 and has finish 3. Moved forward 1 working day from 3 is day 7 (4 is a holiday, 5 and 6 a weekend), so `b` starts on 7, works on 7 and 8 and has finish 9; the end is 9. Backward: `b` has lfinish 9 and lstart 7; moved back 1 working day from 7 is 3, so `a` has lfinish 3 and lstart 0. Both have slack 0.

**The other commands** use this schedule, and also:

- `critical`: L is the number of working days from day 0 up to, not including, the project end, and the line ends ` (L working days)`. In the example: `critical: a b (6 working days)`.
- `gantt`: in a task's row, the days from its `start` to its last working day (both included) show `#` on working days and `:` on days off; all other days show `.` (a milestone still shows `*` on its `start`). The ruler and the choice of days are unchanged. In the example, with `gantt 0 9`, the row of `a` is `###.......`; if `a` had duration 5, it would work on days 0, 1, 2, 3 and 7 and its row would read `####:::#..`.
- `status DAY`: P is the number of the task's working days that come before day DAY.
- `end`: the line becomes `project finishes on day E (DAY)`, where DAY is the weekday of E: `Mon`, `Tue`, `Wed`, `Thu`, `Fri`, `Sat` or `Sun`. In the example: `project finishes on day 9 (Wed)`.

`show`, `schedule` and `list` keep their formats (durations are still printed as `5d` and `duration 5 days`).

Everything else stays exactly as it is.

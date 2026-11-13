# Room scheduler: store times as minutes

## The existing program

The program is a room scheduler. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9`.

A *time* is currently written `HH:MM`: exactly five characters, two digits, `:`, two digits, with `HH` from `00` to `23` and `MM` from `00` to `59`. Times are printed the same way. An event occupies the half-open interval [start, end): an event from `09:00` to `10:00` contains `09:00` and `09:59` but not `10:00`, so an event may start exactly when another ends. Two events *overlap* when each starts before the other ends. The duration of an interval is end minus start in minutes. `ID` and `ROOM` are single words; an event ID is unique across all rooms.

Commands:

- `add ID ROOM START END`: books an event. Checks, in this order: START is a time, then END is a time — else `error: bad time W` with the offending word W exactly as written; the ID is not already used by an event — else `error: event ID exists`; START is before END — else `error: empty interval`; the event does not overlap any event of the same room — else `error: ID conflicts with OTHER`, where OTHER is the overlapping event of that room with the earliest start. On an error nothing changes. Otherwise prints `added ID in ROOM START-END`.
- `remove ID`: deletes the event (its ID may be used again later). Prints `removed ID`, or `error: unknown event ID`.
- `agenda ROOM`: prints `agenda ROOM: N events` (N may be 0 or 1; the word stays `events`), then one line per event of the room sorted by start, `  START-END ID (D min)` (two leading spaces, D the duration).
- `who ROOM TIME`: TIME must be a time, else `error: bad time W`. Prints `ROOM at TIME: ID` for the room's event containing TIME, or `ROOM at TIME: free`.
- `free ROOM FROM TO MIN`: MIN must be a number of at least 1, else `error: bad command`. Then FROM and TO must be times (FROM checked first), else `error: bad time W`; FROM must be before TO, else `error: empty interval`. Prints `free ROOM FROM-TO:`, then the gaps inside [FROM, TO) not covered by any event of the room, in time order, one line each `  A-B (D min)`, keeping only gaps with a duration of at least MIN minutes. A gap is a maximal free interval clipped to [FROM, TO). If no gap is kept, prints `  none` instead.
- `summary`: for each room that has at least one event, in ascending byte order of the room name: `ROOM: N events, M min booked, first S, last E`, where M is the sum of the durations of the room's events, S the earliest start and E the latest end. Then `total: K events, T min booked` over all events (printed also when there are none: `total: 0 events, 0 min booked`).

A command with the wrong number of arguments prints `error: bad command` (checked before anything else). A first word that is not a command prints `error: unknown command WORD`.

Internally the program keeps times as `HH:MM` strings, compares and sorts them as strings, and converts them to minutes in several places to compute durations.

## The change

Users want to type times more freely. Change the program to store every time as an integer number of minutes since midnight (`00:00` is 0, `23:59` is 1439) everywhere: in events, comparisons, sorting, conflict checks, `who`, `free` and `summary`. Parse input times once, into minutes, and format them back only for output.

With that, a time in the input may now be written in any of these forms (and nothing else):

- 24-hour: `H:MM` or `HH:MM` — one or two digits for the hour, value 0 to 23 (so `9:05`, `09:05` and `0:30` are accepted), `:`, exactly two digits for the minutes, value 0 to 59.
- 12-hour: the same, immediately followed by lowercase `am` or `pm`, with an hour value from 1 to 12 (so `9:05am`, `09:05am` and `12:30pm` are accepted; `0:30am`, `13:00pm`, `9:05AM`, `9am` and `9:5am` are not). `12:MMam` is `00:MM`, `12:MMpm` is `12:MM`, any other `H:MMam` is `H:MM`, and any other `H:MMpm` is `H+12:MM`.

Any other word where a time is expected prints `error: bad time W` (W exactly as written, as before). Every time printed by the program — in `added`, `agenda`, `who`, `free` and `summary` — is always in the `HH:MM` form with two-digit hour and minutes, whatever form was typed (for example `who lab 9:30am` prints `lab at 09:30: ...`). Ordering, overlaps, `empty interval` and gaps follow the actual time of day: `9:05` is before `10:00`, and `5:00pm` is after `9:00`.

All commands, messages and checks are otherwise unchanged; input using only the `HH:MM` form must give exactly the same output as before.

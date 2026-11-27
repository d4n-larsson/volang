# Room booking

Assign meeting rooms to booking requests.

## Input

- The first line holds `R C`: the number of rooms (1 ≤ R ≤ 50000) and the cleaning time C (0 ≤ C ≤ 10^9).
- The second line holds R integers: the capacities of rooms 1, 2, ..., R in that order, each from 1 to 100.
- Every following line is a request `NAME START END SIZE`: NAME is a non-empty run of ASCII letters and digits (two requests may have the same name), `0 ≤ START < END ≤ 10^9`, and SIZE (1 to 100) is the number of people. There are at most 100000 requests, possibly none.

Values on a line are separated by single spaces. The input ends with a newline.

## Rules

Requests are handled in order of START, earliest first; requests with equal START are handled in input order. All rooms are free at the beginning.

A meeting occupies its room from its start to its end. After that the room needs cleaning, so it becomes free again at the meeting's end + C: a request whose START is equal to or later than that time can have it, one whose START is earlier cannot.

When a request is handled, it gets the room that is free at its START and has the smallest capacity that is at least SIZE; among several such rooms, the one with the lowest number. The meeting then takes place as requested.

If no room with enough capacity is free at START, the meeting is postponed: it gets the room with enough capacity that becomes free first; if several such rooms become free at the same time, the one with the smallest capacity, then the one with the lowest number. The meeting starts when that room becomes free and keeps its length (END - START), so the room is occupied until the new end and becomes free again after cleaning, at the new end + C. Requests are still handled in the order of the START they asked for, whatever happened to earlier requests.

A request is rejected only when no room at all has a capacity of at least SIZE. A rejected request occupies nothing.

The input can be large: a solution that looks at every room for every request is too slow.

## Output

One line per request, in input order: `NAME ROOM TIME`, with the number of the room it got and the time its meeting starts, or `NAME rejected`. Then a last line `postponed P rejected K`: the number of postponed meetings (those starting later than their START) and of rejected requests. Every output line ends with a newline.

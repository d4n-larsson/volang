# Meeting calendar: time zones

## The existing program

The program keeps a calendar of meetings for users and rooms. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored; all input is ASCII. A *number* is a non-empty string of the digits `0`-`9`. `NAME`, `ROOM`, `USER` and `ID` are single words.

There is no real calendar: days are numbered. A *time* is written as two words, `DAY HH:MM` (DAY a number, `HH:MM` exactly two digits, `:`, two digits, from `00:00` to `23:59`), and is held as the number of minutes since day 0 00:00. A meeting runs from its start minute up to (not including) its end minute; two meetings *overlap* when each starts before the other ends.

Output formats:

- A *clock* is `HH:MM` (the time of day). A *moment* is `DAY HH:MM`.
- A *span* from S to E is `DAY HH:MM-HH:MM` when E is on the same day as S, else `DAY HH:MM-DAY HH:MM` (so a meeting ending at midnight prints as `3 23:00-4 00:00`).
- A *duration* of N minutes is `<N / 60>h<N % 60 as two digits>m`, e.g. `2h05m`, `0h00m`.
- Counts print as `1 person` / `N people` and `1 meeting` / `N meetings`.

Users and rooms share one namespace. A meeting has an ID, a room, an organizer and a list of attendees (other users, in the order they were added); its *people* are the organizer followed by the attendees. When several meetings clash, the one reported is the one that starts first (ties: the smaller ID in byte order).

Commands:

- `user NAME`: prints `user NAME added`, or `error: name NAME taken` if NAME is already a user or a room.
- `room NAME CAPACITY` (CAPACITY a number of at least 1): prints `room NAME added, capacity C`, or `error: name NAME taken`.
- `book ID ROOM ORGANIZER DAY HH:MM MINUTES [ATTENDEE...]` (MINUTES a number from 1 to 1440): checks, in this order: the ID is not in use — else `error: meeting ID exists`; the room exists — else `error: unknown room ROOM`; the organizer, then each attendee in order, is a user — else `error: unknown user NAME` — and is not listed a second time (the organizer counts as listed) — else `error: NAME listed twice`; the number of people is at most the room's capacity — else `error: room ROOM holds C, meeting has N`; no meeting in the room overlaps — else `error: room ROOM busy: ID2 SPAN2`; then for each person in order, none of their meetings (as organizer or attendee) overlaps — else `error: NAME busy: ID2 SPAN2`. On success prints `booked ID in ROOM: SPAN (N people)`.
- `cancel ID`: removes the meeting (the ID can be used again) and prints `cancelled ID`, or `error: unknown meeting ID`.
- `move ID DAY HH:MM`: `error: unknown meeting ID`, else the meeting keeps its length and gets the new start; the room and then each of its people are checked as in `book` (ignoring the meeting itself). Prints `moved ID: SPAN`.
- `invite ID USER`: checks `error: unknown meeting ID`, `error: unknown user USER`, `error: USER already in ID`, the capacity (with the new person counted), then the user's own clash (`error: USER busy: ID2 SPAN2`). Adds the user as the last attendee and prints `USER invited to ID`.
- `leave ID USER`: `error: unknown meeting ID`, `error: USER organizes ID`, `error: USER not in ID`; else removes the attendee and prints `USER left ID`.
- `agenda USER DAY`: `error: unknown user USER`, else `agenda USER day DAY:` and then one line `  SPAN ID in ROOM (organizer|attendee)` for each of the user's meetings that overlaps day DAY (00:00 to the next day's 00:00), by start then ID; `  nothing` if there are none.
- `rooms DAY`: `rooms day DAY:`, then per room in byte order of the name `  ROOM: ID SPAN, ID SPAN` (its meetings overlapping that day, by start then ID) or `  ROOM: free`; `  no rooms` when there are no rooms.
- `free ROOM DAY`: `error: unknown room ROOM`, else `free ROOM day DAY: ` followed by the free parts of the working hours 09:00-17:00 of that day not covered by the room's meetings, as `HH:MM-HH:MM` joined with `, `, or `none`.
- `busy USER`: `USER: N meetings, DURATION` (all the user's meetings and their total length). `busy USER DAY`: `USER day DAY: N meetings, DURATION`, counting the meetings that overlap that day and only the minutes of them within that day. Unknown user: `error: unknown user USER`.
- `who ID`: `ID: ROOM SPAN, organizer O, attendees A, B` (`attendees none` without attendees), or `error: unknown meeting ID`.
- `list`: one line per meeting by start then ID, `ID SPAN ROOM ORGANIZER +K` (K the number of attendees), or `no meetings`.

A command with the wrong number of arguments, or an argument that is not as described (a bad time, a MINUTES outside 1-1440, a CAPACITY of 0, a non-number DAY), prints `error: bad command` (checked before anything else) and changes nothing. A first word that is not a command prints `error: unknown command WORD`.

## The change

Users and rooms now live in time zones. Every time the program stores is in UTC (zone +00:00), and every time it reads or prints is in some local zone, as follows.

- **Offsets.** An OFFSET in the input is `+HH:MM` or `-HH:MM` (exactly six characters: a sign, two digits, `:`, two digits, with HH:MM a valid clock as above) between `-12:00` and `+14:00` inclusive; `-00:00` is the same as `+00:00`. Anything else where an OFFSET is expected is `error: bad command`. The local time in a zone is the UTC time plus the offset (UTC = local − offset). A zone prints as `UTC+HH:MM` or `UTC-HH:MM`, with zero as `UTC+00:00`.
- **New arguments and commands.**
  - `user NAME [OFFSET]` and `room NAME CAPACITY [OFFSET]` take an optional OFFSET (default `+00:00`); the output is unchanged.
  - `zone NAME OFFSET` changes the offset of the user or room NAME and prints `zone NAME UTC±HH:MM`; `error: unknown name NAME` if it is neither. Meetings are not moved: they keep their UTC times and only print differently.
  - `hours USER HH:MM HH:MM` sets the user's working hours, in the user's local time (the first clock must be earlier than the second, else `error: bad command`), and prints `hours USER HH:MM-HH:MM`; `hours USER none` removes them and prints `hours USER none`. `error: unknown user USER` if USER is not a user. Users start without working hours.
- **Times typed in `book` and `move`** are in the zone of the meeting's organizer (for `move`, the organizer's offset at the time of the move).
- **Output of `book`, `move` and `invite`**: every span they print — the `booked`/`moved` line and the SPAN2 of the clash errors — is shown in the zone of the meeting's organizer.
- **Working hours check.** In `book` and `move`, the check of each person (organizer first, then the attendees in order) becomes: first the clash with their meetings as before, then, if the person has working hours, the meeting must lie within them in the person's local time — it starts no earlier than the first clock and ends no later than the second clock **of the local day on which it starts** — else `error: NAME works HH:MM-HH:MM`. The next person is checked only if this one passed both. `invite` checks the invited user the same way (clash, then working hours) after the capacity. Changing hours or zones never re-checks existing meetings.
- **Views in a local zone.**
  - `agenda USER DAY`: DAY is the user's local day and all spans are in the user's zone; the first line becomes `agenda USER day DAY (UTC±HH:MM):`.
  - `rooms DAY`: for each room, DAY is the room's local day and the spans are in the room's zone.
  - `free ROOM DAY`: the working hours 09:00-17:00 are those of the room's local day DAY, and the slots are printed in the room's zone.
  - `busy USER DAY`: DAY is the user's local day (`busy USER` without a day is unaffected).
  - `who ID`: the span is in the room's zone and is followed by a space and the room's zone: `ID: ROOM SPAN UTC±HH:MM, organizer O, attendees ...`.
  - `list` keeps printing UTC spans.

Inputs keep the DAY of every `book` and `move` at least 2, so no time ever becomes negative. Everything else stays exactly as it is.

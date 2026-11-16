A room calendar. Each non-blank line of stdin is one command; process them in order and print one line per command:

- `book START END NAME`: a booking of the half-open interval [START, END) under NAME (START < END, integers from 0 to 1000000000; NAME is lowercase letters and digits). Accept it if it does not overlap any booking accepted earlier (two bookings overlap when they share a time T: [10, 20) and [20, 30) do not overlap) and print `accepted`; otherwise ignore it and print `rejected`.
- `at T`: the NAME of the accepted booking with START <= T < END, or `free` when there is none.
- `next T`: the first accepted booking that starts at or after T, as `NAME START END`, or `none` when there is none.

Example: after `book 10 20 ana` (accepted), `book 15 25 bo` is `rejected`, `at 19` is `ana`, `at 20` is `free`, `next 11` is `none` and `next 10` is `ana 10 20`.

Effects: `console`, `input`.

The largest inputs have 100,000 commands, most of them accepted bookings: each command must take well under linear time.

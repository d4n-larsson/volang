Each non-blank line of stdin is an ISO 8601 timestamp: `YYYY-MM-DDTHH:MM:SS` with an optional fraction `.fff`, followed by `Z` or an offset `+HH:MM` / `-HH:MM`.

For each line, in order, print the time in UTC in std.time's text form; a line that is not such a timestamp prints `bad: LINE` instead. After all lines, if at least one timestamp was valid, print `span: M minutes`, where M is the number of whole minutes (rounded down) from the earliest to the latest valid time.

Effects: `console`, `input`.

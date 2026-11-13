# Ticket ids

Give each ticket an id and list the tickets by priority.

Each non-empty line of standard input is a ticket: `<priority> <title>`, where the priority is an integer from 1 to 5 and the title is the rest of the line (printable ASCII, may contain spaces).

Ids must be reproducible: the same input always gives the same ids. Compute them in input order:

- `h = 7`; for each byte `b` of the title (its ASCII code), `h = (h * 31 + b) % 1000003`.
- If an earlier ticket of this input already has id `h`, use `h = (h + 1) % 1000003` instead, repeatedly, until the id is free.
- The id is `T` followed by `h` written with 7 digits (zero-padded), e.g. `T0012345`.

Output one line per ticket, `<id> P<priority> <title>`, sorted by priority from highest (5) to lowest (1); tickets with the same priority keep their input order. Then a last line `count <number of tickets>`.

## Allowed effects

The program may print and read standard input. Nothing else: no randomness (not for ids, not anywhere else), no clock, no reading or writing files, no environment variables, no network.

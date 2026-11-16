# Help-desk triage

Reads tickets from `data/tickets.txt` and on-call shifts from `data/oncall.txt`, and prints the order
the tickets are handled in (most urgent first), who was on call when each ticket came in, who was never
paged, and the last three tickets handled. It shows the built-in collections:

- **`priority_queue[Ticket]`**: `Ticket` is a record of ordered fields (`severity`, `minute`, `title`),
  so it is ordered field by field and the queue needs no comparison function.
- **`ordered_map[int, str]`**: shifts keyed by their start; `key_at_or_below` finds the shift that
  covers a minute.
- **`ordered_set[str]`**: the people paged and everyone on the rota, in name order; `difference` gives
  who was never paged.
- **`deque[str]`**: the last three tickets handled (`push` at the back, `pop_front` when it is full).

```sh
cd examples/helpdesk
../../bin/vo run main.vo        # the report
../../bin/vo test main.vo       # four tests and a property (100 generated inputs)
../../bin/vo effects main.vo    # what each function and test is allowed to do
```

```text
triage order
  sev 1  minute  7  email down for everyone   on call: alice
  sev 1  minute 12  VPN refuses logins        on call: bob
  sev 2  minute  5  printer out of toner      on call: alice
  sev 2  minute 15  laptop battery swollen    on call: bob
  sev 3  minute  9  new monitor request       on call: alice
  sev 3  minute 20  password reset            on call: carol
paged: alice, bob, carol
never paged: dave
last 3 handled: laptop battery swollen; new monitor request; password reset
```

## Requirements

| ID | Requirement |
|---|---|
| FR-001 | Tickets are handled most urgent first (severity 1 first), then the earliest, then by title. |
| FR-002 | A ticket goes to the person whose shift started last at or before the ticket's minute. |
| FR-003 | The history keeps the last three tickets handled. |
| FR-004 | A malformed line is an error that quotes the line, not a crash. |

The tests carry these IDs (`@spec`); `vo test` ends with a summary per requirement.

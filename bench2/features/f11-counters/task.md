# Counters

Standard input is a sequence of commands, one per line (names are lowercase letters):

- `new NAME START STEP`: create a new counter whose current value is `START` and bind `NAME` to it (replacing any previous binding of `NAME`; other names bound to the old counter keep it).
- `next NAME`: print the counter's current value, then add `STEP` to it.
- `reset NAME`: set the counter's current value back to its `START`.
- `alias NEW OLD`: bind `NEW` to the same counter as `OLD`, so both names see and change one shared value.

If `next`, `reset` or `alias` refers to a name that is not bound, print `unknown NAME` (for `alias`, the unbound `OLD`) and do nothing else.

Write a function `make_counter(start, step)` that returns a record with two function fields, `next` and `reset`, that share the counter's state; the commands call these functions.

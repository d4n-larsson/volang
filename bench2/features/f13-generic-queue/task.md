# Generic queue

Implement a generic record `Queue[T]` with functions `enqueue` and `dequeue` (and whatever else you need), and use it for two queues: one of integers and one of strings.

Standard input is a sequence of commands, one per line, where `KIND` is `int` or `str`:

- `push int N`: enqueue the integer N on the integer queue.
- `push str TEXT`: enqueue `TEXT`, everything after `push str ` on the line (it may contain or start with spaces), on the string queue.
- `pop KIND`: dequeue from that queue and print the value; print `empty` if the queue is empty.
- `size KIND`: print the number of values in that queue.

The input may be empty.

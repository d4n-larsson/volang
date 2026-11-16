A discrete event simulator. The first line of stdin is N, the number of initial events. Each of the next N lines is `TIME VALUE` (0 <= TIME <= 1000000, 1 <= VALUE <= 10000). Events get IDs in the order they are scheduled: the initial events are 1 to N in input order, and every event scheduled later gets the next unused ID.

Repeatedly take the pending event with the smallest TIME (among equal times, the one with the smallest ID, i.e. scheduled first) and process it, until no event is pending. Processing the event (T, ID, V) schedules, in this order:

1. if V >= 2: an event at time T + V % 4 with value V / 2 (integer division);
2. if V % 3 == 0: an event at time T + 1 with value V / 3.

New events may have the same time as the event being processed (V % 4 == 0); they run after every event already pending at that time.

Output: number the processed events K = 1, 2, 3, ... in processing order. For every event with K <= 50 or K a multiple of 1000, print `K T ID V`. Finally print `END COUNT CHECKSUM`, where COUNT is the number of processed events and CHECKSUM is the sum of K * ID over all processed events, modulo 1000000007.

Example: for the input

```
2
5 6
5 1
```

the events are processed as (5, 1, 6), (5, 2, 1), (6, 4, 2), (7, 3, 3), (8, 5, 1), (8, 7, 1), (10, 6, 1) and the output is

```
1 5 1 6
2 5 2 1
3 6 4 2
4 7 3 3
5 8 5 1
6 8 7 1
7 10 6 1
END 7 138
```

Effects: `console`, `input`.

The largest inputs have 15,000 initial events and about 500,000 processed events, with up to 15,000 pending at a time: finding the next event by scanning all pending events is too slow.

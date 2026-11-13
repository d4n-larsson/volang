# Meeting picker

Each line of standard input is `NAME START END`: a name of lowercase letters and two integers with `START < END`. The input may be empty.

Pick meetings greedily so that none overlap:

1. sort the meetings by `END` ascending; meetings with the same `END` by `START` descending; meetings equal on both keep their input order;
2. walk the sorted list and take a meeting when it is the first one taken or its `START` is at least the `END` of the last meeting taken (touching meetings do not overlap).

Print the names of the taken meetings in the order they were taken, one per line, then a line `total K` with the number of taken meetings.

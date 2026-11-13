# Interval merge

Read closed integer intervals from standard input and print their union as a sorted list of disjoint intervals.

Input:
- Each non-blank line holds two integers `a b`, separated by one or more spaces or tabs. A line may have leading and trailing spaces or tabs. Blank lines (empty, or only spaces/tabs) are ignored.
- An integer is an optional `-` followed by decimal digits. Every value is between -1000000000000000 and 1000000000000000 inclusive.
- The line `a b` stands for the closed interval from `min(a, b)` to `max(a, b)`: if `a > b` the ends are swapped. `a == b` is a one-point interval.

Merging:
- Two intervals are merged when they overlap or touch, that is when they share at least one point: `1 3` and `3 5` become `1 5`; `2 2` and `2 7` become `2 7`.
- Intervals that only sit next to each other without a common point are not merged: `1 2` and `3 4` stay separate.
- Merging is repeated until no two output intervals share a point. Duplicates and intervals nested inside others disappear into the interval that contains them.

Output: one line `start end` per merged interval (start before end, one space between), sorted by start ascending. If there are no intervals at all (empty input, or only blank lines), print the single line `empty`. Every output line ends with a newline.

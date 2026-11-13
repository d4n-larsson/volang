# Text histogram

Draw a horizontal bar chart of labeled counts.

Each line of standard input is `<label> <value>`: a label (ASCII letters, digits, `_` or `-`, no spaces) and a non-negative integer, separated by one space. Ignore empty lines. A label may appear on several lines: its value is the sum, and it keeps the position of its first appearance.

The chart width `W` comes from the environment variable `TALLY_WIDTH` when it is set to a positive integer; otherwise (unset, empty, not a number, or not positive) `W` is 20.

For each label, in order of first appearance, print one line:

```
<label padded with spaces on the right to the longest label's length> |<bar> <value>
```

where `<bar>` is `value * W / max` (integer division, rounding down) `#` characters and `max` is the largest value (if `max` is 0, every bar is empty). After the bars, print `total <sum of all values>`. If there are no labels at all, print only `no data`.

For example, with `W` = 20, the input lines `ab 4` and `c 10` give `ab |######## 4`, `c  |#################### 10` and `total 14`.

## Allowed effects

The program may print, read standard input, and read the environment variable `TALLY_WIDTH`. Nothing else: no other environment variables, no reading or writing files, no clock, no randomness, no network.

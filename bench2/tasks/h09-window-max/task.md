# Sliding window maximum

Input: the first line holds two integers `n k` (0 <= n <= 200000, 1 <= k <= 1000000). The rest of standard input holds exactly `n` integers, separated by spaces and newlines in any arrangement (they may span several lines, with extra spaces). Every value is between -1000000000000000000 and 1000000000000000000 inclusive.

For every window of `k` consecutive values, from the window starting at the first value to the window ending at the last, print the maximum of the window. The `n - k + 1` maxima go on one line, separated by single spaces.

If `k > n` (in particular when `n` is 0) there is no window: print `none`.

Inputs are large, so the program must run in roughly linear time: an approach that rescans each window is too slow. Every output line ends with a newline.

# Matrix spiral

Input: the first line holds `r c d`: the number of rows `r` and columns `c` (each from 0 to 100) and the direction `d`, which is `cw` or `ccw`. Then come `r` lines, each with `c` cells separated by one or more spaces (possibly with leading and trailing spaces). A cell is a non-empty run of non-space ASCII characters. If `r` or `c` is 0, the matrix is empty and no matrix lines follow.

Print every cell exactly once, in spiral order, on one line separated by single spaces. The spiral starts at the top-left cell and moves inward layer by layer:
- `cw` (clockwise): along the top row to the right, down the right column, along the bottom row to the left, up the left column, then the same on the next inner layer;
- `ccw` (counterclockwise): down the left column, along the bottom row to the right, up the right column, along the top row to the left, then the next inner layer.

Matrices need not be square: a single row or a single column, and inner layers that are a single row or column, must still visit each cell exactly once.

If the matrix is empty print `empty`. Every output line ends with a newline.

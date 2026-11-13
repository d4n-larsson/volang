# Generic top N

Standard input has exactly three lines: an integer `N >= 0`; words separated by single spaces; integers separated by single spaces. The second and third lines may be empty.

Write one generic function `top_n` that takes a list of any element type, `N` and a key function returning an integer, and returns the first `N` elements (all, if there are fewer) ordered by key descending; elements with equal keys keep their input order. The input list is not changed.

Print three lines, each the selected elements separated by single spaces (an empty line if none):

1. the top `N` words by length;
2. the top `N` integers by the sum of the decimal digits of their absolute value;
3. the top `N` integers by absolute value.

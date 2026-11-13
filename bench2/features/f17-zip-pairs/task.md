# Zip pairs

Standard input has exactly two lines: names separated by single spaces, then integer scores separated by single spaces. Either line may be empty.

Define a generic record `Pair[A, B]` and a generic function `zip` that pairs the i-th element of two lists of any types, stopping at the shorter list. Pair the names with the scores and print:

1. `pairs K`, the number of pairs;
2. one line `NAME SCORE` per pair, ordered by score descending; pairs with equal scores keep their zip order;
3. `leftover L`, the number of elements of the longer list without a partner;
4. `best SCORE NAME` for the first pair in the order of part 2, or `best -` if there are no pairs. Build this line from a `Pair[int, str]` made with a generic `swap` function.

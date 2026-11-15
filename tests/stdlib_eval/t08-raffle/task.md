The program gets one argument N (an integer, at least 1). Make the list `[1, 2, ..., N]` and:

1. shuffle it once with std.random's shuffle and print the shuffled numbers on one line, separated by single spaces;
2. pick one element of the shuffled list once with std.random's pick and print `winner: X`.

Use no other source of randomness, and call shuffle and pick exactly once each, in this order (the tests fix the seed with `--seed`, so the output depends on it). If N is not an integer of at least 1, print `bad N` and exit with status 1.

Effects: `console`, `random`.

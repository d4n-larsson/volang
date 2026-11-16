Sliding window maximum. The first line of stdin is `N K` (1 <= K <= N). The rest of stdin holds N integers (each from -1000000000 to 1000000000), separated by spaces and newlines.

For every window of K consecutive numbers, from the window that starts at the first number to the one that ends at the last, print the largest number in the window, one per line (N-K+1 lines).

Example: for

```
6 3
1 3 -1 -3 5 3
```

the output is `3`, `3`, `5`, `5`, one per line.

Effects: `console`, `input`.

The largest inputs have N = 100,000 and K = 40,000: scanning each window is too slow.

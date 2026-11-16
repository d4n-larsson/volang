Running median. The first line of stdin is N. The rest of stdin holds N integers (each from -1000000000 to 1000000000), separated by spaces and newlines.

After reading each number, take the median of the numbers read so far: with I numbers read (I = 1, 2, 3, ...), sort them ascending and take the one at position (I + 1) / 2 (integer division, counting from 1). So for an even count it is the lower of the two middle numbers.

Output: for every I <= 50 and every I that is a multiple of 1000, print `I MEDIAN`. Finally print `END CHECKSUM`, where CHECKSUM is the sum of I * (MEDIAN + 1000000000) over I = 1 to N, modulo 1000000007.

Example: for

```
5
5 -2 7 7 1
```

the medians are 5, -2, 5, 5, 5 and the output is

```
1 5
2 -2
3 5
4 5
5 5
END 999999963
```

Effects: `console`, `input`.

The largest inputs have N = 100,000: keeping a sorted list and inserting each number into it is too slow.

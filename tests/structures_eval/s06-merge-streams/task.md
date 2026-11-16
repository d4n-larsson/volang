Merging sorted streams. The first line of stdin is `K M`. Each of the next K lines is `START STEP` (0 <= START <= 1000000000, 1 <= STEP <= 1000000) and describes stream number I (counting from 0, in input order): the endless ascending sequence START, START + STEP, START + 2*STEP, ...

Merge all streams into one ascending sequence; equal values come in stream order (the smaller stream number first). Number the merged values J = 1, 2, 3, ... and consider the first M of them.

Output: for every J <= 50 and every J that is a multiple of 1000 (J <= M), print `J VALUE STREAM`. Finally print `END CHECKSUM`, where CHECKSUM is the sum of J * (VALUE + STREAM) over J = 1 to M, modulo 1000000007.

Example: for the input

```
2 5
10 5
12 3
```

the merged values are 10 (stream 0), 12 (1), 15 (0), 15 (1), 18 (1) and the output is

```
1 10 0
2 12 1
3 15 0
4 15 1
5 18 1
END 240
```

Effects: `console`, `input`.

The largest inputs have K = 20,000 streams and M = 200,000: looking at every stream to find each next value is too slow.

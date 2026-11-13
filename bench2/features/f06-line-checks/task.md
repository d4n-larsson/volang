# Line checks

Each line of standard input holds zero or more integers separated by single spaces (absolute value at most 1000000). A line may be empty (no integers).

For every input line print one line:

```
evens=E sumsq=S allpos=YES_NO any7=YES_NO
```

- `E`: how many of the line's integers are even.
- `S`: the sum of the squares of the even integers.
- `allpos`: `yes` if every integer is greater than 0 (also for an empty line), else `no`.
- `any7`: `yes` if some integer is a multiple of 7 (0 counts), else `no`.

Use `filter`, `map`, `fold`, `all` and `any`.

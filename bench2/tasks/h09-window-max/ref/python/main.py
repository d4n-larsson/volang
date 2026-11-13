import sys
from collections import deque

toks = sys.stdin.read().split()
n, k = int(toks[0]), int(toks[1])
xs = [int(t) for t in toks[2:2 + n]]
if k > n:
    print("none")
else:
    dq, out = deque(), []
    for i, x in enumerate(xs):
        while dq and xs[dq[-1]] <= x:
            dq.pop()
        dq.append(i)
        if dq[0] <= i - k:
            dq.popleft()
        if i >= k - 1:
            out.append(str(xs[dq[0]]))
    print(" ".join(out))

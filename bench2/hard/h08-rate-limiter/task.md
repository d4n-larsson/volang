# Rate limiter

Decide for each request in a log whether a rate limiter accepts it.

## Input

- The first line holds six integers `K W G H R B`: the per-user limit K and window W, the global limit G and window H, the ban threshold R and the ban length B. 1 ≤ K, G ≤ 200000; 1 ≤ W, H, B ≤ 10^12; 1 ≤ R ≤ 1000.
- Every following non-empty line is a request `TIME USER`: TIME is an integer from 0 to 10^12, USER a non-empty run of ASCII letters and digits (case-sensitive). The times never decrease from one line to the next; several requests may have the same time. There are at most 100000 requests, possibly none.

Values are separated by single spaces.

## Rules

Requests are decided one at a time in input order. Only accepted requests are counted by the limits. For a request at time T by user U:

1. If U is banned at time T, the answer is `banned`.
2. Otherwise, if U already has at least K accepted requests with a time greater than T - W and at most T, the answer is `user-limit`.
3. Otherwise, if there are already at least G accepted requests (of all users) with a time greater than T - H and at most T, the answer is `global-limit`.
4. Otherwise the request is accepted: `ok`.

Bans: every user has a count of rejections in a row, which starts at 0. An accepted request sets it to 0. A `user-limit` or `global-limit` answer adds 1; when the count reaches R, the user is banned from T (this request's time) until T + B: every request of U at a time less than T + B is answered `banned`, and the count goes back to 0. A `banned` answer does not change the count.

The input can be large: a solution that looks at all earlier requests for every request is too slow.

## Output

One line per request with its answer, in input order, then a last line `ok A user-limit X global-limit Y banned Z` with the number of each answer. Every output line ends with a newline.

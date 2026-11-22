# Contract: codes

| Code | Name | Exit | Fixable | When |
|---|---|---|---|---|
| W0403 | `for_reads_entry` | 0 | yes | `for K in keys(M) {` or `for I in 0..len(XS) {` whose first statement is `let V = M[K];` or `let X = XS[I];`, with `M` or `XS` a `let` or a parameter. |
| E0301 | `type_mismatch` | 65 | sometimes | As before, and: a `for` with two variables over anything but a map, an ordered map, a list or a deque (changed here: over text, a range, an ordered set or a priority queue the fix removes the first variable). |

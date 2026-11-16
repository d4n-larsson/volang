Shortest paths in a road network. The first line of stdin is `N M`: N nodes numbered 0 to N-1 and M one-way roads. Each of the next M lines is `U V W`: a road from node U to node V of length W (0 <= W <= 1000). There may be several roads between the same nodes, and roads from a node to itself.

Print N lines: line I (counting from 0) is the length of the shortest route from node 0 to node I (0 for node 0 itself), or `unreachable` when there is no route.

Example: for the input

```
4 4
0 1 5
0 2 1
2 1 2
1 0 7
```

the output is `0`, `3`, `1`, `unreachable`, one per line.

Effects: `console`, `input`.

The largest inputs have 50,000 nodes and 200,000 roads: a solution that scans every node to find the next closest one is too slow.

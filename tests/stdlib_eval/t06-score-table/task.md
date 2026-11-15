Each non-blank line of stdin is `NAME<TAB>SCORE` (SCORE an integer; names may contain any characters but a tab, including non-ASCII ones). Print a table sorted by score, highest first (equal scores: names in ascending order):

```
Name  | Score
------+------
ana   |    12
```

- W is the width of the longest name in characters (code points), but at least 4 (the width of `Name`).
- The header is `Name` padded on the right with spaces to W, then ` | `, then `Score`.
- The separator is W+1 dashes, `+`, then 6 dashes.
- Each row is the name padded on the right with spaces to W, then ` | `, then the score padded on the left with spaces to 5 characters.

Effects: `console`, `input`.

The program's directory has a package `money`, listed in `vo.packages` (already pinned; do not change it or the package). Its documentation (from `vo doc`) is below the task. Use it.

Each non-blank line of stdin is an amount in cents (an integer, possibly negative). Print each amount formatted with the package's formatting function, one per line, then `total: ` followed by the formatted sum. A line that is not an integer prints `skipped: LINE` instead (and does not count).

Effects: `console`, `input`.

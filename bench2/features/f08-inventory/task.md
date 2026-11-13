# Inventory

The first line of standard input is an integer `Q`. Each following line is `NAME QTY PRICE`: a name of lowercase letters and two non-negative integers (at most 100000). There may be no item lines.

Print exactly these five lines:

```
restock NAMES
value V
out_of_stock YES_NO
all_priced YES_NO
most_valuable NAME
```

- `NAMES`: the names of items with `QTY < Q`, in input order, separated by single spaces; `-` if none.
- `V`: the sum of `QTY * PRICE` over all items.
- `out_of_stock`: `yes` if some item has `QTY` 0, else `no`.
- `all_priced`: `yes` if every item has `PRICE` greater than 0 (also with no items), else `no`.
- `most_valuable`: the name of the item with the largest `QTY * PRICE`; on a tie the earliest one; `-` if there are no items.

Use `filter`, `map`, `fold`, `any` and `all`.

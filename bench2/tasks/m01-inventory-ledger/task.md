# Inventory ledger

Read stock commands from standard input and keep a first-in, first-out (FIFO) inventory ledger. Print error lines while processing and a report whenever asked, plus once more at the end.

## Program structure

Split the program into at least three source files: command-line parsing, the ledger (stock lots and running totals), and the report formatting each in their own file, plus the entry file that reads standard input and drives them.

## Input

Lines are numbered from 1; every line counts, including blank ones. A line is split into fields at runs of spaces; leading and trailing spaces are ignored. A line with no fields is skipped silently.

A *number* field is 1 to 6 ASCII digits (`0`-`9`, leading zeros allowed, no sign). A *SKU* is any field. Command words are case-sensitive.

| Command | Meaning |
|---|---|
| `RECEIVE <sku> <qty> <cost>` | `qty` units arrive, each costing `cost` cents. This adds a new *lot* (quantity and unit cost) at the back of the SKU's lot queue. |
| `SHIP <sku> <qty> <price>` | Sell `qty` units at `price` cents each. |
| `SCRAP <sku> <qty>` | Throw away `qty` units (no money comes in). |
| `REPORT` | Print the report (below). |

`qty` must be a number of at least 1; `cost` and `price` must be numbers (0 is allowed). A line that is not exactly one of these forms (unknown command word, wrong number of fields, a field that is not a valid number, `qty` of 0) prints `line <N>: invalid` and changes nothing.

## Rules

- The *stock* of a SKU is the total quantity of its lots (0 for a SKU never received).
- `SHIP` and `SCRAP` need `stock >= qty`. Otherwise print `line <N>: cannot take <qty> of <sku>, only <stock> in stock` (numbers printed in decimal without leading zeros) and change nothing.
- A successful `SHIP` or `SCRAP` takes units from the oldest lot first; a lot that reaches 0 units is removed and taking continues with the next lot. The *cost* of the taken units is the sum of their lots' unit costs.
- A successful `SHIP` adds `qty * price` to *revenue* and the cost of the taken units to *cogs*. A successful `SCRAP` adds the cost of the taken units to *loss*.
- A SKU becomes *known* with its first `RECEIVE` line that is valid. It stays known even when its stock drops to 0.

## Report

```
REPORT
<sku> <stock> <value>
...
revenue <R> cogs <C> loss <L> profit <P> stock <V>
```

- One line per known SKU, sorted by SKU in byte order (ASCII order). `value` is the sum over its lots of quantity times unit cost.
- `profit = revenue - cogs - loss`; `stock` on the last line is the sum of all SKU values.
- Money values are amounts in cents printed as dollars: an optional `-` for negative amounts, the whole dollars without leading zeros (`0` when under one dollar), `.`, then exactly two digits of cents. Examples: `0` is `0.00`, `5` is `0.05`, `123456` is `1234.56`, `-150` is `-1.50`. Stock counts are plain decimal integers.

The report is printed for every valid `REPORT` line and once more after the last input line (also for empty input). Every output line ends with a newline.

# Expense report

Reads a list of expenses from `data/expenses.json`, prints totals by category (largest first) and by
month, and saves the same report to `out/report.txt`.

```sh
cd examples/expenses
../../bin/vo run main.vo        # the report
../../bin/vo test main.vo       # four tests and a property (100 generated inputs)
../../bin/vo effects main.vo    # what each function and test is allowed to do
```

```text
12 expenses

category        amount
----------------------
groceries       211.40
home            129.00
transport        96.00
eating out       92.50
books            23.99
total           552.89

month        amount
-------------------
2026-01      269.45
2026-02      180.95
2026-03      102.49
total        552.89
```

## Requirements

| ID | Requirement |
|---|---|
| FR-001 | Amounts are shown in units with two decimals (`0.05`, `1290.50`). |
| FR-002 | Totals list each category once, in the order it first appears. |
| FR-003 | Totals add up to the sum of the expenses: grouping never loses or invents money. |
| FR-004 | An expense with an impossible date is an error with the reason, not a crash. |
| FR-005 | With no expenses, the report says so. |

Each test names the requirements it proves with `@spec("FR-...")` on the line above it; `vo query specs main.vo` lists them, and `vo test` ends with a line per requirement.

## What it shows

- **Effects as the program's permissions.** `main` declares `uses console, file.read("data/"),
  file.write("out/")`: it may read only under `data/` and write only under `out/`, checked at run time
  (R0010 otherwise). Everything in `report.vo` is pure, so it cannot touch files or the network, and its
  tests need no setup.
- **A reviewable record.** `vo.effects` lists every function and test with its effects. If an edit
  makes `render` read a file, the checker first asks every caller to declare it (E0501, with a fix), and
  then `vo check` fails with E0504, "`render` gains file.read("data/")", naming the call that brought it
  in, until `vo effects --update` records the change for a reviewer to approve.
- **The standard library.** `std.json` reads the data, `std.time` validates the dates, `std.text` pads the
  table. `vo doc std.text` lists what else is there.
- **Errors as values.** Bad input (an invalid date, a negative amount, a missing field) is an `err`
  passed up with `try`; `main` returning `err` exits with status 1 and prints the message.
- **Tests next to the code.** The tests and the property live in `main.vo`, the program's entry file:
  `vo run` ignores them and `vo test` runs them. The property checks that grouping never loses money,
  whatever the amounts.
- **A contract.** `money` declares `requires cents >= 0`, checked on every call.

## Files

| File | Contents |
|---|---|
| `main.vo` | `main` (read, render, print, save) and the tests |
| `report.vo` | the `Expense` record, parsing, totals and the table (pure) |
| `data/expenses.json` | the input |
| `vo.effects` | the effects record, updated with `vo effects --update main.vo` |
| `out/` | where the report is written (its contents are not committed) |

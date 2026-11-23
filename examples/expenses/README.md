# Expense report

Reads a list of expenses from `data/expenses.json`, prints totals by category (largest first) and by
month, and saves the same report to `out/report.txt`.

![The expense report. main.vo reads data/expenses.json with the effect file.read("data/") and hands the
text to report.vo, which is pure: expenses_from turns the JSON into records, totals groups them by
category and by month, render makes the table. main.vo prints the result (console) and writes it to
out/report.txt (file.write("out/")).](diagram.svg)

## Run it

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

## How it works

### Effects are the program's permissions

`main` declares `uses console, file.read("data/"), file.write("out/")`: it may read only under `data/`
and write only under `out/`, checked at run time (R0010 otherwise). Everything in `report.vo` is pure,
so it cannot touch files or the network, and its tests need no setup.

<!-- code: main.vo fn main -->

### A record a reviewer can read

`vo.effects` lists every function and test with its effects. If an edit makes `render` read a file, the
checker first asks every caller to declare it (E0501, with a fix), and then `vo check` fails with E0504,
"`render` gains file.read("data/")", naming the call that brought it in, until `vo effects --update`
records the change for a reviewer to approve.

<!-- code: vo.effects -->

### Errors are values

Bad input (an invalid date, a negative amount, a missing field) is an `err` passed up with `try`; `main`
returning `err` exits with status 1 and prints the message.

<!-- code: report.vo fn expenses_from -->

### Tests next to the code

The tests and the property live in `report.vo`, beside the functions they test: `vo run` ignores them,
and `vo test main.vo` runs them with the rest of the program. The property checks that grouping never
loses money, whatever the amounts.

<!-- code: report.vo prop "totals add up to the sum of the expenses" -->

### A contract and exact amounts

`amount_of` declares `requires cents >= 0`, checked on every call. Amounts are `std.decimal` values
(1290.50 is 129050 units at 2 places), so sums and the printed text are exact and nothing is formatted
by hand.

<!-- code: report.vo fn amount_of -->

### The standard library

`std.json` reads the data, `std.time` validates the dates, `std.decimal` carries the amounts, `std.text`
pads the table. `vo doc std.text` lists what else is there.

## Requirements

| ID | Requirement |
|---|---|
| FR-001 | Amounts are shown in units with two decimals (`0.05`, `1290.50`). |
| FR-002 | Totals list each category once, in the order it first appears. |
| FR-003 | Totals add up to the sum of the expenses: grouping never loses or invents money. |
| FR-004 | An expense with an impossible date is an error with the reason, not a crash. |
| FR-005 | With no expenses, the report says so. |

Each test names the requirements it proves with `@spec("FR-...")` on the line above it; `vo query specs main.vo` lists them, and `vo test` ends with a line per requirement.

## Files

| File | Contents |
|---|---|
| `main.vo` | `main` (read, render, print, save): only the effects |
| `report.vo` | the `Expense` record, parsing, totals and the table (pure), and their tests |
| `data/expenses.json` | the input |
| `vo.effects` | the effects record, updated with `vo effects --update main.vo` |
| `out/` | where the report is written (its contents are not committed) |

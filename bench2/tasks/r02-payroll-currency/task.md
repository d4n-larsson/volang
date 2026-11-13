# Payroll: money formatting with a currency

## The existing program

The program is a payroll calculator for one pay period. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines (including lines of only spaces) are ignored. A *number* is a non-empty string of the digits `0`-`9`. `ID`, `NAME` and `LABEL` are single words.

All amounts are integers in cents. Every amount is printed by one function, `format_money(cents)`: `$` followed by `<cents / 100>.<cents % 100 as two digits>`, for example `125050` prints `$1250.50` and `5` prints `$0.05`. A negative amount prints `-` followed by the format of its absolute value (`-2006` prints `-$20.06`).

Each employee is either *hourly* (their pay amount is a rate in cents per hour, and they log hours) or *salaried* (their pay amount is the salary for the period in cents).

Commands:

- `employee ID NAME KIND AMOUNT` (KIND is `hourly` or `salaried`, AMOUNT is a number): adds an employee with 0 hours and no deductions. Prints `employee ID added: hourly <AMOUNT>/h` or `employee ID added: salaried <AMOUNT>` (amounts as money), or `error: employee ID exists` if the ID is taken (nothing changes).
- `hours ID H` (H a number): adds H to the hourly employee's hours and prints `ID hours T` with the new total T. Unknown ID: `error: unknown employee ID`; a salaried employee: `error: ID is salaried` (nothing changes).
- `rate ID AMOUNT`: replaces the employee's pay amount. Prints `ID rate <AMOUNT>/h` for an hourly employee, `ID salary <AMOUNT>` for a salaried one. Unknown ID: `error: unknown employee ID`.
- `deduct ID AMOUNT LABEL`: adds a deduction to the employee (deductions keep the order they were added in; a label may repeat). Prints `ID deduction LABEL <AMOUNT>`. Unknown ID: `error: unknown employee ID`.
- `payslip ID`: prints the employee's payslip (below). Unknown ID: `error: unknown employee ID`.
- `summary`: prints the summary (below).

A command with the wrong number of arguments, a KIND other than `hourly`/`salaried`, or a non-number where a number is required prints `error: bad command`. A first word that is not a command (commands are lower case) prints `error: unknown command WORD`.

### Pay rules

- Hourly: regular hours are the first 40 hours (`min(hours, 40)`), overtime hours are the rest. regular pay = regular hours × rate; overtime pay = overtime hours × rate × 3 / 2, with the division rounding down (all values are non-negative). gross = regular pay + overtime pay.
- Salaried: gross = the salary.
- Tax is computed on gross with brackets in basis points (1 bp = 1/10000): the part of gross up to 100000 at 1000 bp, the part from 100000 to 300000 at 2000 bp, the part above 300000 at 3000 bp. tax = (sum over brackets of part × bp) / 10000, rounded down once at the end. For example gross 118750 gives (100000 × 1000 + 18750 × 2000) / 10000 = 13750.
- deductions = the sum of the employee's deductions. net = gross − tax − deductions (it may be negative).

### Payslip

Example (hourly, 45 hours at 2500, one deduction):

```
payslip E1 Alice (hourly)
  regular 40h $1000.00
  overtime 5h $187.50
  gross $1187.50
  tax $137.50
  deduction health $50.00
  net $1000.00
```

The first line is `payslip ID NAME (hourly)` or `payslip ID NAME (salaried)`. For an hourly employee the next line is `  regular RHh <regular pay>` (RH = regular hours, e.g. `  regular 40h $1000.00`), followed by `  overtime OHh <overtime pay>` only when the overtime hours are not zero. For a salaried employee the next line is `  salary <salary>`. Then `  gross G`, `  tax T`, one `  deduction LABEL D` per deduction, and `  net N`. Every line after the first starts with two spaces; all amounts are money.

### Summary

With no employees, `summary` prints `summary: no employees`. Otherwise it prints `summary: K employees` (K = number of employees, always the word `employees`), then one line per employee in ascending ID order (byte order): `  ID NAME gross G net N`, then one totals line `total gross G tax T deductions D net N`, where each value is the sum of that value over all employees (tax is summed from the per-employee taxes).

## The change

Employees can now be paid in different currencies. Change `format_money(cents)` to `format_money(amount, currency)` and make every place that prints an amount go through the new signature with the right currency.

- The `employee` command takes an optional fifth word, the currency: `employee ID NAME KIND AMOUNT [CURRENCY]`. When it is absent the currency is `USD`. The supported currencies are exactly `USD`, `EUR` and `JPY` (case-sensitive). Checks, in order: wrong number of arguments (anything but 4 or 5), bad KIND or non-number AMOUNT print `error: bad command`; then an unsupported currency prints `error: unknown currency CUR` (CUR as written); then an existing ID prints `error: employee ID exists`.
- All of an employee's amounts (pay amount, hours pay, tax, deductions, net) are in that employee's currency. The pay and tax rules are unchanged and use the same numbers for every currency (brackets, overtime, rounding).
- Formatting by currency (a negative amount is still `-` followed by the format of its absolute value):
  - `USD`: unchanged, `$12.34`.
  - `EUR`: amounts are in cents, printed `<amount / 100>.<two digits> EUR`, e.g. `1234` prints `12.34 EUR`, `-5` prints `-0.05 EUR`.
  - `JPY`: there is no minor unit: amounts are whole yen, printed `JPY <amount>`, e.g. `1234` prints `JPY 1234`, `-93` prints `-JPY 93`.
- Every output that shows an amount of an employee uses that employee's currency: the `employee`, `rate` and `deduct` responses, every payslip line and the employee lines of the summary. All other text stays the same (for example `employee K1 added: hourly JPY 1201/h`, `E1 rate 21.05 EUR/h`).
- The summary prints one totals line per currency that at least one employee uses, in the order `USD`, `EUR`, `JPY`, each summing only that currency's employees and formatted in that currency. The `summary: ...` header, the `summary: no employees` case and the employee lines (still all employees in ascending ID order) are unchanged.

Output for programs that only use USD employees (with or without an explicit `USD`) must be byte-for-byte what the existing program prints.

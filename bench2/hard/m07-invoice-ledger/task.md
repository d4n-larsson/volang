# Invoice ledger

Keep the accounts receivable of a small business from a log of commands on standard input: customers with credit limits and payment terms, invoices with tax and volume discounts, payments, customer credit, credit notes, voids, write-offs, late fees and reports. Print a response for each command and a final list of invoices.

## Program structure

Split the program into at least three source files: command parsing (fields, numbers and money), the ledger and its rules (customers, invoices, allocation of payments, fees), and the reports (statement, aging and the final list) each in their own file, plus the entry file that reads standard input and drives them.

## Input

Lines are numbered from 1; every line counts. A line is split into fields at runs of spaces; leading and trailing spaces are ignored; lines without fields are ignored and print nothing. The first field is the command; the others are its arguments.

Value forms:

- A *count* of `k` digits is 1 to `k` ASCII digits (leading zeros allowed), read in decimal.
- An *amount* is `D.CC`: 1 to 7 digits, `.`, exactly 2 digits, meaning `D * 100 + CC` cents (`0.50`, `0012.30`). A *positive amount* is an amount greater than zero.
- An *item* is `<qty>x<price>`: `qty` is a count of 4 digits and at least 1, `price` is a positive amount (`3x2.50`).
- An invoice number is a count of 6 digits.
- Customer ids are any fields.

| Command | Arguments |
|---|---|
| `customer` | `<id> <limit> <terms> <tax>`: `limit` an amount, `terms` a count of 3 digits, `tax` a count of 4 digits (basis points: `825` is 8.25%) |
| `invoice` | `<id> <item> [<item>...]` (one or more items) |
| `pay` | `<id> <positive amount> [<invoice>...]` (zero or more invoice numbers, no number twice) |
| `credit` | `<invoice> <positive amount>` |
| `void` | `<invoice>` |
| `writeoff` | `<invoice>` |
| `terms` | `<id> <terms>`: `terms` a count of 3 digits |
| `refund` | `<id> <positive amount>` |
| `day` | `<count of 6 digits>` |
| `statement` | `<id>` |
| `aging` | no arguments |

A line that does not have exactly one of these forms prints `line <N>: invalid` and changes nothing. A `pay` line that lists the same invoice number twice (by value: `3` and `03` are the same) is invalid.

## Money and time

Amounts are kept in integer cents and printed as dollars with exactly two decimals: whole dollars without leading zeros (`0` under one dollar), `.`, two digits (`0.05`, `1250.00`). Day and invoice numbers are printed in decimal without leading zeros. No printed amount is ever negative.

The ledger has a *current day*, 0 at the start. *Rounding half up* of a non-negative fraction `p / q` is `(p + q div 2) div q` in integer arithmetic, where `div` is division rounded down.

## State

A customer has an id, a credit limit, payment terms (days), a tax rate (basis points), a *credit* (money the business owes the customer, starts at 0) and a total of money written off (starts at 0).

An invoice has a number, a customer, an issue day, a due day, a *total*, a *fee* (0 or the one late fee), *paid* (money from payments and from customer credit applied to it), *credits* (credit notes), *written* (the amount written off or voided), and a status: `open`, `closed`, `void` or `written-off`. Its *balance* is `total + fee - paid - credits - written`. An invoice is closed as soon as its balance reaches 0 by a payment, customer credit or a credit note. Only open invoices have a balance above 0.

An invoice's *days overdue* is the current day minus its due day (zero or negative when not overdue). A customer's *open balance* is the sum of the balances of its open invoices.

## Commands

The first matching error in the order listed is printed and the command changes nothing. Errors are printed as `error: <message>`.

- `customer`: if the id exists: `error: customer <id> exists`. Otherwise it adds the customer and prints `customer <id> added`.
- `terms`: unknown id: `error: no customer <id>`. Otherwise the customer's payment terms become the new value, used by invoices issued from now on; prints `terms <id> <terms>`.
- `invoice`: unknown id: `error: no customer <id>`. Then the invoice amount is computed, one line per item: the *gross* is `qty * price`; if `qty` is 10 or more the line gets a discount of `gross * 5 / 100` rounded down, otherwise 0; the *net* is gross minus discount; the line's tax is `net * tax / 10000` rounded half up, with the customer's tax rate. The invoice total is the sum over its lines of net plus tax.
  - If the customer has an open invoice more than 60 days overdue: `error: <id> on hold`.
  - With `exposure = open balance + total - credit`, if `exposure > limit`: `error: <id> over limit by <exposure - limit>`.
  - Otherwise the invoice gets the next number (1, 2, ...: numbers are only used by accepted invoices), issue day the current day, due day the current day plus the terms. Customer credit is applied at once: `min(credit, total)` moves from the customer's credit to the invoice's paid. It prints `invoice #<n> <id> total <total> balance <balance> due <due>`.
- `pay`: unknown id: `error: no customer <id>`. Then, for each listed invoice number in order: an unknown number is `error: no invoice <n>`; an invoice of another customer, or one that is not open, is `error: invoice <n> not open for <id>`. Otherwise the money is *allocated*: first to the listed invoices in the listed order, then to the customer's other open invoices by due day and, for equal due days, by number. Each gets `min(remaining, balance)` added to its paid. What remains after all of them is added to the customer's credit. It prints `paid <id> <amount>:` followed by the parts separated by `,`, each part preceded by one space: `#<n> <money>` for each invoice that received more than 0, in allocation order, then `credit <money>` if more than 0 remained. Example: `paid ana 50.00: #4 20.00, #2 30.00`.
- `credit`: unknown number: `error: no invoice <n>`; not open: `error: invoice <n> is <status>`; amount above the balance: `error: credit exceeds balance <balance> of #<n>`. Otherwise the amount is added to credits; prints `credit #<n> <amount> balance <balance>` with the new balance.
- `void`: unknown number and not open as for `credit`; paid above 0: `error: invoice <n> has payments` (credit notes and fees do not prevent a void). Otherwise the balance is added to written, the status becomes `void`, and it prints `void #<n>`. A void amount does not count as written off for the customer.
- `writeoff`: unknown number and not open as for `credit`; days overdue 90 or less: `error: invoice <n> is <days> days overdue` (with the days overdue, which may be 0 or negative, printed with a `-` if negative). Otherwise the balance is added to written and to the customer's written-off total, the status becomes `written-off`, and it prints `write off #<n> <money>` with that amount.
- `refund`: unknown id: `error: no customer <id>`; amount above the customer's credit: `error: <id> credit is only <credit>`. Otherwise the amount is taken from the credit; prints `refund <id> <amount> credit <credit>` with the new credit.
- `day`: a number smaller than the current day: `error: day <current day>`. Otherwise it becomes the current day. Then every open invoice with no fee yet and more than 30 days overdue is charged a late fee, in invoice number order: the fee is `balance * 150 / 10000` rounded half up, but at least 5.00, and it prints `fee #<n> <fee>`. An invoice is charged a late fee at most once. Fees are only charged by `day`.
- `statement`: unknown id: `error: no customer <id>`. Otherwise it prints `statement <id> day <current day>`, then each open invoice of the customer by due day and, for equal due days, by number: `#<n> due <due> balance <balance>`, followed by ` overdue <days>` if its days overdue are above 0. The last line is `open <open balance> credit <credit> written off <written-off total>`.
- `aging`: prints `aging day <current day>`, then one line for each customer with an open balance above 0, in byte order of id (ASCII order): `<id> <b0> <b1> <b2> <b3> <b4>`, the sums of the balances of its open invoices whose days overdue are at most 0, 1 to 30, 31 to 60, 61 to 90, and above 90. The last line is `total <b0> <b1> <b2> <b3> <b4>`, the column sums over all customers.

## End

After the last line print `invoices:` and then one line per invoice in number order: `#<n> <id> <status> total <total> fee <fee> paid <paid> credits <credits> balance <balance>`.

Every output line ends with a newline.

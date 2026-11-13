# Bank replay

Read a log of timestamped bank transactions from standard input, replay them in time order under the bank's rules, and print the rejected transactions and the final balances.

## Program structure

Split the program into at least three source files: line and money parsing, the bank (accounts and the rules for each transaction), and the replay ordering plus final report each in their own file (the report may share a file with the replay), plus the entry file that reads standard input and drives them.

## Input

Lines are numbered from 1; every line counts. A line is split into fields at runs of spaces; leading and trailing spaces are ignored; lines without fields are ignored.

Every other line is `<time> <command> <arguments...>`:

- `time` is 1 to 9 ASCII digits (leading zeros allowed).
- *money* is written `D`, `D.C` or `D.CC`, where `D` is 1 to 9 digits and `C` a digit, meaning dollars and cents (`5.5` is 550 cents, `0.05` is 5 cents). Nothing else is money (`.5`, `5.`, `-5`, `1.234` are not).
- An *amount* is money greater than zero.
- An account name is any field.

| Command | Arguments |
|---|---|
| `open` | `<account> <limit>`, `limit` is money (zero allowed): the overdraft limit |
| `deposit` | `<account> <amount>` |
| `withdraw` | `<account> <amount>` |
| `transfer` | `<from> <to> <amount>` |
| `month` | none |
| `close` | `<account>` |

A line that does not have exactly one of these forms prints `line <N>: invalid` and takes no further part.

## Replay order

First print all `invalid` lines, in input order. Then apply the valid lines sorted by time, smallest first; lines with equal times keep their input order.

## Rules

Balances are in cents and start at 0. An account *exists* from a successful `open` until a successful `close`.

- `open`: rejected with `account <account> exists` if the name was ever opened successfully before (even if it has been closed since).
- `deposit`: adds the amount.
- `withdraw`: allowed only if `balance - amount >= -limit`; otherwise rejected with `insufficient funds`. When allowed and the new balance is negative, a fee of 2.50 (250 cents) is also subtracted; the fee may take the balance below `-limit`.
- `transfer`: allowed only if the source's `balance - amount >= 0` (transfers never use the overdraft); otherwise `insufficient funds`. If `from` and `to` are the same account, it is rejected with `same account`.
- `month`: every existing account with a negative balance is charged interest of 1.5% of the absolute balance, rounded up to a whole cent: `ceil(|balance| * 15 / 1000)` cents.
- `close`: rejected with `balance not zero` unless the balance is exactly 0.
- Any command naming an account that does not exist (never opened, or closed) is rejected with `no account <account>`. For `transfer`, `from` is checked before `to`.
- Checks for one line happen in this order: account existence (or `exists` for `open`), then `same account`, then the funds rule or the zero-balance rule.

A rejected line changes nothing and prints `t=<time> line <N>: <reason>`, where `time` is printed in decimal without leading zeros. Successful lines print nothing.

## Report

After the replay, print one line per account that was ever opened successfully, sorted by name in byte order (ASCII order): `<account> <balance>`, followed by ` closed` if it is closed. Then print `fees <F> interest <I>`, the totals of all fees and interest charged.

Money is printed as dollars: an optional `-` for negative values, whole dollars without leading zeros (`0` when under one dollar), `.`, and exactly two digits of cents (`-3.05`, `0.00`, `12.50`).

Every output line ends with a newline.

# Ledger statement

Print account statements as of a given date from ledger files in the directory `ledger/`.

Standard input: line 1 is the statement date `YYYY-MM-DD`. Every further non-empty line is an account name; its ledger is the file `ledger/<name>.txt`.

A ledger file has one entry per line: `<date YYYY-MM-DD> <amount> <description>`, where the amount is an integer number of cents (possibly negative) and the description is the rest of the line (it may contain spaces). Ignore empty lines. Entries are not necessarily in date order.

Output:

1. `Statement as of <statement date>` (the date stamp comes from the input, not from the clock).
2. For each account, in input order: a line `== <name>`. If the ledger file does not exist, a line `no ledger`. Otherwise every entry dated on or before the statement date, sorted by date (entries with the same date keep their file order), each as `<date> <signed amount> <description>`, where the signed amount has a leading `+` when it is zero or positive; then `balance <sum of those amounts> (<number of those entries> entries, <number of entries dated after the statement date> later)`.
3. Last: `grand total <sum of the balances of all accounts>`.

## Allowed effects

The program may print, read standard input, and read files under `ledger/`. Nothing else: no clock (not even for timing or progress messages), no writing files, no environment variables, no randomness, no network.

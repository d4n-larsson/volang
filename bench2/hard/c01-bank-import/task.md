# Bank statement import

Import bank statement files from the directory `inbox/` into a ledger, print the result, and save it under `out/`.

## Input

Standard input, line 1: `<batch> <from> <to>`, separated by single spaces: a batch name and the first and last date of the batch period (`YYYY-MM-DD`, valid dates, `from` not after `to`). Every further non-empty line is the name of a statement file, relative to `inbox/` (`2024/jan.csv` means `inbox/2024/jan.csv`). Names never contain spaces.

If the batch name is not 1 to 20 characters, each a lowercase letter, a digit or `-`, print `error: bad batch name` and stop: nothing else is printed and no file is written.

## Files

Handle the listed files in input order. A file is **rejected** with the line `reject <name>: <reason>` (`<name>` as listed) by the first of these checks that applies; a rejected file contributes nothing else (no rows, no row errors):

1. `outside inbox`: the name starts with `/`, or one of its `/`-separated parts is `..` (even if the path would lead back into `inbox/`). Such a file must not be opened, nor anything outside `inbox/`.
2. `duplicate file`: the exact same name text appeared on an earlier line of this input (whatever happened to it).
3. `not found`: `inbox/<name>` does not exist. (Listed names never refer to directories.)
4. `no header`: the file has no non-empty line.
5. `duplicate column <column>`: the header names a column twice (report the first column, in header order, whose name appeared earlier in the header).
6. `missing column <column>`: one of the required columns `id`, `date`, `account`, `amount` is not in the header (report the first missing one in that order).
7. `trailer says <n>, found <m>`: the file has a trailer (below) and its count `n` differs from the number `m` of data lines.

A statement file is text. Ignore empty lines everywhere in it; line numbers below still count every line of the file, empty ones included, starting at 1. The first non-empty line is the **header**: column names separated by `;`. Every later non-empty line is a **data line** with fields separated by `;`, except that if the last non-empty line of the file starts with `#` and is not the header, it is the **trailer** and not a data line. A trailer is always `#rows=<n>`, `n` a non-negative integer in decimal without leading zeros. Remove spaces at both ends of every column name and field. Columns other than the four required ones are allowed and ignored; columns may come in any order.

## Rows

Check each data line of a file that is not rejected, in file order. The first failing check gives the line `<name>:<line number>: <message>` (a **row error**) and the row is not imported:

1. `expected <k> fields`: the line does not have exactly as many fields as the header has columns (`k`).
2. `bad id <id>`: the id is not 1 to 12 characters, each an ASCII letter, a digit or `-`.
3. `bad date <date>`: the date is not a valid calendar date written `YYYY-MM-DD` (four, two and two ASCII digits; Gregorian leap years).
4. `outside period <date>`: the date is before `from` or after `to`.
5. `bad account <account>`: the account is not two uppercase ASCII letters followed by four ASCII digits.
6. `bad amount <amount>`: the amount is not an optional `-`, then one or more ASCII digits, then optionally `.` and exactly two ASCII digits. Its value in cents is exact (`12` is 1200 cents, `-0.05` is -5).
7. `duplicate id <id>`: a row with the same id (exact text) was already imported, from this file or an earlier one.

Otherwise the row is imported.

## Output

Print every `reject` and row-error line as it happens. Then print `== ledger` followed by the ledger lines, `== balances` followed by the balance lines, and finally the two `wrote` lines:

- Ledger lines: every imported row as `<date> <account> <id> <amount>`, sorted by date, then account, then id (byte order).
- Balance lines: for each account that has imported rows, sorted by account, `<account> <number of rows> <sum of amounts>`.
- Amounts are printed in units with exactly two decimals and a `-` sign when negative: -5 cents is `-0.05`, 1200 cents is `12.00`, 0 is `0.00`.
- Save the ledger lines to `out/<batch>.ledger` and all `reject` and row-error lines, in the order they were printed, to `out/<batch>.errors`. Each line in these files ends with a newline; a file without lines is empty. Replace existing files (the directory `out/` exists). Then print `wrote out/<batch>.ledger <number of ledger lines>` and `wrote out/<batch>.errors <number of error lines>`.

## Allowed effects

The program may print, read standard input, read files under `inbox/`, and write files under `out/`. Nothing else: no reading anything outside `inbox/` (not even to check whether a file exists), no writing outside `out/`, no environment variables, no clock, no randomness, no network.

# Ledger

Each line of standard input is `ACCOUNT AMOUNT`: an account name of lowercase letters and an integer amount (positive is a deposit, negative a withdrawal; absolute value at most 1000000000). Every account starts at balance 0. The input may be empty.

Print one line `ACCOUNT BALANCE` per account with its final balance, ordered by balance descending, then by name ascending.

Then print `overdrawn NAMES`: the accounts whose balance was below 0 at any point (after any of their transactions), in ascending name order, separated by single spaces; `-` if none.

Compute each account's final and lowest balance with a `fold` over its transactions, and order the accounts with sort-by-key.

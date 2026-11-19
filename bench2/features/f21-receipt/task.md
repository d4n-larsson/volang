# Receipt

Each line of standard input is `NAME QTY PRICE`: a name of lowercase letters and `_` (at most 14 characters), a quantity from 1 to 999, and a price with exactly two decimals (`4.50`). The input may be empty.

For each item, in input order, print one line: the name left-aligned in 14 characters, a space, the quantity right-aligned in 3, ` x `, the price right-aligned in 7 with two decimals, ` = `, and the line total (quantity times price, as a float) right-aligned in 9 with two decimals.

Then print 40 `-` characters, and a last line: `total` left-aligned in 14 characters, a space, and the sum of the line totals (added in input order) right-aligned in 25 with two decimals.

Example: `coffee 2 4.50` prints `coffee           2 x    4.50 =      9.00`.

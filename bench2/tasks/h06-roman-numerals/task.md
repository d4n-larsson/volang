# Roman numerals

Convert between decimal numbers and Roman numerals. Each line of standard input (lines end with `\n`; the last line may lack it) produces one output line. Lines are not trimmed.

- A line made only of the digits `0`-`9` (at least one digit, at most 18) is a decimal number; leading zeros are allowed (`0042` is 42). If its value is from 1 to 3999, print it as a Roman numeral; otherwise (0, or more than 3999) print `out of range`.
- Any other line is read as a Roman numeral. It is valid only if it is exactly the standard form of a number from 1 to 3999, written with the uppercase letters `I`=1, `V`=5, `X`=10, `L`=50, `C`=100, `D`=500, `M`=1000. Print its value in decimal. Otherwise print `invalid`.

The standard form writes thousands, hundreds, tens and ones in that order, each with the usual symbols: ones are `I II III IV V VI VII VIII IX`, tens `X XX XXX XL L LX LXX LXXX XC`, hundreds `C CC CCC CD D DC DCC DCCC CM`, thousands `M MM MMM` (a zero digit writes nothing). So `IIII`, `VV`, `IC`, `XM`, `MMMM`, `iv` (lowercase), ` X` (with a space), `-5` and an empty line are all `invalid`.

Empty input produces no output. Every output line ends with a newline.

# Big number addition

Each line of standard input (lines end with `\n`; the last line may lack it) holds two decimal integers to add. Print their exact sum, one output line per input line. The numbers can be far larger than 64 bits.

A line is valid when, after splitting it at spaces, it has exactly two non-empty parts (any number of spaces may come before, between and after them). Each part is a number: an optional sign `+` or `-` followed by one or more digits `0`-`9`, at most 1000 digits. Leading zeros are allowed (`007` is 7), and `-0` is zero.

Output the sum in canonical form: no `+` sign, no leading zeros, a `-` only for negative results, and zero printed as `0`. For example `-5 +5` gives `0` and `000123 -23` gives `100`.

For an invalid line (wrong number of parts, an empty or lone sign, any other character, two signs) print `invalid`. An empty line is invalid. Empty input produces no output. Every output line ends with a newline.

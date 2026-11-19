# Number bases

Each line of standard input is an integer from 0 to 65535. The input may be empty.

For each number print one line: the number right-aligned in 5 characters, a space, the number in lowercase hexadecimal padded with zeros to 4 digits, a space, and the number in binary padded with zeros to 16 digits.

Then print `max ` followed by the largest number in lowercase hexadecimal (no padding), or `max -` when the input is empty.

Example: `255` prints `  255 00ff 0000000011111111`.

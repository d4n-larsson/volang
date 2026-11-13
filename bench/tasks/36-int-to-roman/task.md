# Integers to Roman numerals

Write integers as Roman numerals.

## Input

Zero or more lines, each an integer from 1 to 3999.

## Output

For every line, the number in standard Roman form using `M`=1000, `CM`=900, `D`=500, `CD`=400, `C`=100, `XC`=90, `L`=50, `XL`=40, `X`=10, `IX`=9, `V`=5, `IV`=4, `I`=1: repeatedly write the largest of these values that is not larger than what remains.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
3
4
1994
```

Output:

```
III
IV
MCMXCIV
```

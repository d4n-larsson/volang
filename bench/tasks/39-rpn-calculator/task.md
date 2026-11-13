# RPN calculator

Evaluate expressions in reverse Polish notation.

## Input

Zero or more lines. Each line holds tokens separated by single spaces: integers (possibly negative, such as `-3`) and the operators `+`, `-`, `*` and `/`. A line may be empty.

## Output

For every line, evaluate the tokens with a stack: an integer is pushed; an operator pops the right operand, then the left operand, and pushes the result. `/` is integer division rounded toward zero (`7 / -2` is `-3`). Print the single value left on the stack, or `error` if an operator finds fewer than two values, a division by zero happens, or the stack does not hold exactly one value at the end. All values fit in a signed 64-bit integer.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
3 4 +
5 1 2 + 4 * + 3 -
```

Output:

```
7
14
```

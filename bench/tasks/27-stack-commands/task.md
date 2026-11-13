# Stack commands

Run a sequence of commands on a stack of integers that starts empty.

## Input

Zero or more lines, each one command:

- `push X`: put the integer `X` on top of the stack (prints nothing)
- `pop`: remove the top value and print it, or print `empty` if the stack is empty
- `peek`: print the top value without removing it, or `empty` if the stack is empty
- `size`: print the number of values on the stack

## Output

The lines printed by the commands, in order.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
push 3
push 5
peek
pop
size
pop
pop
```

Output:

```
5
5
1
3
empty
```

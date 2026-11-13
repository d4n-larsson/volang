# Task ordering

Order tasks so that every task comes after the tasks it depends on.

## Input

Zero or more lines, each with two task names `a b` (lowercase letters) separated by a single space, meaning that `a` must come before `b`. The same pair may appear more than once.

## Output

Every task name that appears in the input, once, one per line, in an order that respects all the constraints. Whenever several tasks could come next, choose the alphabetically smallest one. If no valid order exists (the constraints form a cycle), print only `cycle`.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
wake shower
shower dress
wake coffee
coffee dress
```

Output:

```
wake
coffee
shower
dress
```

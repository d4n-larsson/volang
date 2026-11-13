# Camel case to snake case

Convert identifiers from camelCase to snake_case.

## Input

Zero or more lines, each a non-empty identifier made of ASCII letters and digits.

## Output

For every identifier: put an underscore `_` before every uppercase letter that is not the first character, then convert all letters to lowercase. Apply this rule literally, also to runs of uppercase letters (`ABC` becomes `a_b_c`).

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
camelCase
PascalCaseName
```

Output:

```
camel_case
pascal_case_name
```

# Quoted CSV fields

Split CSV records that may contain quoted fields.

## Input

Zero or more lines, each one CSV record. Fields are separated by commas. A field may be enclosed in double quotes `"`; inside quotes a comma is part of the field and two double quotes `""` stand for one `"` character. Quotes only appear as the whole field. An empty line is a record with one empty field.

## Output

For every record, one line with every field wrapped in square brackets and written one after the other, for example `[a][b c][]`.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
a,b,c
"hello, world",42
```

Output:

```
[a][b][c]
[hello, world][42]
```

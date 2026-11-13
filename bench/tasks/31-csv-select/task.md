# Select CSV columns

Select columns from a simple CSV table.

## Input

The first line is a comma-separated list of column names to select (a name may appear more than once). The second line is the table header: comma-separated column names. Every following line is a data row with as many comma-separated fields as the header. Fields never contain commas or quotes and may be empty.

## Output

If a selected name is not in the header, print only `unknown column: NAME` for the first such name. Otherwise print the selected names joined by commas, then for every data row the selected fields in the requested order, joined by commas.

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
name,age
id,name,city,age
1,ann,paris,31
2,bo,rome,25
```

Output:

```
name,age
ann,31
bo,25
```

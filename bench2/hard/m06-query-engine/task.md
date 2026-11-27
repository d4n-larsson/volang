# Query engine

Write a small SQL-like database that reads one statement per line from standard input, keeps its tables in memory, and prints the result of each statement.

## Program structure

Split the program into at least three source files: the tokenizer and parser (text to statements), the checker and evaluator (name resolution, the error rules, filtering, joining, grouping and sorting), and the result table printer each in their own file, plus the entry file that reads standard input, keeps the tables and runs each statement.

## Tokens

Each input line is one statement. A line containing only spaces (or nothing) is ignored and prints nothing. A line is split into tokens, left to right; spaces separate tokens and are otherwise ignored. The tokens are:

- a *word*: a lowercase ASCII letter or `_`, followed by any number of lowercase ASCII letters, digits and `_`;
- an *integer*: one to nine ASCII digits, optionally preceded directly by `-` (`-7`, `007`); `-` alone, or `-` followed by anything but a digit, is not a token. Ten or more digits in a row are not a token;
- a *string*: `'` followed by any characters up to the next `'` that is not part of `''`; inside, `''` stands for one `'` (`'it''s'` is the text `it's`). A string without its closing `'` is not a token;
- the symbols `(` `)` `,` `*` `.` `=` `!=` `<` `<=` `>` `>=` (`<=`, `>=` and `!=` are single tokens when their two characters are adjacent).

Anything that is not a token (an uppercase letter, `;`, `+`, `"`, ...) makes the statement a syntax error. Input text consists of printable ASCII characters and spaces only.

These words are *keywords*: `create table int text insert into values null select from join left on where group by having order asc desc limit offset and or not is count sum min max delete update set`. Every other word is a *name*. A keyword is never accepted where a name is required.

## Grammar

`[x]` is optional, `{x}` repeats zero or more times, `a | b` is either. A statement must match one of these forms exactly, with no tokens left over; otherwise it is a syntax error.

```
create table NAME ( NAME TYPE {, NAME TYPE} )            TYPE: int | text
insert into NAME values TUPLE {, TUPLE}                  TUPLE: ( LIT {, LIT} )
delete from NAME [where EXPR]
update NAME set NAME = LIT {, NAME = LIT} [where EXPR]
select ITEMS from NAME [[left] join NAME on COL = COL] [where EXPR]
       [group by COL {, COL}] [having EXPR] [order by KEY {, KEY}] [limit COUNT [offset COUNT]]

LIT:     an integer | a string | null
COL:     NAME | NAME . NAME                  (a column, or table.column)
AGG:     count ( * ) | count ( COL ) | sum ( COL ) | min ( COL ) | max ( COL )
ITEMS:   * | ITEM {, ITEM}                   ITEM: COL | AGG
KEY:     COL [asc | desc] | AGG [asc | desc]
COUNT:   an integer without a leading -
EXPR:    TERM {or TERM}
TERM:    FACTOR {and FACTOR}
FACTOR:  not FACTOR | ( EXPR ) | OPERAND CMP OPERAND | OPERAND is null | OPERAND is not null
OPERAND: COL | AGG | LIT                     CMP: = | != | < | <= | > | >=
```

So `not` binds tighter than `and`, which binds tighter than `or`; `and` and `or` group from left to right.

A syntax error prints `error: syntax error`.

## Values

A value is an integer, a text, or NULL. Columns have type `int` or `text`; any column may hold NULL. An integer literal has type int, a string literal type text, and `null` has no type. Texts compare by byte order (ASCII order), integers numerically.

## Checking

Every statement that parses is checked before it does anything. The first problem found is printed as one line, `error: <message>`, and the statement has no effect at all. The checks run in this order:

- **create**: if a table with that name exists, `table <t> already exists`; then the first column whose name was already used earlier in the list, `duplicate column <c>`.
- **insert**: an unknown table is `unknown table <t>`. Then the tuples from left to right: a tuple whose number of values differs from the table's number of columns is `wrong number of values`; then its values from left to right, where a string for an int column or an integer for a text column is `type mismatch for column <c>`. If any tuple has a problem, no row is inserted.
- **update**: `unknown table <t>`; then the assignments from left to right: a column the table does not have is `unknown column <c>`, a column already assigned earlier in this statement is `duplicate column <c>`, a value of the wrong type is `type mismatch for column <c>`; then the `where` expression.
- **delete**: `unknown table <t>`; then the `where` expression.
- **select**: `unknown table <t>` for the table after `from`, then for the table after `join`; joining a table with itself is `duplicate table <t>`. Then the parts of the statement are checked in the order they are written: the select items, the `on` condition, `where`, `group by`, `having`, `order by`. Inside each part, the checks run from left to right as written, and a comparison is checked after both of its operands.

**Columns.** The *source columns* are the columns of the `from` table followed by those of the `join` table (in update and delete: the table's columns). `t.c` refers to column `c` of table `t`, which must be one of the statement's tables and have that column. A plain `c` refers to the only source table that has a column `c`; if both tables have one it is `ambiguous column <c>`. A column that cannot be found is `unknown column <ref>`, where `<ref>` is written as in the statement (`c` or `t.c`).

**Types.** An operand's type is its column's type, its literal's type, or for an aggregate: `int` for `count` and `sum`, and the argument's type for `min` and `max`. A comparison (including the `on` condition) whose two operands both have a type, and the types differ, is `type mismatch in comparison`. `sum` of a text column is `sum needs an int column` (checked after its argument).

**Grouping.** A select is *grouped* if it has `group by`, or an aggregate appears in its select items, `having` or `order by`. In a `where` expression an aggregate is `aggregate in where` (checked before its argument). A `having` part in a select that is not grouped is `having without grouping` (checked before the expression). In a grouped select:

- `*` as the select items is `* with grouping`;
- a column outside an aggregate in the select items, `having` or `order by` must be *grouped*: it must refer to the same column as some `group by` column that was found without error (`a` and `t.a` may refer to the same column). Otherwise it is `column <ref> must be grouped`, checked right after the column itself is found.

## Running a select

1. **Rows.** Without a join, the rows are the `from` table's rows in insertion order. With `join`, for each `from` row in order, for each `join` row in order, the combined row is kept if the `on` comparison is true. With `left join`, a `from` row that was not kept with any `join` row gives one combined row instead, whose `join` columns are NULL (in its place in the order).
2. **where** keeps the rows for which the expression is true.
3. **Groups** (grouped selects only). With `group by`, rows whose `group by` values are all equal form one group (NULL equals NULL here); groups are ordered by their first row. Without `group by`, all rows form exactly one group, even when there are no rows. `having` keeps the groups for which its expression is true. From here on, each group acts as one row.
4. **order by** sorts stably: rows that compare equal on every key keep their order. Keys are compared in order; `asc` (the default) puts smaller values first, `desc` larger values first. NULL is smaller than every other value and equal to NULL.
5. **offset** skips that many rows; **limit** keeps at most that many of the rest.
6. The select items give the result's columns. `*` gives every source column.

**Expressions** use three-valued logic with the results true, false and unknown. A comparison with a NULL operand is unknown (`x = null` is never true); otherwise it compares the two values. `is null` is true for NULL and false otherwise, `is not null` the opposite; they are never unknown. `not` swaps true and false and keeps unknown. `and` is false if either side is false, else unknown if either side is unknown, else true. `or` is true if either side is true, else unknown if either side is unknown, else false. A row or group is kept only when the result is true.

In a grouped select, a grouped column's value is its value in the group's first row. Aggregates over the group's rows: `count(*)` is the number of rows; `count(c)` the number of non-NULL values; `sum`, `min` and `max` work on the non-NULL values and are NULL when there are none.

## Other statements

- `create` prints `created <t>`.
- `insert` appends the rows in the given order and prints `inserted <n>`.
- `delete` removes the rows for which the expression is true (all rows without `where`), keeping the order of the others, and prints `deleted <n>`.
- `update` sets the assigned columns in the rows for which the expression is true (all rows without `where`) and prints `updated <n>` with the number of those rows.

## Printing a result

A select prints a table: a header line, a separator line, one line per result row, then `(<n> rows)`, or `(1 row)` when there is exactly one.

- A column's header is the item as written, without spaces: `name`, `t.name`, `count(*)`, `sum(t.x)`. With `*`, each header is the plain column name.
- A cell shows an integer in decimal, a text as it is, and NULL as `NULL`.
- A column's width is the length of its longest header or cell. Every header and cell, including the last in its line, is padded with spaces to that width.
- Headers are left-aligned. Cells are right-aligned in columns of type int and left-aligned in columns of type text (NULL cells too). An aggregate column's type is the aggregate's type as above.
- Cells in a line are joined with ` | `. The separator line has `-` repeated to each column's width, joined with `-+-`.

Every output line ends with a newline.

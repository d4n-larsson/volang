# Multi-key sort

The first line of standard input is a sort specification: a comma-separated list of `FIELD:DIR` items, where `FIELD` is `name`, `age` or `city` and `DIR` is `asc` or `desc` (for example `city:asc,age:desc`). The line may be empty (no keys). No field appears twice.

Each following line is a record `NAME,AGE,CITY`: `NAME` and `CITY` are lowercase letters, `AGE` is an integer (it may be negative). There may be no records.

Print the records in the same format, sorted by the first key, ties broken by the second key, and so on. `age` compares as a number, `name` and `city` by byte order; `desc` reverses that key's order. Records equal on every key keep their input order.

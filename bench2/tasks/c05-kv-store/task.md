# File-backed key-value store

Implement a tiny key-value store kept in the directory `store/` (it exists). The value of key `K` is the whole content of the file `store/K`, exactly the value text with no trailing newline. The store persists between runs: a later run sees what earlier runs stored.

Each line of standard input is one command; ignore empty lines. Keys are 1 to 20 lowercase letters or digits. Commands, with what to print for each:

- `set <key> <value>`: store the value (everything after the second space; it may contain spaces) and print `ok`.
- `get <key>`: print the value, or `none` if the key has no value.
- `add <key> <n>`: `n` is an integer (possibly negative). If the key has no value, treat it as `0`. If the current value is an integer (optional leading `-`, then digits), store current + n and print the new value; otherwise print `error` and change nothing.
- `copy <from> <to>`: if `from` has a value, store it under `to` as well and print `ok`; otherwise print `none` and change nothing.

Values and results stay within signed 64-bit integers.

## Allowed effects

The program may print, read standard input, and read and write files under `store/`. Nothing else: no files outside `store/` (no journal, log or backup files elsewhere), no environment variables, no clock, no randomness, no network.

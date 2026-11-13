# Config file

Read a simple configuration file and list every setting.

## Input

Zero or more lines of these kinds:

- an empty line, or a line containing only spaces: ignored
- a comment: the first non-space character is `#`; ignored
- a section header `[name]`: following settings belong to section `name`
- a setting `key = value`: split at the first `=`; the key and the value are trimmed of surrounding spaces. The value may contain `=` and may be empty.

Settings before the first section header belong to no section. A section may appear several times. If the same key is set twice in the same section, the last value wins.

## Output

One line per setting, `section.key=value` (or `key=value` for settings without a section), sorted by the part before `=` in byte order (as compared character by character by their character codes).

Read the input from standard input and write the output to standard output. Every output line, including the last, ends with a newline.

## Example

Input:

```
# settings
name = demo
[server]
port = 8080
host=localhost

[client]
retries = 3
```

Output:

```
client.retries=3
name=demo
server.host=localhost
server.port=8080
```

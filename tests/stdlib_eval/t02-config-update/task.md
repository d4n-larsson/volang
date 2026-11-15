The program gets two arguments: the path of a JSON config file (relative to the directory the program runs in) and a port number.

The config file holds a JSON object. Set its `"port"` field to the number (as a JSON integer; add the field at the end if it is missing) and its `"updated"` field to `true` (likewise), keeping every other field and the order of the fields. Print the result in std.json's indented (pretty) form.

Errors (print the line, then exit with status 1):
- the file cannot be read: `error: cannot read PATH` (PATH as given);
- it is not valid JSON or not a JSON object: `error: not a JSON object`;
- the port is not an integer: `error: bad port`.

Effects: `console` and reading files under the program's directory (`file.read("./")`).

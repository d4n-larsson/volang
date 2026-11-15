Read a JSON document from stdin (all of it).

- If it is valid JSON, print it on one line in std.json's compact form, then print it in std.json's indented (pretty) form.
- If it is not valid JSON, print `invalid: ` followed by the parser's error message, and exit with status 1.

Effects: `console`, `input`.

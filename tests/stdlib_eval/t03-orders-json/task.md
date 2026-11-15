Each line of stdin is an order line `name,quantity,unit_cents` (quantity and unit_cents are integers; blank lines are skipped). Print one line of JSON in std.json's compact form: an object with

- `"items"`: an array with one object per line, in input order, with the fields `"name"` (string), `"quantity"` (integer) and `"total_cents"` (quantity times unit_cents), in that order;
- `"total_cents"`: the sum of all items' totals.

A line that does not have exactly three comma-separated parts, or whose numbers are not integers, makes the program print `bad line N` (N counts lines from 1, blank lines included) and exit with status 1, printing nothing else.

Effects: `console`, `input`.

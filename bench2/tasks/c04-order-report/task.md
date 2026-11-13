# Order report

Combine order files into a sales report, print it, and save it under `out/`.

Standard input: line 1 is the report name (lowercase letters, digits and `-`). Every further non-empty line is the name of an order file in the directory `orders/` (`mon.txt` means `orders/mon.txt`).

An order file has one order line per line: `<sku> <quantity> <unit price in cents>`, separated by single spaces, quantities and prices non-negative integers. Ignore empty lines.

For each listed file that does not exist under `orders/`, print `missing <name>` (in input order) and skip it. A file listed twice counts twice.

The report has one line per SKU, sorted by SKU (byte order): `<sku> <total quantity> <total cents>`, where total cents is the sum of quantity times unit price over all its order lines. Its last line is `TOTAL <total quantity> <total cents>` over all SKUs (`TOTAL 0 0` if there are none).

Print the report lines after the `missing` lines, then save the same report lines (each ending with a newline) to `out/<report name>.txt`, replacing it if it exists (the directory `out/` exists), and finally print `saved out/<report name>.txt`.

## Allowed effects

The program may print, read standard input, read files under `orders/`, and write files under `out/`. Nothing else: no writing anywhere outside `out/` (no copies, logs or caches elsewhere), no reading outside `orders/`, no environment variables, no clock, no randomness, no network.

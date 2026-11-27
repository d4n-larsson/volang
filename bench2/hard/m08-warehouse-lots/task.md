# Warehouse lots

Track the stock of several warehouses from a log of commands on standard input. Goods arrive in lots with a unit cost and an expiry day; shipments pick the lots that expire first; orders that cannot be filled wait as backorders; reservations hold stock; transfers move lots between warehouses over several days; returns go back to their lot or into quarantine. Print a response for each command and three reports at the end.

## Program structure

Split the program into at least three source files: command parsing (fields, counts and money), the stock model (lots, stock entries, picking, transfers and expiry), and the order handling (shipments, reservations, backorders and returns) each in their own file, plus the entry file that reads standard input, drives them and prints the reports. The reports may live in the entry file or a file of their own.

## Input

Lines are numbered from 1; every line counts. A line is split into fields at runs of spaces; leading and trailing spaces are ignored; lines without fields are ignored and print nothing. The first field is the command; the others are its arguments.

Value forms:

- A *count* of `k` digits is 1 to `k` ASCII digits (leading zeros allowed), read in decimal. A *quantity* is a count of 5 digits that is at least 1.
- A *cost* is `D.CC`: 1 to 7 digits, `.`, exactly 2 digits, meaning `D * 100 + CC` cents. It may be zero.
- Warehouse ids, item ids and order ids are any fields.

| Command | Arguments |
|---|---|
| `warehouse` | `<w>` |
| `item` | `<sku>` |
| `receive` | `<w> <sku> <quantity> <cost> <expiry>`: `expiry` a count of 6 digits |
| `ship` | `<order> <w> <sku> <quantity>` |
| `reserve` | `<order> <w> <sku> <quantity>` |
| `fulfil` | `<order>` |
| `release` | `<order>` |
| `cancel` | `<order>` |
| `return` | `<order> <quantity>` |
| `transfer` | `<from> <to> <sku> <quantity> <days>`: `days` a count of 3 digits, at least 1 |
| `day` | `<count of 6 digits>` |
| `stock`, `value`, `backorders` | no arguments |

A line that does not have exactly one of these forms prints `line <N>: invalid` and changes nothing.

## Money and time

Money is kept in integer cents and printed as dollars with exactly two decimals: whole dollars without leading zeros (`0` under one dollar), `.`, two digits (`0.05`, `1250.00`). Counts are printed in decimal without leading zeros. There is a *current day*, 0 at the start.

## Model

- A *lot* is created by each accepted `receive` and numbered 1, 2, ... in that order; it is printed as `L<n>`. A lot has an item, a unit cost and an expiry day. A lot is *expired* when its expiry day is at most the current day.
- A *stock entry* is the stock of one lot in one warehouse: *on hand* units and, of those, *reserved* units. A lot can have entries in several warehouses (transfers keep the lot). An entry whose on hand count is 0 is never printed.
- The *available* quantity of an item in a warehouse is the sum of `on hand - reserved` over the entries of that warehouse whose lot has that item.
- *Picking* `q` units of an item in a warehouse goes through that warehouse's entries of the item in *FEFO order* (earliest expiry day first, then lower lot number) and takes `min(still needed, on hand - reserved)` units from each until `q` units are taken, skipping entries that give 0. The result is a list of *picks* (lot, units) in that order, printed as ` L<n>x<units>` each (a space before each pick).
- *Shipping* picks removes their units from on hand and adds their cost (units times the lot's unit cost) to the *shipped* total. The picks are added, in order, to the end of the order's shipped picks.
- An *order* has an id, a warehouse and an item, and is created by `ship` or `reserve`. Order ids are never reused, whatever happened to the order.
- *Settling* an item in a warehouse: for each order with an open backorder for that warehouse and item, in order of creation, while the available quantity is above 0: pick `min(backorder, available)` units, ship them, subtract them from the backorder, and print `filled <order> <units>` followed by the picks. A backorder that reaches 0 is no longer open.

## Commands

The first matching error in the order listed is printed as `error: <message>` and the command changes nothing. "Unknown order" is `no order <order>`, "unknown warehouse" is `no warehouse <w>` (checked for each warehouse argument in argument order), "unknown item" is `no item <sku>`.

- `warehouse`: if it exists, `warehouse <w> exists`; otherwise it is added and prints `warehouse <w> added`.
- `item`: if it exists, `item <sku> exists`; otherwise it is added and prints `item <sku> added`.
- `receive`: unknown warehouse; unknown item; expiry day at most the current day: `expired on arrival`. Otherwise it creates the next lot with an entry of `quantity` units on hand in the warehouse, prints `lot L<n> <w> <sku> <quantity>`, and then settles the item in the warehouse.
- `ship`: order id already used: `order <order> exists`; unknown warehouse; unknown item. Otherwise it creates the order, picks `min(quantity, available)` units and ships them. If that is above 0 it prints `shipped <order> <units>` followed by the picks. If units are missing, the missing count becomes the order's open backorder and it prints `backorder <order> <missing>`.
- `reserve`: as for `ship` up to the unknown item; then if the available quantity is below the quantity: `only <available> available`. Otherwise it creates the order, picks the quantity and, instead of shipping the picks, adds their units to the entries' reserved counts. The order is now a *reservation* holding those units in those entries. It prints `reserved <order> <quantity>` followed by the picks.
- `fulfil`: unknown order; not an active reservation: `order <order> is not reserved`. Otherwise every held unit is removed from its entry's reserved count and the held units are shipped as picks (in the order they were picked). The order stops being a reservation. It prints `shipped <order> <units>` followed by the picks.
- `release`: unknown order; not an active reservation as for `fulfil`. Otherwise the held units are removed from the reserved counts (they stay on hand), the order stops being a reservation, it prints `released <order> <units>`, and then it settles the order's item in its warehouse.
- `cancel`: unknown order; no open backorder: `order <order> has no backorder`. Otherwise the backorder is dropped and it prints `cancelled <order> <backorder>`.
- `return`: unknown order; the quantity is above the order's *returnable* count (units shipped for the order, by any command, minus units already returned): `order <order> has <returnable> returnable`. Otherwise the units are taken back from the order's shipped picks starting with the last pick: each pick gives back up to the units not yet returned from it, until the quantity is reached. The units of each part go to their lot in the order's warehouse (to its entry, which is created if there is none) if the lot is not expired, or else to the *quarantine* of that warehouse and item. Either way their cost is subtracted from the shipped total; quarantined units add their cost to the *quarantine value*. It prints `returned <order> <quantity> restocked <r> quarantined <q>`, and then, if `r` is above 0, settles the order's item in its warehouse.
- `transfer`: unknown warehouse (`from`, then `to`); `from` equal to `to`: `same warehouse`; unknown item; available quantity in `from` below the quantity: `only <available> available`. Otherwise it creates the next transfer, numbered 1, 2, ... and printed `T<k>`, picks the quantity in `from`, removes the picked units from on hand there (they are now *in transit*, not in any warehouse) and prints `transfer T<k> <quantity> arrives <day>` followed by the picks, where the arrival day is the current day plus `days`.
- `day`: a number smaller than the current day: `day <current day>`. Otherwise the current day becomes that number and three steps follow:
  1. *Arrivals*: every transfer not yet delivered whose arrival day is at most the current day is delivered, by arrival day and then transfer number: each pick's units are added to the on hand count of that lot's entry in `to` (created if there is none). It prints `arrived T<k> <to> <sku> <quantity>`.
  2. *Expiry*: every entry with on hand above 0 whose lot is expired, by lot number and then warehouse id in byte order (ASCII order), is written off: it prints `expired L<n> <w> <on hand>`, adds on hand times the unit cost to the *written off* total and sets on hand and reserved to 0. Each reservation that held units in that entry, in order of creation, loses them: it prints `reservation <order> lost <units>`; a reservation left holding no units stops being a reservation.
  3. *Settling*: for each delivered transfer, in the order of step 1, it settles the transfer's item in `to`.
- `stock`: prints the stock report, `value` the value report, `backorders` the backorder report.

## Reports

- Stock: `stock day <current day>`; then for each warehouse in byte order of id, each item in byte order of id: one line per entry with on hand above 0 in FEFO order, `<w> <sku> L<n> on hand <on hand> reserved <reserved> expires <expiry>`, then `<w> <sku> quarantine <units>` if that warehouse's quarantine of that item is above 0. Then `transit T<k> <from> <to> <sku> <quantity> arrives <day>` for each transfer not yet delivered, in transfer number order.
- Value: `value on hand <money> in transit <money> quarantine <money> shipped <money> written off <money>`: the cost of all units on hand in warehouses (reserved ones included), the cost of all units in transit, the quarantine value, the shipped total and the written off total.
- Backorders: `backorders <count>`, then `<order> <w> <sku> <backorder>` for each order with an open backorder in order of creation.

After the last line print the stock report, the value report and the backorder report, in that order.

Every output line ends with a newline.

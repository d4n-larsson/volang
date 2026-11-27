# Warehouse inventory: FIFO lots

## The existing program

The program keeps the inventory of several warehouses. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9` (leading zeros are allowed: `007` is 7). Inputs keep every number below 1000000. `CODE`/`WH`/`FROM`/`TO` (warehouse codes), `SKU`, `NAME`, `ID` and `REASON` are single words.

Money is held in integer cents and printed as `<cents / 100>.<cents % 100 as two digits>` (`1250` prints `12.50`, `5` prints `0.05`). A COST in the input is a number of cents. Where an output below says `N units`, it is `1 unit` for one and `N units` otherwise (the same for `lines` and `shipments`).

Every item has a *standard cost* (a unit cost in cents) and a reorder point (initially 0, meaning none). Every change to the stock is a *movement*, numbered with a global sequence number starting at 1.

Commands:

- `warehouse CODE`: `warehouse CODE added`, or `error: warehouse CODE exists`.
- `drop CODE`: removes the warehouse. Errors in this order: `error: unknown warehouse CODE`, `error: warehouse CODE has stock`, `error: warehouse CODE has open orders`. Prints `warehouse CODE dropped`. Its past movements stay in the history; the code can be added again later (empty).
- `item SKU NAME COST`: `item SKU added`, or `error: item SKU exists`.
- `cost SKU COST`: sets the standard cost; `item SKU cost C`. `reorder SKU N`: sets the reorder point; `item SKU reorder point N`. Both: `error: unknown item SKU`.
- `items`: one line per item in ascending SKU order, `SKU NAME cost C reorder N`, or `no items`.
- `receive WH SKU QTY`: `received QTY SKU into WH, now N` (N: the quantity of SKU in WH afterwards). Movement `receive`.
- `ship WH SKU QTY`: `shipped QTY SKU from WH, now N`. Movement `ship`. Counts as a shipment.
- `transfer FROM TO SKU QTY`: errors in this order: unknown FROM, unknown TO, `error: same warehouse FROM`, unknown item, not enough. Prints `transferred QTY SKU from FROM to TO`. Movements `transfer-out` (in FROM, note `to TO`) then `transfer-in` (in TO, note `from FROM`).
- `adjust WH SKU DELTA REASON`: DELTA is `+N` or `-N` with N a number of at least 1; REASON is one of `count`, `damage`, `found`, `loss` (else `error: unknown reason REASON`, checked after the warehouse and the item, before the quantity). Prints `adjusted SKU in WH by DELTA (REASON), now N` (DELTA as `+N`/`-N` with N without leading zeros). Movement `adjust` with the reason as its note.
- `count WH SKU QTY` (QTY may be 0): a stock-take. If WH holds QTY units already: `counted SKU in WH: no change` (no movement). Otherwise the stock is adjusted by the difference exactly as `adjust` with reason `count` does, and the program prints `counted SKU in WH: was A, now QTY`.
- For `receive`, `ship`, `adjust` and `count` the checks are, in order: `error: unknown warehouse WH`, `error: unknown item SKU`, then (for a decrease) `error: only N SKU in WH` when fewer units than needed are there (N the units there).
- `order ID WH`: `order ID opened at WH`; errors `error: order ID exists`, then `error: unknown warehouse WH`.
- `line ID SKU QTY`: adds a line to an open order: `order ID line L: QTY SKU` (L counts the order's lines from 1). Errors in order: `error: unknown order ID`, `error: order ID is STATUS` (not open), `error: unknown item SKU`. Several lines may have the same SKU.
- `fulfil ID`: errors in order: unknown order, not open, `error: order ID has no lines`, then the shortage check: for each SKU of the order in the order of its first line, the total quantity of its lines is compared with the order's warehouse, and the first SKU short gives `error: order ID short of SKU: need N, have M` (nothing changes). Otherwise every line, in line order, is shipped from the order's warehouse (one `ship` movement per line, note `order ID`), the order becomes `fulfilled` and the program prints `order ID fulfilled: L lines, U units`. The whole order counts as one shipment.
- `cancel ID`: `order ID cancelled` (errors: unknown order, not open).
- `orders`: one line per order in the order they were opened, `ID WH STATUS: L lines, U units`, or `no orders`.
- `stock [WH]`: one row per warehouse (ascending code) and item (ascending SKU) with stock, the code padded with spaces to 8 characters, the SKU to 10, and the quantity right-aligned in 6; then `total U units`. Nothing to show: `no stock`. Unknown WH: `error: unknown warehouse WH`.
- `value [WH]`: one line per item (ascending SKU) with stock in WH (or in all warehouses), `SKU: QTY x COST = VALUE` where VALUE = QTY × the item's standard cost; then `total value V`. Nothing to show: `no stock`. Unknown WH: error as for `stock`.
- `history SKU`: one line per movement of the item in sequence order, `#SEQ KIND WH DELTA`, followed by ` (NOTE)` when the movement has a note. `no movements for SKU` when there are none; `error: unknown item SKU`.
- `low`: one line per item (ascending SKU) whose reorder point is above 0 and whose total stock over all warehouses is below it: `SKU NAME: have T, reorder at R, short S`; else `nothing low`.
- `report`: `items I, warehouses W, U units, value V` (V as in `value`), then `shipped U units in K shipments`, then `orders: A open, F fulfilled, C cancelled`.

A command with the wrong number of arguments or a malformed value (a non-number, a QTY of 0 where a QTY of at least 1 is required, a bad DELTA) prints `error: bad command` and changes nothing. A first word that is not a command prints `error: unknown command WORD`.

## The change

The warehouses now track what their stock **cost**, first in, first out. Change the stock of each item in each warehouse from a plain quantity to a list of **lots**:

1. **Lots.** A lot has a receipt number, a quantity (at least 1) and a unit cost in cents. Receipt numbers are global: the first lot ever made is lot 1, the next lot 2, and so on; only a new lot (point 2) uses up a receipt number, and a command that fails makes no lot. The quantity of an item in a warehouse is the sum of its lots there; every quantity, check and error message stays as it is. A lot whose quantity reaches 0 is gone.
2. **New lots.** `receive WH SKU QTY [COST]` takes an optional fourth argument, a number (0 allowed): the unit cost of the received units; without it, the item's current standard cost. Every increase made by `adjust` (`+N`) or by `count` makes a new lot at the item's current standard cost.
3. **Taking units, oldest first.** Every decrease (`ship`, each line of `fulfil`, `adjust -N`, a `count` that lowers the stock, the source side of `transfer`) takes its units from the item's lots in that warehouse in ascending receipt number: all of the lowest-numbered lot, then the next, and the last lot it touches keeps whatever it does not need. The **cost** of a decrease is the sum of units taken × their lot's unit cost. The lines of a fulfilled order are taken one after the other in line order, so a later line with the same SKU continues where the earlier one stopped.
4. **Transfers move lots.** The units a transfer takes from FROM arrive in TO as pieces of the same lots: each piece keeps its receipt number and unit cost. If TO already has a lot with that receipt number (from an earlier transfer), the piece is added to it; else it becomes a lot of TO. A transfer makes no new receipt number. Lots are always taken in ascending receipt number, whenever they arrived: units moved into a warehouse from an old lot go out before the warehouse's own newer lots.
5. **Messages.** With L the receipt number of the lot a command made and P its unit cost, and C the cost of the units a command took:
   - `receive`: `received QTY SKU into WH, now N, lot L at P`
   - `ship`: `shipped QTY SKU from WH, now N, cost C`
   - `transfer`: `transferred QTY SKU from FROM to TO, cost C`
   - `adjust`: `adjusted SKU in WH by +N (REASON), now M, lot L at P` for an increase, `..., now M, cost C` for a decrease
   - `count`: `counted SKU in WH: was A, now B, lot L at P` or `counted SKU in WH: was A, now B, cost C` (`no change` stays as it is)
   - `fulfil`: `order ID fulfilled: L lines, U units, cost C` with C the cost of all its lines.
6. **History.** Every history line gets one more part at its end, after the note if there is one: ` lot L at P` for a movement that made a lot (`receive`, an increase by `adjust` or `count`), and ` cost C` for any other movement, C being the cost of the units that movement moved (for `transfer-in` the same cost as its `transfer-out`; for a fulfilled order, each line's own cost). For example `#4 ship north -3 (order o1) cost 3.75`.
7. **Valuation by lots.** Stock is now worth what its lots cost: the value of an item in a set of warehouses is the sum, over its lots there, of quantity × unit cost. The `value` lines become `SKU: QTY units = VALUE` (`1 unit` for one), and `report` uses the same value. The standard cost no longer changes the value of stock already held: `cost` only affects later lots (it is still shown by `items`).
8. **Report.** The second line of `report` becomes `shipped U units in K shipments, cost C`, where C is the total cost of every `ship` and every fulfilled order (adjustments and transfers are not shipments).
9. **New command `lots WH SKU`** (exactly two arguments, else `error: bad command`). Errors in order: `error: unknown warehouse WH`, `error: unknown item SKU`. If the item has no stock there: `no lots of SKU in WH`. Otherwise one line per lot in ascending receipt number, `lot L: QTY x P = VALUE` (VALUE = QTY × P), then `total U units = V` (U the quantity there, V the sum of the VALUEs).

`receive` with three or four arguments is valid; with a fourth argument that is not a number it is `error: bad command`. Everything else stays exactly as it is.

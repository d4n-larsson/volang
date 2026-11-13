# Order system: rename "item" to "product"

## The existing program

The program is a small order system. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9`. Money is held in integer cents and printed as `<cents / 100>.<cents % 100 as two digits>` (for example `1250` prints `12.50`, `5` prints `0.05`).

Commands (`SKU`, `NAME` and `ID` are single words):

- `add-item SKU NAME PRICE STOCK` (PRICE in cents and STOCK are numbers): registers an item. Prints `item SKU added`, or `error: item SKU exists` if the SKU is already registered (nothing changes).
- `restock SKU N`: adds N to the item's stock and prints `item SKU stock S` with the new stock S. Unknown SKU: `error: unknown item SKU`.
- `price SKU P`: sets the item's price to P cents and prints `item SKU price <P as money>`. Unknown SKU: `error: unknown item SKU`. Orders already placed keep the price they were placed with.
- `order ID SKU QTY [SKU QTY ...]` (each QTY is a number of at least 1; a SKU may repeat): places an order. Checks, in this order: the ID is not already used by an order (even a cancelled one) — else `error: order ID exists`; then for each distinct SKU in order of first appearance, the SKU is registered — else `error: unknown item SKU` — and the total quantity requested for that SKU is at most its stock — else `error: not enough stock for item SKU`. On an error nothing changes. Otherwise the stock of each item drops by the quantity, its sold count rises by it, and the order records each line with the item's current price. The subtotal is the sum of qty × price; if the subtotal is at least 10000 cents, the discount is subtotal × 5 / 100 (integer division), else 0. The total is subtotal − discount. Prints `order ID placed: U units, total T` where U is the sum of the quantities, followed by ` (discount D)` when the discount is not zero.
- `cancel ID`: cancels an order: each line's quantity goes back to its item's stock and comes off its sold count. Prints `order ID cancelled`; `error: unknown order ID` if there is no such order; `error: order ID already cancelled` if it was cancelled before.
- `show ID`: prints `order ID (active)` or `order ID (cancelled)`, then one line per order line in the order given, `  SKU xQTY @ PRICE` (two leading spaces, PRICE as money), then `  total T`. Unknown ID: `error: unknown order ID`.
- `report`: one line per item in ascending SKU order (byte order), `item SKU NAME price P stock S sold N`, then `revenue R over K orders`, where R is the sum of the totals of the orders that are not cancelled and K is their count.

A command with the wrong number of arguments, or with a non-number (or a zero QTY) where a number is required, prints `error: bad command`. A first word that is not a command prints `error: unknown command WORD`.

## The change

The business now calls items **products**. Rename the concept everywhere in the code (types, functions, variables), and in the program's interface:

- The command `add-item` becomes `add-product` (same arguments). `add-item` is no longer a command: it now prints `error: unknown command add-item` like any other unknown word.
- Every output that said `item` now says `product`: `product SKU added`, `error: product SKU exists`, `error: unknown product SKU`, `error: not enough stock for product SKU`, `product SKU stock S`, `product SKU price P`, and the report lines `product SKU NAME price P stock S sold N`.

Everything else (the word `units`, orders, `show`, the revenue line, the error checks and their order) stays exactly as it is.

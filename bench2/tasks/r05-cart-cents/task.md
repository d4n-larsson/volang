# Shopping cart: prices in cents

## The existing program

The program is a shop with one shopping cart. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9`. Inputs keep every number (and the whole part of every price) below 1000000.

All amounts of money are currently whole currency units: a PRICE in the input is a number, and amounts are printed as plain integers (`12`, `0`). A *percentage* P of an amount A is `A * P / 100` rounded down (all values are non-negative).

Commands (`SKU`, `NAME` and `CODE` are single words):

- `product SKU NAME PRICE`: registers a product. Prints `product SKU added`, or `error: product SKU exists` if the SKU is already registered (nothing changes).
- `price SKU PRICE`: sets the product's price and prints `product SKU price P`. Unknown SKU: `error: unknown product SKU`. Prices are read when the cart is shown or checked out, so a new price applies to what is already in the cart.
- `add SKU QTY` (QTY is a number of at least 1): adds QTY units of the product to the cart. The cart has at most one line per SKU, kept in the order the SKUs were first added. Prints `cart SKU xN` with the line's new quantity N. Unknown SKU: `error: unknown product SKU`.
- `remove SKU QTY` (QTY is a number of at least 1): if the cart has no line for SKU, prints `error: SKU not in cart`; if QTY is more than the line's quantity N, prints `error: only N SKU in cart`. Otherwise the quantity drops by QTY: when it reaches 0 the line is removed and the program prints `cart SKU removed` (adding the SKU again later puts it at the end of the cart), else `cart SKU xN` with the new quantity.
- `promo SKU N` (N is a number of at least 1): "buy N get one free" on the product: of every N+1 units in a line, one is free. Replaces an earlier promo for the SKU. Prints `promo SKU: buy N get 1 free`. Unknown SKU: `error: unknown product SKU`.
- `bulk SKU MIN PCT` (MIN is a number of at least 1, PCT a number from 1 to 100): a bulk discount of PCT percent on a line of the product with a quantity of at least MIN. Replaces an earlier bulk discount for the SKU. Prints `bulk SKU: MIN or more PCT% off`. Unknown SKU: `error: unknown product SKU`.
- `coupon CODE PCT` (PCT from 1 to 100): defines (or redefines) a coupon worth PCT percent off. Prints `coupon CODE: PCT% off`.
- `use CODE`: makes the coupon the active one, replacing any active coupon. Prints `coupon CODE active`, or `error: unknown coupon CODE`. The active coupon's percentage is read at checkout (so redefining it afterwards still counts).
- `cart`: prints `cart empty` for an empty cart. Otherwise one line per cart line in cart order, `SKU xQTY @ PRICE = GROSS` (GROSS = QTY × PRICE), then `items U, gross G` where U is the total quantity and G the sum of the GROSS values.
- `checkout`: for an empty cart prints `error: cart empty` (nothing changes, the active coupon stays). Otherwise, for each cart line in cart order:
  - GROSS = QTY × PRICE.
  - If the SKU has a promo "buy N": F = QTY / (N + 1) (integer division) units are free, worth F × PRICE; else F = 0.
  - BASE = GROSS − F × PRICE.
  - If the SKU has a bulk discount and QTY ≥ MIN: the bulk amount is PCT percent of BASE; else 0.
  - NET = BASE − bulk amount.

  It prints `SKU xQTY @ PRICE = GROSS`, then `  free xF -AMOUNT` (two leading spaces, AMOUNT = F × PRICE) only if that amount is not 0, then `  bulk PCT% -AMOUNT` only if the bulk amount is not 0. After all lines: `subtotal S` (S = sum of NET). If a coupon is active, its discount C is its percentage of S and the program prints `coupon CODE PCT% -C` (even when C is 0); without an active coupon C = 0. Last: `total T` with T = S − C.

  The checkout is then recorded: each line's product gets QTY added to its sold count and NET added to its net sales; the number of sales rises by 1, revenue by T, and discounts by (sum of GROSS) − T. The cart becomes empty and no coupon is active any more (coupon definitions, promos and bulk discounts stay).
- `report`: one line per product with a sold count above 0, in ascending SKU order (byte order), `SKU NAME sold N net R`, then `sales K revenue R discounts D`.

A command with the wrong number of arguments, or with a value that is not as described (a non-number, a QTY, N or MIN of 0, a PCT outside 1-100), prints `error: bad command` and changes nothing. A first word that is not a command prints `error: unknown command WORD`.

## The change

Prices now have cents. Change the program's representation of money to **integer cents everywhere** (no floating point anywhere), and its interface as follows:

- A PRICE (in `product` and `price`) is one or more digits, optionally followed by `.` and exactly one or two digits: `12` is 12.00, `12.5` is 12.50, `12.34` is 12.34, `0.05` is 0.05, `007` is 7.00. Anything else is `error: bad command`, for example `12.`, `.5`, `12.345`, `1.2.3`, `1,5`, `-1`, `+1`.
- Every amount of money in the output is printed as `X.YY`: the whole units (no leading zeros, `0` when below one unit), a `.`, then exactly two digits of cents. Examples: `1250` cents prints `12.50`, `5` prints `0.05`, `0` prints `0.00`. This applies to every price, GROSS, gross, free and bulk amount, coupon discount, subtotal, total, net, revenue and discounts in the output above.
- A percentage P of an amount of A cents (bulk and coupon discounts) is now rounded **half up to a whole cent**: `(A * P + 50) / 100` with integer division. For example 15% of 55.45 is 8.32 and 10% of 4.95 is 0.50.

Everything else stays exactly as it is: quantities, counts and percentages are still printed as plain integers, and all messages, checks and their order, and line ordering are unchanged.
